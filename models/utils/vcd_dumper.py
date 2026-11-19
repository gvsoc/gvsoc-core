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

from typing import Annotated

import gvsoc.runner
import gvsoc.systree as st
from config_tree import Config, cfg_field
from gvrun.runtime import Runtime


class VcdElementSizeConfig(Config):
    """Splits a VCD signal into elements of a given width."""

    signal: str = cfg_field(default="", desc="Full name of the VCD signal")
    size: int = cfg_field(default=0, desc="Width in bits of each element")


class VcdDumperConfig(Config):
    """Configuration of the VCD dumper."""

    # Runtime: given on the command line of each run
    vcd_file: Annotated[str, Runtime] = cfg_field(default="", dump=True, desc=(
        "VCD file to replay, empty for none"
    ))
    element_size: list[VcdElementSizeConfig] = cfg_field(default_factory=list, desc=(
        "Signals to split into elements"
    ))



class VcdDumper(st.Component):

    def __init__(self, parent, name, parser, options):

        config = VcdDumperConfig()

        super(VcdDumper, self).__init__(parent, name, options=options, config=config)

        parser.add_argument("--vcd-file", dest="vcd_file", type=str, default=None,
            help="VCD input file")

        [args, __] = parser.parse_known_args()

        self.add_sources(['utils/vcd_dumper.cpp'])

        if args.vcd_file is not None:
            config.vcd_file = args.vcd_file

    def add_element_size(self, signal: str, size: int):
        """Split the VCD signal ``signal`` into elements of ``size`` bits."""
        self.get_config().element_size.append(VcdElementSizeConfig(signal=signal, size=size))

class Target(gvsoc.runner.Target):

    gapy_description="VCD dumper"

    def __init__(self, parser, options):
        super(Target, self).__init__(parser, options,
            model=VcdDumper)
