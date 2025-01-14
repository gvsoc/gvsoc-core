/*
 * Copyright (C) 2020 ETH Zurich and University of Bologna
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Authors: Germain Haugou, ETH Zurich (germain.haugou@iis.ee.ethz.ch)
 */

#include <vp/vp.hpp>
#include <vp/itf/io.hpp>
#include <vector>
#include <thread>
#include <unistd.h>
#include <mutex>
#include <condition_variable>
#include <gv/gvsoc.hpp>
#include <vp/controller.hpp>

static int nb_running = 0;

#define MAX_MEMINFO 64

class emulation : public vp::Component, public gv::Io_binding
{

public:
    emulation(vp::ComponentConf &conf);

    void reset(bool active);

    void *external_bind(std::string comp_name, std::string itf_name, void *handle);

    void grant(gv::Io_request *req);
    void reply(gv::Io_request *req);
    void access(gv::Io_request *req);

private:
    void sync_state(std::unique_lock<std::mutex> &lock);
    void check_state();
    static void clock_sync(vp::Block *__this, bool active);
    static void fetchen_sync(vp::Block *__this, bool active);
    static void bootaddr_sync(vp::Block *__this, uint32_t value);
    static void irq_req_sync(vp::Block *__this, int irq);
    static void data_grant(vp::Block *__this, vp::IoReq *req);
    static void data_response(vp::Block *__this, vp::IoReq *req);
    static void flush_cache_sync(vp::Block *__this, bool active);
    static void flush_cache_ack_sync(vp::Block *__this, bool active);

    vp::Trace trace;

    vp::IoSlave dbg_unit;
    vp::WireSlave<uint32_t> bootaddr_itf;
    vp::IoMaster data;
    vp::IoMaster data_debug;
    vp::IoMaster fetch;
    vp::WireMaster<void *>     meminfo[MAX_MEMINFO];
    vp::WireSlave<int>      irq_req_itf;
    vp::WireMaster<int>     irq_ack_itf;
    vp::WireSlave<bool>     flush_cache_itf;
    vp::WireSlave<bool>     flush_cache_ack_itf;
    vp::WireMaster<bool>    flush_cache_req_itf;
    vp::WireSlave<bool>     halt_itf;
    vp::WireMaster<bool>    halt_status_itf;
    vp::WireMaster<bool>     busy_itf;
    vp::WireMaster<uint32_t> ext_counter[32];
    vp::WireSlave<bool>     clock_itf;
    vp::WireSlave<bool>     fetchen_itf;

    std::vector<std::thread> threads;

    gv::Io_user   *user;

    std::mutex mutex;
    std::condition_variable cond;

    vp::IoReq core_req;

    bool reset_value;
    bool fetchen_value;
    bool clock_value;
    bool is_active;
    bool stalled;
    bool sleeping;
    bool irq_enabled;

    int core_id;
    int cluster_id;

    uint32_t pending_irq;
    void (*irq_handler[32])(void *);
    void *irq_handler_arg[32];
};


static __thread void *__this;


emulation::emulation(vp::ComponentConf &config)
    : vp::Component(config)
{
    traces.new_trace("trace", &trace, vp::DEBUG);

    new_slave_port("dbg_unit", &this->dbg_unit);

    bootaddr_itf.set_sync_meth(&emulation::bootaddr_sync);
    new_slave_port("bootaddr", &this->bootaddr_itf);

    this->data.set_resp_meth(&emulation::data_response);
    this->data.set_grant_meth(&emulation::data_grant);
    new_master_port("data", &this->data);
    new_master_port("data_debug", &this->data_debug);
    new_master_port("fetch", &this->fetch);

    for (int i=0; i<MAX_MEMINFO; i++)
    {
        new_master_port("meminfo_" + std::to_string(i), &this->meminfo[i]);
    }

    this->irq_req_itf.set_sync_meth(&emulation::irq_req_sync);
    new_slave_port("irq_req", &irq_req_itf);
    new_master_port("irq_ack", &this->irq_ack_itf);

    this->flush_cache_itf.set_sync_meth(&emulation::flush_cache_sync);
    new_slave_port("flush_cache", &this->flush_cache_itf);

    this->flush_cache_ack_itf.set_sync_meth(&emulation::flush_cache_ack_sync);
    new_slave_port("flush_cache_ack", &this->flush_cache_ack_itf);
    new_master_port("flush_cache_req", &this->flush_cache_req_itf);

    new_slave_port("halt", &this->halt_itf);
    new_master_port("halt_status", &this->halt_status_itf);

    new_master_port("busy", &this->busy_itf);

    this->clock_itf.set_sync_meth(&emulation::clock_sync);
    new_slave_port("clock_en", &this->clock_itf);

    this->fetchen_itf.set_sync_meth(&emulation::fetchen_sync);
    new_slave_port("fetchen", &this->fetchen_itf);

    for (int i=0; i<32; i++)
    {
        new_master_port("ext_counter[" + std::to_string(i) + "]", &this->ext_counter[i]);
    }

    nb_running++;
    this->reset_value = true;
    this->is_active = true;
    this->clock_value = true;
    this->stalled = false;
    this->sleeping = false;
    this->irq_enabled = false;
    this->fetchen_value = get_js_config()->get("fetch_enable")->get_bool();
    this->core_id = get_js_config()->get_child_int("core_id");
    this->cluster_id = get_js_config()->get_child_int("cluster_id");

}

