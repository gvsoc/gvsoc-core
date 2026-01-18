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
 * Authors: Germain Haugou (germain.haugou@gmail.com)
 */

#pragma once

#include "io_common.hpp"
#include "vp/queue.hpp"
#include "vp/vp.hpp"

namespace vp {

class IoAccSlave;
class IoAccReq;

typedef enum { IO_ACC_REQ_GRANTED, IO_ACC_REQ_DENIED, IO_ACC_REQ_DONE } IoAccReqStatus;

typedef enum { IO_ACC_RESP_OK, IO_ACC_RESP_INVALID } IoAccRespStatus;

typedef IoAccReqStatus(IoAccSlaveReqMeth)(vp::Block *, vp::IoAccReq *);
typedef IoAccReqStatus(IoAccSlaveReqMethMuxed)(vp::Block *, IoAccReq *, int id);

typedef void(IoAccMasterRespMeth)(vp::Block *, vp::IoAccReq *);
typedef void(IoAccMasterRespMethMuxed)(vp::Block *, vp::IoAccReq *, int id);
typedef void(IoAccMasterRetryMeth)(vp::Block *);
typedef void(IoAccMasterRetryMethMuxed)(vp::Block *, int id);

#define IO_REQ vp::IoAccReq
#define IO_MASTER vp::IoAccMaster
#define IO_SLAVE vp::IoAccSlave
#define IO_REQ_STATUS vp::IoAccReqStatus

class IoAccReq {
    friend class IoAccMaster;
    friend class IoAccSlave;

  public:
    IoAccReq() {}

    IoAccReq(uint64_t addr, uint8_t *data, uint64_t size, bool is_write)
        : addr(addr), data(data), size(size), opcode((IoReqOpcode)is_write) {}

    void set_next(IoAccReq *req) { next = req; }
    IoAccReq *get_next() { return next; }

    uint64_t get_addr() { return addr; }
    void set_addr(uint64_t value) { addr = value; }

    bool get_is_write() { return (bool)this->opcode; }
    void set_is_write(bool is_write) { this->opcode = (IoReqOpcode)is_write; }

    IoReqOpcode get_opcode() { return this->opcode; }
    void set_opcode(IoReqOpcode opcode) { this->opcode = opcode; }

    void set_size(uint64_t size) { this->size = size; }
    uint64_t get_size() { return size; }

    uint8_t *get_data() { return data; }
    void set_data(uint8_t *data) { this->data = data; }

    uint8_t *get_second_data() { return this->second_data; }
    void set_second_data(uint8_t *data) { this->second_data = data; }

    IoAccRespStatus get_resp_status() { return this->status; }
    void set_resp_status(IoAccRespStatus status) { this->status = status; }

    uint64_t addr;
    uint8_t *data;
    // Non-initialized flags for additional atomics data
    uint8_t *second_data;
    uint64_t size;
    IoReqOpcode opcode;
    IoAccRespStatus status;
    bool is_first;
    bool is_last;

    IoAccReq *next;
    IoAccReq *parent;
    void *initiator;
    // Temporary field
    uint64_t remaining_size;
};

/*
 * Class for IO master ports
 */
class IoAccMaster : public vp::MasterPort {
    friend class IoAccSlave;

  public:
    // Constructor
    inline IoAccMaster(IoAccMasterRetryMeth *retry_meth, IoAccMasterRespMeth *resp_meth);

    inline IoAccMaster(int id, IoAccMasterRetryMethMuxed *retry_meth, IoAccMasterRespMethMuxed *resp_meth);

    /*
     * Master binding methods
     */

    // Return if this master port is bound.
    bool is_bound();

    // Can be called by master component to send an IO request.
    inline IoAccReqStatus req(IoAccReq *req);

    /*
     * Reserved for framework
     */

    // Called by the framework to bind the master port to a slave port.
    virtual inline void bind_to(vp::Port *port, js::Config *config);

    // Called by the framework to finalize the binding, for example in order
    // to take into account cross-domains bindings
    void finalize();

  private:
    /*
     * Master callbacks
     */

    // Grant callback set by the user.
    // This gets called anytime the slave is granting an IO request.
    // This is set to an empty callback by default.
    void *retry_meth;

    // Response callback set by the user.
    // This gets called anytime the slave is sending back a response.
    // This is set to an empty callback by default.
    void *resp_meth;

