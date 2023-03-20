#!/usr/bin/env python3

#
# Copyright (C) 2022 GreenWaves Technologies
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


parser = argparse.ArgumentParser(description='Parse power report')

parser.add_argument("--name", dest="name", default=None, help="specify the block name")
parser.add_argument("--input", dest="input", default=None, help="specify the input file")
parser.add_argument("--depth", dest="depth", type=int, default=0, help="specify the dump depth")

args = parser.parse_args()


class Block(object):

    def __init__(self, name, int_power, switch_power, leakage_power, total_power, percentage):

        self.blocks = []
        self.name = name
        self.total_power = total_power
        self.int_power = int_power
        self.switch_power = switch_power
        self.leakage_power = leakage_power
        self.percentage = percentage

    def add(self, name, int_power, switch_power, leakage_power, total_power, percentage):
        index = 0
        for index, block in enumerate(self.blocks):
            if percentage > block.percentage:
                break

        block = Block(name, int_power, switch_power, leakage_power, total_power, percentage)
        self.blocks.insert(index, block)
        return block

    def dump(self, table, name, depth, indent=''):
        table.add_row(['%s%s' % (indent, self.name), self.int_power + self.switch_power, self.leakage_power, self.total_power, self.percentage])

        if len(name) != 0:
            for block in self.blocks:
                if block.name == name[0]:
                    block.dump(table, name[1:], depth, indent + '    ')
                    break
        else:
            if depth > 0:
                for block in self.blocks:
                    block.dump(table, name, depth-1, indent + '    ')

with open(args.input, 'r') as fd:

    blocks = []
    level = 0
    started = False

    for line in fd.readlines():

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

    print (name)

    table = PrettyTable()
    table.field_names = ["Block name", "Dynamic power", "Leakage power", "Total power", "Percentage"]
    if len(name) == 0 or name[0] == blocks[0].name:
        blocks[0].dump(table, name[1:], args.depth)

    table.align = 'r'
    table.align['Block name'] = 'l'
    print (table)