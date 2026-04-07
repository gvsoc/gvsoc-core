import gvsoc.systree

from gvrun.parameter import TargetParameter

"""
You can pass a faults file to the gvsoc instance 
(w/o recompiling) by specifying parameter:

  --parameter {fic_path}/faults_path={file_path}

"""

class FIC(gvsoc.systree.Component):
    def __init__(self, parent: gvsoc.systree.Component, 
                 name: str, faults_path: str="",
                 nb_memories: int=0):
        super().__init__(parent, name)

        if faults_path == "":
            faults_path = TargetParameter(
                self, name='faults_path', value=None, description='Path to file with faults',
                cast=str
            ).get_value()

        self.add_sources(['fault_injection/fic.cpp'])

        self.add_properties({
            "nb_memories": nb_memories,
            "faults_path": faults_path,
        })

    def o_GLOBAL_AS(self, itf: gvsoc.systree.SlaveItf):
        self.itf_bind('global_as', itf, signature='io')

    def o_FAULT_REQUEST(self, id, itf: gvsoc.systree.SlaveItf):
        self.itf_bind('fault_request_%d' % id, itf, signature='wire<FIRequest>')

    def o_HASH_REQUEST(self, id, itf: gvsoc.systree.SlaveItf):
        self.itf_bind('hash_request_%d' % id, itf, signature='wire<FIHashRequest>')