    /*
     * Slave callbacks
     */

    // Callback set by the user on slave port and retrieved during binding
    void *slave_req_meth;

    /*
     * Stubs
     */

    // This is a stub setup when the slave is multiplexing the port so that we
    // can capture the master call and transform it into the slave call with the
    // right mux ID.
    static inline IoAccReqStatus req_muxed_stub(IoAccMaster *_this, IoAccReq *req);

    /*
     * Internal data
     */

    // Slave context when slave port is multiplexed.
    // We keep here a copy of the slave context when the slave port is multiplexed
    // as the normal variable for this context is used to store ourself so that
    // the stub is working well.
    vp::Block *slave_context_for_mux;

    void *slave_req_meth_for_mux;

    // This data is the multiplex ID that we need to send to the slave when the slave port
    // is multiplexed.
    int slave_mux_id;

    // Multiplexed ID set by the slave when port is multiplxed
    int mux_id = -1;
};

/*
 * Class for IO slave ports
 */
class IoAccSlave : public vp::SlavePort {

    friend class IoAccMaster;

  public:

    // Constructor
    inline IoAccSlave(IoAccSlaveReqMeth *meth);
    inline IoAccSlave(int id, IoAccSlaveReqMethMuxed *meth);

    /*
     * Slave binding methods
     */

    // Can be called to grant an IO request.
    // Granting a request means that the request is accepted and owned by the slave
    // and that the master can consider the request gone and then proceeed with
    // the rest.
    inline void retry()
    {
        IoAccMasterRetryMeth *meth = (IoAccMasterRetryMeth *)this->master_retry_meth;
        meth((vp::Block *)this->get_remote_context());
    }

    // Can be called to reply to an IO request.
    // Replying means that the slave has finished handing the request and it is now
    // owned back by the master which can then proceed with the request.
    inline void resp(IoAccReq *req)
    {
        IoAccMasterRespMeth *meth = (IoAccMasterRespMeth *)this->master_resp_meth;
        printf("CALL RESP %p %p\n", this, meth);
        meth((vp::Block *)this->get_remote_context(), req);
    }

    /*
     * Reserved for framework
     */

    // Called by the framework to bind the slave port to a master port.
    inline void bind_to(vp::Port *_port, js::Config *config);

    // Called by the framework to finalize the binding, for example in order
    // to take into account cross-domains bindings
    void finalize();

  private:
    /*
     * Slave callbacks
     */

    // Request callback set by the user.
    // This gets called anytime the master is sending a request.
    // This is set to an empty callback by default.
    void *req_meth;

    /*
     * Master callbacks
     */

    // Response callback set by the user on master port and retrived during binding
    void *master_resp_meth;

    // Grant callback set by the user on master port and retrived during binding
    void *master_retry_meth;

    /*
     * Stubs
     */

    // This is a stub setup when the slave is multiplexing the port so that we
    // can capture the master call and transform it into the slave call with the
    // right mux ID.
    static inline void retry_muxed_stub(IoAccSlave *_this);

    // This is a stub setup when the slave is multiplexing the port so that we
    // can capture the master call and transform it into the slave call with the
    // right mux ID.
    static inline void resp_muxed_stub(IoAccSlave *_this, IoAccReq *req);

    /*
     * Internal data
     */

    // Multiplexed ID set by the slave when port is multiplxed
    int mux_id = -1;

    // This data is the multiplex ID that we need to send to the slave when the slave port
    // is multiplexed.
    int master_mux_id = -1;

    // Slave context when slave port is multiplexed.
    // We keep here a copy of the slave context when the slave port is multiplexed
    // as the normal variable for this context is used to store ourself so that
    // the stub is working well.
    vp::Block *master_context_for_mux = NULL;

    void *master_retry_meth_for_mux;

