import gvsoc.systree
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
        super().__init__(parent, name)
        self.add_sources(['stub_master_v1.cpp'])
        self.add_property('logname', logname or name)
        self.add_property('schedule', schedule or [])
        self.add_property('quit_after_cycles', quit_after_cycles)

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
