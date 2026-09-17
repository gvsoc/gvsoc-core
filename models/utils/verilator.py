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


def elf_to_verilog_hex(elf_path, hex_path, address_offset=0):
    """Write the loadable contents of an ELF as a verilog hex file, like
    ``objcopy -O verilog`` does: one ``@address`` line per section at its load
    address, followed by its bytes, 16 per line.

    ``address_offset`` is added to every address, like ``--change-addresses``.
    """
    from elftools.elf.elffile import ELFFile
    from elftools.common.utils import struct_parse

    with open(elf_path, 'rb') as elf_file, open(hex_path, 'w', newline='') as hex_file:
        elf = ELFFile(elf_file)
        segments = [segment for segment in elf.iter_segments() if segment['p_type'] == 'PT_LOAD']
        contents = []
        # Only the raw section headers are read: pyelftools fails to build the
        # section object of the .riscv.attributes clang generates.
        for index in range(elf['e_shnum']):
            section = struct_parse(elf.structs.Elf_Shdr, elf_file,
                stream_pos=elf['e_shoff'] + index * elf['e_shentsize'])
            if (section['sh_flags'] & 0x2) == 0 or section['sh_type'] == 'SHT_NOBITS' \
                    or section['sh_size'] == 0:
                continue
            # A section is loaded at the physical address of its segment
            address = section['sh_addr']
            for segment in segments:
                if segment['p_vaddr'] <= address and \
                        address + section['sh_size'] <= segment['p_vaddr'] + segment['p_memsz']:
                    address += segment['p_paddr'] - segment['p_vaddr']
                    break
            address += address_offset
            if address < 0:
                raise RuntimeError(
                    f'{elf_path}: address offset {address_offset:#x} moves the section at '
                    f'{section["sh_addr"]:#x} below address 0')
            elf_file.seek(section['sh_offset'])
            contents.append((address, elf_file.read(section['sh_size'])))

        # Sections are written by increasing address, like objcopy does
        for address, data in sorted(contents, key=lambda content: content[0]):
            hex_file.write(f'@{address:08X}\r\n')
            for offset in range(0, len(data), 16):
                line = ' '.join(f'{byte:02X}' for byte in data[offset:offset + 16])
                hex_file.write(line + '\r\n')


class VerilatorBoard(st.Component):
    """Generic single-component board hosting a :class:`VerilatorControl`.

    Parameters
    ----------
    target_name : str
        Chip target name (used by the build system to compile the right
        executable, e.g. ``"acu.acu_core_v2"``).
    objcopy : str, optional
        Path to an ``objcopy`` binary to convert the ELFs to verilog hex
        with. By default the hex files are written by this module, so no
        binutils are needed.
    objcopy_args : list[str], optional
        Extra arguments for the conversion. Typically
        ``['--change-addresses=-0x10000000']`` when the testbench's
        firmware loader expects addresses rebased to its memory base.
        Without ``objcopy``, only ``--change-addresses`` is supported.
    config : optional
        Forwarded to :class:`Component` ``__init__``.
    """

    def __init__(self, parent, name, target_name,
                 objcopy=None, objcopy_args=None, config=None,
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

        self._objcopy = objcopy
        self._objcopy_args = list(objcopy_args) if objcopy_args else []
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
            if self._objcopy is None:
                elf_to_verilog_hex(elf, hex_path, self._address_offset())
                continue
            cmd = [self._objcopy, '-O', 'verilog', *self._objcopy_args,
                   elf, hex_path]
            proc = subprocess.run(cmd, text=True, capture_output=True)
            if proc.returncode != 0:
                raise RuntimeError(
                    f'VerilatorBoard: objcopy failed (cmd: {" ".join(cmd)}):\n'
                    f'{proc.stderr}')

    def _address_offset(self):
        offset = 0
        for arg in self._objcopy_args:
            name, _, value = arg.partition('=')
            if name != '--change-addresses' or value == '':
                raise RuntimeError(
                    f'VerilatorBoard: unsupported objcopy argument {arg}, only '
                    '--change-addresses=<offset> is handled without an objcopy binary')
            offset += int(value, 0)
        return offset
