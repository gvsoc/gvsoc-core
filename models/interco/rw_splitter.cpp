// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

#include <vp/itf/io.hpp>
#include <vp/vp.hpp>

class RwSplitter : public vp::Component {
  public:
    RwSplitter(vp::ComponentConf &conf);

  private:
    static vp::IoReqStatus req(vp::Block *__this, vp::IoReq *req);
    static void grant(vp::Block *__this, vp::IoReq *req);
    static void resp(vp::Block *__this, vp::IoReq *req);

    vp::Trace trace;

    vp::IoSlave input;
    vp::IoMaster output_read, output_write;
};

RwSplitter::RwSplitter(vp::ComponentConf &config) : vp::Component(config) {
    traces.new_trace("trace", &trace, vp::DEBUG);

    this->input.set_req_meth(&RwSplitter::req);
    this->new_slave_port("input", &this->input);

    this->output_read.set_grant_meth(&RwSplitter::grant);
    this->output_read.set_resp_meth(&RwSplitter::resp);
    this->new_master_port("output_read", &this->output_read);

    this->output_write.set_grant_meth(&RwSplitter::grant);
    this->output_write.set_resp_meth(&RwSplitter::resp);
    this->new_master_port("output_write", &this->output_write);
}

vp::IoReqStatus RwSplitter::req(vp::Block *__this, vp::IoReq *req) {
    RwSplitter *_this = (RwSplitter *)__this;
    uint64_t offset = req->get_addr();
    bool is_write = req->get_is_write();
    uint64_t size = req->get_size();
    uint8_t *data = req->get_data();

    _this->trace.msg(vp::Trace::LEVEL_DEBUG,
        "Received IO req (req: %p, offset: 0x%llx, size: 0x%llx, is_write: %d)\n",
        req, offset, size, is_write);

    if (is_write) {
        return _this->output_write.req(req);
    } else {
        return _this->output_read.req(req);
    }

    return vp::IO_REQ_OK;
}

void RwSplitter::grant(vp::Block *__this, vp::IoReq *req) {
    RwSplitter *_this = (RwSplitter *)__this;
    _this->input.grant(req);
}

void RwSplitter::resp(vp::Block *__this, vp::IoReq *req) {
    RwSplitter *_this = (RwSplitter *)__this;
    _this->input.resp(req);
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config) { return new RwSplitter(config); }
