// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

#include <vp/itf/io.hpp>
#include <vp/vp.hpp>
#include <interco/demux_config.hpp>

class Demux : public vp::Component {
  public:
    Demux(vp::ComponentConf &conf);

  private:
    static vp::IoReqStatus req(vp::Block *__this, vp::IoReq *req);

    DemuxConfig cfg;
    vp::Trace trace;

    std::vector<vp::IoMaster> out;
    vp::IoSlave in;
};

Demux::Demux(vp::ComponentConf &config) : vp::Component(config, this->cfg) {
    this->traces.new_trace("trace", &trace, vp::DEBUG);

    this->in.set_req_meth(&Demux::req);
    this->new_slave_port("input", &in);

    int nb_outs = 1<<this->cfg.width;
    this->out.resize(nb_outs);
    for (int i=0; i<nb_outs; i++)
    {
        this->new_master_port("output_" + std::to_string(i), &this->out[i]);
    }
}

vp::IoReqStatus Demux::req(vp::Block *__this, vp::IoReq *req) {
    Demux *_this = (Demux *)__this;
    uint64_t offset = req->get_addr();
    bool is_write = req->get_is_write();
    uint64_t size = req->get_size();
    uint8_t *data = req->get_data();

    _this->trace.msg(vp::Trace::LEVEL_DEBUG,
        "Received IO req (req: %p, offset: 0x%llx, size: 0x%llx, is_write: %d)\n",
        req, offset, size, is_write);

    int output_id = (offset >> _this->cfg.offset) & ((1 << _this->cfg.width) - 1);

    return _this->out[output_id].req(req);
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config) { return new Demux(config); }
