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

import gvsoc
import json
import os
from collections import deque

class DisplayStringBox(object):
    def get(self):
        return { 'type': 'string_box' }

class DisplayString(object):
    def get(self):
        return { 'type': 'string' }

class DisplayPulse(object):
    def get(self):
        return { 'type': 'pulse' }

class DisplayAnalog(object):
    def get(self):
        return { 'type': 'analog' }

class DisplayBox(object):
    def __init__(self, format="hex"):
        self.format = format

    def get(self):
        return { 'type': 'box', 'format': self.format }

class DisplayLogicBox(object):
    def __init__(self, message):
        self.message = message

    def get(self):
        return { 'type': 'logic_box', 'message': self.message }

def get_comp_path(comp, inc_top=False, child_path=None):
    if os.environ.get('USE_GVRUN2') is not None:
        return '/' + comp.get_path(child_path=child_path)
    else:
        return comp.get_comp_path(inc_top, child_path)

class SignalGenFunctionFromBinary(object):
    def __init__(self, comp, parent, from_signal, to_signal, binaries):
        comp_path = get_comp_path(comp, inc_top=True)
        if comp_path is None:
            self.from_signal = '/' + from_signal
            self.to_signal = '/' + to_signal
        else:
            self.from_signal = comp_path + '/' + from_signal
            self.to_signal = get_comp_path(comp, inc_top=True) + '/' + to_signal
        self.binaries = []
        for binary in binaries:
            if comp_path is None:
                binary = '/' + binary
            else:
                binary = comp_path + '/' + binary
            self.binaries.append(binary)

        parent.gen_signals.append(self)

    def get(self):
        return {
            "path": self.to_signal,
            "type": "binary_function",
            "from_signal": self.from_signal,
            "binaries": self.binaries
        }

class SignalGenThreads(object):
    def __init__(self, comp, parent, name, pc_signal, function_gen, binary_info):
        thread = Signal(comp, parent, name='threads', path='threads',
            include_traces=['thread_lifecycle', 'thread_current', 'insn_is_jal_reg', 'insn_is_jal_noreg', 'irq_enter', 'irq_exit'], display=gvsoc.gui.DisplayStringBox())

        self.config = {
            "type": "threads",
            "path": get_comp_path(comp, True, "threads"),
            "signal_path": '/' + thread.get_path(),
            "pc_trace": get_comp_path(comp, True, pc_signal),
            "thread_lifecyle": get_comp_path(comp, True, 'thread_lifecycle'),
            "thread_current": get_comp_path(comp, True, 'thread_current'),
            "function_gen": get_comp_path(comp, True, function_gen),
            "insn_is_jal_noreg": get_comp_path(comp, True, 'insn_is_jal_noreg'),
            "insn_is_jal_reg": get_comp_path(comp, True, 'insn_is_jal_reg'),
            "binary_info": get_comp_path(comp, True, binary_info),
            "irq_enter": get_comp_path(comp, True, 'irq_enter'),
            "irq_exit": get_comp_path(comp, True, 'irq_exit'),
        }

        parent.gen_signals.append(self)

    def get(self):
        return self.config

