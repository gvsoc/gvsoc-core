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
from gvrun.runtime import Runtime
from config_tree import Config, cfg_field
from typing import Annotated


class FsdbElementSizeConfig(Config):
    """Splits a FSDB signal into elements of a given width."""

    signal: str = cfg_field(default="", desc="Path of the signal, with or without a leading '/'")
    size: int = cfg_field(default=0, desc="Width in bits of each element")


class FsdbDumperConfig(Config):
    """Configuration of the FSDB dumper."""

    # Runtime: both come from target parameters, set for each run
    fsdb_file: Annotated[str, Runtime] = cfg_field(default="", dump=True, desc=(
        "FSDB file to replay, empty for none"
    ))
    element_size: Annotated[list[FsdbElementSizeConfig], Runtime] = cfg_field(
        default_factory=list, desc="Signals to split into elements, from the layout file")


class FsdbDumper(st.Component):

    def __init__(self, parent, name, parser, options):

        config = FsdbDumperConfig()

        super(FsdbDumper, self).__init__(parent, name, options=options, config=config)

        self.set_component('utils.fsdb_dumper')

        self.fsdb_file = TargetParameter(
            self, name='fsdb_file', value=None, description='FSDB input file',
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

        if self.fsdb_file is not None:
            config.fsdb_file = self.fsdb_file

        # Pre-load the layout so any element_size overrides it carries can be
        # handed to the C++ side through the config before the simulator
        # config is dumped. gen_gui() will pick up `self._layout` again to
        # emit the signals_generate rules at GUI-config time.
        self._layout = None
        if self.layout_file is not None:
            self._layout = WaveLayout.load(self.layout_file)
            config.element_size = [FsdbElementSizeConfig(signal=signal, size=int(size))
                for signal, size in self._layout.element_sizes.items()]

    def gen_gui(self, parent_signal):
        root = gvsoc.gui.Signal(self, parent_signal, name='fsdb', opened=True)
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
            # Default: mirror the FSDB path hierarchy under /fsdb.
            gvsoc.gui.SignalGenAll(self, root, signal_path=root_path)

        return parent_signal


class Target(gvsoc.runner.Target):

    gapy_description = "FSDB dumper"

    def __init__(self, parser, options=None, name=None):
        super(Target, self).__init__(parser, options, model=FsdbDumper, name=name)
