// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

/*
 * Tie cell: drives a constant on a wire input, like an input tied to '0 or
 * '1 in the RTL. The value goes out at every reset release.
 */

#include <vp/vp.hpp>
#include <vp/itf/wire.hpp>
#include <utils/tie/tie_config.hpp>

class Tie : public vp::Component
{
public:
    Tie(vp::ComponentConf &config);

private:
    void reset(bool active) override;

    TieConfig cfg;
    // Only the one of the type of the input is declared as port
    vp::WireMaster<bool> out_bool;
    vp::WireMaster<int> out_int;
    vp::WireMaster<uint32_t> out_uint32;
    vp::WireMaster<uint64_t> out_uint64;
};

Tie::Tie(vp::ComponentConf &config)
    : vp::Component(config, this->cfg)
{
    switch (this->cfg.type)
    {
        case 0: this->new_master_port("out", &this->out_bool); break;
        case 1: this->new_master_port("out", &this->out_int); break;
        case 2: this->new_master_port("out", &this->out_uint32); break;
        case 3: this->new_master_port("out", &this->out_uint64); break;
        default:
            this->get_trace()->fatal("Unsupported wire type (type: %ld)\n", this->cfg.type);
    }
}

void Tie::reset(bool active)
{
    if (active)
    {
        return;
    }

    switch (this->cfg.type)
    {
        case 0: this->out_bool.sync(this->cfg.value != 0); break;
        case 1: this->out_int.sync((int)this->cfg.value); break;
        case 2: this->out_uint32.sync((uint32_t)this->cfg.value); break;
        case 3: this->out_uint64.sync((uint64_t)this->cfg.value); break;
    }
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config) { return new Tie(config); }
