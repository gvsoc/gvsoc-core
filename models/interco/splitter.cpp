// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

#include <vp/itf/io.hpp>
#include <vp/vp.hpp>
#include <interco/splitter/splitter_config.hpp>

class Splitter : public vp::Component {
  public:
    Splitter(vp::ComponentConf &conf);

  private:
    void reset(bool active);
    vp::IoReqStatus handle_reqs();
    static vp::IoReqStatus req(vp::Block *__this, vp::IoReq *req);
    static void grant(vp::Block *__this, vp::IoReq *req);
    static void resp(vp::Block *__this, vp::IoReq *req);
    vp::IoReq *req_alloc();
    void req_free(vp::IoReq *);

    SplitterConfig cfg;
    vp::Trace trace;

    std::vector<vp::IoMaster> outputs;
    vp::IoSlave input;

    int nb_outputs;
    vp::IoReq *req_queue = NULL;
    int nb_current_reqs;
    bool denied;
    std::vector<vp::IoReq *> current_reqs;
};

Splitter::Splitter(vp::ComponentConf &config) : vp::Component(config, this->cfg) {
    traces.new_trace("trace", &trace, vp::DEBUG);

    this->nb_outputs = this->cfg.input_width / this->cfg.output_width;

    this->input.set_req_meth(&Splitter::req);
    this->new_slave_port("input", &this->input);

    this->outputs.resize(this->nb_outputs);
    for (int i=0; i<this->nb_outputs; i++)
    {
        this->outputs[i].set_grant_meth(&Splitter::grant);
        this->outputs[i].set_resp_meth(&Splitter::resp);
        this->new_master_port("output_" + std::to_string(i), &this->outputs[i]);
    }

    this->current_reqs.resize(this->nb_outputs);
}

void Splitter::reset(bool active) {
    if (active) {
        this->nb_current_reqs = 0;
        this->denied = false;
    }
}

vp::IoReq *Splitter::req_alloc()
{
    if (!this->req_queue)
    {
        this->req_queue = new vp::IoReq();
        this->req_queue->set_next(NULL);
    }

    vp::IoReq *req = this->req_queue;
    this->req_queue = req->get_next();
    return req;
}

void Splitter::req_free(vp::IoReq *req)
{
    req->set_next(this->req_queue);
    this->req_queue = req;
}

vp::IoReqStatus Splitter::handle_reqs() {
    bool pending = false;

    for (int i=0; i<this->nb_outputs; i++)
    {
        vp::IoReq *port_req = this->current_reqs[i];
        if (port_req == NULL) continue;

        vp::IoReq *req = port_req->parent_req;

        vp::IoReqStatus status = this->outputs[i].req(this->current_reqs[i]);
        if (status != vp::IO_REQ_DENIED) {
            this->current_reqs[i] = NULL;
            this->nb_current_reqs--;
            if (status == vp::IO_REQ_OK || status == vp::IO_REQ_INVALID) {
                req->set_int(req->arg_current_index(), req->get_int(req->arg_current_index()) - port_req->size);
                this->req_free(port_req);
                if (status == vp::IO_REQ_INVALID) req->status = vp::IO_REQ_INVALID;
            } else if (status == vp::IO_REQ_PENDING) {
                pending = true;
            }
        }
    }

    if (this->nb_current_reqs == 0 && this->denied) {
        this->denied = false;
        this->input.grant(NULL);
    }

    if (this->nb_current_reqs > 0) return vp::IO_REQ_DENIED;
    else if (pending) return vp::IO_REQ_PENDING;
    else return vp::IO_REQ_OK;
}

vp::IoReqStatus Splitter::req(vp::Block *__this, vp::IoReq *req) {
    Splitter *_this = (Splitter *)__this;

    _this->trace.msg(vp::Trace::LEVEL_DEBUG,
        "Received IO req (req: %p, offset: 0x%llx, size: 0x%llx, is_write: %d)\n", req,
        req->get_addr(), req->get_size(), req->get_is_write());

    if (_this->nb_current_reqs > 0)
    {
        _this->denied = true;
        return vp::IO_REQ_DENIED;
    }

    uint64_t addr = req->get_addr();
    vp::IoReqOpcode is_write = req->get_opcode();
    uint64_t size = req->get_size();
    uint8_t *data = req->get_data();

    req->set_int(req->arg_current_index(), size);
    req->status = vp::IO_REQ_OK;

    for (int i=0; i<_this->nb_outputs && size > 0; i++)
    {
        int port_size = std::min((uint64_t)_this->cfg.output_width,
            _this->cfg.output_width - (addr & (_this->cfg.output_width - 1)));
        int iter_size = std::min((uint64_t)port_size, size);
        vp::IoReq *port_req = _this->req_alloc();
        port_req->prepare();

        port_req->addr = addr;
        port_req->size = iter_size;
        port_req->is_write = is_write;
        port_req->data = data;
        port_req->parent_req = req;

        addr += iter_size;
        size -= iter_size;
        data += iter_size;

        _this->current_reqs[i] = port_req;
        _this->nb_current_reqs++;
    }

    vp::IoReqStatus status = _this->handle_reqs();
    return status;
}

void Splitter::grant(vp::Block *__this, vp::IoReq *req) {
    Splitter *_this = (Splitter *)__this;
    _this->handle_reqs();
}

void Splitter::resp(vp::Block *__this, vp::IoReq *port_req) {
    Splitter *_this = (Splitter *)__this;
    vp::IoReq *req = port_req->parent_req;
    if (port_req->status == vp::IO_REQ_INVALID) req->status = vp::IO_REQ_INVALID;

    req->set_int(req->arg_current_index(), req->get_int(req->arg_current_index()) - port_req->size);
    _this->req_free(port_req);
    if (req->get_int(req->arg_current_index()) == 0)
    {
        _this->input.resp(req);
    }
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config) { return new Splitter(config); }