class Signal(object):

    def __init__(self, comp, parent, name=None, path=None, is_group=False, groups=None, display=None, properties=None,
                 skip_if_no_child=False, required_traces=None, include_traces=None):
        if path is not None and comp is not None and len(path) != 0 and path[0] != '/':
            comp_path = get_comp_path(comp, inc_top=True)
            if comp_path is not None:
                path = comp_path + '/' + path
            else:
                path = '/' + path
        self.parent = parent
        self.name = name
        self.path = path
        self.child_signals = []
        self.parent = parent
        self.groups = groups if groups is not None else []
        self.type = type

        if not isinstance(self.groups, list):
            self.groups = [self.groups]

        self.gen_signals = []
        self.display = display
        self.properties = properties
        self.is_group = is_group
        self.comp = comp
        self.skip_if_no_child = skip_if_no_child
        if parent is not None:
            parent.child_signals.append(self)
        self.required_traces = required_traces
        self.include_traces = []
        if path is not None:
            self.include_traces.append(path)
        if include_traces is not None:
            self.include_traces += include_traces

    def resolve(self):
        pass

    def resolve_all(self):
        for signal in self.child_signals:
            signal.resolve_all()

        self.resolve()


    def is_combiner(self):
        return False

    def combine(self):
        return True

    def get_path(self):
        if self.parent is None:
            return self.name
        else:
            parent_path = self.parent.get_path()
            if parent_path is None:
                return self.name
            else:
                return parent_path + '/' + self.name

    def get_childs_config(self):
        config = []
        for child_signal in self.child_signals:
            child_config = child_signal.get_config()
            if child_config is not None:
                config.append(child_config)

        return config

    def get_config(self):
        if self.name is None or self.skip_if_no_child and len(self.child_signals) == 0:
            return None

        config = {}

        config['name'] = self.name
        config['groups'] = self.groups
        if self.is_group:
            if self.path is not None:
                config['group'] = self.path
            else:
                config['group'] = get_comp_path(self.comp, inc_top=True)
        if self.path is not None:
            config['path'] = self.path
        if self.display is not None:
            config['display'] = self.display.get()
        childs_config = self.get_childs_config()
        if len(childs_config) != 0:
            config['signals'] = childs_config
        if self.properties is not None:
            config['properties'] = self.properties
        if self.required_traces is not None:
            config['required'] = []
            for trace in self.required_traces:
                path = get_comp_path(self.comp, inc_top=True) + '/' + trace
                config['required'].append(path)
        if self.include_traces is not None:
            config['include_traces'] = []
            for trace in self.include_traces:
                if len(trace) == 0 or trace[0] == '/':
                    path = trace
                else:
                    path = get_comp_path(self.comp, inc_top=True) + '/' + trace
                config['include_traces'].append(path)

        return config

    def get_signals(self):
        signals = [self]
        for child_signal in self.child_signals:
            signals += child_signal.get_signals()

        return signals

    def get_childs_gen_signals(self):
        result = []
        for gen_signal in self.gen_signals:
            config = gen_signal.get()
            if config is not None:
                result.append(config)
        for child_signal in self.child_signals:
            result += child_signal.get_childs_gen_signals()
        return result


class SignalGenFromSignals(Signal):
    def __init__(self, comp, parent, to_signal, from_signals=None, mode="analog_stacked",
        from_groups=None, groups=None, display=None, skip_if_no_child=False):
        super().__init__(comp, parent, to_signal, path=to_signal, groups=groups, display=display,
            skip_if_no_child=skip_if_no_child)

        comp_path = get_comp_path(comp, inc_top=True)
        self.from_signals = []
        self.mode = mode
        self.from_groups = from_groups

        if from_signals is not None:
            for signal in from_signals:
                self.from_signals.append(comp_path + '/' + signal)

        self.to_signal = get_comp_path(comp, inc_top=True) + '/' + to_signal

        parent.gen_signals.append(self)

    def is_combiner(self):
        return True

    def combine(self):
        return len(self.collected_signals) != 0

    def resolve(self):
        from_signals = self.from_signals
        if self.from_groups is not None:

            # We need to collect all the child signals which contain the group fro which
            # we are collecting signals
            signals = deque(self.child_signals)
            while signals:
                signal = signals.popleft()

                if not signal.combine():
                    continue

                # We want to collect all child signals but not within combiners since they are
                # already combine child signals
                if not signal.is_combiner():
                    signals.extend(signal.child_signals)

                # Add the signal if one its group matches one of our group
                for group in self.from_groups:
                    if group in signal.groups:
                        from_signals.append(signal.path)

        self.collected_signals = from_signals


    def get(self):

        if self.name is None or self.skip_if_no_child and len(self.collected_signals) == 0:
            return None

        return {
            "path": self.to_signal,
            "type": "from_signals",
            "subtype": self.mode,
            "from_signals": self.collected_signals
        }


class GuiConfig(Signal):

    def __init__(self, args):
        super().__init__(comp=None, parent=None, name=None, path=None, groups=None)

        self.args = args

    def gen(self, fd):

        self.resolve_all()

        config = {}

        config['config'] = {
            'verbose': self.args.gui_verbose
        }

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
                        "enabled": group != 'power' or self.args.power,
                        "signals": []
                    }

                if signal.is_group:
                    # groups[group]['signals'].append(signal.get_comp_path(comp, inc_top=True))
                    groups[group]['signals'].append(signal.path)
                else:
                    groups[group]['signals'].append(signal.path)

        config['signal_groups'] = list(groups.values())
        config['signals_generate'] = self.get_childs_gen_signals()

        fd.write(json.dumps(config, indent=4))
