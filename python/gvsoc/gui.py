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
    """Analog curve display.

    Parameters
    ----------
    aggregation : str, optional
        How zoomed-out slots and area averages combine the values of their time
        range: 'average' (time-weighted mean, default, e.g. power) or 'max'
        (peaks stay visible, e.g. stack usage).
    """
    def __init__(self, aggregation: str='average'):
        self.aggregation = aggregation

    def get(self):
        if self.aggregation == 'max':
            return { 'type': 'analog', 'aggregation': 'max' }
        return { 'type': 'analog' }

class DisplayBox(object):
    """Integer value display.

    Parameters
    ----------
    format : str, optional
        'hex' (default) or 'dec'.
    aggregation : str, optional
        Default metric of the area average column: 'average' (per-cycle
        average), 'sum' (sum of value x cycles over the selection, i.e. the
        total number of events for a count asserted one cycle per event), or
        None to let the box decide (average for a 1-bit value, blank for a
        wider one). The GUI context menu can override it per signal.
    """
    def __init__(self, format="hex", aggregation: str=None):
        self.format = format
        self.aggregation = aggregation

    def get(self):
        config = { 'type': 'box', 'format': self.format }
        if self.aggregation is not None:
            config['aggregation'] = self.aggregation
        return config

class DisplayLogicBox(object):
    def __init__(self, message):
        self.message = message

    def get(self):
        return { 'type': 'logic_box', 'message': self.message }

# A small subset of the X11/GTKWave colour names that were used by the old
# gen_gtkw() map_files, mapped to 0xAARRGGBB so DisplayStateBox can accept them
# for drop-in parity. Any other colour can be passed directly as an int.
_STATE_COLORS = {
    'black':     0xFF000000,
    'white':     0xFFFFFFFF,
    'red':       0xFFFF0000,
    'green':     0xFF008000,
    'blue':      0xFF0000FF,
    'yellow':    0xFFFFFF00,
    'orange':    0xFFFFA500,
    'cyan':      0xFF00FFFF,
    'magenta':   0xFFFF00FF,
    'gray':      0xFF808080,
    'grey':      0xFF808080,
    'cadetblue': 0xFF5F9EA0,
}

class DisplayStateBox(object):
    """Render an integer state signal as named, coloured boxes.

    This is the GUI equivalent of the old gen_gtkw() `map_file`: each integer
    value is mapped to a human-readable label and (optionally) a colour, so an
    FSM-state signal shows e.g. `IDLE`/`MATRIXVEC`/`END` instead of a raw hex
    value. Unmapped values fall back to plain hex/dec formatting.

    labels : dict mapping ``int_value`` to either ``name``, ``(name, color)`` or
             ``None``. ``color`` may be a 0xAARRGGBB int or an X11/GTKWave colour
             name. ``None`` renders that value as a high-Z (inactive) line
             instead of a box -- handy for an IDLE state so only active states
             draw a labelled box.
    format : fallback formatting ("hex" or "int") for unmapped values.
    """

    def __init__(self, labels, format="hex"):
        self.labels = labels
        self.format = format

    def get(self):
        out = []
        for value, spec in self.labels.items():
            if spec is None:
                # High-Z: no label, rendered as an inactive line.
                out.append({ 'v': int(value), 'highz': True })
                continue
            if isinstance(spec, (tuple, list)):
                name, color = spec
            else:
                name, color = spec, None
            entry = { 'v': int(value), 'l': name }
            if color is not None:
                if isinstance(color, str):
                    color = _STATE_COLORS[color.lower()]
                entry['c'] = int(color)
            out.append(entry)
        return { 'type': 'state_box', 'format': self.format, 'labels': out }

