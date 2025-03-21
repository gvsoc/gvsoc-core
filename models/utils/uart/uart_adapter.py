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

class UartAdapter(object):

    """UART adapter

    Instantiating this class will make sure the parent component can instantiate any of the UART
    adapters from its C++ model.
    This will in practice compile the UART adapter source code together with the model source code.

    Attributes
    ----------
    component: gvsoc.systree.Component
        The parent component where the UART adapter blocks will be instantiated.
    """
    def __init__(self, component):

        component.add_sources([
            'utils/uart/uart_adapter.cpp',
            'utils/uart/uart_adapter_buffered.cpp',
        ])
