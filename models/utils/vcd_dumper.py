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

import gvsoc.runner
import gvsoc.systree as st



class VcdDumper(st.Component):

    def __init__(self, parent, name, parser, options):

        super(VcdDumper, self).__init__(parent, name, options=options)

        parser.add_argument("--vcd-file", dest="vcd_file", type=str, default=None,
            help="VCD input file")

        [args, __] = parser.parse_known_args()

        self.add_sources(['utils/vcd_dumper.cpp'])

        if args.vcd_file is not None:
            self.add_properties({
                'vcd_file': args.vcd_file
            })

class Target(gvsoc.runner.Target):

    gapy_description="VCD dumper"

    def __init__(self, parser, options):
        super(Target, self).__init__(parser, options,
            model=VcdDumper)
