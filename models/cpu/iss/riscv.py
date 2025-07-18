from inspect import stack
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

import os
import gvsoc.systree
import gvsoc.systree as st
import os.path
import gvsoc.gui
import cpu.iss.isa_gen.isa_riscv_gen
from cpu.iss.isa_gen.isa_riscv_gen import *
from elftools.elf.elffile import *

binaries_info = {}

binaries = {}


class RiscvCommon(st.Component):
    """
    Riscv instruction set simulator

    Attributes
    ----------
    isa : str, optional
        A string describing the list of ISA groups that the ISS should simulate (default: "rv32imfc").
    misa : int, optional
        The initial value of the MISA CSR (default: 0).
    first_external_pcer : int, optional
        The index of the first PCER which is retrieved externally (default: 0).
    riscv_dbg_unit : bool, optional
        True if a riscv debug unit should be included, False otherwise (default: False).
    debug_binaries : list, optional
        A list of path to riscv binaries debug info which can be used to get debug symbols for the assembly trace (default: []).
    binaries : list, optional
        A list of path to riscv binaries (default: []).
    debug_handler : int, optional
        The address where the core should jump when switching to debug mode (default: 0).
    power_models : dict, optional
        A dictionnay describing all the power models used to estimate power consumption in the ISS (default: {})
    power_models_file : file, optional
        A path to a file describing all the power models used to estimate power consumption in the ISS (default: None)
    cluster_id : int, optional
        The cluster ID of the core simulated by the ISS (default: 0).
    core_id : int, optional
        The core ID of the core simulated by the ISS (default: 0).
    fetch_enable : bool, optional
        True if the ISS should start executing instructins immediately, False if it will start after the fetch_enable signal
        starts it (default: False).
    boot_addr : int, optional
        Address of the first instruction (default: 0)

    """

    def __init__(self,
            parent,
            name,
            isa,
            misa: int=0,
            first_external_pcer: int=0,
            riscv_dbg_unit: bool=False,
            debug_binaries: list=[],
            binaries: list=[],
            debug_handler: int=0,
            power_models: dict={},
            power_models_file: str=None,
            cluster_id: int=0,
            core_id: int=0,
            fetch_enable: bool=False,
            boot_addr: int=0,
            mmu: bool=False,
            pmp: bool=False,
            riscv_exceptions: bool=False,
            core='riscv',
            supervisor=False,
            user=False,
            internal_atomics=False,
            timed=True,
            scoreboard=False,
            cflags=None,
            prefetcher_size=None,
            wrapper="pulp/cpu/iss/default_iss_wrapper.cpp",
            memory_start=None,
            memory_size=None,
            handle_misaligned=False,
            external_pccr=False,
            htif=False,
            custom_sources=False,
            float_lib='flexfloat',
            stack_checker=False
        ):

        super().__init__(parent, name)

        self.isa = isa

        self.add_sources([
            isa.get_source()
        ])

        self.add_c_flags([
            f'--include {isa.get_header()}'
        ])

        self.add_sources([
            wrapper
        ])

        if not custom_sources:
            self.add_sources([
                "cpu/iss/src/prefetch/prefetch_single_line.cpp",
                "cpu/iss/src/csr.cpp",
                "cpu/iss/src/exec/exec_inorder.cpp",
                "cpu/iss/src/decode.cpp",
                "cpu/iss/src/lsu.cpp",
                "cpu/iss/src/timing.cpp",
                "cpu/iss/src/insn_cache.cpp",
                "cpu/iss/src/iss.cpp",
                "cpu/iss/src/core.cpp",
                "cpu/iss/src/exception.cpp",
                "cpu/iss/src/regfile.cpp",
                "cpu/iss/src/resource.cpp",
                "cpu/iss/src/trace.cpp",
                "cpu/iss/src/syscalls.cpp",
                "cpu/iss/src/htif.cpp",
                "cpu/iss/src/memcheck.cpp",
                "cpu/iss/src/mmu.cpp",
                "cpu/iss/src/pmp.cpp",
                "cpu/iss/src/gdbserver.cpp",
                "cpu/iss/src/dbg_unit.cpp",
                "cpu/iss/flexfloat/flexfloat.c",
            ])

        if power_models_file is not None:
            power_models = self.load_property_file(power_models_file)

        self.add_properties({
            'isa': isa.isa_string,
            'misa': misa,
            'first_external_pcer': first_external_pcer,
            'riscv_dbg_unit': riscv_dbg_unit,
            'debug_binaries': debug_binaries,
            'binaries': binaries,
            'debug_handler': debug_handler,
            'power_models': power_models,
            'cluster_id': cluster_id,
            'core_id': core_id,
            'fetch_enable': fetch_enable,
            'boot_addr': boot_addr,
            'has_double': isa.has_isa('rvd'),
        })

        fp_size = 64 if isa.has_isa('rvd') else 32
        self.add_c_flags([f'-DCONFIG_GVSOC_ISS_FP_WIDTH={fp_size}'])

        if memory_start is not None and memory_size is not None:
            self.add_c_flags(['-DCONFIG_GVSOC_ISS_MEMORY=1'])

            self.add_properties({
                'memory_start': memory_start,
                'memory_size': memory_size,
            })


        if cflags is not None:
            self.add_c_flags(cflags)

        self.add_c_flags([
            "-DRISCV=1",
            "-DRISCY",
            "-fno-strict-aliasing",
        ])

        self.add_c_flags([f"-DISS_WORD_{self.isa.word_size}"])

        self.add_c_flags([f'-DCONFIG_GVSOC_ISS_{core.upper()}=1'])

        if supervisor:
            self.add_c_flags(['-DCONFIG_GVSOC_ISS_SUPERVISOR_MODE=1'])

        if stack_checker:
            self.add_c_flags(['-DCONFIG_GVSOC_ISS_STACK_CHECKER=1'])

        if external_pccr:
            self.add_c_flags(['-DCONFIG_GVSOC_ISS_EXTERNAL_PCCR=1'])

        if scoreboard:
            self.add_c_flags(['-DCONFIG_GVSOC_ISS_SCOREBOARD=1'])

        if user:
            self.add_c_flags(['-DCONFIG_GVSOC_ISS_USER_MODE=1'])

        if mmu:
            self.add_c_flags(['-DCONFIG_GVSOC_ISS_MMU=1'])

        if prefetcher_size is not None:
            self.add_c_flags([f'-DCONFIG_GVSOC_ISS_PREFETCHER_SIZE={prefetcher_size}'])

        if timed:
            self.add_c_flags(['-DCONFIG_GVSOC_ISS_TIMED=1'])

        if not float_lib in ['flexfloat', 'native', 'softfloat']:
            raise RuntimeError(f'Unsupported float lib: {float_lib}')

        self.add_c_flags([f'-DCONFIG_GVSOC_ISS_FLOAT_USE_{float_lib.upper()}=1'])

        if float_lib == 'softfloat':
            self.add_sources([
                "cpu/iss/softfloat/softfloat_state.cpp",
                "cpu/iss/softfloat/softfloat_raiseFlags.cpp",
                "cpu/iss/softfloat/s_subMagsF32.cpp",
                "cpu/iss/softfloat/s_addMagsF32.cpp",
                "cpu/iss/softfloat/s_subMagsF128.cpp",
                "cpu/iss/softfloat/s_addMagsF128.cpp",
                "cpu/iss/softfloat/s_add128.cpp",
                "cpu/iss/softfloat/s_sub128.cpp",
                "cpu/iss/softfloat/s_normRoundPackToF128.cpp",
                "cpu/iss/softfloat/s_lt128.cpp",
                "cpu/iss/softfloat/s_eq128.cpp",
                "cpu/iss/softfloat/s_countLeadingZeros64.cpp",
                "cpu/iss/softfloat/s_countLeadingZeros32.cpp",
                "cpu/iss/softfloat/s_countLeadingZeros16.cpp",
                "cpu/iss/softfloat/s_countLeadingZeros8.cpp",
                "cpu/iss/softfloat/s_shiftRightJam32.cpp",
                "cpu/iss/softfloat/s_shiftRightJam64.cpp",
                "cpu/iss/softfloat/s_shiftRightJam128.cpp",
                "cpu/iss/softfloat/s_shiftRightJam128Extra.cpp",
                "cpu/iss/softfloat/s_shortShiftLeft128.cpp",
                "cpu/iss/softfloat/s_shortShiftRightJam64.cpp",
                "cpu/iss/softfloat/s_shortShiftRightJam128.cpp",
                "cpu/iss/softfloat/s_shortShiftRightJam128Extra.cpp",
                "cpu/iss/softfloat/s_roundPackToF32.cpp",
                "cpu/iss/softfloat/s_normRoundPackToF32.cpp",
                "cpu/iss/softfloat/s_propagateNaNF128UI.cpp",
                "cpu/iss/softfloat/s_propagateNaNF64UI.cpp",
                "cpu/iss/softfloat/s_propagateNaNF32UI.cpp",
                "cpu/iss/softfloat/s_propagateNaNF16UI.cpp",
                "cpu/iss/softfloat/s_roundPackToF16.cpp",
                "cpu/iss/softfloat/s_roundPackToF64.cpp",
                "cpu/iss/softfloat/s_roundPackToF128.cpp",
                "cpu/iss/softfloat/s_normSubnormalF16Sig.cpp",
                "cpu/iss/softfloat/s_normSubnormalF32Sig.cpp",
                "cpu/iss/softfloat/s_normSubnormalF64Sig.cpp",
                "cpu/iss/softfloat/s_normSubnormalF128Sig.cpp",
                "cpu/iss/softfloat/s_mulAddF32.cpp",
                "cpu/iss/softfloat/s_mulAddF64.cpp",
                "cpu/iss/softfloat/s_mulAddF16.cpp",
                "cpu/iss/softfloat/f32_add.cpp",
                "cpu/iss/softfloat/f64_mulAdd.cpp",
                "cpu/iss/softfloat/f32_mulAdd.cpp",
                "cpu/iss/softfloat/f16_mulAdd.cpp",
                "cpu/iss/softfloat/f128_mul.cpp",
                "cpu/iss/softfloat/f128_add.cpp",
                "cpu/iss/softfloat/f128_sub.cpp",
            ])
            self.add_c_flags(['-DSOFTFLOAT_FAST_INT64=1'])

        if htif:
            self.add_c_flags(['-DCONFIG_GVSOC_ISS_HTIF=1'])

            if binaries is not None:
                for binary in binaries:
                    binary_info = binaries_info.get(binary)

                    if binary_info is None:
                        tohost_addr = None
                        fromhost_addr = None
                        with open(binary, 'rb') as file:
                            elffile = ELFFile(file)
                            for section in elffile.iter_sections():
                                if isinstance(section, SymbolTableSection):
                                    for symbol in section.iter_symbols():
                                        if symbol.name == 'tohost':
                                            tohost_addr = symbol.entry['st_value']
                                        if symbol.name == 'fromhost':
                                            fromhost_addr = symbol.entry['st_value']

                        binary_info = [tohost_addr, fromhost_addr]
                        binaries_info[binary] = binary_info

                    else:
                        tohost_addr, fromhost_addr = binary_info

                    if fromhost_addr is not None:
                        self.add_property('htif_fromhost', f'0x{fromhost_addr:x}')

                    if tohost_addr is not None:
                        self.add_property('htif_tohost', f'0x{tohost_addr:x}')

        if pmp:
            self.add_c_flags([
                '-DCONFIG_GVSOC_ISS_PMP=1',
                '-DCONFIG_GVSOC_ISS_PMP_NB_ENTRIES=16'])

        if riscv_exceptions:
            self.add_sources(["cpu/iss/src/irq/irq_riscv.cpp"])
            self.add_c_flags(['-DCONFIG_GVSOC_ISS_RISCV_EXCEPTIONS=1'])
        else:
            self.add_sources(["cpu/iss/src/irq/irq_external.cpp"])

        if handle_misaligned:
            self.add_c_flags(['-DCONFIG_GVSOC_ISS_HANDLE_MISALIGNED=1'])

    def handle_executable(self, binary):

        global binaries

        path = binary
        debug_info_path = os.path.join(os.path.dirname(path), f'debug_binary_{os.path.basename(path)}.debugInfo')

        if binaries.get(path) is None:
            binaries[path] = True

            # Only generate debug symbols for small binaries, otherwise it is too slow
            # To allow it, the ISS should itself read the symbols.
            if os.path.getsize(path) < 5 * 1024*1024:
                if os.system('gen-debug-info %s %s' % (path, debug_info_path)) != 0:
                    print('Error while generating debug symbols information, make sure the toolchain and the binaries are accessible ')

        if os.path.getsize(path) < 5 * 1024*1024:
            self.get_property('debug_binaries').append(debug_info_path)

    def generate(self, builddir):
        for executable in self.get_executables():
            self.handle_executable(executable.binary)

    def gen_gtkw(self, tree, comp_traces):

        if tree.get_view() == 'overview':
            map_file = tree.new_map_file(self, 'core_state')
            map_file.add_value(1, 'CadetBlue', 'ACTIVE')

            tree.add_trace(self, self.name, 'state', '[7:0]', map_file=map_file, tag='overview')

        else:
            tree.add_trace(self, 'irq_enable', 'irq_enable', tag='overview')
            tree.add_trace(self, 'irq_enter', 'irq_enter', tag='overview')
            tree.add_trace(self, 'irq_exit', 'irq_exit', tag='overview')
            tree.add_trace(self, 'pc', 'pc', '[31:0]', tag='pc')
            tree.add_trace(self, 'label', 'label', tag='label')
            tree.add_trace(self, 'func', 'func', tag='debug')
            tree.add_trace(self, 'inline_func', 'inline_func', tag='debug')
            tree.add_trace(self, 'file', 'file', tag='debug')
            tree.add_trace(self, 'line', 'line', '[31:0]', tag='debug')

            tree.begin_group('events')
            tree.add_trace(self, 'cycles', 'pcer_cycles', tag='core_events')
            tree.add_trace(self, 'instr', 'pcer_instr', tag='core_events')
            tree.add_trace(self, 'ld_stall', 'pcer_ld_stall', tag='core_events')
            tree.add_trace(self, 'jmp_stall', 'pcer_jmp_stall', tag='core_events')
            tree.add_trace(self, 'imiss', 'pcer_imiss', tag='core_events')
            tree.add_trace(self, 'ld', 'pcer_ld', tag='core_events')
            tree.add_trace(self, 'st', 'pcer_st', tag='core_events')
            tree.add_trace(self, 'jump', 'pcer_jump', tag='core_events')
            tree.add_trace(self, 'branch', 'pcer_branch', tag='core_events')
            tree.add_trace(self, 'taken_branch', 'pcer_taken_branch', tag='core_events')
            tree.add_trace(self, 'rvc', 'pcer_rvc', tag='core_events')
            tree.add_trace(self, 'ld_ext', 'pcer_ld_ext', tag='core_events')
            tree.add_trace(self, 'st_ext', 'pcer_st_ext', tag='core_events')
            tree.add_trace(self, 'ld_ext_cycles', 'pcer_ld_ext_cycles', tag='core_events')
            tree.add_trace(self, 'st_ext_cycles', 'pcer_st_ext_cycles', tag='core_events')
            tree.add_trace(self, 'tcdm_cont', 'pcer_tcdm_cont', tag='core_events')
            tree.add_trace(self, 'misaligned', 'pcer_misaligned', tag='core_events')
            tree.add_trace(self, 'insn_cont', 'pcer_insn_cont', tag='core_events')
            tree.end_group('events')

    def o_FETCH(self, itf: gvsoc.systree.SlaveItf):
        """Binds the fetch port.

        This port is used for fetching instructions from memory.\n
        It instantiates a port of type vp::IoMaster.\n
        It is mandatory to bind it.\n

        Parameters
        ----------
        slave: gvsoc.systree.SlaveItf
            Slave interface
        """
        self.itf_bind('fetch', itf, signature='io')

    def o_DATA(self, itf: gvsoc.systree.SlaveItf):
        """Binds the data port.

        This port is used for issuing data accesses to the memory.\n
        It instantiates a port of type vp::IoMaster.\n
        It is mandatory to bind it.\n

        Parameters
        ----------
        slave: gvsoc.systree.SlaveItf
            Slave interface
        """
        self.itf_bind('data', itf, signature='io')

    def o_MEMINFO(self, itf: gvsoc.systree.SlaveItf):
        self.itf_bind('meminfo', itf, signature='io')

    def o_TIME(self, itf: gvsoc.systree.SlaveItf):
        self.itf_bind('time', itf, signature='wire<uint64_t>')

    def o_DATA_DEBUG(self, itf: gvsoc.systree.SlaveItf):
        """Binds the data debug port.

        This port is used for issuing data accesses from gdb server to the memory.\n
        It instantiates a port of type vp::IoMaster.\n
        If gdbserver is used It is mandatory to bind it.\n

        Parameters
        ----------
        slave: gvsoc.systree.SlaveItf
            Slave interface
        """
        self.itf_bind('data_debug', itf, signature='io')

    def o_FLUSH_CACHE(self, itf: gvsoc.systree.SlaveItf):
        self.itf_bind('flush_cache_req', itf, signature='wire<bool>')

    def i_FLUSH_CACHE_ACK(self) -> gvsoc.systree.SlaveItf:
            return gvsoc.systree.SlaveItf(self, itf_name='flush_cache_ack', signature='wire<bool>')

    def i_FETCHEN(self) -> gvsoc.systree.SlaveItf:
        """Returns the fetch enable port.

        This can be used to control whether the core should execute instructions or not.\n
        It instantiates a port of type vp::WireSlave<bool>.\n

        Returns
        ----------
        gvsoc.systree.SlaveItf
            The slave interface
        """
        return gvsoc.systree.SlaveItf(self, itf_name='fetchen', signature='wire<bool>')

    def i_ENTRY(self) -> gvsoc.systree.SlaveItf:
        """Returns the boot address port.

        This can be used to set the address of the first instruction to be executed, i.e. when the
        core executes instructions for the first time, or after reset.\n
        It instantiates a port of type vp::WireSlave<uint64_t>.\n

        Returns
        ----------
        gvsoc.systree.SlaveItf
            The slave interface
        """
        return gvsoc.systree.SlaveItf(self, itf_name='bootaddr', signature='wire<uint64_t>')

    def i_IRQ_REQ(self) -> gvsoc.systree.SlaveItf:
        return gvsoc.systree.SlaveItf(self, itf_name='irq_req', signature='wire<int>')

    def i_IRQ(self, irq) -> gvsoc.systree.SlaveItf:

        if irq == 3:
            name = 'msi'
        elif irq == 7:
            name = 'mti'
        elif irq == 11:
            name = 'mei'
        elif irq == 9:
            name = 'sei'
        else:
            name = f'external_irq_{irq}'

        return gvsoc.systree.SlaveItf(self, itf_name=name, signature='wire<bool>')

    def o_OFFLOAD(self, itf: gvsoc.systree.SlaveItf):
        self.itf_bind('offload', itf, signature=f'wire<IssOffloadInsn<uint{self.isa.word_size}_t>*>')

    def i_OFFLOAD_GRANT(self) -> gvsoc.systree.SlaveItf:
        return gvsoc.systree.SlaveItf(self, itf_name='offload_grant',
            signature=f'wire<IssOffloadInsnGrant<uint{self.isa.word_size}_t>*>')

    def gen_gtkw_conf(self, tree, traces):
        if tree.get_view() == 'overview':
            self.vcd_group(skip=True)
        else:
            self.vcd_group(skip=False)


    def gen_gui(self, parent_signal):
        active = gvsoc.gui.Signal(self, parent_signal, name=self.name, path='active_function',
            display=gvsoc.gui.DisplayStringBox(), include_traces=['active_pc', 'binaries'])

        gvsoc.gui.Signal(self, active, path='active_pc', groups=['pc'])
        gvsoc.gui.Signal(self, active, path='binaries', groups=['pc'])
        gvsoc.gui.SignalGenFunctionFromBinary(self, active, from_signal='active_pc',
            to_signal='active_function', binaries=['binaries'])

        gvsoc.gui.Signal(self, active, name='label', path='label', groups=['core'], display=gvsoc.gui.DisplayStringBox())
        gvsoc.gui.Signal(self, active, name='active', path='busy', groups=['core'],
            display=gvsoc.gui.DisplayLogicBox('ACTIVE'))
        pc_signal = gvsoc.gui.Signal(self, active, name='PC', path='pc', groups=['pc'],
            properties={'is_hotspot': True})

        gvsoc.gui.SignalGenFunctionFromBinary(self, active, from_signal='pc',
            to_signal='function', binaries=['binaries'])
        gvsoc.gui.Signal(self, active, name='function', path='function',
            display=gvsoc.gui.DisplayString())

        power_signal = gvsoc.gui.Signal(self, active, name='power', path='power', groups='power',
            required_traces=['static_power_trace', 'dyn_power_trace'], display=gvsoc.gui.DisplayAnalog())
        gvsoc.gui.Signal(self, power_signal, name='dynamic', path='dyn_power_trace', groups='power')
        gvsoc.gui.Signal(self, power_signal, name='static', path='static_power_trace', groups='power')

        stalls = gvsoc.gui.Signal(self, active, name='stalls')
        gvsoc.gui.Signal(self, stalls, name="cycles",        path="pcer_cycles",        display=gvsoc.gui.DisplayPulse(), groups=['stall'])
        gvsoc.gui.Signal(self, stalls, name="instr",         path="pcer_instr",         display=gvsoc.gui.DisplayPulse(), groups=['stall'])
        gvsoc.gui.Signal(self, stalls, name="ld_stall",      path="pcer_ld_stall",      display=gvsoc.gui.DisplayPulse(), groups=['stall'])
        gvsoc.gui.Signal(self, stalls, name="jmp_stall",     path="pcer_jmp_stall",     display=gvsoc.gui.DisplayPulse(), groups=['stall'])
        gvsoc.gui.Signal(self, stalls, name="imiss",         path="pcer_imiss",         display=gvsoc.gui.DisplayPulse(), groups=['stall'])
        gvsoc.gui.Signal(self, stalls, name="ld",            path="pcer_ld",            display=gvsoc.gui.DisplayPulse(), groups=['stall'])
        gvsoc.gui.Signal(self, stalls, name="st",            path="pcer_st",            display=gvsoc.gui.DisplayPulse(), groups=['stall'])
        gvsoc.gui.Signal(self, stalls, name="jump",          path="pcer_jump",          display=gvsoc.gui.DisplayPulse(), groups=['stall'])
        gvsoc.gui.Signal(self, stalls, name="branch",        path="pcer_branch",        display=gvsoc.gui.DisplayPulse(), groups=['stall'])
        gvsoc.gui.Signal(self, stalls, name="taken_branch",  path="pcer_taken_branch",  display=gvsoc.gui.DisplayPulse(), groups=['stall'])
        gvsoc.gui.Signal(self, stalls, name="rvc",           path="pcer_rvc",           display=gvsoc.gui.DisplayPulse(), groups=['stall'])
        gvsoc.gui.Signal(self, stalls, name="ld_ext",        path="pcer_ld_ext",        display=gvsoc.gui.DisplayPulse(), groups=['stall'])
        gvsoc.gui.Signal(self, stalls, name="st_ext",        path="pcer_st_ext",        display=gvsoc.gui.DisplayPulse(), groups=['stall'])
        gvsoc.gui.Signal(self, stalls, name="ld_ext_cycles", path="pcer_ld_ext_cycles", display=gvsoc.gui.DisplayPulse(), groups=['stall'])
        gvsoc.gui.Signal(self, stalls, name="st_ext_cycles", path="pcer_st_ext_cycles", display=gvsoc.gui.DisplayPulse(), groups=['stall'])
        gvsoc.gui.Signal(self, stalls, name="tcdm_cont",     path="pcer_tcdm_cont",     display=gvsoc.gui.DisplayPulse(), groups=['stall'])
        gvsoc.gui.Signal(self, stalls, name="misaligned",    path="pcer_misaligned",    display=gvsoc.gui.DisplayPulse(), groups=['stall'])

        thread = gvsoc.gui.SignalGenThreads(self, active, 'thread', 'pc', 'active_function')

        return active

    def gen(self, builddir, installdir):
        self.isa.gen(self, builddir, installdir)



