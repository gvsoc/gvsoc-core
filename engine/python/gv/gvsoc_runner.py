#
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
import gsystree as st
import json_tools as js
import os.path
from tools.runner.gtkwave import Gtkwave_tree


def gen_config(args, config):

    full_config =  js.import_config(config, interpret=False, gen=False)

    gvsoc_config = full_config.get('gvsoc')

    gvsoc_config.set('werror', args.werror)
    gvsoc_config.set('wunconnected-device', args.w_unconnected_device)
    gvsoc_config.set('wunconnected-padfun', args.w_unconnected_padfun)

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
        len(gvsoc_config.get('events/include_regex')) != 0

    gvsoc_config.set("debug-mode", debug_mode)

    if debug_mode:
        debug_binaries = []

        if args.binary is not None:
            debug_binaries.append(args.binary)

        rom_binary = full_config.get_str('**/soc/rom/config/binary')

        if rom_binary is not None:
            
            if os.path.exists(rom_binary):
                debug_binaries.append(rom_binary)

        for binary in debug_binaries:
            full_config.set('**/debug_binaries', binary + '.debugInfo')
            full_config.set('**/binaries', binary)

    gvsoc_config_path = 'gvsoc_config.json'

    return full_config, gvsoc_config_path


def dump_config(full_config, gvsoc_config_path):
    with open(gvsoc_config_path, 'w') as file:
        file.write(full_config.dump_to_string())


class Runner(gapylib.target.Target, st.Component):

    def __init__(self, parent, name, options):

        gapylib.target.Target.__init__(self, options)
        st.Component.__init__(self, parent, name, options)

        self.add_property("gvsoc/debug-mode", False)
        self.add_property("gvsoc/events/gen_gtkw", False)
        self.add_property("gvsoc/proxy/enabled", False)
        self.add_property("gvsoc/proxy/port", 42951)


    def append_args(self, parser, rtl_cosim_runner=None):
        super().append_args(parser)

        choices = ['gvsoc']

        self.rtl_runner = None

        if rtl_cosim_runner is not None:

            parser.add_argument("--rtl-cosimulation", dest="rtl_cosimulation", action="store_true",
                help="Launch in RTL cosimulation mode")

            [args, _] = parser.parse_known_args()

            if args.rtl_cosimulation:

                parser.add_argument("--gvsoc-path", dest="gvsoc_path", default=None,
                    type=str, help="path to GVSOC install folder")

                choices.append('rtl')
                self.rtl_runner = rtl_cosim_runner(self)
                self.rtl_runner.append_args(parser)

        parser.add_argument("--platform", dest="platform", required=True, choices=choices,
            type=str, help="specify the platform used for the target")

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

        parser.add_argument("--wno-unconnected-device", dest="w_unconnected_device",
            action="store_false", help="Deactivate warnings when updating padframe with no connected device")
        parser.add_argument("--wno-unconnected-padfun", dest="w_unconnected_padfun",
            action="store_false", help="Deactivate warnings when updating padframe with no connected padfun")

    def parse_args(self, args):
        super().parse_args(args)

        gvsoc_config_dict = {
            "proxy": {
                "enabled": self.get_property("gvsoc/proxy/enabled"),
                "port": self.get_property("gvsoc/proxy/port")
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
                "gen_gtkw": self.get_property("gvsoc/events/gen_gtkw"),
                "files": [ ],
                "traces": {},
                "tags": [ "overview" ],
                "gtkw": False,
            },

            "runner_module": "gv.gvsoc",
        
            "cycles_to_seconds": "int(max(cycles * nb_cores / 5000000, 600))",
        
            "werror": True,
            "verbose": True,
            "debug-mode": self.get_property("gvsoc/debug-mode"),
            "sa-mode": True,
        
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

        target_config = self.get_config()

        config = {
            "target": target_config,
            "gvsoc": gvsoc_config_dict
        }

        self.full_config, self.gvsoc_config_path = gen_config(args, config)



        gvsoc_config = self.full_config.get('gvsoc')

        if gvsoc_config.get_bool('events/gen_gtkw'):
            path = os.path.join(self.get_working_dir(), 'view.gtkw')

            traces = self.gen_gtkw_script(
                work_dir=self.get_working_dir(),
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
                gvsoc_config_path=self.gvsoc_config_path)


    def handle_command(self, cmd):

        if cmd == 'run':
            self.run()

        elif cmd == 'traces' and self.rtl_runner is not None:
            self.rtl_runner.traces()

        elif cmd == 'prepare':
            self.run(norun=True)

        elif cmd == 'image':
            gapylib.target.Target.handle_command(self, cmd)

            if self.rtl_runner is not None:
                self.rtl_runner.image()
            else:
                self.gen_stimuli()

        elif cmd == 'components':

            component_list = []

            if self.get_args().component_file_append:
                with open(self.get_args().component_file, "r") as file:
                    for comp_desc in file.readlines():
                        comp = comp_desc.replace('CONFIG_', '').replace('=1\n', '')
                        component_list.append(comp)

            component_list += self.get_component_list() + ['vp.trace_domain_impl', 'vp.time_domain_impl', 'vp.power_domain_impl', 'utils.composite_impl']

            with open(self.get_args().component_file, "w") as file:
                for comp in component_list:
                    file.write(f'CONFIG_{comp}=1\n')

        else:
            gapylib.target.Target.handle_command(self, cmd)



    def __gen_debug_info(self, full_config, gvsoc_config):
        debug_binaries_config = full_config.get('**/debug_binaries')
        if debug_binaries_config is not None:
            for binary in debug_binaries_config.get_dict():
                if os.system('gen-debug-info %s %s' % (binary.replace('.debugInfo', ''), binary)) != 0:
                # if os.system('pulp-pc-info --file %s --all-file %s' % (binary.replace('.debugInfo', ''), binary)) != 0:
                    raise RuntimeError('Error while generating debug symbols information, make sure the toolchain and the binaries are accessible ')


    def run(self, norun=False):

        gvsoc_config = self.full_config.get('gvsoc')

        dump_config(self.full_config, self.get_abspath(self.gvsoc_config_path))

        self.__gen_debug_info(self.full_config, self.full_config.get('gvsoc'))

        if norun:
            return 0

        if self.rtl_runner is not None:
            self.rtl_runner.run()

        elif self.get_args().emulation:

            launcher = self.get_args().binary
            command = [launcher]

            print ('Launching GVSOC with command: ')
            print (' '.join(command))

            os.chdir(self.get_working_dir())

            return os.execvp(launcher, command)

        else:

            if gvsoc_config.get_bool("debug-mode"):
                launcher = gvsoc_config.get_str('launchers/debug')
            else:
                launcher = gvsoc_config.get_str('launchers/default')

            command = [launcher, '--config=' + self.gvsoc_config_path]

            if True: #self.verbose:
                print ('Launching GVSOC with command: ')
                print (' '.join(command))

            os.chdir(self.get_working_dir())

            return os.execvp(launcher, command)


    def get_target(self):
        return self


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
            self.gen_gtkw_tree(tree, traces=traces)
            tree.end_group('overview')

            tree.begin_group('system')
            tree.set_view('system')
            self.gen_gtkw_tree(tree, traces=traces)
            tree.end_group('system')


            tree.gen()

            return tree.activate_traces