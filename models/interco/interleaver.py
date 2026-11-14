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

import gvsoc.systree as st
from config_tree import Config, cfg_field


class InterleaverConfig(Config):
    """Configuration of the memory interleaver."""

    nb_slaves: int = cfg_field(default=0, dump=True, desc=(
        "Number of slave ports where the accesses are dispatched"
    ))
    nb_masters: int = cfg_field(default=0, dump=True, desc=(
        "Number of master ports"
    ))
    interleaving_bits: int = cfg_field(default=0, dump=True, desc=(
        "Size in bits of the accesses dispatched to banks"
    ))
    stage_bits: int = cfg_field(default=0, dump=True, desc=(
        "Number of bits used for dispatching the accesses, 0 for log2(nb_slaves)"
    ))
    remove_offset: int = cfg_field(default=0, dump=True, fmt="hex", desc=(
        "Offset removed from the address of the dispatched accesses"
    ))
    enable_shift: int = cfg_field(default=0, dump=True, desc=(
        "Shift applied to the address"
    ))
    offset_translation: bool = cfg_field(default=True, dump=True, desc=(
        "True if the address is translated into the bank offset"
    ))

class Interleaver(st.Component):
    """
    Memory interleaver

    Can be used to dispatch accesses to memory banks based on access address MSB.

    Attributes
    ----------
    nb_slaves : int
        The number of slave ports where the accesses will be dispatched.
    nb_masters : int, optional
        The number of masters ports where the accesses will be required.
    interleaving_bits : int
        Number of interleaving bits, i.e. the size in bits of the accesseses dispatched
        to banks.
    stage_bits : int, optional
        Number of bits used for dispatching the accesses. If it is not specified, it is
        assigned log2(nb_slaves).
    remove_offset: int, optional
        Specify an offset to be removed to the incoming access address when it is
        dispatched (default: 0).

    """

    def __init__(self, parent, name, nb_slaves: int, interleaving_bits: int, nb_masters: int=0, stage_bits: int=0, remove_offset: int=0, enable_shift: int=0, offset_translation: bool=True):

        super(Interleaver, self).__init__(parent, name, config=InterleaverConfig(
            nb_slaves=nb_slaves, nb_masters=nb_masters, interleaving_bits=interleaving_bits,
            stage_bits=stage_bits, remove_offset=remove_offset, enable_shift=enable_shift,
            offset_translation=offset_translation))

        self.add_sources(['interco/interleaver_impl.cpp'])
