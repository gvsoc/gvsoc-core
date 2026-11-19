import gvsoc.systree
from config_tree import Config, cfg_field


class DramsysStubMasterV1EntryConfig(Config):
    """One request of the schedule of the StubMasterV1."""

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


class DramsysStubMasterV1Config(Config):
    """Configuration of the StubMasterV1."""

    logname: str = cfg_field(default='', desc="Name used in the logs, the component name if empty")
    quit_after_cycles: int = cfg_field(default=0, desc=(
        "Cycles after the last response before quitting, the model default if not positive"))
    schedule: list[DramsysStubMasterV1EntryConfig] = cfg_field(default_factory=list, desc=(
        "Requests to issue"))


def _schedule_entry(entry: dict) -> DramsysStubMasterV1EntryConfig:
    """Build a schedule entry config from a schedule dict (keys: cycle, addr, size,
    is_write, name, data_hex, chain_to_prev, is_first, is_last, burst_id)."""
    entry = dict(entry)
    if 'name' in entry:
        entry['req_name'] = entry.pop('name')
    return DramsysStubMasterV1EntryConfig(**entry)

import gvsoc.gui


class StubMasterV1(gvsoc.systree.Component):
    """io v1 testbench initiator for the dramsys wrapper.

    Issues a pre-programmed schedule of requests. Each entry is a dict with
    keys: cycle, addr, size, is_write, name; optional data_hex pre-fills the
    request payload (used for writes and for the expected value of reads
    that get logged as DONE / RESP).
    """

    def __init__(self, parent: gvsoc.systree.Component, name: str,
                 schedule: list | None = None, logname: str | None = None,
                 quit_after_cycles: int = 10000):
        super().__init__(parent, name, config=DramsysStubMasterV1Config(
            logname=logname or name,
            schedule=[_schedule_entry(entry) for entry in (schedule or [])],
            quit_after_cycles=quit_after_cycles))
        self.add_sources(['stub_master_v1.cpp'])

    def o_OUTPUT(self, itf: gvsoc.systree.SlaveItf):
        self.itf_bind('output', itf, signature='io')

    def gen_gui(self, parent_signal):
        # Register the request-trace signals in the GUI signal browser so
        # the user can drag them onto a timeline. All four are sticky for
        # the lifetime of an in-flight request (SEND -> RESP/DONE) and
        # drop back to high-Z once nothing is in flight.
        top = gvsoc.gui.Signal(self, parent_signal, name=self.name,
                               path="req_addr", groups=['request'])
        gvsoc.gui.Signal(self, top, "req_size",     path="req_size",     groups=['request'])
        gvsoc.gui.Signal(self, top, "req_is_write", path="req_is_write", groups=['request'])
        gvsoc.gui.Signal(self, top, "pending",      path="pending",      groups=['request'])
