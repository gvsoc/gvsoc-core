import gvsoc.systree

from gvrun.parameter import TargetParameter
from typing import Annotated
from config_tree import Config, cfg_field
from gvrun.runtime import Runtime


class FicConfig(Config):
    """Configuration of the fault injection controller."""

    faults_path: Annotated[str, Runtime] = cfg_field(default='', desc=(
        "Path of the file describing the faults, empty for none"))
    max_cycle: Annotated[int, Runtime] = cfg_field(default=-1, desc=(
        "Cycle where the simulation is stopped, -1 for none"))


"""
You can pass a faults file to the gvsoc instance as follows:

  --parameter {fic_path}/faults_path={file_path}

"""


class FIC(gvsoc.systree.Component):
    """
    Attributes
    ----------
    parent: gvsoc.systree.Component
        The parent component where this one should be instantiated.
    name: str
        The name of the component within the parent space.
    faults_path: str
        Path to the file containing faults to be preloaded.
    max_cycle: int
        Force the simulation to exit upon reaching specified number of cycles.
    """
    def __init__(self, parent: gvsoc.systree.Component, name: str, 
                 faults_path: str="", max_cycle: int=-1):
        config = FicConfig()
        super().__init__(parent, name, config=config)

        if faults_path == "":
            faults_path = TargetParameter(
                self, name='faults_path', value=None, description='Path to file with faults',
cast=str
            ).get_value()

        if max_cycle == -1:
            max_cycle = TargetParameter(
                self, name='max_cycles', value=-1, 
                description='If specified, exit the simulation after this amount of cycles.'
            ).get_value()

        self.add_sources(['fault_injection/fic.cpp'])

        config.faults_path = faults_path if faults_path is not None else ''
        config.max_cycle = max_cycle if max_cycle is not None else -1

    def o_GLOBAL_AS(self, itf: gvsoc.systree.SlaveItf):
        self.itf_bind('global_as', itf, signature='io')

    def o_FAULT_REQUEST(self, id, itf: gvsoc.systree.SlaveItf):
        self.itf_bind('fault_request_%d' % id, itf, signature='wire<FIRequest>')

    def o_HASH_REQUEST(self, id, itf: gvsoc.systree.SlaveItf):
        self.itf_bind('hash_request_%d' % id, itf, signature='wire<FIHashRequest>')
