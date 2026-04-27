// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

#include <vp/itf/io.hpp>
#include <vp/vp.hpp>
#include <interco/remapper/remapper_config.hpp>

class Remapper : public vp::Component {
  public:
    Remapper(vp::ComponentConf &conf);

  private:
    static vp::IoReqStatus req(vp::Block *__this, vp::IoReq *req);
    static void grant(vp::Block *__this, vp::IoReq *req);
    static void resp(vp::Block *__this, vp::IoReq *req);

    RemapperConfig cfg;
    vp::Trace trace;

    vp::IoMaster out;
    vp::IoSlave in;
};

Remapper::Remapper(vp::ComponentConf &config) : vp::Component(config, this->cfg) {
    this->traces.new_trace("trace", &trace, vp::DEBUG);

    this->in.set_req_meth(&Remapper::req);
    this->new_slave_port("input", &this->in);

    this->out.set_grant_meth(&Remapper::grant);
    this->out.set_resp_meth(&Remapper::resp);
    this->new_master_port("output", &this->out);
}

vp::IoReqStatus Remapper::req(vp::Block *__this, vp::IoReq *req) {
    Remapper *_this = (Remapper *)__this;
    uint64_t offset = req->get_addr();
    bool is_write = req->get_is_write();
    uint64_t size = req->get_size();
    uint8_t *data = req->get_data();

    _this->trace.msg(vp::Trace::LEVEL_DEBUG,
        "Received IO req (req: %p, offset: 0x%llx, size: 0x%llx, is_write: %d)\n",
        req, offset, size, is_write);

    if (offset >= _this->cfg.base && offset < _this->cfg.base + _this->cfg.size)
    {
        req->addr = offset - _this->cfg.base + _this->cfg.target_base;
    }


    return _this->out.req(req);
}

void Remapper::grant(vp::Block *__this, vp::IoReq *req) {
    Remapper *_this = (Remapper *)__this;
    _this->in.grant(req);
}

void Remapper::resp(vp::Block *__this, vp::IoReq *req) {
    Remapper *_this = (Remapper *)__this;
    _this->in.resp(req);
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config) { return new Remapper(config); }
