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

from devices.flash.flash import Flash

class Spiflash(Flash):

    def __init__(self, parent, name, size=0x04000000):
        super(Spiflash, self).__init__(parent, name)

        # Register all parameters as properties so that they can be overwritten from the command-line
        self.add_property('content/partitions/readfs/files', [])
        self.add_property('content/partitions/readfs/type', 'readfs')
        self.add_property('content/partitions/readfs/enabled', False)

        self.add_property('content/partitions/hostfs/files', [])
        self.add_property('content/partitions/hostfs/type', 'hostfs')

        self.add_property('content/partitions/lfs/root_dir', None)
        self.add_property('content/partitions/lfs/type', 'lfs')
        self.add_property('content/partitions/lfs/enabled', False)

        self.set_component('devices.spiflash.spiflash_impl')

        self.add_property('writeback', True)
        self.add_property('size', size)

        self.add_property('preload_file', self.get_image_path())

        # TODO this is needed by GAPY but is not aligned with the size given to model
        # That should be resolved once the flash images are built by the system tree instead of gapy
        self.add_property('datasheet/type', 'spi')
        self.add_property('datasheet/size', '64MB')
        self.add_property('datasheet/block-size', '4KB')
