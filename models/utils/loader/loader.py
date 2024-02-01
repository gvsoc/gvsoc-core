#
# Copyright (C) 2020 GreenWaves Technologies, SAS, ETH Zurich and University of Bologna
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

import gvsoc.systree

class ElfLoader(gvsoc.systree.Component):
    """ELF loader

    This model can be used to load an ELF binary from the workstation into a simulated memory.
    It will load the specified binary from workstation and will issues requests to the connected
    interconnect in order to write the sections into the memories, using the addresses found in the
    ELF binary.

    At the end of the loading phase, it can also set the entry to the conected target and requests
    it to start execution.

    Attributes
    ----------
    parent: gvsoc.systree.Component
        The parent component where this one should be instantiated.
    name: str
        The name of the component within the parent space.
    binary: str
        Path to the binary to be loaded.
    entry: int
        Address of the first instruction to be executed. If it is None, the entry will be
        taken from the binary.
    """
    def __init__(self, parent: gvsoc.systree.Component, name: str, binary: str=None, binaries: list=None, entry: int=None):

        super().__init__(parent, name)

        whole_binaries = []
        if binary is not None:
            whole_binaries.append(binary)

        if binaries is not None:
            whole_binaries += binaries

        self.set_component('utils.loader.loader')

        self.add_properties({
            'binary': whole_binaries
        })

        if entry is not None:
            self.add_properties({
                'entry': entry
            })

    def o_OUT(self, itf: gvsoc.systree.SlaveItf):
        """Binds the output port.

        This ports is used by the loader to inject memory requests to transfer the binary
        into the target memories.\n
        It should be connected to an interconnect which have access to all memories containing
        at laest one section of the binary.\n
        It instantiates a port of type vp::IoMaster.\n

        Parameters
        ----------
        slave: gvsoc.systree.SlaveItf
            Slave interface
        """
        self.itf_bind('out', itf, signature='io')

    def o_START(self, itf: gvsoc.systree.SlaveItf):
        """Binds the fetch enable port.

        This ports is used by the loader to tell to another component that the loading is done, so
        that for example a core starts execution.\n
        It instantiates a port of type vp::WireMaster<bool>.\n

        Parameters
        ----------
        slave: gvsoc.systree.SlaveItf
            Slave interface
        """
        self.itf_bind('start', itf, signature='wire<bool>')

    def o_ENTRY(self, itf: gvsoc.systree.SlaveItf):
        """Binds the entry port.

        This ports is used by the loader to tell to another component what is the entry point of
        the laoded binary so that for example a core starts execution directly at the entry point.\n
        It instantiates a port of type vp::WireMaster<uint64_t>.\n

        Parameters
        ----------
        slave: gvsoc.systree.SlaveItf
            Slave interface
        """
        self.itf_bind('entry', itf, signature='wire<uint64_t>')