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

#include "cpu/dtu-accel/accelerator.hh"
#include "cpu/dtu-accel/connector.hh"
#include "debug/DtuConnector.hh"

DtuAccelConnector::DtuAccelConnector(const DtuAccelConnectorParams *p)
  : BaseConnector(p),
    acc(p->accelerator)
{
    acc->setConnector(this);
}

void
DtuAccelConnector::wakeup()
{
    DPRINTF(DtuConnector, "Waking up accelerator\n");
    acc->wakeup();
}

void
DtuAccelConnector::setIrq()
{
    DPRINTF(DtuConnector, "Sending interrupt signal to accelerator\n");
    acc->interrupt();
}

void
DtuAccelConnector::reset(Addr, Addr)
{
    DPRINTF(DtuConnector, "Resetting accelerator\n");
    acc->reset();
}

void
DtuAccelConnector::signalFinished(size_t off)
{
    DPRINTF(DtuConnector, "Signaling finish (off=%lu)\n", off);
    acc->signalFinished(off);
}

DtuAccelConnector*
DtuAccelConnectorParams::create()
{
    return new DtuAccelConnector(this);
}
