# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

"""GVSoC + Verilator integration.

This module provides two reusable classes that any gvrun target can plug
in to drive a Verilator-built RTL design from GVSoC's TimeEngine:

  * :class:`VerilatorControl` — thin wrapper around the C++ ``utils.verilator``
    component, which dlopens a per-design plugin ``.so`` at runtime and
    drives its ``step()`` from a permanent :class:`vp::ClockEvent`. The
    plugin contract is documented in ``verilator_plugin.h``.

  * :class:`VerilatorBoard` — a generic top-level :class:`Component` that
    instantiates one :class:`VerilatorControl`, wires a clock domain to
    it, exposes the ``plugin_path``/``trace_path`` :class:`TargetParameter`
    pair, and converts ELF firmwares to verilog hex on demand.

Use by any gvrun target via, e.g.:

    from functools import partial
    from utils.verilator import VerilatorBoard

    class Target(gvsoc.runner.Target):
        model = partial(VerilatorBoard, target_name='acu.acu_core_v2')

The target's ``run()`` override must call ``self.model.run_objcopy()``
**after** ``build`` has completed (i.e. before ``super().run()``), so the
``.hex`` files survive any earlier ``clean``.
"""

import os
import subprocess

import gvsoc.gui
import gvsoc.systree as st
from gvrun.parameter import TargetParameter


class VerilatorControl(st.Component):
    """Bind to the C++ ``utils.verilator`` model that owns the
    :class:`VerilatedContext` and steps the design once per clock cycle."""

    def __init__(self, parent, name, plugin_path=None, firmwares=None, trace_path=None,
                 inject_signals=False):
        super().__init__(parent, name)
        self.add_sources(['utils/verilator.cpp'])
        if plugin_path is not None:
            self.add_property('plugin_path', plugin_path)
        if firmwares:
            self.add_property('firmwares', list(firmwares))
        if trace_path is not None:
            self.add_property('trace_path', trace_path)
        if inject_signals:
            self.add_property('inject_signals', True)

    def gen_gui(self, parent_signal):
        # Only emit the SignalGenAll entry when the plugin will actually
        # inject signals. Otherwise the GUI subscribes to a stream that
        # never produces events.
        if not self.properties.get('inject_signals'):
            return parent_signal
        # SignalGenAll ("type": "all") tells the GUI to listen for trace-
        # creation events under prefix and auto-populate the signal tree
        # at signal_path. We anchor the vp::Signal objects at the model's
        # top (see vl_reg_logical), so trace paths look like
        # "/<rtl-name>" — empty prefix matches them all and empty
        # signal_path drops them at the GUI tree root.
        gvsoc.gui.SignalGenAll(self, parent_signal, signal_path='', prefix='')
        return parent_signal


class VerilatorBoard(st.Component):
    """Generic single-component board hosting a :class:`VerilatorControl`.

    Parameters
    ----------
    target_name : str
        Chip target name (used by the build system to compile the right
        executable, e.g. ``"acu.acu_core_v2"``).
    objcopy : str, optional
        Path to the ``objcopy`` binary that converts ELFs to verilog hex.
        Defaults to ``$RISCV32_GCC_TOOLCHAIN/bin/riscv32-unknown-elf-objcopy``.
    config : optional
        Forwarded to :class:`Component` ``__init__``.
    """

    def __init__(self, parent, name, target_name,
                 objcopy=None, config=None,
                 inject_signals=False):
        if config is not None:
            super().__init__(parent, name, config=config)
        else:
            super().__init__(parent, name)
        self.set_target_name(target_name)

        TargetParameter(
            self, name='plugin_path', value=None, cast=str,
            description='Path to the verilator plugin .so to load at runtime',
        )
        TargetParameter(
            self, name='trace_path', value=None, cast=str,
            description='Optional VCD/FST trace output path forwarded to the plugin',
        )

        # No clock domain — VerilatorControl uses TimeEvent driven by the
        # plugin's reported next-event delta, so it doesn't need a tick
        # source. The plugin owns its own VerilatedContext time, GVSoC
        # advances absolute time by whatever the plugin returns.
        self.verilator = VerilatorControl(self, 'verilator', inject_signals=inject_signals)

        self._objcopy = objcopy or (
            os.environ.get('RISCV32_GCC_TOOLCHAIN', '') + '/bin/riscv32-unknown-elf-objcopy')
        self._firmwares = []
        self.register_binary_handler(self._handle_binary)

    def _handle_binary(self, binary):
        if binary is not None:
            self._firmwares.append(binary)

    def configure(self):
        # plugin_path is a runtime requirement only — the per-target SDK
        # build (gvrun ... components) walks the systree without it, so
        # leaving it unset must not raise. If we still don't have one at
        # actual run time the C++ model raises a clear fatal.
        plugin_path = self.get_parameter('plugin_path')
        if plugin_path is not None:
            self.verilator.add_property('plugin_path', plugin_path)
        trace_path = self.get_parameter('trace_path')
        if trace_path is not None:
            self.verilator.add_property('trace_path', trace_path)

        # Register the predicted .hex paths *now*. The ELF→hex conversion
        # itself is deferred to :meth:`run_objcopy` because configure()
        # runs before the user's `clean`/`build` commands; producing the
        # actual hex here would get wiped by `clean`. The C++ model
        # serialises this property to JSON at build-time, so it must be
        # set early even though the file doesn't exist yet.
        if self._firmwares:
            self.verilator.add_property(
                'firmwares', [elf + '.hex' for elf in self._firmwares])

    def run_objcopy(self):
        """Create the .hex files from the registered ELFs. Must be called
        **after** ``build`` (so the ELF exists on disk) and **before**
        ``super().run()`` launches gvsoc (which reads the hex)."""
        for elf in self._firmwares:
            if elf is None or not os.path.exists(elf):
                continue
            hex_path = elf + '.hex'
            cmd = [self._objcopy, '-O', 'verilog', elf, hex_path]
            proc = subprocess.run(cmd, text=True, capture_output=True)
            if proc.returncode != 0:
                raise RuntimeError(
                    f'VerilatorBoard: objcopy failed (cmd: {" ".join(cmd)}):\n'
                    f'{proc.stderr}')
