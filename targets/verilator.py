# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

#
# Generic gvrun target for verilator-driven RTL designs.
#
# Reachable as `--target verilator`. Loads a verilator plugin .so (built
# against `gvsoc/core/models/utils/verilator_plugin.h`) and drives it from
# GVSoC's TimeEngine via the generic `utils.verilator` model.
#
# Designs typically build two .so flavours:
#   .../obj_dir_plugin/<name>.so                --trace-fst, normal runs
#   .../obj_dir_plugin_inject/<name>_inject.so  --trace, signal injection
#
# The user picks which one to load by passing it as --rtl-plugin (or via
# $VLT_PLUGIN). With --gui, point at the inject flavour. Without --gui,
# point at the FST flavour. The per-test Makefile usually has a `gui`
# target that wires this up correctly.
#

import argparse
import os
from functools import partial

import gvsoc.runner
from utils.verilator import VerilatorBoard


def _is_rtl(args) -> bool:
    return args is not None and args.platform == 'rtl'


class Target(gvsoc.runner.Target):

    gapy_description: str = "Generic verilator-plugin driver"
    name: str = ""

    def __init__(self, parser, options=None, **kwargs):
        if parser is not None:
            try:
                parser.add_argument('--rtl-plugin', dest='rtl_plugin',
                    default=os.environ.get('VLT_PLUGIN'),
                    help='Path to the FST-flavour verilator plugin .so. '
                         'Defaults to $VLT_PLUGIN. Required for --platform rtl.')
                parser.add_argument('--rtl-trace', dest='rtl_trace',
                    default=None,
                    help='Optional trace output path. Off by default.')
            except argparse.ArgumentError:
                pass

        # The whole target is verilator-specific, so we always use the
        # VerilatorBoard model — this lets `make TARGETS=verilator build`
        # discover the C++ component to compile. The plugin path is a
        # runtime concern, validated below only when --platform rtl.
        peek = argparse.ArgumentParser(add_help=False)
        peek.add_argument('--gui', action='store_true', default=False)
        peek_args, _ = peek.parse_known_args()
        self.model = partial(VerilatorBoard,
            target_name='verilator',
            inject_signals=peek_args.gui)

        super().__init__(parser=parser, options=options, **kwargs)

        args = self.args
        if not _is_rtl(args):
            return

        plugin = args.rtl_plugin
        if plugin is None:
            raise RuntimeError(
                'verilator target: --rtl-plugin <path> (or $VLT_PLUGIN) is '
                'required when --platform rtl is used.')

        if not os.path.exists(plugin):
            raise RuntimeError(
                f'verilator target: plugin .so not found at {plugin}. '
                f'Build it with the design\'s Makefile (typically '
                f'`make plugin`).')

        self.model.set_parameter('plugin_path', plugin)
        if args.rtl_trace is not None:
            self.model.set_parameter('trace_path', args.rtl_trace)
