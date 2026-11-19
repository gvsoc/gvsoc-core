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

from typing import ClassVar

import gvsoc.systree as st
from config_tree import Config, cfg_field


class NinaB112BehaviorConfig(Config):
    """Behaviour of the NINA-B112 model."""

    _defer_parent_init: ClassVar[bool] = True

    loopback_size: int = cfg_field(default=200, dump=True, desc=(
        "Size of the loopback buffer, 200 if not positive"
    ))


class NinaB112RtsConfig(Config):
    """RTS generation of the NINA-B112 model."""

    _defer_parent_init: ClassVar[bool] = True

    enabled: bool = cfg_field(default=False, dump=True, desc=(
        "True to enable the RTS generation"
    ))
    buffer_limit: int = cfg_field(default=0, dump=True, desc=(
        "Number of words received before triggering an RTS event, 1 if not positive"
    ))
    duration: int = cfg_field(default=0, dump=True, desc=(
        "Duration of the RTS, in uart cycles, 1 if not positive"
    ))
    random_seed: int = cfg_field(default=1234, dump=True, desc=(
        "Seed of the random RTS generation"
    ))
    random_high: int = cfg_field(default=0, dump=True, desc=(
        "High limit of the random RTS generation, 10000 if not positive"
    ))
    random_threshold: int = cfg_field(default=0, dump=True, desc=(
        "Below this threshold, a random RTS is triggered"
    ))


class NinaB112Config(Config):
    """Configuration of the NINA-B112 BLE module model."""

    behavior: NinaB112BehaviorConfig = cfg_field(default_factory=NinaB112BehaviorConfig,
        desc="Behaviour of the model")
    rts: NinaB112RtsConfig = cfg_field(default_factory=NinaB112RtsConfig,
        desc="RTS generation")


class NinaB112(st.Component):
    """NINA-B112 BLE module, connected through a UART."""

    def __init__(self, parent: st.Component, name: str, config: NinaB112Config | None=None):
        if config is None:
            config = NinaB112Config()

        super().__init__(parent, name, config=config)

        self.set_component('devices.ble.nina_b112')
