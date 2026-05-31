import gvsoc.systree
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
        super().__init__(parent, name)
        self.add_sources(['stub_master_v2.cpp'])
        self.add_property('logname', logname or name)
        self.add_property('schedule', schedule or [])
        self.add_property('quit_after_cycles', quit_after_cycles)

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
