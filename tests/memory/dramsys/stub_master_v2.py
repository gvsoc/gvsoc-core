import gvsoc.systree
from config_tree import Config, cfg_field


class DramsysStubMasterV2EntryConfig(Config):
    """One request of the schedule of the StubMasterV2."""

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


class DramsysStubMasterV2Config(Config):
    """Configuration of the StubMasterV2."""

    logname: str = cfg_field(default='', desc="Name used in the logs, the component name if empty")
    quit_after_cycles: int = cfg_field(default=0, desc=(
        "Cycles after the last response before quitting, the model default if not positive"))
    schedule: list[DramsysStubMasterV2EntryConfig] = cfg_field(default_factory=list, desc=(
        "Requests to issue"))


def _schedule_entry(entry: dict) -> DramsysStubMasterV2EntryConfig:
    """Build a schedule entry config from a schedule dict (keys: cycle, addr, size,
    is_write, name, data_hex, chain_to_prev, is_first, is_last, burst_id)."""
    entry = dict(entry)
    if 'name' in entry:
        entry['req_name'] = entry.pop('name')
    return DramsysStubMasterV2EntryConfig(**entry)

import gvsoc.signature
import gvsoc.gui


class StubMasterV2(gvsoc.systree.Component):
    """io v2 testbench initiator for the dramsys_v2 wrapper.

    Schedule entries:
        cycle, addr, size, is_write, name, data_hex,
        is_first (default True), is_last (default True),
        burst_id (default -1), chain_to_prev (default False)

    Single-beat reqs (default): is_first = is_last = True.
    Beat-form writes: multiple entries with shared burst_id, first carries
    is_first=True, last carries is_last=True.
    Reads: always a single entry (is_first=is_last=True, size = total burst).
    The wrapper streams beat resps; this stub XOR-accumulates beats and
    emits ONE "RESP" line on the final beat for checker matching.
    """

    def __init__(self, parent: gvsoc.systree.Component, name: str,
                 schedule: list | None = None, logname: str | None = None,
                 quit_after_cycles: int = 10000):
        super().__init__(parent, name, config=DramsysStubMasterV2Config(
            logname=logname or name,
            schedule=[_schedule_entry(entry) for entry in (schedule or [])],
            quit_after_cycles=quit_after_cycles))
        self.add_sources(['stub_master_v2.cpp'])

    def o_OUTPUT(self, itf: gvsoc.systree.SlaveItf):
        # Use the legacy string signature on the master side — matches the
        # router's 'io_v2' string i_INPUT cleanly (the framework's typed
        # IoV2BigPacket() on the master errors against a string slave).
        self.itf_bind('output', itf, signature='io_v2')

    def gen_gui(self, parent_signal):
        top = gvsoc.gui.Signal(self, parent_signal, name=self.name,
                               path="req_addr", groups=['request'])
        gvsoc.gui.Signal(self, top, "req_size",     path="req_size",     groups=['request'])
        gvsoc.gui.Signal(self, top, "req_is_write", path="req_is_write", groups=['request'])
        gvsoc.gui.Signal(self, top, "pending",      path="pending",      groups=['request'])
