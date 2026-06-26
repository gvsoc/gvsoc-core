// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)
//
// Watchpoint address-alias declaration component.
//
// A pure declaration carrier: it has no ports. At construction it reads the alias
// windows from its typed config and registers them with the trace engine, so console
// watchpoints match accesses done through either a master's global address or its
// local alias. The matching itself lives in vp::TraceEngine (register_alias /
// normalize_addr / check_access); this component only feeds the table.

#include <vp/vp.hpp>
#include <vp/trace/trace_engine.hpp>
#include <utils/watchpoint_alias/watchpoint_alias_config.hpp>

class WatchpointAlias : public vp::Component
{
public:
    WatchpointAlias(vp::ComponentConf &config);

private:
    WatchpointAliasConfig cfg;
};

WatchpointAlias::WatchpointAlias(vp::ComponentConf &config)
    : vp::Component(config, this->cfg)
{
    vp::TraceEngine *engine = this->traces.get_trace_engine();
    for (size_t i = 0; i < this->cfg.aliases_count; i++)
    {
        const WatchpointAliasRegion &alias = this->cfg.aliases[i];
        engine->register_alias(alias.master, (uint64_t)alias.local_base,
            (uint64_t)alias.global_base, (uint64_t)alias.size);
    }
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new WatchpointAlias(config);
}
