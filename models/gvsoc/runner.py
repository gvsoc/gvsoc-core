8#
# Copyright (C) 2020 GreenWaves Technologies, SAS, ETH Zurich and
#                    University of Bologna
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

# 
# Authors: Germain Haugou, GreenWaves Technologies (germain.haugou@greenwaves-technologies.com)
#

import gapylib.target
import gvsoc.systree as st
import gvsoc.json_tools as js
import os.path
from gvsoc.gtkwave import Gtkwave_tree
from gvsoc.gui import GuiConfig
import gvsoc.gui
import gapylib.target as gapy
import sys
import rich.tree
import rich
import rich.table
import random
import pexpect
import threading
import importlib
import gvsoc.gvsoc_control
import signal
import traceback
import shlex


def gen_config(args, config, working_dir, runner, cosim_mode):

    full_config =  js.import_config(config, interpret=False, gen=False)

    gvsoc_config = full_config.get('target/gvsoc')

    gvsoc_config.set('systemc', full_config.get('**/require_systemc') is not None)
    gvsoc_config.set('werror', args.werror)
    gvsoc_config.set('events/use-external-dumper', args.gui and not cosim_mode)
    gvsoc_config.set('wunconnected-padfun', args.w_unconnected_padfun)
    gvsoc_config.set('memcheck', args.memcheck)

    for trace in args.traces:
        gvsoc_config.set('traces/include_regex', trace)

    if args.trace_level is not None:
        gvsoc_config.set('traces/level', args.trace_level)

    if args.trace_format is not None:
        gvsoc_config.set('traces/format', args.trace_format)

    if args.vcd:
        gvsoc_config.set('events/enabled', True)
        gvsoc_config.set('events/gen_gtkw', True)

    for event in args.events:
        gvsoc_config.set('events/include_regex', event)

    for tag in args.event_tags:
        gvsoc_config.set('events/tags', tag)

    if args.format is not None:
        gvsoc_config.set('events/format', args.format)

    if args.gtkwi:
        gvsoc_config.set('events/gtkw', True)

    debug_mode = gvsoc_config.get_bool('debug-mode') or \
        gvsoc_config.get_bool('traces/enabled') or \
        gvsoc_config.get_bool('events/enabled') or \
        len(gvsoc_config.get('traces/include_regex')) != 0 or \
        len(gvsoc_config.get('events/include_regex')) != 0 or \
        args.gui and not cosim_mode or \
        args.memcheck

    gvsoc_config.set("debug-mode", debug_mode)

    if debug_mode:
        debug_binaries = []

        if args.binary is not None:
            debug_binaries.append(args.binary)

        if args.debug_binary is not None:
            debug_binaries += args.debug_binary

        rom_binary = full_config.get_str('**/soc/rom/config/binary')

        if rom_binary is not None:

            if runner is not None:
                rom_binary = runner.gapy_target.get_file_path(rom_binary)

            if os.path.exists(rom_binary):
                debug_binaries.append(rom_binary)

        for index, binary in enumerate(debug_binaries):
            debug_binary = os.path.join(working_dir,
                f'debug_binary_{index}_{os.path.basename(binary)}.debugInfo')
            full_config.set('**/debug_binaries', debug_binary)
            full_config.set('**/binaries', binary)

    gvsoc_config_path = 'gvsoc_config.json'

    return full_config, gvsoc_config_path


def dump_config(full_config, gvsoc_config_path):
    with open(gvsoc_config_path, 'w') as file:
        file.write(full_config.dump_to_string())


