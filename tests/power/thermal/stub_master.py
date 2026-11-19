# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

import gvsoc.systree
from config_tree import Config, cfg_field


class ThermalStubMasterEntryConfig(Config):
    """One request of the schedule of the StubMaster."""

    cycle: int = cfg_field(default=0, desc="Cycle at which the request is issued")
    addr: int = cfg_field(default=0, fmt="hex", desc="Address of the request")
    size: int = cfg_field(default=0, desc="Size of the request in bytes")
    is_write: bool = cfg_field(default=False, desc="True for a write")
    req_name: str = cfg_field(default='', desc="Name of the request in the logs (the 'name' key of the schedule entry)")
    data_hex: str = cfg_field(default='', desc="Hex string pre-filling the request data")
    chain_to_prev: bool = cfg_field(default=False, desc="True to issue the request once the previous one completed")
    is_first: bool = cfg_field(default=True, desc="True for the first beat of a burst")
    is_last: bool = cfg_field(default=True, desc="True for the last beat of a burst")
    burst_id: int = cfg_field(default=-1, desc="Burst identifier, -1 for none")


class ThermalStubMasterConfig(Config):
    """Configuration of the StubMaster."""

    logname: str = cfg_field(default='', desc="Name used in the logs, the component name if empty")
    quit_after_cycles: int = cfg_field(default=0, desc=(
        "Cycles after the last response before quitting, the model default if not positive"))
    schedule: list[ThermalStubMasterEntryConfig] = cfg_field(default_factory=list, desc=(
        "Requests to issue"))


def _schedule_entry(entry: dict) -> ThermalStubMasterEntryConfig:
    """Build a schedule entry config from a schedule dict (keys: cycle, addr, size,
    is_write, name, data_hex, chain_to_prev, is_first, is_last, burst_id)."""
    entry = dict(entry)
    if 'name' in entry:
        entry['req_name'] = entry.pop('name')
    return ThermalStubMasterEntryConfig(**entry)



class StubMaster(gvsoc.systree.Component):
    """io_v2 testbench initiator.

    Issues a pre-programmed schedule of requests. Each schedule entry is a dict with
    keys: cycle, addr, size, is_write, name.
    """
    def __init__(self, parent: gvsoc.systree.Component, name: str,
                 schedule: list | None = None, logname: str | None = None):
        super().__init__(parent, name, config=ThermalStubMasterConfig(
            logname=logname or name,
            schedule=[_schedule_entry(entry) for entry in (schedule or [])]))
        self.add_sources(['stub_master.cpp'])

    def o_OUTPUT(self, itf: gvsoc.systree.SlaveItf):
        self.itf_bind('output', itf, signature='io_v2')
