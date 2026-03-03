// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

#include <vp/itf/io.hpp>
#include <vp/vp.hpp>
#include <interco/log_ico_config.hpp>

class LogIco : public vp::Component {
  public:
    LogIco(vp::ComponentConf &conf);

  private:
    static vp::IoReqStatus req(vp::Block *__this, vp::IoReq *req, int id);

    LogIcoConfig cfg;
    vp::Trace trace;

    std::vector<vp::IoMaster> out;
    std::vector<vp::IoSlave> in;
    int slave_bits;
};

LogIco::LogIco(vp::ComponentConf &config) : vp::Component(config, this->cfg) {
    traces.new_trace("trace", &trace, vp::DEBUG);

    this->out.resize(this->cfg.nb_slaves);
    for (int i=0; i<this->cfg.nb_slaves; i++)
    {
        this->new_master_port("output_" + std::to_string(i), &this->out[i]);
    }

    this->in.resize(this->cfg.nb_masters);
    for (int i=0; i<this->cfg.nb_masters; i++)
    {
        this->in[i].set_req_meth_muxed(&LogIco::req, i);
        this->new_slave_port("input_" + std::to_string(i), &this->in[i]);
    }

    this->slave_bits = 32 - __builtin_clz(this->cfg.nb_slaves - 1);
}

vp::IoReqStatus LogIco::req(vp::Block *__this, vp::IoReq *req, int id) {
    LogIco *_this = (LogIco *)__this;
    uint64_t offset = req->get_addr();
    bool is_write = req->get_is_write();
    uint64_t size = req->get_size();
    uint8_t *data = req->get_data();

    _this->trace.msg(vp::Trace::LEVEL_DEBUG, "Received IO req (port: %d, req: %p, offset: 0x%llx, size: 0x%llx, is_write: %d)\n",
       id, req, offset, size, is_write);

    offset -= _this->cfg.remove_offset;

    int bank_id = (offset >> _this->cfg.interleaving_width) & ((1 << _this->slave_bits) - 1);
    uint64_t bank_offset = ((offset >> (_this->slave_bits + _this->cfg.interleaving_width)) <<
        _this->cfg.interleaving_width) + (offset & ((1 << _this->cfg.interleaving_width) - 1));

    req->set_addr(bank_offset);

    return _this->out[bank_id].req(req);
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config) { return new LogIco(config); }
