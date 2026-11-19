# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

import gvsoc.systree
from config_tree import Config, cfg_field


class RamulatorBeatMasterEntryConfig(Config):
    """One request of the schedule of the BeatMaster."""

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


class RamulatorBeatMasterConfig(Config):
    """Configuration of the BeatMaster."""

    logname: str = cfg_field(default='', desc="Name used in the logs, the component name if empty")
    quit_after_cycles: int = cfg_field(default=0, desc=(
        "Cycles after the last response before quitting, the model default if not positive"))
    beat_width: int = cfg_field(default=0, desc="Width in bytes of the beats")
    quiet: bool = cfg_field(default=False, desc="True to suppress logging and asserts")
    chain: bool = cfg_field(default=False, desc="True to keep one burst in flight, re-issuing the schedule")
    repeat: int = cfg_field(default=0, desc="Number of times the schedule is issued, 1 if not positive")
    schedule: list[RamulatorBeatMasterEntryConfig] = cfg_field(default_factory=list, desc=(
        "Requests to issue"))


def _schedule_entry(entry: dict) -> RamulatorBeatMasterEntryConfig:
    """Build a schedule entry config from a schedule dict (keys: cycle, addr, size,
    is_write, name, data_hex, chain_to_prev, is_first, is_last, burst_id)."""
    entry = dict(entry)
    if 'name' in entry:
        entry['req_name'] = entry.pop('name')
    return RamulatorBeatMasterEntryConfig(**entry)

import gvsoc.signature


class BeatMaster(gvsoc.systree.Component):
    """Beat-streaming io_v2 master.

    Declares ``IoV2Beat(beat_width)`` on its output so it binds directly to a
    beat slave. Each schedule entry (a burst) is streamed as a sequence of
    beat-sized requests (one per cycle), sharing a burst_id, with is_first /
    is_last framing. Schedule entry keys: cycle, addr, size, is_write, name,
    [data_hex].
    """

    def __init__(self, parent: gvsoc.systree.Component, name: str,
                 schedule: list | None = None, beat_width: int = 64,
                 logname: str | None = None, quit_after_cycles: int = 200,
                 quiet: bool = False, chain: bool = False, repeat: int = 1):
        super().__init__(parent, name, config=RamulatorBeatMasterConfig(
            logname=logname or name,
            beat_width=beat_width,
            quit_after_cycles=quit_after_cycles,
            quiet=quiet,
            chain=chain,
            repeat=repeat,
            schedule=[_schedule_entry(entry) for entry in (schedule or [])]))
        self.beat_width = beat_width
        self.add_sources(['beat_master.cpp'])

    def o_OUTPUT(self, itf: gvsoc.systree.SlaveItf):
        self.itf_bind('output', itf,
                      signature=gvsoc.signature.IoV2Beat(self.beat_width))