class Runner():

    class ExpectLoopThread(threading.Thread):

        def __init__(self, process):
            super().__init__()
            self.process = process
            self.kill = False

        def run(self):
            while not self.kill:
                match = self.process.expect([pexpect.EOF, pexpect.TIMEOUT], timeout=0.01)
                if match == 0:
                    self.kill = False
                    break

            if self.kill:
                self.process.kill(signal.SIGKILL)


    def __init__(self, parser, args, options, gapy_target, target, rtl_cosim_runner=None):

        self.target = target
        self.gapy_target = gapy_target

        if parser is not None:
            self.target.add_properties({
                "gvsoc": {
                    "proxy": {
                        "enabled": False,
                        "port": 42951
                    },
                    "events": {
                        "enabled": False,
                        "include_raw": [],
                        "include_regex": [],
                        "exclude_regex": [],
                        "format": "fst",
                        "active": False,
                        "all": True,
                        "gtkw": False,
                        "gen_gtkw": False,
                        "files": [ ],
                        "traces": {},
                        "tags": [ "overview" ],
                        "gtkw": False,
                    },

                    "include_dirs": args.install_dirs,

                    "runner_module": "gv.gvsoc",
                
                    "cycles_to_seconds": "int(max(cycles * nb_cores / 5000000, 600))",
                
                    "werror": True,
                    "verbose": True,
                    "debug-mode": False,
                
                    "launchers": {
                        "default": "gvsoc_launcher",
                        "debug": "gvsoc_launcher_debug"
                    },
                
                    "traces": {
                        "level": "debug",
                        "format": "long",
                        "enabled": False,
                        "include_regex": [],
                        "exclude_regex": []
                    }
                }
            })

        cosim_mode = False
        choices = ['gvsoc']

        self.rtl_runner = None

        if rtl_cosim_runner is not None:

            parser.add_argument("--rtl-cosimulation", dest="rtl_cosimulation", action="store_true",
                help="Launch in RTL cosimulation mode")

            [args, _] = parser.parse_known_args()

            cosim_mode = args.rtl_cosimulation

            if args.rtl_cosimulation:

                parser.add_argument("--gvsoc-path", dest="gvsoc_path", default=None,
                    type=str, help="path to GVSOC install folder")

                choices.append('rtl')
                self.rtl_runner = rtl_cosim_runner(self.gapy_target)
                self.rtl_runner.append_args(parser)

        if not cosim_mode:
            parser.add_argument("--gui", dest="gui", default=None, action="store_true",
                help="Open GVSOC gui")

        gapy_target.register_command_handler(self.gv_handle_command)



        [args, _] = parser.parse_known_args()

        self.full_config, self.gvsoc_config_path = gen_config(
            args, { 'target': self.target.get_config() }, gapy_target.get_working_dir(), self, cosim_mode)

        if args.gdbserver:
            self.full_config.set('**/gdbserver/enabled', True)
            self.full_config.set('**/gdbserver/port', args.gdbserver_port)

        gvsoc_config = self.full_config.get('target/gvsoc')

        if gvsoc_config.get_bool('events/gen_gtkw'):
            path = os.path.join(gapy_target.get_working_dir(), 'view.gtkw')

            traces = self.gen_gtkw_script(
                work_dir=self.gapy_target.get_working_dir(),
                path=path,
                tags=gvsoc_config.get('events/tags').get_dict(),
                level=gvsoc_config.get_child_int('events/level'),
                gen_full_tree=False
            )

            gvsoc_config.get('events').set('include_raw', traces)

            print ()
            print ('A Gtkwave script has been generated and can be opened with the following command:')
            print ('gtkwave ' + path)
            print ()

        if self.rtl_runner is not None:
            gvsoc_config.set('sv-mode', True)

            if args.gvsoc_path is None:
                raise RuntimeError('GVSOC install path must be specified through option'
                    ' --gvsoc-path when using RTL GVSOC cosimulation')

            self.rtl_runner.parse_args(args, gvsoc_cosim=args.gvsoc_path,
                gvsoc_config_path=self.gvsoc_config_path, full_config=self.full_config)


    def gv_handle_command(self, cmd):

        if cmd == 'run':
            self.run()
            return True

        elif cmd == 'regmap_copy':
            self.target.regmap(copy=True)
            return True

        elif cmd == 'regmap_gen':
            self.target.regmap(gen=True)
            return True

        elif cmd == 'traces' and self.rtl_runner is not None:
            self.rtl_runner.traces()
            return True

        elif cmd == 'prepare':
            self.run(norun=True)
            return True

        elif cmd == 'image':
            self.gapy_target.handle_command_image()

            if self.rtl_runner is not None:
                self.rtl_runner.image()
            else:
                self.target.gen_stimuli()
            return True

        elif cmd == 'components':

            if self.gapy_target.get_args().builddir is None:
                raise RuntimeError('Build diretory must be specified when components are being generated')

            if self.gapy_target.get_args().installdir is None:
                raise RuntimeError('Install diretory must be specified when components are being generated')

            self.target.gen_all(self.gapy_target.get_args().builddir, self.gapy_target.get_args().installdir)

            generated_components = self.target.get_generated_components()

            gen_comp_list = []
            for comp in generated_components.values():
                gen_comp_list.append(comp.name)

            component_list = []

            c_flags = {}

            if self.gapy_target.get_args().component_file_append:
                with open(self.gapy_target.get_args().component_file, "r") as file:
                    for comp_desc in file.readlines():
                        comp = comp_desc.replace('CONFIG_', '').replace('=1\n', '')
                        component_list.append(comp)

            component_list += self.target.get_component_list(c_flags) + ['vp.power_domain_impl', 'utils.composite_impl']

            with open(self.gapy_target.get_args().component_file, "w") as file:
                file.write(f'CONFIG_COMPONENTS={" ".join(gen_comp_list)}\n')
                for comp in component_list:
                    file.write(f'CONFIG_{comp}=1\n')
                for comp, c_flags_list in c_flags.items():
                    file.write(f'CONFIG_CFLAGS_{comp}={" ".join(c_flags_list)}\n')

                for comp in generated_components.values():
                    sources = []
                    for source in comp.sources:
                        found_source = None
                        for dirpath in self.gapy_target.get_args().target_dirs:
                            path = os.path.join(dirpath, source)
                            if os.path.exists(path):
                                found_source = path
                                break
                        if found_source is None:
                            found_source = source

                        sources.append(found_source)


                    file.write(f'CONFIG_{comp.name}=1\n')
                    file.write(f'CONFIG_SRCS_{comp.name}={" ".join(sources)}\n')
                    file.write(f'CONFIG_CFLAGS_{comp.name}={" ".join(comp.cflags)}\n')


            return True

        return False


    def __gen_debug_info(self, full_config, gvsoc_config):
        debug_binaries_config = full_config.get('**/debug_binaries')
        if debug_binaries_config is not None:
            binaries_config = full_config.get('**/binaries')
            if binaries_config is not None:
                binaries = binaries_config.get_dict()
                for index, binary in enumerate(debug_binaries_config.get_dict()):
                    # Only generate debug symbols for small binaries, otherwise it is too slow
                    # To allow it, the ISS should itself read the symbols.
                    if os.path.getsize(binaries[index]) < 5 * 1024*1024:
                        if os.system('gen-debug-info %s %s' % (binaries[index], binary)) != 0:
                            print('Error while generating debug symbols information, make sure the toolchain and the binaries are accessible ')


    def run(self, norun=False):

        args = self.gapy_target.get_args()
        gvsoc_config = self.full_config.get('target/gvsoc')

        dump_config(self.full_config, self.gapy_target.get_abspath(self.gvsoc_config_path))

        self.__gen_debug_info(self.full_config, self.full_config.get('target/gvsoc'))

        if norun:
            return 0

        stub = args.stub

        if args.gdb:
            stub = ['gdb', '--args'] + stub

        if args.valgrind:
            stub = ['valgrind'] + stub

        if args.emulation:

            launcher = args.binary

            command = stub

            command.append(launcher)

            if args.verbose in ['debug', 'info']:
                print ('Launching GVSOC with command: ')
                print (' '.join(command))

            os.chdir(self.gapy_target.get_working_dir())

            return os.execvp(command[0], command)

        else:

            
            if self.rtl_runner is not None:
                os.chdir(self.gapy_target.get_working_dir())
                command = self.rtl_runner.get_command()

            else:

                if args.valgrind:
                    stub = ['valgrind'] + stub

                if args.gui:
                    command = stub + ['gvsoc-gui',
                        '--gv-config=' + self.gvsoc_config_path,
                        '--gui-config=gvsoc_gui_config.json',
                    ]

                    path = os.path.join(self.gapy_target.get_working_dir(), 'gvsoc_gui_config.json')

                    gui_config = self.gen_gui_config(
                        work_dir=self.gapy_target.get_working_dir(),
                        path=path
                    )
                else:
                    if gvsoc_config.get_bool("debug-mode"):
                        launcher = gvsoc_config.get_str('launchers/debug')
                    else:
                        launcher = gvsoc_config.get_str('launchers/default')

                    command = stub

                    command += [launcher, '--config=' + self.gvsoc_config_path]

            os.chdir(self.gapy_target.get_working_dir())

            if args.gvcontrol is not None:

                # Import the user gvcontrol script
                try:
                    spec = importlib.util.spec_from_file_location(args.gvcontrol, args.gvcontrol)
                    module = importlib.util.module_from_spec(spec)
                    sys.modules["module.name"] = module
                    spec.loader.exec_module(module)
                except FileNotFoundError as exc:
                    raise RuntimeError('Unable to open test configuration file: ' + args.gvcontrol)

                # Launch gvsoc with a random port for the proxy and iterate until we manage
                # to launch it
                while True:

                    port = random.randint(4000, 20000)

                    self.full_config.set('target/gvsoc/proxy/port', port)
                    self.full_config.set('target/gvsoc/proxy/enabled', True)
                    dump_config(self.full_config, self.gapy_target.get_abspath(self.gvsoc_config_path))

                    run = pexpect.spawn(' '.join(command), encoding='utf-8', logfile=sys.stdout,
                        codec_errors='replace')
                    match = run.expect(['Opened proxy on socket '], timeout=None)
                    if match == 0:
                        break

                    run.expect(pexpect.EOF, timeout=None)
                    port = port + 1
                    if port == 20000:
                        port = 4000

                # Keep pexpect checking gvsoc output in the thread so that its output is
                # displayed in real time
                gv_thread = Runner.ExpectLoopThread(run)
                gv_thread.start()

                # And call user script with gvsoc proxy
                try:
                    proxy = gvsoc.gvsoc_control.Proxy('localhost', port)
                    status = module.target_control(proxy)
                    proxy.quit(status)
                    proxy.close()
                except:
                    traceback.print_exc()
                    gv_thread.kill = True
                    proxy.close()
                    gv_thread.join()
                    return -1

                # Once script is over, wait for gvsoc to finish and return its status
                gv_thread.join()
                return run.exitstatus

            else:
                if args.verbose in ['debug', 'info']:
                    print ('Launching GVSOC with command: ')
                    print (' '.join(command))

                return os.system(' '.join(command))


    def gen_gui_config(self, work_dir, path):
        with open(path, 'w') as fd:
            config = GuiConfig()
            self.target.gen_gui_stub(config)
            config.gen(fd)

    def gen_gtkw_script(self, work_dir, path, tags=[], level=0, trace_file=None, gen_full_tree=False):
        self.vcd_group_create = False
        self.is_top = True

        traces = []

        if trace_file is not None:
            with open(trace_file, 'r') as file:
                for line in file.readlines():
                    trace_path, trace, name = line.split(' ')
                    if trace_path.find('/') == 0:
                        trace_path = trace_path[1:]
                    name = name.replace('\n', '')
                    traces.append([trace_path.split('/'), trace.split('/'), name])

        with open(path, 'w') as fd:
            tree = Gtkwave_tree(fd, work_dir, gen_full_tree=gen_full_tree, tags=tags)

            tree.begin_group('overview', closed=False)
            tree.set_view('overview')
            self.target.gen_gtkw_tree(tree, traces=traces)
            tree.end_group('overview')

            tree.begin_group('system')
            tree.set_view('system')
            self.target.gen_gtkw_tree(tree, traces=traces)
            tree.end_group('system')


            tree.gen()

            return tree.activate_traces


    def gen_gui(self, parent_signal):
        gvsoc.gui.Signal(self, parent_signal, name='kernel', path='/user/kernel/state',
            display=gvsoc.gui.DisplayStringBox(), groups=["user"])
        return parent_signal