    void *master_resp_meth_for_mux;

};

inline IoAccMaster::IoAccMaster(IoAccMasterRetryMeth *retry_meth, IoAccMasterRespMeth *resp_meth)
    : resp_meth((void *)resp_meth), retry_meth((void *)retry_meth)
{
}

inline IoAccMaster::IoAccMaster(int id, IoAccMasterRetryMethMuxed *retry_meth, IoAccMasterRespMethMuxed *resp_meth)
    : mux_id(id), resp_meth((void *)resp_meth), retry_meth((void *)retry_meth)
{
}

inline IoAccReqStatus IoAccMaster::req(IoAccReq *req)
{
    IoAccSlaveReqMeth *meth = (IoAccSlaveReqMeth *)this->slave_req_meth;
    return meth((vp::Block *)this->get_remote_context(), req);
}

inline void IoAccMaster::bind_to(vp::Port *_port, js::Config *config) {
    IoAccSlave *port = (IoAccSlave *)_port;
    this->remote_port = port;

    vp_assert(port != NULL, this->get_owner()->get_trace(), "Binding to NULL slave port\n");

    if (port->mux_id == -1)
    {
        // Normal binding, just register the method and context into the master
        // port for fast access
        this->slave_req_meth = port->req_meth;
        this->set_remote_context(port->get_context());
    }
    else
    {
        // Multiplexed binding, tweak the normal callback so that we enter
        // the stub to insert the multiplex ID.
        this->slave_req_meth_for_mux = port->req_meth;
        this->slave_req_meth = (void *)&IoAccMaster::req_muxed_stub;

        this->set_remote_context(this);
        this->slave_context_for_mux = (vp::Block *)port->get_context();
        this->slave_mux_id = port->mux_id;
    }
}

inline bool IoAccMaster::is_bound() { return this->remote_port != NULL; }

inline IoAccReqStatus IoAccMaster::req_muxed_stub(IoAccMaster *_this, IoAccReq *req)
{
    // The normal callback was tweaked in order to get there when the master is sending a
    // request. Now generate the normal call with the mux ID using the saved handler
    IoAccSlaveReqMethMuxed *meth = (IoAccSlaveReqMethMuxed *)_this->slave_req_meth_for_mux;
    return meth((vp::Block *)_this->slave_context_for_mux, req, _this->slave_mux_id);
}

inline void IoAccMaster::finalize()
{
}

inline IoAccSlave::IoAccSlave(IoAccSlaveReqMeth *meth)
: req_meth((void *)meth)
{
}

inline IoAccSlave::IoAccSlave(int id, IoAccSlaveReqMethMuxed *meth)
: mux_id(id), req_meth((void *)meth)
{
}

inline void IoAccSlave::retry_muxed_stub(IoAccSlave *_this) {
    // The normal callback was tweaked in order to get there when the master is sending a
    // request. Now generate the normal call with the mux ID using the saved handler
    IoAccMasterRetryMethMuxed *meth = (IoAccMasterRetryMethMuxed *)_this->master_retry_meth_for_mux;
    meth((vp::Block *)_this->master_context_for_mux, _this->master_mux_id);
}

inline void IoAccSlave::resp_muxed_stub(IoAccSlave *_this, IoAccReq *req) {
    // The normal callback was tweaked in order to get there when the master is sending a
    // request. Now generate the normal call with the mux ID using the saved handler
    IoAccMasterRespMethMuxed *meth = (IoAccMasterRespMethMuxed *)_this->master_resp_meth_for_mux;
    printf("MUX CALL RESP %p %p\n", _this, meth);
    meth((vp::Block *)_this->master_context_for_mux, req, _this->master_mux_id);
}

inline void IoAccSlave::bind_to(vp::Port *_port, js::Config *config) {
    // Instantiate a new slave port which is just used as a reference to reply
    // to the correct master port
    SlavePort::bind_to(_port, config);
    IoAccMaster *port = (IoAccMaster *)_port;

    if (port->mux_id == -1)
    {
        printf("BIND %p to %p %p\n", this, port, port->resp_meth);
        this->master_resp_meth = port->resp_meth;
        this->master_retry_meth = port->retry_meth;
        this->set_remote_context(port->get_context());
    }
    else
    {
        this->master_retry_meth_for_mux = port->retry_meth;
        this->master_retry_meth = (void *)&IoAccSlave::retry_muxed_stub;

        this->master_resp_meth_for_mux = port->resp_meth;
        this->master_resp_meth = (void *)&IoAccSlave::resp_muxed_stub;

        this->set_remote_context(this);
        this->master_context_for_mux = (vp::Block *)port->get_context();
        this->master_mux_id = port->mux_id;
    }
}

inline void IoAccSlave::finalize()
{
}

};
