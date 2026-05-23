#
# Copyright (C) 2022-2026 ETH Zurich and University of Bologna
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

add_subdirectory(adv_dbg_unit)
add_subdirectory(cluster)
add_subdirectory(padframe)
add_subdirectory(cpu/iss)
add_subdirectory(event_unit)
add_subdirectory(mchan)
add_subdirectory(timer)
add_subdirectory(ne16)
add_subdirectory(icache_ctrl)
add_subdirectory(fll)
add_subdirectory(itc)
add_subdirectory(chips)
add_subdirectory(gpio)
add_subdirectory(udma)
add_subdirectory(soc_eu)
add_subdirectory(stdout)

#### SOLUTION INTEGRATE HWPE - Added simple_hwpe subdirectory
add_subdirectory(simple_hwpe)

add_subdirectory(neureka)
add_subdirectory(datamover)
add_subdirectory(spatz)
add_subdirectory(snitch)
add_subdirectory(redmule)
add_subdirectory(pcie_vfio_bridge)
