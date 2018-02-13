/*
 * Copyright (c) 2016, Nils Asmussen
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are
 * those of the authors and should not be interpreted as representing official
 * policies, either expressed or implied, of the FreeBSD Project.
 */

#include "cpu/dtu-accel/ctxsw.hh"

AccelContextSwitch::AccelContextSwitch(DtuAccel *_accel)
    : ctxSize(_accel->contextSize()), accel(_accel), state(), stateChanged(),
      offset(), ctxSwPending()
{
}

std::string
AccelContextSwitch::stateName() const
{
    const char *names[] =
    {
        "SAVE",
        "SAVE_WRITE",
        "SAVE_SEND",
        "SAVE_WAIT",
        "SAVE_DONE",
        "WAIT",
        "CHECK",
        "FLAGS",
        "RESTORE",
        "RESTORE_WAIT",
        "RESTORE_READ",
        "RESTORE_DONE",
    };
    return names[static_cast<size_t>(state)];
}

PacketPtr
AccelContextSwitch::tick()
{
    PacketPtr pkt = nullptr;

    switch(state)
    {
        case State::WAIT:
            break;

        case State::SAVE:
        {
            Addr regAddr = accel->getRegAddr(CmdReg::ABORT);
            uint64_t value = Dtu::Command::ABORT_VPE;
            pkt = accel->createDtuRegPkt(regAddr, value, MemCmd::WriteReq);
            break;
        }
        case State::SAVE_WRITE:
        {
            size_t rem = ctxSize - offset;
            size_t size = std::min(accel->chunkSize, rem);
            pkt = accel->createPacket(
                (accel->bufferAddr() - ctxSize) + offset,
                size,
                MemCmd::WriteReq
            );
            memcpy(pkt->getPtr<uint8_t>(), (char*)accel->context() + offset, size);
            break;
        }
        case State::SAVE_SEND:
        {
            size_t rem = ctxSize + accel->stateSize() - offset;
            size_t size = std::min(accel->maxDataSize, rem);
            pkt = accel->createDtuCmdPkt(
                Dtu::Command::WRITE,
                accel->contextEp(),
                (accel->bufferAddr() - ctxSize) + offset,
                size,
                offset
            );
            offset += size;
            break;
        }
        case State::SAVE_WAIT:
        {
            Addr regAddr = accel->getRegAddr(CmdReg::COMMAND);
            pkt = accel->createDtuRegPkt(regAddr, 0, MemCmd::ReadReq);
            break;
        }
        case State::SAVE_DONE:
        {
            pkt = accel->createPacket(
                DtuAccel::RCTMUX_FLAGS, sizeof(uint64_t), MemCmd::WriteReq
            );
            *pkt->getPtr<uint64_t>() = DtuAccel::RCTMuxCtrl::SIGNAL;
            break;
        }

        case State::CHECK:
        {
            Addr regAddr = accel->getRegAddr(ReqReg::EXT_REQ);
            pkt = accel->createDtuRegPkt(
                regAddr, sizeof(uint64_t), MemCmd::WriteReq
            );
            *pkt->getPtr<uint64_t>() = 0;
            break;
        }
        case State::FLAGS:
        {
            pkt = accel->createPacket(
                DtuAccel::RCTMUX_FLAGS, sizeof(uint64_t), MemCmd::ReadReq
            );
            break;
        }

        case State::RESTORE:
        {
            size_t rem = ctxSize + accel->stateSize() - offset;
            size_t size = std::min(accel->maxDataSize, rem);
            pkt = accel->createDtuCmdPkt(
                Dtu::Command::READ,
                accel->contextEp(),
                (accel->bufferAddr() - ctxSize) + offset,
                size,
                offset
            );
            offset += size;
            break;
        }
        case State::RESTORE_WAIT:
        {
            Addr regAddr = accel->getRegAddr(CmdReg::COMMAND);
            pkt = accel->createDtuRegPkt(regAddr, 0, MemCmd::ReadReq);
            break;
        }
        case State::RESTORE_READ:
        {
            size_t rem = ctxSize - offset;
            size_t size = std::min(accel->chunkSize, rem);
            pkt = accel->createPacket(
                (accel->bufferAddr() - ctxSize) + offset,
                size,
                MemCmd::ReadReq
            );
            break;
        }
        case State::RESTORE_DONE:
        {
            pkt = accel->createPacket(
                DtuAccel::RCTMUX_FLAGS, sizeof(uint64_t), MemCmd::WriteReq
            );
            *pkt->getPtr<uint64_t>() = DtuAccel::RCTMuxCtrl::SIGNAL;
            break;
        }
    }

    return pkt;
}

bool
AccelContextSwitch::handleMemResp(PacketPtr pkt)
{
    const uint8_t *pkt_data = pkt->getConstPtr<uint8_t>();

    auto lastState = state;

    switch(state)
    {
        case State::WAIT:
        {
            assert(false);
            break;
        }

        case State::SAVE:
        {
            offset = 0;
            state = State::SAVE_WRITE;
            break;
        }
        case State::SAVE_WRITE:
        {
            offset += pkt->getSize();
            if (offset == ctxSize)
            {
                offset = 0;
                state = State::SAVE_SEND;
            }
            break;
        }
        case State::SAVE_SEND:
        {
            state = State::SAVE_WAIT;
            break;
        }
        case State::SAVE_WAIT:
        {
            Dtu::Command::Bits cmd =
                *reinterpret_cast<const RegFile::reg_t*>(pkt_data);
            if (cmd.opcode == 0)
            {
                // don't continue on errors here; maybe we don't have the
                // memory EP yet.
                if (cmd.error != 0 || offset == ctxSize + accel->stateSize())
                    state = State::SAVE_DONE;
                else
                    state = State::SAVE_SEND;
            }
            break;
        }
        case State::SAVE_DONE:
        {
            state = State::WAIT;
            ctxSwPending = true;
            break;
        }

        case State::CHECK:
        {
            state = State::FLAGS;
            break;
        }
        case State::FLAGS:
        {
            uint64_t val = *pkt->getConstPtr<uint64_t>();
            if (val & DtuAccel::RCTMuxCtrl::RESTORE)
            {
                offset = 0;
                state = State::RESTORE;
            }
            else if (val & DtuAccel::RCTMuxCtrl::STORE)
                state = State::SAVE;
            else if (val & DtuAccel::RCTMuxCtrl::WAITING)
                state = State::RESTORE_DONE;
            else if (ctxSwPending)
                state = State::WAIT;
            else
            {
                state = State::CHECK;
                return true;
            }
            break;
        }
        case State::RESTORE:
        {
            ctxSwPending = false;
            state = State::RESTORE_WAIT;
            break;
        }
        case State::RESTORE_WAIT:
        {
            Dtu::Command::Bits cmd =
                *reinterpret_cast<const RegFile::reg_t*>(pkt_data);
            if (cmd.opcode == 0)
            {
                if (offset == ctxSize + accel->stateSize())
                {
                    offset = 0;
                    state = State::RESTORE_READ;
                }
                else
                    state = State::RESTORE;
            }
            break;
        }
        case State::RESTORE_READ:
        {
            memcpy((char*)accel->context() + offset,
                   pkt->getPtr<char>(),
                   pkt->getSize());

            offset += pkt->getSize();
            if (offset == ctxSize)
                state = State::RESTORE_DONE;
            break;
        }
        case State::RESTORE_DONE:
        {
            ctxSwPending = false;
            state = State::CHECK;
            accel->setSwitched();
            return true;
        }
    }

    stateChanged = state != lastState;

    return false;
}