void emulation::reset(bool active)
{
    this->trace.msg(vp::Trace::LEVEL_DEBUG, "Setting reset (active: 0x%x)\n", active);
    if (active)
    {
        this->pending_irq = -1;
    }

    this->reset_value = active;
    std::unique_lock<std::mutex> lock(this->mutex);
    this->check_state();
    lock.unlock();
}

void *emulation::external_bind(std::string comp_name, std::string itf_name, void *handle)
{
    if (comp_name == this->get_path())
    {
        this->user = (gv::Io_user *)handle;
        return static_cast<gv::Io_binding *>(this);
    }
    else
    {
        return NULL;
    }
}


void emulation::grant(gv::Io_request *io_req)
{
}

void emulation::reply(gv::Io_request *io_req)
{
}

void emulation::access(gv::Io_request *req)
{
    this->trace.msg(vp::Trace::LEVEL_DEBUG,
        "Received IO req  (offset: 0x%llx, size: 0x%x, type: %d)\n",
        req->addr, req->size, req->type);

    // Sync state request
    if (req->type == 2)
    {
        std::unique_lock<std::mutex> lock(this->mutex);
        this->sync_state(lock);
        lock.unlock();
    }
    // WFI request
    else if (req->type == 3)
    {
        std::unique_lock<std::mutex> lock(this->mutex);
        this->sync_state(lock);
        this->sleeping = true;
        this->check_state();
        this->sync_state(lock);
        lock.unlock();
    }
    // IRQ DISABLE
    else if (req->type == 4)
    {
        std::unique_lock<std::mutex> lock(this->mutex);
        *(uint32_t *)req->data = this->irq_enabled;
        this->irq_enabled = false;
        this->sync_state(lock);
        lock.unlock();
    }
    // IRQ RESTORE
    else if (req->type == 5)
    {
        std::unique_lock<std::mutex> lock(this->mutex);
        this->irq_enabled = *(uint32_t *)req->data;
        this->sync_state(lock);
        lock.unlock();
    }
    // IRQ SET HANDLER
    else if (req->type == 6)
    {
        std::unique_lock<std::mutex> lock(this->mutex);
        this->irq_handler[req->addr] = (void (*)(void *))req->data;
        this->irq_handler_arg[req->addr] = (void *)req->size;
        lock.unlock();
    }
    // MEMINFO
    else if (req->type == 7)
    {
        gv::Controller::get().engine_lock();
        this->meminfo[0].sync_back((void **)&req->data);
        gv::Controller::get().engine_unlock();
    }
    // CORE ID
    else if (req->type == 8)
    {
        req->addr = this->core_id;
    }
    // Register range
    else if (req->type == 10)
    {
        if (req->size >= MAX_MEMINFO)
        {
            throw logic_error("Invalid meminfo (index: " + std::to_string(req->size) + ")");
        }

        this->meminfo[req->size+1].sync((void *)req->addr);
    }
    // Read write request
    else
    {
        this->core_req.init();
        this->core_req.set_addr(req->addr);
        this->core_req.set_size(req->size);
        this->core_req.set_is_write(req->type == gv::Io_request_write);
        this->core_req.set_data(req->data);

        gv::Controller::get().engine_lock();
        int err = this->data.req(&this->core_req);
        gv::Controller::get().engine_unlock();

        if (err == vp::IO_REQ_OK || err == vp::IO_REQ_INVALID)
        {
            req->retval = err == vp::IO_REQ_INVALID ? gv::Io_request_ko : gv::Io_request_ok;
            this->user->reply(req);
        }
        else
        {
            std::unique_lock<std::mutex> lock(this->mutex);
            this->stalled = true;
            this->check_state();
            this->sync_state(lock);
            lock.unlock();
        }
    }
}

