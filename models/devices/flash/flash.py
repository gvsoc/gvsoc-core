#
# Copyright (C) 2022 GreenWaves Technologies, SAS, ETH Zurich and
#                    University of Bologna
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
import gvrun.flash
import gvrun.target


class Flash(st.Component, gvrun.target.BinaryLoader):

    def __init__(self, parent, name, size=0, type='undefined'):
        super(Flash, self).__init__(parent, name)

        self.size = size
        self.type = type
        self.binaries = []

        self.declare_flash()

        self.declare_target_property(
            gvrun.target.Property(
                name='template', value=None,
                description='Specify the template describing how to generate flash image'
            )
        )

        self.declare_target_property(
            gvrun.target.Property(
                name='section_start_align', value=16, cast=int,
                description='Specify flash section start alignment'
            )
        )

        self.declare_target_property(
            gvrun.target.Property(
                name='section_size_align', value=16, cast=int,
                description='Specify flash section size alignment'
            )
        )

        self.generator = gvrun.flash.Flash(self)

    def register_binary(self, binary):
        self.binaries.append(binary)

    def get_image_path(self):
        return self.get_path().replace('/', '.') + '.bin'

    def generate(self, builddir):
        self.generator.generate_image(builddir)


    def configure(self):
        self.generator.parse_content(self.get_target_property('template'))


    def register_section_template(self, name, template):
        self.generator.register_section_template(name, template)
