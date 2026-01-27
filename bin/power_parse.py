#!/usr/bin/env python3

#
# Copyright (C) 2024 GreenWaves Technologies, ETH Zurich and University of Bologna
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

import argparse
from prettytable import PrettyTable
import bisect


parser = argparse.ArgumentParser(description='Parse power report')

parser.add_argument("--name", dest="name", default=None, help="specify the block name")
parser.add_argument("--input", dest="input", default=None, help="specify the input file")
parser.add_argument("--group", dest="group", default=None, help="specify the ichip for grouping")
parser.add_argument("--depth", dest="depth", type=int, default=0, help="specify the dump depth")
parser.add_argument("--sort-by-dynamic-power", dest="sort_dyn", action="store_true", default=0, help="sort results by dynamic power")
parser.add_argument("--sort-by-static-power", dest="sort_static", action="store_true", default=0, help="sort results by dynamic power")

args = parser.parse_args()


groups = {
    'gap10': {
        'pulp_chip:logic_domain_i:cluster_domain_i:cluster_i:i_shared_fpu_cluster': 'i_shared_fpu_cluster',
        'pulp_chip:logic_domain_i:cluster_domain_i:cluster_i:tcdm_sram_island': 'tcdm_sram_island',
        'pulp_chip:logic_domain_i:cluster_domain_i:cluster_i:cluster_bus_wrap_i': 'cluster_bus_wrap_i',
        'pulp_chip:logic_domain_i:cluster_domain_i:cluster_i:ulpcluster__RC_CG': 'ulpcluster__RC_CG',
        'pulp_chip:logic_domain_i:cluster_domain_i:cluster_i:tem_ff_tem_siu': 'tem_ff_tem_siu',
        'pulp_chip:logic_domain_i:cluster_domain_i:cluster_i:tem_siu': 'tem_siu',
        'pulp_chip:logic_domain_i:cluster_domain_i:cluster_i:axi2per_wrap_i': 'axi2per_wrap_i',
        'pulp_chip:logic_domain_i:cluster_domain_i:cluster_i:dmac_wrap_i': 'dmac_wrap_i',
        'pulp_chip:logic_domain_i:cluster_domain_i:cluster_i:axi_size_UPSIZE_32_64_wrap_i': 'axi_size_UPSIZE_32_64_wrap_i',
        'pulp_chip:logic_domain_i:cluster_domain_i:cluster_i:DFT_RWM': 'DFT_RWM',
        'pulp_chip:logic_domain_i:cluster_domain_i:cluster_i:tem_dcu': 'tem_dcu',
        'pulp_chip:logic_domain_i:cluster_domain_i:cluster_i:i_apb_slave_asynch_ll_if': 'i_apb_slave_asynch_ll_if',
        'pulp_chip:logic_domain_i:cluster_domain_i:cluster_i:rstgen_i_i_rstgen_bypass': 'rstgen_i_i_rstgen_bypass',
        'pulp_chip:logic_domain_i:cluster_domain_i:cluster_i:cluster_peripherals_i_cluster_control_unit_i_ulpcluster__RC_CG': 'cluster_peripherals_i_cluster_control_unit_i_ulpcluster__RC_CG',
    }
}


class Block(object):

    def __init__(self, name, int_power, switch_power, leakage_power, total_power, percentage, full_name = None):

        self.blocks = {}
        self.name = name
        self.total_power = total_power
        self.int_power = int_power
        self.switch_power = switch_power
        self.leakage_power = leakage_power
        self.percentage = percentage

        if full_name is not None:
            self.full_name = full_name
        else:
            self.full_name = self.name

    def add(self, name, int_power, switch_power, leakage_power, total_power, percentage):

        full_name = self.full_name + ':' + name
        merged_block = None

        # print ('ADD %s' % full_name)

        if args.group is not None:
            group = groups.get(args.group)
            if group is not None:
                parent_full_name = full_name.rsplit(':', 1)[0]
                for group_path, group_name in group.items():
                    parent_group_path = group_path.rsplit(':', 1)[0]
                    if parent_group_path == parent_full_name and full_name.find(group_path) == 0:
                        merged_block = self.blocks.get(group_name)
                        if merged_block is None:
                            merged_block = Block(group_name, 0, 0, 0, 0, 0, self.full_name + ':' +  group_name)
                            self.blocks[group_name] = merged_block

        block = Block(name, int_power, switch_power, leakage_power, total_power, percentage, full_name=full_name)

        if merged_block is not None:
            merged_block.merge(block)
        else:
            self.blocks[name] = block
        return block

    def merge(self, block):
        self.blocks[block.name] = block
        self.int_power += block.int_power
        self.switch_power += block.switch_power
        self.leakage_power += block.leakage_power
        self.total_power += block.total_power
        self.percentage += block.percentage

    def dump(self, table, name, depth, indent=''):

        if args.sort_dyn:
            blocks = sorted(self.blocks.values(), key=lambda x: -x.int_power - x.switch_power)
        elif args.sort_static:
            blocks = sorted(self.blocks.values(), key=lambda x: -x.leakage_power)
        else:
            blocks = sorted(self.blocks.values(), key=lambda x: -x.percentage)

        table.add_row(['%s%s' % (indent, self.name), self.int_power + self.switch_power, self.leakage_power, self.total_power, self.percentage])

        if len(name) != 0:
            for block in blocks:
                if block.name == name[0]:
                    block.dump(table, name[1:], depth, indent + '    ')
                    break
        else:
            if depth > 0:
                for block in blocks:
                    block.dump(table, name, depth-1, indent + '    ')

with open(args.input, 'r') as fd:

    blocks = []
    level = 0
    started = False

    for line in fd.readlines():

        if started and line.find('Hierarchy') != -1:
            break

        if line.find('----------------------------------------------------------------------------------') == 0:
            started = True
            continue

        if not started:
            continue

        line_array = line.split()

        dump = False
        if len(line_array) in [5, 6, 7]:
            try:
                percentage = float(line_array.pop())
            except:
                percentage = 0.0
            total_power = float(line_array.pop())
            leakage_power = float(line_array.pop())
            switch_power = float(line_array.pop())
            int_power = float(line_array.pop())
            dump = True

        if len(line_array) > 0:
            name = line_array[0]
            level = int((len(line) - len(line.lstrip())) / 2)

        if dump:
            if level == 0:
                blocks.append(Block(name, int_power, switch_power, leakage_power, total_power, percentage))
            else:
                block = blocks[level-1].add(name, int_power, switch_power, leakage_power, total_power, percentage)
                if level >= len(blocks):
                    blocks.append(block)
                else:
                    blocks[level] = block

    name = []
    if args.name is not None:
        name = args.name.split(':')

    table = PrettyTable()
    table.field_names = ["Block name", "Dynamic power", "Leakage power", "Total power", "Percentage"]
    if len(name) == 0 or name[0] == blocks[0].name:
        blocks[0].dump(table, name[1:], args.depth)

    table.align = 'r'
    table.align['Block name'] = 'l'
    print (table)