void emulation::sync_state(std::unique_lock<std::mutex> &lock)
{
    if (this->irq_enabled && this->pending_irq != -1)
    {
        int irq = this->pending_irq;
        void (*handler)(void *) = this->irq_handler[irq];

        this->pending_irq = -1;

        gv::Controller::get().engine_lock();
        this->irq_ack_itf.sync(irq);
        gv::Controller::get().engine_unlock();

        if (handler)
        {
            handler(this->irq_handler_arg[irq]);
        }

        lock.lock();

    }


    if (!this->is_active)
    {
        nb_running--;
        if (nb_running == 0)
        {
            this->trace.msg(vp::Trace::LEVEL_DEBUG, "Running engine\n");
            lock.unlock();
            this->get_launcher()->run_async(gv::Controller::get().default_client_get());
            lock.lock();
        }

        while(!this->is_active)
        {
            this->cond.wait(lock);
        }

        nb_running++;
        if (nb_running == 1)
        {
            this->trace.msg(vp::Trace::LEVEL_DEBUG, "Stopping engine\n");
            lock.unlock();
            this->get_launcher()->stop(gv::Controller::get().default_client_get());
            lock.lock();
        }
    }
}

void emulation::check_state()
{
    this->trace.msg(vp::Trace::LEVEL_DEBUG,
        "Checking core state (is_active: %d, reset: %d, fetchen: %d, clock: %d, stalled: %d, sleeping: %d)\n",
        this->is_active, this->reset_value, this->fetchen_value, this->clock_value, this->stalled, this->sleeping
    );

    if (this->is_active)
    {
        if (this->reset_value || !this->fetchen_value || !this->clock_value || this->stalled || this->sleeping)
        {
            this->trace.msg(vp::Trace::LEVEL_DEBUG, "Deactivating core\n");
            this->is_active = false;
        }
    }
    else
    {
        if (!this->reset_value && this->fetchen_value && this->clock_value && !this->stalled && !this->sleeping)
        {
            this->trace.msg(vp::Trace::LEVEL_DEBUG, "Activating core\n");
            this->is_active = true;
            this->cond.notify_all();
        }
    }
}

void emulation::data_grant(vp::Block *__this, vp::IoReq *req)
{
}

void emulation::data_response(vp::Block *__this, vp::IoReq *req)
{
    emulation *_this = (emulation *)__this;
    std::unique_lock<std::mutex> lock(_this->mutex);
    _this->stalled = false;
    _this->check_state();
    lock.unlock();
}


void emulation::flush_cache_sync(vp::Block *__this, bool active)
{
}

void emulation::flush_cache_ack_sync(vp::Block *__this, bool active)
{
}


void emulation::bootaddr_sync(vp::Block *__this, uint32_t value)
{
  emulation *_this = (emulation *)__this;
  _this->trace.msg(vp::Trace::LEVEL_DEBUG, "Setting boot address (value: 0x%x)\n", value);
}


void emulation::irq_req_sync(vp::Block *__this, int irq)
{
    emulation *_this = (emulation *)__this;
    _this->trace.msg(vp::Trace::LEVEL_DEBUG, "IRQ request (irq: %d)\n", irq);

    std::unique_lock<std::mutex> lock(_this->mutex);
    _this->pending_irq = irq;
    _this->sleeping = false;
    _this->check_state();
    lock.unlock();
}


void emulation::fetchen_sync(vp::Block *__this, bool active)
{
    emulation *_this = (emulation *)__this;
    _this->trace.msg(vp::Trace::LEVEL_DEBUG,"Setting fetch enable (active: %d)\n", active);
    std::unique_lock<std::mutex> lock(_this->mutex);
    _this->fetchen_value = active;
    _this->check_state();
    lock.unlock();
}


void emulation::clock_sync(vp::Block *__this, bool active)
{
    emulation *_this = (emulation *)__this;
    _this->trace.msg(vp::Trace::LEVEL_DEBUG, "Setting clock (active: %d)\n", active);
    std::unique_lock<std::mutex> lock(_this->mutex);
    _this->clock_value = active;
    _this->check_state();
    lock.unlock();
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new emulation(config);
}