class Riscv(RiscvCommon):
    """Generic riscv model

    This models a generic riscv core using the ISS.
    The ISA can be chosen from the standard riscv isa.
    Instantiating several times this core is only possible if they all have the same isa.
    To get different ISAs, the RiscvCommon class must be used instead so that several ISAs
    are instantiated.

    Attributes
    ----------
    parent: gvsoc.systree.Component
        The parent component where this one should be instantiated.
    name: str
        The name of the component within the parent space.
    isa: str
        A string describing the ISA to be simulated. The format is the one described by the riscv
        specifications.
    binaries: list
        List of static binaries which will be simulated. This is used when profiler is connected
        or in instruction traces to get debug symbols.
    fetch_enable: bool
        True if the core should immediately start executing instructions.
    boot_addr: int
        Boot address, i.e. address where the core will start executing instructions.
    timed: bool
        True if the core should model timing.
    core_id : int, optional
        The core ID of the core simulated by the ISS (default: 0).
    """
    def __init__(self,
            parent: st.Component, name: str, isa: str='rv64imafdc', binaries: list=[],
            fetch_enable: bool=False, boot_addr: int=0, timed: bool=True,
            core_id: int=0, memory_start=None, memory_size=None, htif: bool=False,
            float_lib='flexfloat'):

        # Instantiates the ISA from the provided string.
        isa_instance = cpu.iss.isa_gen.isa_riscv_gen.RiscvIsa(isa, isa, inc_supervisor=True,
            inc_user=True)

        # And instantiate common class with default parameters
        super().__init__(parent, name, isa=isa_instance, misa=isa_instance.misa,
            riscv_exceptions=True, riscv_dbg_unit=True, binaries=binaries, mmu=True, pmp=True,
            fetch_enable=fetch_enable, boot_addr=boot_addr, internal_atomics=True,
            supervisor=True, user=True, timed=timed, prefetcher_size=64, core_id=core_id,
            memory_start=memory_start, memory_size=memory_size, scoreboard=True,
            htif=htif, float_lib=float_lib)

        self.add_c_flags([
            "-DCONFIG_ISS_CORE=riscv",
        ])
