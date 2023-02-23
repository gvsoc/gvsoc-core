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

import json

class DisplayStringBox(object):
    def get(self):
        return { 'type': 'string_box' }

class DisplayString(object):
    def get(self):
        return { 'type': 'string' }

class DisplayPulse(object):
    def get(self):
        return { 'type': 'pulse' }

class DisplayBox(object):
    def __init__(self, message):
        self.message = message

    def get(self):
        return { 'type': 'logic_box', 'message': self.message }

class SignalGenFunctionFromBinary(object):
    def __init__(self, comp, parent, from_signal, to_signal, binaries):
        comp_path = comp.get_comp_path()
        self.from_signal = comp_path + '/' + from_signal
        self.to_signal = comp.get_comp_path() + '/' + to_signal
        self.binaries = []
        for binary in binaries:
            self.binaries.append(comp_path + '/' + binary)

        parent.gen_signals.append(self.get())


    def get(self):
        return {
            "path": self.to_signal,
            "type": "binary_function",
            "from_signal": self.from_signal,
            "binaries": self.binaries
        }

class SignalGenFromSignals(object):
    def __init__(self, comp, parent, from_signals, to_signal):
        comp_path = comp.get_comp_path()
        self.from_signals = []

        for signal in from_signals:
            self.from_signals.append(comp_path + '/' + signal)

        self.to_signal = comp.get_comp_path() + '/' + to_signal

        parent.gen_signals.append(self.get())


    def get(self):
        return {
            "path": self.to_signal,
            "type": "from_signals",
            "subtype": "analog_stacked",
            "from_signals": self.from_signals
        }

class Signal(object):

    def __init__(self, comp, parent, name=None, path=None, is_group=False, groups=None, display=None, properties=None):
        if path is not None and comp is not None:
            path = comp.get_comp_path() + '/' + path
        self.parent = parent
        self.name = name
        self.path = path
        self.child_signals = []
        self.parent = parent
        self.groups = groups if groups is not None else []
        self.gen_signals = []
        self.display = display
        self.properties = properties
        self.is_group = is_group
        self.comp = comp
        if parent is not None:
            parent.child_signals.append(self)

    def get_childs_config(self):
        config = []
        for child_signal in self.child_signals:
            child_config = child_signal.get_config()
            if child_config is not None:
                config.append(child_config)

        return config

    def get_config(self):
        if self.name is None:
            return None

        config = {}

        config['name'] = self.name
        if self.is_group:
            config['group'] = self.comp.get_comp_path()
        if self.path is not None:
            config['path'] = self.path
        if self.display is not None:
            config['display'] = self.display.get()
        childs_config = self.get_childs_config()
        if len(childs_config) != 0:
            config['signals'] = childs_config
        if self.properties is not None:
            config['properties'] = self.properties

        return config

    def get_signals(self):
        signals = [self]
        for child_signal in self.child_signals:
            signals += child_signal.get_signals()

        return signals

    def get_childs_gen_signals(self):
        gen_signals = self.gen_signals
        for child_signal in self.child_signals:
            gen_signals += child_signal.get_childs_gen_signals()
        return gen_signals


class GuiConfig(Signal):

    def __init__(self):
        super().__init__(comp=None, parent=None, name=None, path=None, groups=None)

    def gen(self, fd):
        config = {}

        config['views'] = {}
        config['views']['timeline'] = {}
        config['views']['timeline']['type'] = 'timeline'
        config['views']['timeline']['signals'] = self.get_childs_config()

        groups = {}
        for signal in self.get_signals():
            for group in signal.groups:
                if groups.get(group) is None:
                    groups[group] = {
                        "name": group,
                        "enabled": True,
                        "signals": []
                    }

                if signal.is_group:
                    groups[group]['signals'].append(signal.comp.get_comp_path())
                else:
                    groups[group]['signals'].append(signal.path)

        config['signal_groups'] = list(groups.values())
        config['signals_generate'] = self.get_childs_gen_signals()

        fd.write(json.dumps(config, indent=4))
