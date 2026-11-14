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
from config_tree import Config, cfg_field


class HyperflashConfig(Config):
    """Configuration of the HyperFlash."""

    size: int = cfg_field(default=0x04000000, fmt="hex", desc="Size of the flash")
    preload_file: str = cfg_field(default='', desc=(
        "Image the flash is loaded with, empty for none"))
    writeback: bool = cfg_field(default=True, desc=(
        "Map the preloaded image so that the flash writes go to it"))
    writeback_file: str = cfg_field(default='', desc=(
        "File where the flash content is written back, empty for none"))


class Hyperflash(Flash):
    """
    Hyperflash

    This instantiates a s26ks512s flash (hyperflash).

    Attributes
    ----------
    size : int
        Size of the flash (default: 0x04000000).

    """

    def __init__(self, parent, name, size=0x04000000):
        config = HyperflashConfig(size=size)
        super(Hyperflash, self).__init__(parent, name, config=config)
        config.preload_file = self.get_image_path()

        # Register all parameters as properties so that they can be overwritten from the command-line
        self.add_property('content/image', self.get_image_path())

        self.add_property('content/partitions/readfs/files', [])
        self.add_property('content/partitions/readfs/type', 'readfs')
        self.add_property('content/partitions/readfs/enabled', False)

        self.add_property('content/partitions/hostfs/files', [])
        self.add_property('content/partitions/hostfs/type', 'hostfs')

        self.add_property('content/partitions/lfs/root_dir', None)
        self.add_property('content/partitions/lfs/type', 'lfs')
        self.add_property('content/partitions/lfs/enabled', False)

        self.add_sources(['devices/hyperbus/hyperflash_impl.cpp'])


        # TODO this is needed by GAPY but is not aligned with the size given to model
        # That should be resolved once the flash images are built by the system tree instead of gapy
        self.add_property('datasheet/type', 'hyper')
        self.add_property('datasheet/size', '64MB')
        self.add_property('datasheet/block-size', '256KB')
