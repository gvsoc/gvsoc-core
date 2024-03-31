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
import regmap.regmap
import regmap.regmap_md
import regmap.regmap_c_header


class Ssm6515(st.Component):

    def __init__(self, parent, name, i2c_address=0x00, output_filepath = "ssm6515.wav"):
        super(Ssm6515, self).__init__(parent, name)

        self.set_component('devices.sound.dac.ssm6515.ssm6515')
        #self.add_sources(['devices/sound/dac/ssm6515/ssm6515.cpp'])


        self.add_property('i2c_address', i2c_address)
        self.add_property('output_filepath', output_filepath)


    def gen(self, builddir, installdir):
        comp_path = 'devices/sound/dac/ssm6515'
        regmap_path = f'{comp_path}/regmap.md'
        regmap_instance = regmap.regmap.Regmap('i2c_regmap')
        regmap.regmap_md.import_md(regmap_instance, self.get_file_path(regmap_path))
        regmap.regmap_c_header.dump_to_header(regmap=regmap_instance, name='i2c_regmap',
            header_path=f'{builddir}/{comp_path}/headers', headers=['regfields', 'gvsoc'])

