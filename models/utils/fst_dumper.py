#
# Copyright (C) 2026 ETH Zurich, University of Bologna
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

import gvsoc.gui
import gvsoc.runner
import gvsoc.systree as st
from gvsoc.gui import WaveLayout
from gvrun.parameter import TargetParameter


class FstDumper(st.Component):

    def __init__(self, parent, name, parser, options):

        super(FstDumper, self).__init__(parent, name, options=options)

        self.set_component('utils.fst_dumper')

        self.fst_file = TargetParameter(
            self, name='fst_file', value=None, description='FST input file',
            cast=str
        ).get_value()

        self.layout_file = TargetParameter(
            self, name='layout', value=None,
            description='Path to a layout file organising signals into custom groups '
                        'in the GUI. Accepts either a .json produced by '
                        'WaveLayout.save(), or a .py script that constructs a '
                        'WaveLayout at module scope (executed in a private namespace, '
                        'so any `if __name__ == "__main__"` block is skipped).',
            cast=str
        ).get_value()

        if self.fst_file is not None:
            self.add_properties({
                'fst_file': self.fst_file
            })

        # Pre-load the layout so any element_size overrides it carries can be
        # handed to the C++ side as a component property before the simulator
        # config is dumped. gen_gui() will pick up `self._layout` again to
        # emit the signals_generate rules at GUI-config time.
        self._layout = None
        if self.layout_file is not None:
            self._layout = WaveLayout.load(self.layout_file)
            if self._layout.element_sizes:
                self.add_properties({'element_size': self._layout.element_sizes})

    def gen_gui(self, parent_signal):
        root = gvsoc.gui.Signal(self, parent_signal, name='fst', opened=True)
        root_path = '/' + root.get_path()

        if self._layout is not None:
            # User-provided layout: emit one glob rule per entry.
            for entry in self._layout.to_signals_generate(root_signal_path=root_path):
                gvsoc.gui.SignalGenGlob(
                    self, root,
                    pattern=entry['pattern'],
                    signal_path=entry['signal_path'],
                    display=entry.get('display'),
                )
        else:
            # Default: mirror the FST path hierarchy under /fst.
            gvsoc.gui.SignalGenAll(self, root, signal_path=root_path)

        return parent_signal


class Target(gvsoc.runner.Target):

    gapy_description = "FST dumper"

    def __init__(self, parser, options=None, name=None):
        super(Target, self).__init__(parser, options, model=FstDumper, name=name)
