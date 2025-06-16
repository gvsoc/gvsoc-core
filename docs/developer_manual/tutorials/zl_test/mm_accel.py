
import gvsoc.systree

class mm_accel(gvsoc.systree.Component):
    #def __init__(self, parent: gvsoc.systree.Component, name: str, value: int):
    def __init__(self, parent: gvsoc.systree.Component, name: str):

        super().__init__(parent, name)

        self.add_sources(['mm_accel.cpp'])

        #self.add_properties({
        #    "value": value
        #})

    def i_INPUT(self) -> gvsoc.systree.SlaveItf:
        return gvsoc.systree.SlaveItf(self, 'input', signature='io')