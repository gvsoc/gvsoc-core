/*
 * Copyright (C) 2020 GreenWaves Technologies, SAS, ETH Zurich and
 *                    University of Bologna
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Authors: Germain Haugou, GreenWaves Technologies (germain.haugou@greenwaves-technologies.com)
 */

#include <vp/vp.hpp>
#include <vp/mapping_tree.hpp>

vp::MappingTreeEntry::MappingTreeEntry(int id, std::string name, js::Config *config)
{
    this->id = id;
    this->name = name;
    js::Config *conf;
    this->base = config->get_uint("base");
    this->size = config->get_uint("size");
    this->lowest_base = base;
}

vp::MappingTreeEntry::MappingTreeEntry(uint64_t base, vp::MappingTreeEntry *left,
    vp::MappingTreeEntry *right)
{
    this->base = base;
    this->left = left;
    this->right = right;
    this->lowest_base = left->lowest_base;
}

vp::MappingTree::MappingTree(vp::Trace *trace)
{
    this->trace = trace;
}

void vp::MappingTree::insert(int id, std::string name, js::Config *config)
{
    vp::MappingTreeEntry *entry = new vp::MappingTreeEntry(id, name, config);

    if (name == "error")
    {
        this->error_entry = entry;
    }
    else
    {
        if (entry->size == 0)
        {
            this->default_entry = entry;
        }
        else
        {
            vp::MappingTreeEntry *current = this->first_map_entry;
            vp::MappingTreeEntry *prev = NULL;

            while (current && current->base < entry->base)
            {
                prev = current;
                current = current->next;
            }

            if (prev == NULL)
            {
                entry->next = this->first_map_entry;
                this->first_map_entry = entry;
            }
            else
            {
                entry->next = current;
                prev->next = entry;
            }
        }
    }
}

vp::MappingTreeEntry *vp::MappingTree::get(uint64_t base, uint64_t size, bool is_write)
{
    vp::MappingTreeEntry *entry = this->top_map_entry;

    if (entry)
    {
        while(1) {
            // The entry does not have any child, this means we are at a final entry
            if (entry->left == NULL)
            {
                break;
            }

            if (base >= entry->base)
            {
                entry = entry->right;
            }
            else
            {
                entry = entry->left;
            }
        }

        if (entry && (base < entry->base || base > entry->base + entry->size - 1))
        {
            entry = NULL;
        }
    }

    if (entry == NULL)
    {
        entry = this->default_entry;
    }

    return entry;
}

void vp::MappingTree::build()
{
    vp::MappingTreeEntry *current = this->first_map_entry;

    this->trace->msg(vp::Trace::LEVEL_INFO, "Building router table\n");
    while(current)
    {
        this->trace->msg(vp::Trace::LEVEL_INFO, "  0x%16llx : 0x%16llx -> %s\n",
            current->base, current->base + current->size, current->name.c_str());
        current = current->next;
    }

    if (this->error_entry != NULL)
    {
        this->trace->msg(vp::Trace::LEVEL_INFO, "  0x%16llx : 0x%16llx -> ERROR\n",
            this->error_entry->base, this->error_entry->base + this->error_entry->size);
    }

    if (this->default_entry != NULL)
    {
        this->trace->msg(vp::Trace::LEVEL_INFO, "       -     :      -     -> %s\n",
            this->default_entry->name.c_str());
    }

    vp::MappingTreeEntry *first_in_level = this->first_map_entry;

    // Loop until we merged everything into a single entry
    // Start with the routing table entries
    current = this->first_map_entry;

    // The first loop is here to loop until we have merged everything to a single entry
    while(1) {
        // Gives the map entry that should be on the left side of the next created entry
        vp::MappingTreeEntry *left = NULL;

        // Gives the last allocated entry where the entry should be inserted
        vp::MappingTreeEntry *current_in_level=NULL;

        // The second loop is iterating on a single level to merge entries 2 by 2
        while(current)
        {
            if (left == NULL)
            {
                left = current;
            }
            else
            {
                vp::MappingTreeEntry *entry = new vp::MappingTreeEntry(current->lowest_base, left, current);

                left = NULL;
                if (current_in_level)
                {
                    current_in_level->next = entry;
                }
                else
                {
                    first_in_level = entry;
                }
                current_in_level = entry;
            }

            current = current->next;
        }

        current = first_in_level;

        // Stop in case we got a single entry
        if (current_in_level == NULL)
        {
            break;
        }

        // In case an entry is alone, insert it at the end, it will be merged with an upper entry
        if (left != NULL)
        {
            current_in_level->next = left;
        }
    }

    this->top_map_entry = first_in_level;
}
