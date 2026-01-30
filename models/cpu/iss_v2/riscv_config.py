#
# Copyright (C) 2020 ETH Zurich and University of Bologna
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


from dataclasses import dataclass
from gvrun.config import Config, cfg_field


@dataclass(repr=False)
class RiscvConfig(Config):
    isa: str = cfg_field(default='rv32imafdc', dump=True, desc=(
        "ISA string of the core"
    ))
    irq: str = cfg_field(default='riscv', dump=True, desc=(
        "Interrupt controller"
    ))
    fetch_enable: bool = cfg_field(default=False, dump=True, desc=(
        "True if the ISS should start executing instructins immediately, False if it will start "
        "after the fetch_enable signal starts it."
    ))
    boot_addr: int = cfg_field(default=0, dump=True, fmt="hex", desc=(
        "Address of the first instruction."
    ))
    hart_id: int = cfg_field(default=0, dump=True, desc=(
        "The core ID of the core simulated by the ISS."
    ))
    htif: bool = cfg_field(default=False, dump=True, desc=(
        "True if the ISS should start executing instructins immediately, False if it will start "
        "after the fetch_enable signal starts it."
    ))
