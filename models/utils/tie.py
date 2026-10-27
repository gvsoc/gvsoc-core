# SPDX-FileCopyrightText: 2026 ETH Zurich and University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

"""
Tie cell: drives a wire input with a constant, like an input tied to '0 or
'1 in the RTL. Platforms do not instantiate it directly, they call
``tie(value)`` on the slave interface of the input.
"""

from gvsoc.systree import Component, SlaveItf
from config_tree import Config, cfg_field

# Wire types a tie cell can drive, from the signature of the input
TIE_TYPES = {
    'wire<bool>': 0,
    'wire<int>': 1,
    'wire<uint32_t>': 2,
    'wire<uint64_t>': 3,
}


class TieConfig(Config):
    value: int = cfg_field(default=0, dump=True, desc=(
        "Constant driven on the input"))
    type: int = cfg_field(default=0, dump=True, desc=(
        "Wire type of the input: 0 bool, 1 int, 2 uint32_t, 3 uint64_t"))


class Tie(Component):
    """Tie cell driving a constant on a wire input.

    The value is sent when the reset is released, and again at every reset
    release, so the input must keep the last value it received and not reset
    it on its own.

    Parameters
    ----------
    parent : Component
        Parent component.
    name : str
        Name of the cell.
    value : int or bool
        Constant driven on the input.
    signature : str
        Signature of the input, one of the ``wire<...>`` types of TIE_TYPES.
    """

    def __init__(self, parent: Component, name: str, value, signature: str):
        if signature not in TIE_TYPES:
            raise RuntimeError(f'Cannot tie an input of signature {signature}, '
                f'supported: {", ".join(TIE_TYPES)}')
        self.signature = signature

        super().__init__(parent, name, config=TieConfig(value=int(value),
            type=TIE_TYPES[signature]))

        self.add_sources(['utils/tie.cpp'])

    def o_OUT(self, itf: SlaveItf):
        self.itf_bind('out', itf, signature=self.signature)