class Target(gapy.Target):

    def __init__(self, parser, options, model, rtl_cosim_runner=None):
        super(Target, self).__init__(parser, options)

        args = None

        if parser is not None:
            parser.add_argument("--trace", dest="traces", default=[], action="append",
                help="Specify gvsoc trace")

            parser.add_argument("--trace-level", dest="trace_level", default=None,
                help="Specify trace level")

            parser.add_argument("--trace-format", dest="trace_format", default="long",
                help="Specify trace format")

            parser.add_argument("--vcd", dest="vcd", action="store_true", help="Activate VCD traces")

            parser.add_argument("--event", dest="events", default=[], action="append",
                help="Specify gvsoc event (for VCD traces)")

            parser.add_argument("--event-tags", dest="event_tags", default=[], action="append",
                help="Specify gvsoc event through tags(for VCD traces)")

            parser.add_argument("--event-format", dest="format", default=None,
                help="Specify events format (vcd or fst)")

            parser.add_argument("--gtkwi", dest="gtkwi", action="store_true",
                help="Dump events to pipe and open gtkwave in interactive mode")

            parser.add_argument("--emulation", dest="emulation", action="store_true",
                help="Launch in emulation mode")

            parser.add_argument("--component-file", dest="component_file", default=None,
                help="Component file")

            parser.add_argument("--component-file-append", dest="component_file_append",
                action="store_true", help="Component file")

            parser.add_argument("--no-werror", dest="werror",
                action="store_false", help="Do not consider warnings as errors")

            parser.add_argument("--stub", dest="stub", default=[], action="append",
                help="Launch GVSOC through the specified command")

            parser.add_argument("--gdb", dest="gdb", default=None, action="store_true",
                help="Launch GVSOC through gdb")

            parser.add_argument("--gdbserver", dest="gdbserver", default=None, action="store_true",
                help="Launch GVSOC with a GDB server to connect gdb to the simulated program")

            parser.add_argument("--gdbserver-port", dest="gdbserver_port", default=12345, type=int,
                help="Specifies the GDB server port")

            parser.add_argument("--valgrind", dest="valgrind",
                action="store_true", help="Launch GVSOC through valgrind")

            parser.add_argument("--wno-unconnected-device", dest="w_unconnected_device",
                action="store_false", default=False, help="Deactivate warnings when updating padframe with no connected device")
            parser.add_argument("--wno-unconnected-padfun", dest="w_unconnected_padfun",
                action="store_false", default=False, help="Deactivate warnings when updating padframe with no connected padfun")

            parser.add_argument("--memcheck", dest="memcheck",
                action="store_true", default=False, help="Enable memory checks")

            parser.add_argument("--wunconnected-device", dest="w_unconnected_device",
                action="store_true", help="Activate warnings when updating padframe with no connected device")
            parser.add_argument("--wunconnected-padfun", dest="w_unconnected_padfun",
                action="store_true", help="Activate warnings when updating padframe with no connected padfun")

            parser.add_argument("--builddir", dest="builddir", default=None,
                help="Specify build directory. This can be used when generating components code.")

            parser.add_argument("--installdir", dest="installdir", default=None,
                help="Specify install directory. This can be used when generating components code.")

            parser.add_argument("--control-script", dest="gvcontrol", default=None,
                help="Specify gvcontrol script")

            [args, otherArgs] = parser.parse_known_args()

        self.model = model(parent=self, name=None, parser=parser, options=options)
        self.runner = Runner(parser, args, options, self, self.model, rtl_cosim_runner=rtl_cosim_runner)

    def get_path(self, child_path=None, gv_path=False, *kargs, **kwargs):
        return child_path

    def declare_flash(self, path=None):
        pass

    def get_target(self):
        return self


    def dump_target_properties(self):

        class PropTree:

            def __init__(self, name=None):
                self.name = name
                self.prop_trees = {}
                self.properties = {}

            def add_property_recursive(self, name, value, name_array):
                if len(name_array) == 1:
                    self.properties[name] = value
                else:
                    if self.prop_trees.get(name_array[0]) is None:
                        self.prop_trees[name_array[0]] = PropTree(name_array[0])

                    self.prop_trees[name_array[0]].add_property_recursive(name, value, name_array[1:])

            def add_property(self, name, value):
                self.add_property_recursive(name, value, name.split('/'))

            def fill_tree(self, tree):
                if self.name is None:
                    subtree = tree
                else:
                    subtree = tree.add(self.name)

                if len(self.properties) > 0:
                    table = rich.table.Table(title='Properties')
                    table.add_column('Name')
                    table.add_column('Value')
                    table.add_column('Full name')
                    table.add_column('Allowed values')
                    table.add_column('Description')

                    for prop_full_name, prop in self.properties.items():
                        value_str = prop.value
                        if prop.format is not None:
                            value_str = prop.format % prop.value
                        value_str = str(value_str)
                        prop_name = prop_full_name.split('/')[-1]

                        if prop.allowed_values is None:
                            if prop.cast == int:
                                allowed_values = 'any integer'
                            else:
                                allowed_values = 'any string'
                        else:
                            allowed_values = ', '.join(prop.allowed_values)

                        table.add_row(prop_name, value_str, prop_full_name, allowed_values, prop.description)

                    subtree.add(table)

                for prop_tree in self.prop_trees.values():
                    prop_tree.fill_tree(subtree)

        prop_tree = PropTree()

        for prop in self.target_properties.values():
            prop_tree.add_property(prop.full_name, prop)

        tree = rich.tree.Tree('Properties')
        prop_tree.fill_tree(tree)
        rich.print (tree)
