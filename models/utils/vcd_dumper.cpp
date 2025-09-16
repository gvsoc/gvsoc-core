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
 * Authors: Germain Haugou (germain.haugou@gmail.com)
 */

#include <vp/vp.hpp>
#include <vp/signal.hpp>
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <map>
#include <string>
#include <sstream>
#include <regex>

class VcdDumper;

class Signal
{
public:
    virtual void finalize(VcdDumper *dumper) {}

    std::string identifier;
    std::string name;
    std::string full_name;
    char value = 'x';
};

class SignalWire : public Signal
{
public:
    void finalize(VcdDumper *dumper) override;
    int width;
    SignalWire *parent=NULL;
    int parent_bit;
    uint64_t value;
    vp::Signal<uint64_t> *signal;
    std::vector<vp::Signal<uint64_t> *> signal_array;
    int element_size;
};

class VcdDumper : public vp::Component
{

    friend class SignalWire;

public:
    VcdDumper(vp::ComponentConf &config);

private:
    void reset(bool active);
    bool read_next();
    void handle_next_timestamp();
    void enqueue_next();
    static void event_handler(vp::Block *__this, vp::TimeEvent *event);

    std::ifstream file;
    std::unordered_map<std::string, Signal *> signals;
    std::unordered_map<std::string, Signal *> bitwise_signal_map;

    int64_t current_time;
    Signal *current_signal;
    std::string current_value;
    vp::TimeEvent event;
    std::unordered_map<std::string, int> element_size;
};


void SignalWire::finalize(VcdDumper *dumper)
{
    if (this->parent == NULL)
    {
        auto it = dumper->element_size.find(this->full_name);
        if (it != dumper->element_size.end())
        {
            int element_size = it->second;
            int nb_elements = this->width / element_size;
            this->element_size = element_size;

            for (int i=0; i<nb_elements; i++)
            {
                this->signal_array.push_back(
                    new vp::Signal<uint64_t>(*dumper, this->full_name + "[" + std::to_string(i) + "]", element_size));
            }
        }
        else
        {
            this->signal = new vp::Signal<uint64_t>(*dumper, this->full_name, this->width);
        }
    }
}

VcdDumper::VcdDumper(vp::ComponentConf &config)
    : vp::Component(config), event(this, &VcdDumper::event_handler)
{
    this->current_time = -1;

    js::Config *element_size = this->get_js_config()->get("element_size");
    if (element_size != NULL)
    {
        for (auto it: element_size->get_childs())
        {
            this->element_size[it.first] = it.second->get_int();
        }
    }

    std::string vcd_file_path = this->get_js_config()->get_child_str("vcd_file");
    if (vcd_file_path != "")
    {

        this->file.open(vcd_file_path);

        std::string line;
        std::vector<std::string> scope_stack;
        while (std::getline(file, line))
        {
            if (line == "$enddefinitions $end")
            {
                break;
            }

            std::istringstream iss(line);
            std::string token;
            iss >> token;

            if (token == "$scope")
            {
                std::string type, name;
                iss >> type >> name;
                scope_stack.push_back(name);
            }
            else if (token == "$upscope")
            {
                if (!scope_stack.empty())
                {
                    scope_stack.pop_back();
                }
            }
            else if (token == "$var")
            {
                std::string type, size, id, name, bit;
                iss >> type >> size >> id >> name >> bit;

                // If name has brackets, parse bit index
                int bit_index = -1;
                std::string base_name = name;
                size_t bracket_pos = bit.find('[');
                int width;
                if (bracket_pos != std::string::npos)
                {
                    int column_pos = bit.find(':');
                    if (column_pos == std::string::npos)
                    {
                        size_t end_bracket = bit.find(']', bracket_pos);
                        if (end_bracket != std::string::npos)
                        {
                            std::string bit_str = bit.substr(bracket_pos + 1, end_bracket - bracket_pos - 1);
                            bit_index = std::stoi(bit_str);
                        }
                    }
                    else
                    {
                        std::string high_str = bit.substr(bracket_pos + 1, column_pos - 1);
                        std::string low_str = bit.substr(column_pos + 1, bit.size() - (column_pos + 1) - 1);
                        width = std::stoi(high_str) - std::stoi(low_str) + 1;
                    }
                }

                std::ostringstream fullName;
                for (const auto &s : scope_stack)
                {
                    fullName << s << ".";
                }
                fullName << name;

                if (bit_index != -1)
                {
                    if (this->bitwise_signal_map.find(fullName.str()) ==
                        this->bitwise_signal_map.end())
                    {
                        SignalWire *parent = new SignalWire();
                        parent->width = 0;
                        parent->name = name;
                        parent->full_name = fullName.str();
                        this->bitwise_signal_map[fullName.str()] = (Signal *)parent;
                    }
                }

                SignalWire *signal = new SignalWire();
                signal->identifier = id;
                signal->name = name;
                signal->full_name = fullName.str();
                signal->parent_bit = bit_index;
                this->signals[id] = signal;

                if (bit_index != -1)
                {
                    signal->parent = (SignalWire *)this->bitwise_signal_map[fullName.str()];
                    if (signal->parent->width <= bit_index)
                    {
                        signal->parent->width = bit_index + 1;
                    }
                }
                else
                {
                    signal->width = width;
                }
            }
        }

        for (auto it: this->signals)
        {
            it.second->finalize(this);
        }

        for (auto it: this->bitwise_signal_map)
        {
            it.second->finalize(this);
        }

        this->read_next();
    }
}

