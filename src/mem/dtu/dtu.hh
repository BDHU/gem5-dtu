/*
 * Copyright (c) 2015, Christian Menard
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are those
 * of the authors and should not be interpreted as representing official policies,
 * either expressed or implied, of the FreeBSD Project.
 */

#ifndef __MEM_DTU_DTU_HH__
#define __MEM_DTU_DTU_HH__

#include "mem/dtu/base.hh"
#include "mem/mem_object.hh"
#include "mem/tport.hh"
#include "params/Dtu.hh"

#include <queue>

class Dtu : public BaseDtu
{
  private:

    struct DeferredPacket
    {
        Tick tick;
        PacketPtr pkt;
        DeferredPacket(Tick _tick, PacketPtr _pkt) : tick(_tick), pkt(_pkt) {}
    };

    class DtuMasterPort : public MasterPort
    {
      public:

        DtuMasterPort( const std::string& _name, Dtu& _dtu)
            : MasterPort(_name, &_dtu),
              dtu(_dtu),
              reqPkt(nullptr),
              waitForRetry(false),
              respQueue(),
              responseEvent(*this)
        { }

      protected:

        Dtu& dtu;

        PacketPtr reqPkt;

        bool waitForRetry;

        std::queue<DeferredPacket> respQueue;

        struct ResponseEvent : public Event
        {
            DtuMasterPort& port;

            ResponseEvent(DtuMasterPort& _port)
                : port(_port)
            { }

            const char *description() const override
            {
                return "DTU ResponseEvent";
            }

            void process() override;
        };

        ResponseEvent responseEvent;

      public:

        void sendRequest(PacketPtr pkt);

        virtual void completeRequest(PacketPtr pkt) = 0;

        bool recvTimingResp(PacketPtr pkt) override;

        void recvReqRetry() override;

        bool isReady() { return !waitForRetry; }
    };

    class NocMasterPort : public DtuMasterPort
    {
      public:

        NocMasterPort(Dtu& _dtu)
            : DtuMasterPort(_dtu.name() + ".noc_master_port", _dtu)
        { }

        void completeRequest(PacketPtr pkt) override;

    };

    class ScratchpadPort : public DtuMasterPort
    {
      public:

        ScratchpadPort(Dtu& _dtu)
          : DtuMasterPort(_dtu.name() + ".scratchpad_port", _dtu)
        { }

        void completeRequest(PacketPtr pkt) override;
    };

    class DtuSlavePort : public SlavePort
    {
      protected:

        Dtu& dtu;

        bool busy;

        bool sendRetry;

        PacketPtr reqPkt;

        PacketPtr respPkt;

        bool waitForRetry;

      public:

        DtuSlavePort(const std::string _name, Dtu& _dtu)
            : SlavePort(_name, &_dtu),
              dtu(_dtu),
              busy(false),
              sendRetry(false),
              reqPkt(nullptr),
              respPkt(nullptr),
              waitForRetry(false),
              requestEvent(*this),
              responseEvent(*this)
        { }

        bool isBusy() { return busy; }

        virtual bool handleRequest(PacketPtr pkt) = 0;

        void schedTimingResp(PacketPtr pkt, Cycles latency);

        Tick recvAtomic(PacketPtr pkt) override;

        void recvFunctional(PacketPtr pkt) override;

        bool recvTimingReq(PacketPtr pkt) override;

        void recvRespRetry() override;

        struct RequestEvent : public Event
        {
            DtuSlavePort& port;

            RequestEvent(DtuSlavePort& _port) : port(_port) {}

            void process() override;

            const char* description() const override
            {
                return "DTU RequestEvent";
            }
        };

        RequestEvent requestEvent;

        struct ResponseEvent : public Event
        {
            DtuSlavePort& port;

            ResponseEvent(DtuSlavePort& _port) : port(_port) {}

            void process() override;

            const char* description() const override
            {
                return "DTU ResponseEvent";
            }
        };

        ResponseEvent responseEvent;
    };

    class CpuPort : public DtuSlavePort
    {
      public:

        CpuPort(Dtu& _dtu)
          : DtuSlavePort(_dtu.name() + ".cpu_port", _dtu)
        { }

      protected:

        AddrRangeList getAddrRanges() const override;

        bool handleRequest(PacketPtr pkt) override;
    };

    class NocSlavePort : public DtuSlavePort
    {
      public:

        NocSlavePort(Dtu& _dtu)
          : DtuSlavePort(_dtu.name() + ".noc_slave_port", _dtu)
        { }

      protected:

        AddrRangeList getAddrRanges() const override;

        bool handleRequest(PacketPtr pkt) override;
    };

    CpuPort        cpu;
    ScratchpadPort scratchpad;
    NocMasterPort  master;
    NocSlavePort   slave;

    void sendSpmRequest(PacketPtr pkt) override;

    void sendNocRequest(PacketPtr pkt) override;

    bool isSpmPortReady() override;

    bool isNocPortReady() override;

    void sendNocResponse(PacketPtr pkt, Cycles latency) override;

    void sendCpuResponse(PacketPtr pkt, Cycles latency) override;

  public:

    Dtu(const DtuParams *p);

    void init() override;

    BaseMasterPort& getMasterPort(const std::string &if_name,
                                  PortID idx = InvalidPortID) override;

    BaseSlavePort& getSlavePort(const std::string &if_name,
                                PortID idx = InvalidPortID) override;
};

#endif // __MEM_DTU_DTU_HH__