def display_build(display_type: str, format: str=None, aggregation: str=None, message: str=None,
        labels: dict=None):
    """Build a display from its type name and options, as given on the command line.

    display_type is one of box, string_box, string, pulse, analog, logic_box or state_box.
    The options which do not apply to the display type are rejected.
    """
    options = { 'format': format, 'aggregation': aggregation, 'message': message,
        'labels': labels }
    allowed = {
        'box': ['format', 'aggregation'],
        'string_box': [],
        'string': [],
        'pulse': [],
        'analog': ['aggregation'],
        'logic_box': ['message'],
        'state_box': ['labels', 'format'],
    }
    if display_type not in allowed:
        raise RuntimeError(f'Unknown display type: {display_type} (known: '
            f'{", ".join(allowed.keys())})')
    for name, value in options.items():
        if value is not None and name not in allowed[display_type]:
            raise RuntimeError(f'Display option {name} does not apply to {display_type}')

    if display_type == 'box':
        return DisplayBox(format=format if format is not None else 'hex', aggregation=aggregation)
    elif display_type == 'string_box':
        return DisplayStringBox()
    elif display_type == 'string':
        return DisplayString()
    elif display_type == 'pulse':
        return DisplayPulse()
    elif display_type == 'analog':
        return DisplayAnalog(aggregation=aggregation if aggregation is not None else 'average')
    elif display_type == 'logic_box':
        if message is None:
            raise RuntimeError('Display logic_box needs a message')
        return DisplayLogicBox(message)
    else:
        if labels is None:
            raise RuntimeError('Display state_box needs labels')
        return DisplayStateBox(labels, format=format if format is not None else 'hex')

def get_comp_path(comp, inc_top=False, child_path=None):
    return '/' + comp.get_path(child_path=child_path)

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

class SignalGenAll(object):
    """Catch-all dynamic signal generator.

    Emits a `signals_generate` entry of type `all`. At runtime the GUI
    timeline subscribes to trace creation events matching the given prefix
    and auto-builds the signal tree from their paths. Used by dumper-style
    components (utils.fsdb_dumper, ...) that inject external waveforms
    whose signal list is not known at config-generation time.
    """

    def __init__(self, comp, parent, signal_path, prefix=""):
        self.config = {
            "type": "all",
            "signal_path": signal_path,
            "prefix": prefix,
        }
        parent.gen_signals.append(self)

    def get(self):
        return self.config


class SignalGenGlob(object):
    """Glob-based dynamic signal generator.

    Emits a `signals_generate` entry of type `glob`. At runtime the GUI
    timeline matches every new trace path against `pattern` (fnmatch, with
    FNM_PATHNAME semantics -- '*' does not cross '/') and places matches
    under `signal_path` in the GUI signal tree. Intended for explicit,
    SimVision-style organisation of signals into custom groups.
    """

    def __init__(self, comp, parent, pattern, signal_path, display=None):
        self.config = {
            "type": "glob",
            "pattern": pattern,
            "signal_path": signal_path,
        }
        if display is not None:
            self.config["display"] = display
        parent.gen_signals.append(self)

    def get(self):
        return self.config


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
                 skip_if_no_child=False, required_traces=None, include_traces=None, opened=False,
                 expand_fields=False):
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
        self.opened = opened
        # When this is a regmap group, auto-expand each imported register into
        # its named bit-field rows in the timeline.
        self.expand_fields = expand_fields
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
            if self.expand_fields:
                config['expand_fields'] = True
        if self.path is not None:
            config['path'] = self.path
        if self.display is not None:
            config['display'] = self.display.get()
        childs_config = self.get_childs_config()
        if len(childs_config) != 0:
            config['signals'] = childs_config
        if self.opened:
            config['opened'] = True
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