void VcdDumper::reset(bool active)
{
    if (!active && this->time.get_time() == 0)
    {
        this->enqueue_next();
    }
}

void VcdDumper::event_handler(vp::Block *__this, vp::TimeEvent *event)
{
    VcdDumper *_this = (VcdDumper *)__this;

    int64_t time = _this->time.get_time();
    while(_this->current_time == time)
    {
        if (_this->read_next())
        {
            break;
        }
        _this->handle_next_timestamp();
    }

    _this->enqueue_next();
}

void VcdDumper::enqueue_next()
{
    if (this->current_time != -1)
    {
        this->event.enqueue(this->current_time - this->time.get_time());
    }
}

void VcdDumper::handle_next_timestamp()
{
    SignalWire *signal = (SignalWire *)this->current_signal;
    if (signal->signal_array.size() > 0)
    {
        int pos = this->current_value.size();
        for (int i=0; i<signal->signal_array.size() && pos > 0; i++)
        {
            int chunkSize = std::min(signal->element_size, pos);
            int start = pos - chunkSize;
            std::string chunk = this->current_value.substr(start, chunkSize);
            uint64_t value = std::stoll(chunk, nullptr, 2);
            pos -= chunkSize;
            signal->signal_array[i]->set(value);
        }
    }
    else
    {
        if (signal->parent)
        {
            uint64_t value = std::stoll(this->current_value);
            signal->parent->value = (signal->parent->value & ~(1 << signal->parent_bit)) | (value << signal->parent_bit);
            signal->parent->signal->set(signal->parent->value);
        }
        else
        {
            uint64_t value = std::stoll(this->current_value, nullptr, 2);
            signal->signal->set(value);
        }
    }
}

bool VcdDumper::read_next()
{
    std::string line;
    while (std::getline(file, line))
    {
        if (line.empty()) continue;

        if (line[0] == '#')
        {
            this->current_time = std::stoll(line.substr(1));
            this->current_signal = NULL;
            this->current_value = '\0';
            return true;
        }
        else
        {
            std::string val;
            std::string id;

            if (line[0] == 'b')
            {
                std::istringstream iss(line);
                std::string prefix;
                iss >> prefix >> id;
                val = prefix.substr(1);
            }
            else
            {
                val = line[0];
                id = line.substr(1);
            }
            auto it = signals.find(id);
            if (it != signals.end())
            {
                this->current_signal = it->second;
                this->current_value = val;
                return false;
            }
        }
    }
    this->current_time = -1;
    this->time.get_engine()->quit(0);
    return true;
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new VcdDumper(config);
}
