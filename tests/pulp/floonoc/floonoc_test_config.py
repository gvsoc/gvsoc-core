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

"""Configuration of the FlooNoC test driver (test.cpp)."""

from config_tree import Config, cfg_field


class FloonocTestConfig(Config):
    """Configuration of the FlooNoC test driver."""

    nb_cluster_x: int = cfg_field(default=0, desc="Number of clusters on the X axis")
    nb_cluster_y: int = cfg_field(default=0, desc="Number of clusters on the Y axis")
    cluster_base: int = cfg_field(default=0, fmt="hex", desc="Base address of the clusters")
    cluster_size: int = cfg_field(default=0, fmt="hex", desc="Size of a cluster area")
    use_memory: bool = cfg_field(default=False, desc="True to target the memories around the NoC")
    mem_bw: int = cfg_field(default=0, desc="Bandwidth of the memories")
