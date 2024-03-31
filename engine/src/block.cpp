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
#include <vp/signal.hpp>
#include <vp/register.hpp>

vp::BlockObject::BlockObject(Block &parent)
{
    parent.register_object(this);
}

vp::Block::Block(Block *parent, std::string name, vp::TimeEngine *time_engine,
        vp::TraceEngine *trace_engine, vp::PowerEngine *power_engine)
    : parent(parent), time(parent, *this, time_engine), traces(parent, *this, trace_engine),
    power(parent, *this, power_engine), clock(*this)
{
    if (parent)
    {
        parent->add_block(this);
    }

    this->name = name;
    this->path = this->get_path_from_parents();

    if (this->time.get_engine() == NULL)
    {
        this->block_trace.fatal("BLock has no time engine\n");
    }

    this->power.build();

    this->traces.new_trace("trace", &this->block_trace, vp::DEBUG);

}

vp::Block::~Block()
{
    if (this->parent)
    {
        this->parent->remove_block(this);
    }
}

void vp::Block::register_object(vp::BlockObject *object)
{
    this->objects.push_back(object);
}

std::string vp::Block::get_path_from_parents()
{
    std::string parent_name;
    if (this->parent)
    {
        parent_name = this->parent->get_path_from_parents();
    }

    if (this->name == "")
    {
        return parent_name;
    }
    else
    {
        return parent_name + '/' + this->name;
    }
}


void vp::Block::reset_all(bool active, bool from_itf)
{
    if (!this->reset_is_bound || from_itf)
    {
        this->get_trace()->msg("Reset (active: %d)\n", active);

        for (Block *block: this->childs)
        {
            block->reset_all(active, false);
        }

        for (vp::BlockObject *object: this->objects)
        {
            object->reset(active);
        }

        for (SignalCommon *signal: this->signals)
        {
            signal->reset(active);
        }

        for (RegisterCommon *reg: this->registers)
        {
            reg->reset(active);
        }

        if (active)
        {
            for (ClockEvent *event: this->clock.events)
            {
                this->event_cancel(event);
            }
        }

        for (auto reg : this->regs)
        {
            reg->reset(active);
        }

        this->time.reset(active);

        this->reset(active);
    }
}



void vp::Block::add_block(Block *block)
{
    this->childs.push_back(block);
    block->parent = this;
}

void vp::Block::remove_block(Block *block)
{
    this->childs.erase(std::remove(this->childs.begin(), this->childs.end(), block),
        this->childs.end());
}

void vp::Block::add_signal(vp::SignalCommon *signal)
{
    this->signals.push_back(signal);
}

void vp::Block::add_register(vp::RegisterCommon *reg)
{
    this->registers.push_back(reg);
}

void vp::Block::new_service(std::string name, void *service)
{
    // this->get_trace()->msg(vp::Trace::LEVEL_DEBUG, "New service (name: %s, service: %p)\n", name.c_str(), service);

    if (this->parent)
        this->parent->add_service(name, service);

    services[name] = service;
}

void *vp::Block::get_service(string name)
{
    if (services[name])
        return services[name];

    if (this->parent)
        return this->parent->get_service(name);

    return NULL;
}

void vp::Block::add_service(std::string name, void *service)
{
    if (this->parent)
        this->parent->add_service(name, service);
    else if (services[name] == NULL)
        services[name] = service;
}


vp::Block *vp::Block::get_block_from_path(std::vector<std::string> path_list)
{
    if (path_list.size() == 0)
    {
        return this;
    }

    std::string name = "";
    unsigned int name_pos= 0;
    for (auto x: path_list)
    {
        if (x != "*" && x != "**")
        {
            name = x;
            break;
        }
        name_pos += 1;
    }

    for (auto x:this->get_childs())
    {
        vp::Block *comp;
        if (name == x->get_name())
        {
            comp = x->get_block_from_path({ path_list.begin() + name_pos + 1, path_list.end() });
        }
        else if (path_list[0] == "**")
        {
            comp = x->get_block_from_path(path_list);
        }
        else if (path_list[0] == "*")
        {
            comp = x->get_block_from_path({ path_list.begin() + 1, path_list.end() });
        }
        if (comp)
        {
            return comp;
        }
    }

    return NULL;
}

void *vp::Block::external_bind(std::string comp_name, std::string itf_name, void *handle)
{
    for (auto &x : this->get_childs())
    {
        void *result = x->external_bind(comp_name, itf_name, handle);
        if (result != NULL)
            return result;
    }

    return NULL;
}

void vp::Block::get_trace_from_path(std::vector<vp::Trace *> &traces, std::string path)
{
    if (this->get_path() != "" && path.find(this->get_path()) != 0)
    {
        return;
    }

    for (vp::Block *component: this->get_childs())
    {
        component->get_trace_from_path(traces, path);
    }

    for (auto x: this->traces.traces)
    {
        if (x.second->get_full_path().find(path) == 0)
        {
            traces.push_back(x.second);
        }
    }

    for (auto x: this->traces.trace_events)
    {
        if (x.second->get_full_path().find(path) == 0)
        {
            traces.push_back(x.second);
        }
    }
}

string vp::Block::get_path()
{
    return this->path;
}

string vp::Block::get_name()
{
    return this->name;
}

void vp::Block::flush_all()
{
    for (auto &x : this->get_childs())
    {
        x->flush_all();
    }

    this->flush();
}


void vp::Block::start_all()
{
    for (auto &x : this->get_childs())
    {
        x->start_all();
    }

    this->start();
}



void vp::Block::stop_all()
{
    for (auto &x : this->get_childs())
    {
        x->stop_all();
    }

    this->stop();
}




void vp::Block::pre_start_all()
{
    for (auto &x : this->get_childs())
    {
        x->pre_start_all();
    }

    this->pre_start();
}
