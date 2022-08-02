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


def gen_config(config):

    full_config =  js.import_config(config, interpret=True, gen=True)

    gvsoc_config = full_config.get('gvsoc')

    debug_mode = gvsoc_config.get_bool('debug-mode') or \
        gvsoc_config.get_bool('traces/enabled') or \
        gvsoc_config.get_bool('events/enabled') or \
        len(gvsoc_config.get('traces/include_regex')) != 0 or \
        len(gvsoc_config.get('events/include_regex')) != 0

    gvsoc_config.set("debug-mode", debug_mode)

    if debug_mode:
        debug_binaries = []

        # if args.binary is not None:
        #     debug_binaries.append(args.binary)

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


class Runner(st.Component):

    def __init__(self, parent, name, options):

        st.Component.__init__(self, parent, name, options)

    def append_args(self, parser):
        super().append_args(parser)

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

    def parse_args(self, args):
        super().parse_args(args)

        gvsoc_config_dict = {
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

            "description": "GAP simulator.",

            "runner_module": "gv.gvsoc",
        
            "cycles_to_seconds": "int(max(cycles * nb_cores / 5000000, 600))",
        
            "verbose": True,
            "debug-mode": False,
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

        self.full_config, self.gvsoc_config_path = gen_config(config)

        gvsoc_config = self.full_config.get('gvsoc')


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


    def handle_command(self, cmd):

        if cmd == 'run':
            return self.run()

        elif cmd == 'image':
            if gapylib.target.Target.handle_command(self, cmd) != 0:
                return -1

            self.gen_stimuli()

            return 0

        else:
            return gapylib.target.Target.handle_command(self, cmd)


    def run(self):

        gvsoc_config = self.full_config.get('gvsoc')

        dump_config(self.full_config, self.gvsoc_config_path)

        if gvsoc_config.get_bool("debug-mode"):
            launcher = gvsoc_config.get_str('launchers/debug')
        else:
            launcher = gvsoc_config.get_str('launchers/default')

        command = [launcher, '--config=' + self.gvsoc_config_path]

        if True: #self.verbose:
            print ('Launching GVSOC with command: ')
            print (' '.join(command))

        return os.execvp(launcher, command)