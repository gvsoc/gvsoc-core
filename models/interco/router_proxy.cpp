/*
 * Copyright (C) 2021 GreenWaves Technologies, SAS, SAS, ETH Zurich and University of Bologna
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
 * Authors: Germain Haugou, GreenWaves Technologies (germain.haugou@greenwaves-technologies.com)
 */

#include <vp/vp.hpp>
#include <vp/itf/io.hpp>
#include <stdio.h>
#include <gv/gvsoc.hpp>
#include <vp/launcher.hpp>


class Router_proxy : public vp::Component, public gv::Io_binding
{

public:

    Router_proxy(vp::ComponentConf &conf);

    void grant(gv::Io_request *req);
    void reply(gv::Io_request *req);
    void access(gv::Io_request *req);

    void *external_bind(std::string comp_name, std::string itf_name, void *handle);

    static vp::IoReqStatus req(vp::Block *__this, vp::IoReq *req);

    static void grant(vp::Block *__this, vp::IoReq *req);

    static void response(vp::Block *__this, vp::IoReq *req);

private:
    vp::Trace     trace;
    vp::IoSlave  in;
    vp::IoMaster out;
    gv::Io_user   *user = NULL;
};

Router_proxy::Router_proxy(vp::ComponentConf &config)
: vp::Component(config)
{
    traces.new_trace("trace", &trace, vp::DEBUG);

    in.set_req_meth(&Router_proxy::req);
    new_slave_port("input", &in);

    out.set_resp_meth(&Router_proxy::response);
    out.set_grant_meth(&Router_proxy::grant);
    new_master_port("out", &out);


}

vp::IoReqStatus Router_proxy::req(vp::Block *__this, vp::IoReq *req)
{
    Router_proxy *_this = (Router_proxy *)__this;
    gv::Io_request *io_req = new gv::Io_request();
    io_req->addr = req->get_addr();
    io_req->size = req->get_size();
    io_req->data = req->get_data();
    io_req->type = req->get_is_write() ? gv::Io_request_write : gv::Io_request_read;
    io_req->handle = (void *)req;

    if (_this->user == NULL)
    {
        _this->trace.warning("Trying to access router proxy while it is not connected\n");
        return vp::IO_REQ_INVALID;
    }

    _this->user->access(io_req);

    // Mark the request as sent so that the callbacks can now handle it as asynchronous
    io_req->sent = true;

    // Check if the request was already handled during the access call in order to 
    // return the proper code
    if (io_req->replied)
    {
        return vp::IO_REQ_OK;
    }
    else if (io_req->granted)
    {
        return vp::IO_REQ_PENDING;
    }
    else
    {
        return vp::IO_REQ_DENIED;
    }
}


void *Router_proxy::external_bind(std::string comp_name, std::string itf_name, void *handle)
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


void Router_proxy::grant(vp::Block *__this, vp::IoReq *req)
{
    Router_proxy *_this = (Router_proxy *)__this;

    gv::Io_request *io_req = (gv::Io_request *)req->arg_pop();

    _this->user->reply(io_req);
}

void Router_proxy::response(vp::Block *__this, vp::IoReq *req)
{
    Router_proxy *_this = (Router_proxy *)__this;

    gv::Io_request *io_req = (gv::Io_request *)req->arg_pop();
    io_req->retval = req->status == vp::IO_REQ_INVALID ? gv::Io_request_ko : gv::Io_request_ok;

    _this->user->reply(io_req);
}

void Router_proxy::grant(gv::Io_request *io_req)
{
    // We hve to check if the call to this callback is asynchronous or done during
    // the handling of the request.
    if (io_req->sent)
    {
        // Asynchronous, just forward to the engine after locking
        this->time.get_engine()->lock();
        vp::IoReq *req = (vp::IoReq *)io_req->handle;
        req->get_resp_port()->grant(req);
        this->time.get_engine()->unlock();
    }
    else
    {
        // Otherwise, just mark it so that the synchronous call can return the proper error code
        io_req->granted = true;
    }
}

void Router_proxy::reply(gv::Io_request *io_req)
{
    // We hve to check if the call to this callback is asynchronous or done during
    // the handling of the request.
    if (io_req->sent)
    {
        // Asynchronous, just forward to the engine after locking
        this->time.get_engine()->lock();
        vp::IoReq *req = (vp::IoReq *)io_req->handle;
        req->get_resp_port()->resp(req);
        this->time.get_engine()->unlock();
        delete io_req;
    }
    else
    {
        // Otherwise, just mark it so that the synchronous call can return the proper error code
        io_req->replied = true;
    }
}

void Router_proxy::access(gv::Io_request *io_req)
{
    if (this->get_launcher()->get_is_async())
    {
        this->time.get_engine()->lock();
    }
    vp::IoReq *req = new vp::IoReq();
    req->init();
    req->set_addr(io_req->addr);
    req->set_size(io_req->size);
    req->set_is_write(io_req->type == gv::Io_request_write);
    req->set_data(io_req->data);
    req->arg_push(io_req);

    int err = this->out.req(req);
    if (err == vp::IO_REQ_OK || err == vp::IO_REQ_INVALID)
    {
        this->response(this, req);
    }
    if (this->get_launcher()->get_is_async())
    {
        this->time.get_engine()->unlock();
    }
}



extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new Router_proxy(config);
}
