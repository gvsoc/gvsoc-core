#
# Copyright (C) 2024 GreenWaves Technologies, ETH Zurich and University of Bologna
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

import regmap.regmap as rmap
from reggen.ip_block import IpBlock

from reggen import (
    gen_cfg_md, gen_cheader, gen_dv, gen_fpv, gen_md, gen_html,
    gen_json, gen_rtl, gen_selfdoc, version
)

from reggen.multi_register import MultiRegister
from reggen.reg_block import RegBlock
from reggen.register import Register
from reggen.window import Window

from typing import List, TextIO, Dict, Any, Optional

def import_hjson(regmap, path, registers=[]):

    with open(path, "rt") as fh:

        try:
            block = IpBlock.from_text(fh.read(), [], 'myfile')
        except ValueError as err:
            print (err)
            exit(1)

        # Handle the case where there's just one interface.
        if len(block.reg_blocks) == 1:
            rb = next(iter(block.reg_blocks.values()))
            gen_reg_block(regmap, rb, block.name, block.regwidth)
            return 0

        # Handle the case where there is more than one device interface and,
        # correspondingly, more than one reg block.
        for iface_name, rb in block.reg_blocks.items():
            assert iface_name
            gen_reg_block(regmap, rb, block.name, block.regwidth, iface_name)

        return 0


def gen_reg_block(regmap, rb: RegBlock, comp: str, width: int, iface_name: Optional[str] = None
) -> None:
    if len(rb.entries) == 0:
        raise RuntimeError('This interface does not expose any registers.')

    # Generate overview table.
    # gen_md_register_summary(output, rb.entries, comp, width, iface_name)

    # Generate detailed entries.
    for x in rb.entries:
        if isinstance(x, Register):
            gen_register(regmap, x, comp, width)
        elif isinstance(x, MultiRegister):
            gen_multiregister(regmap, x, comp, width)
        else:
            assert isinstance(x, Window)
            raise RuntimeError("Unsupported Window")


def gen_register(regmap, reg: Register, comp: str, width: int) -> None:

    regmap_reg = regmap.add_register(
        rmap.Register(name=reg.name, desc=reg.desc, offset=reg.offset))

    for regfield in reg.fields:

        regmap_regfield = regmap_reg.add_regfield(rmap.Regfield(
            name=regfield.name,
            width=regfield.bits.width(),
            bit=regfield.bits.lsb,
            reset=regfield.resval,
            desc=regfield.desc
        ))

def multireg_is_compact(mreg: MultiRegister, width: int) -> bool:
    # Note that validation guarantees that compacted multiregs only ever have one field.
    return mreg.compact and (mreg.reg.fields[0].bits.msb + 1) <= width // 2

def gen_multiregister(regmap, mreg: MultiRegister, comp: str, width: int) -> None:

    # Check whether this is a compacted multireg, in which case we cannot use
    # the general definition of the first register as an example for all other instances.
    if multireg_is_compact(mreg, width):
        for reg in mreg.regs:
            gen_register(regmap, reg, comp, width)
        return

    # The general definition of the registers making up this multiregister block.
    reg_def = mreg.reg

    # For now, generate all multiregs as simple registers
    for reg in mreg.regs:
        gen_register(regmap, reg, comp, width)