class WaveLayout(object):
    """Organise signals into a user-defined group hierarchy for the GUI.

    A small helper to arrange signals in the GVSoC GUI timeline into
    arbitrary nested groups, independent of the underlying trace paths.
    Entries pair a group-path (a list of nested group names) with an
    fnmatch-style glob pattern that the runtime matches against trace
    paths at display time.

    Typical usage:

        from gvsoc.gui import WaveLayout

        w = WaveLayout()
        for i in range(8):
            w.add(["cores", f"core_{i}", "fpu_0"],
                  f"/tb_top/dut/CORE[{i}]/core_region_i/i_snitch_cc/i_riscv_core/i_riscv_fpu_wrapper/*")
            w.add(["cores", f"core_{i}", "core"],
                  f"/tb_top/dut/CORE[{i}]/core_region_i/i_snitch_cc/i_riscv_core/*")
        w.add(["top"], "/tb_top/dut/*")

        w.save("my_layout.json")

    The layout is consumed as a sequence of `signals_generate` entries of
    type 'glob'; components that accept a `layout=` parameter load it via
    `WaveLayout.load()`, which also accepts a Python script that builds a
    WaveLayout at module scope (the script runs with `__name__` set to a
    private sentinel so an `if __name__ == "__main__": ...` save stanza at
    the bottom is skipped).
    """

    def __init__(self):
        self.entries = []
        self.element_sizes = {}

    def element_size(self, signal_path, size):
        """Override the auto-chunking width for a wide signal.

        When a trace is exposed as a flat 1-D variable but actually
        represents N elements of M bits (e.g. a 2D packed array flattened
        to a single `foo[1535:0]` of 128 x 12-bit elements), set the
        element size so consumers can split the signal at element
        boundaries instead of the default 64-bit chunks.

        Parameters
        ----------
        signal_path : str
            Full trace path of the wide signal including the trailing
            bit-range suffix, e.g.
            ``"/tb_top/dut/u_acu_queue_wrap/u_acu_queue/push_elem_q[1535:0]"``.
        size : int
            Element width in bits, capped at 64.
        """
        if size <= 0 or size > 64:
            raise ValueError("element size must be in [1, 64]")
        self.element_sizes[signal_path] = int(size)

    def add(self, group_path, pattern, display=None):
        """Add an entry mapping a glob pattern to a GUI group path.

        Parameters
        ----------
        group_path : list[str]
            Ordered list of group names, e.g. ["cores", "core_0", "fpu_0"].
            Each becomes a nested node in the GUI signal tree.
        pattern : str
            fnmatch-style glob over trace paths, with shell-like semantics
            ('*' does not cross '/'). Matches are placed under `group_path`.
            Character classes like `[0]` stay literal -- fnmatch interprets
            them as classes but that matches the same characters, so a
            pattern of the form `CORE[0]` works as-is.
        display : dict | None
            Optional override, e.g. `{"type": "box", "format": "dec"}` or
            `{"type": "pulse"}`. Defaults are auto-picked from trace width
            (1-bit -> pulse, else -> hex box).
        """
        if not isinstance(group_path, (list, tuple)):
            raise TypeError("group_path must be a list of group names")
        self.entries.append({
            "group_path": list(group_path),
            "pattern": pattern,
            "display": display,
        })

    def add_regex(self, group_path, pattern, display=None):
        """Reserved for future use; emits the same glob entry today."""
        self.add(group_path, pattern, display=display)

    def to_signals_generate(self, root_signal_path=""):
        """Return the list of signals_generate entries ready for the GUI.

        Parameters
        ----------
        root_signal_path : str
            Absolute path in the GUI signal tree under which every group
            path is rooted. Empty string to root at the signal tree itself.
        """
        out = []
        for e in self.entries:
            if root_signal_path:
                full = root_signal_path.rstrip("/") + "/" + "/".join(e["group_path"])
            else:
                full = "/".join(e["group_path"])
            entry = {
                "type": "glob",
                "pattern": e["pattern"],
                "signal_path": full,
            }
            if e["display"] is not None:
                entry["display"] = e["display"]
            out.append(entry)
        return out

    def save(self, path):
        """Write the layout to `path` as JSON, suitable for a component's
        `layout=` parameter."""
        with open(path, "w") as f:
            json.dump({
                "entries": [
                    {"group_path": e["group_path"],
                     "pattern": e["pattern"],
                     "display": e["display"]}
                    for e in self.entries
                ],
                "element_sizes": self.element_sizes,
            }, f, indent=2)

    @classmethod
    def load(cls, path):
        """Load a layout from `path`.

        Accepts either a JSON file saved with `WaveLayout.save()` or a
        Python script that constructs a WaveLayout at module scope. For
        .py scripts the module is executed with `__name__` set to a
        private sentinel so an `if __name__ == "__main__": ...` stanza at
        the bottom (used when the same script is run standalone to
        produce a JSON) is skipped. Returns the first WaveLayout instance
        found in the script's globals.
        """
        if str(path).endswith(".py"):
            import runpy
            ns = runpy.run_path(str(path), run_name="_gvsoc_wave_layout")
            for value in ns.values():
                if isinstance(value, cls):
                    return value
            raise RuntimeError(
                f"No WaveLayout instance found at module scope in {path}")

        with open(path) as f:
            data = json.load(f)
        w = cls()
        # New object form: {"entries": [...], "element_sizes": {...}}
        # Legacy list form: [ ... ]  (no element_sizes)
        if isinstance(data, dict):
            entries = data.get("entries", [])
            w.element_sizes = dict(data.get("element_sizes", {}))
        else:
            entries = data
        for e in entries:
            w.add(e["group_path"], e["pattern"], display=e.get("display"))
        return w
