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


#include <iostream>
#include <sstream>
#include <string>
#include <stdio.h>
#include <vp/vp.hpp>
#include <stdio.h>
#include "string.h"
#include <iostream>
#include <sstream>
#include <string>
#include <dlfcn.h>
#include <algorithm>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <regex>
#include <sys/types.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <vp/proxy.hpp>
#include <vp/queue.hpp>
#include <vp/signal.hpp>
#include <vp/proxy_client.hpp>
#include <vp/launcher.hpp>
#include <sys/stat.h>
#include <vp/trace/trace_engine.hpp>
#include <vp/register.hpp>



void vp::Component::final_bind()
{
    this->reset_is_bound = this->reset_port.is_bound();

    for (auto port : this->ports)
    {
        if (port.second->is_slave())
        {
            port.second->final_bind();
        }
    }

    for (auto port : this->ports)
    {
        if (port.second->is_master())
        {
            port.second->final_bind();
        }
    }

    for (vp::Block *x : this->get_childs())
    {
        if (x->is_component())
        {
            vp::Component *component = (vp::Component *)x;
            component->final_bind();
        }
    }
}





int vp::Component::build_all()
{
    this->bind_comps();

    this->pre_start_all();

    this->start_all();

    this->final_bind();

    return 0;
}





void vp::Component::new_master_port(std::string name, vp::MasterPort *port, vp::Block *comp)
{
    this->get_trace()->msg(vp::Trace::LEVEL_DEBUG, "New master port (name: %s, port: %p)\n", name.c_str(), port);

    if (comp == NULL)
    {
        comp = this;
    }

    port->set_owner(this);
    port->set_context(comp);
    port->set_name(name);
    this->add_port(name, port);
}




void vp::Component::add_port(std::string name, vp::Port *port)
{
    if (this->ports.find(name) != this->ports.end() && !this->ports[name]->is_virtual())
    {
        this->get_trace()->fatal("Trying to create already existing port (name: %s)\n", name.c_str());
    }
    vp_assert_always(port != NULL, this->get_trace(), "Adding NULL port\n");
    //vp_assert_always(this->SlavePorts[name] == NULL, this->get_trace(), "Adding already existing port\n");
    this->ports[name] = port;
}


void vp::Component::new_slave_port(std::string name, vp::SlavePort *port, void *comp)
{
    this->get_trace()->msg(vp::Trace::LEVEL_DEBUG, "New slave port (name: %s, port: %p)\n", name.c_str(), port);

    if (comp == NULL)
    {
        comp = this;
    }

    port->set_owner(this);
    port->set_context(comp);
    port->set_name(name);
    this->add_port(name, port);
}




void vp::Component::add_child(std::string name, vp::Component *child)
{
    this->childs_dict[name] = child;
}




void vp::Component::new_reg(std::string name, vp::reg_1 *reg, uint8_t reset_val, bool reset)
{
    this->get_trace()->msg(vp::Trace::LEVEL_DEBUG, "New register (name: %s, width: %d, reset_val: 0x%x, reset: %d)\n",
        name.c_str(), 1, reset_val, reset
    );

    reg->init(this, name, reset ? (uint8_t *)&reset_val : NULL);
    this->regs.push_back(reg);
}

void vp::Component::new_reg(std::string name, vp::reg_8 *reg, uint8_t reset_val, bool reset)
{
    this->get_trace()->msg(vp::Trace::LEVEL_DEBUG, "New register (name: %s, width: %d, reset_val: 0x%x, reset: %d)\n",
        name.c_str(), 8, reset_val, reset
    );

    reg->init(this, name, reset ? (uint8_t *)&reset_val : NULL);
    this->regs.push_back(reg);
}

void vp::Component::new_reg(std::string name, vp::reg_16 *reg, uint16_t reset_val, bool reset)
{
    this->get_trace()->msg(vp::Trace::LEVEL_DEBUG, "New register (name: %s, width: %d, reset_val: 0x%x, reset: %d)\n",
        name.c_str(), 16, reset_val, reset
    );

    reg->init(this, name, reset ? (uint8_t *)&reset_val : NULL);
    this->regs.push_back(reg);
}

void vp::Component::new_reg(std::string name, vp::reg_32 *reg, uint32_t reset_val, bool reset)
{
    this->get_trace()->msg(vp::Trace::LEVEL_DEBUG, "New register (name: %s, width: %d, reset_val: 0x%x, reset: %d)\n",
        name.c_str(), 32, reset_val, reset
    );

    reg->init(this, name, reset ? (uint8_t *)&reset_val : NULL);
    this->regs.push_back(reg);
}

void vp::Component::new_reg(std::string name, vp::reg_64 *reg, uint64_t reset_val, bool reset)
{
    this->get_trace()->msg(vp::Trace::LEVEL_DEBUG, "New register (name: %s, width: %d, reset_val: 0x%x, reset: %d)\n",
        name.c_str(), 64, reset_val, reset
    );

    reg->init(this, name, reset ? (uint8_t *)&reset_val : NULL);
    this->regs.push_back(reg);
}


void vp::Component::bind_comps()
{
    // printf("%s BIND COMPS\n", this->get_path().c_str());
    this->get_trace()->msg(vp::Trace::LEVEL_DEBUG, "Creating final bindings\n");

    for (vp::Block *x : this->get_childs())
    {
        if (x->is_component())
        {
            vp::Component *component = (vp::Component *)x;
            component->bind_comps();
        }
    }

    for (auto x : this->ports)
    {
        // printf("BIND %p\n", x.second);
        // printf("BIND %s\n", x.second->get_name().c_str());
        if (x.second->is_master() && !x.second->is_virtual())
        {
            x.second->bind_to_slaves();
        }
    }
}

void vp::Component::create_comps()
{
    js::Config *config = this->get_js_config();
    js::Config *comps = config->get("vp_comps");
    if (comps == NULL)
    {
        comps = config->get("components");
    }

    if (comps != NULL)
    {
        this->get_trace()->msg(vp::Trace::LEVEL_DEBUG, "Creating components\n");

        for (auto x : comps->get_elems())
        {
            std::string comp_name = x->get_str();

            js::Config *comp_config = config->get(comp_name);

            std::string vp_component = comp_config->get_child_str("vp_component");

            if (vp_component == "")
                vp_component = "utils.composite_impl";

            this->new_component(comp_name, comp_config, vp_component);
        }
    }
}



std::string vp::Component::get_module_path(js::Config *gv_config, std::string relpath)
{
    js::Config *inc_dirs = gv_config->get("include_dirs");
    std::string inc_dirs_str = "";
    for (auto x: inc_dirs->get_elems())
    {
        std::string inc_dir = x->get_str();
        std::string path = inc_dir + "/" + relpath + ".so";
        inc_dirs_str += inc_dirs_str == "" ? inc_dir : ":" + inc_dir;
        struct stat buffer;
        if (stat(path.c_str(), &buffer) == 0)
        {
            return path;
        }
    }

     throw std::invalid_argument("Couldn't find component (name: " + relpath + ", inc_dirs: " + inc_dirs_str );
}


vp::Component *vp::Component::load_component(js::Config *config, js::Config *gv_config,
    vp::Component *parent, std::string name, vp::TimeEngine *time_engine,
    vp::TraceEngine *trace_engine, vp::PowerEngine *power_engine)
{
    std::string module_name = config->get_child_str("vp_component");

    if (module_name == "")
    {
        module_name = "utils.composite_impl";
    }

#ifdef __M32_MODE__
    if (gv_config->get_child_bool("debug-mode"))
    {
        module_name = "debug_m32." + module_name;
    }
    else
    {
        module_name = "m32." + module_name;
    }
#else
    if (gv_config->get_child_bool("debug-mode"))
    {
        module_name = "debug." + module_name;
    }
#endif


    std::replace(module_name.begin(), module_name.end(), '.', '/');

    std::string module_path = vp::Component::get_module_path(gv_config, module_name);

    void *module = dlopen(module_path.c_str(), RTLD_NOW | RTLD_GLOBAL | RTLD_DEEPBIND);
    if (module == NULL)
    {
        throw std::invalid_argument("ERROR, Failed to open periph model (module: " + module_name + ", error: " + std::string(dlerror()) + ")");
    }

    vp::Component *(*gv_new)(ComponentConf &conf) = (vp::Component * (*)(ComponentConf &conf)) dlsym(module, "gv_new");
    if (gv_new)
    {
        ComponentConf conf(name, parent, config, gv_config, time_engine, trace_engine,
            power_engine);
        return gv_new(conf);
    }

    throw std::invalid_argument("ERROR, couldn't find gv_new loaded module (module: " + module_name + ")");
}

gv::GvsocLauncher *vp::Component::get_launcher()
{
    if (this->parent)
    {
        return this->parent->get_launcher();
    }

    return this->launcher;
}


void vp::Component::reset_sync(vp::Block *__this, bool active)
{
    Component *_this = (Component *)__this;
    _this->reset_all(active, true);
}


vp::Component *vp::Component::new_component(std::string name, js::Config *config, std::string module_name)
{
    vp::Component *instance = vp::Component::load_component(config, this->gv_config, this, name,
        this->time.get_engine(), this->traces.get_trace_engine(), this->power.get_engine());

    this->get_trace()->msg(vp::Trace::LEVEL_DEBUG, "New component (name: %s)\n", name.c_str());

    return instance;
}

vp::Component::Component(vp::ComponentConf &config)
    : Block(config.parent, config.name, config.time_engine, config.trace_engine,
    config.power_engine)
{
    this->js_config = config.config;
    this->name = config.name;
    this->parent = config.parent;
    this->gv_config = config.gv_config;
    if (config.parent)
    {
        parent->add_child(name, this);
    }

    this->create_comps();

    if (!this->childs_dict.empty())
    {
        this->create_ports();
        this->create_bindings();
    }

    clock_port.set_reg_meth((vp::ClkRegMeth *)&Component::clk_reg);
    clock_port.set_set_frequency_meth((vp::ClkSetFrequencyMeth *)&Component::clk_set_frequency);
    this->new_slave_port("clock", &clock_port);

    reset_port.set_sync_meth(&Component::reset_sync);
    this->new_slave_port("reset", &reset_port);
    this->traces.new_trace("comp", this->get_trace(), vp::DEBUG);

    this->power_port.set_sync_meth(&this->power_supply_sync);
    this->new_slave_port("power_supply", &this->power_port, (vp::Block *)this);

    this->voltage_port.set_sync_meth(&this->voltage_sync);
    this->new_slave_port("voltage", &this->voltage_port, (vp::Block *)this);
}


void vp::Component::create_ports()
{
    js::Config *config = this->get_js_config();
    js::Config *ports = config->get("vp_ports");
    if (ports == NULL)
    {
        ports = config->get("ports");
    }

    if (ports != NULL)
    {
        this->get_trace()->msg(vp::Trace::LEVEL_DEBUG, "Creating ports\n");

        for (auto x : ports->get_elems())
        {
            std::string port_name = x->get_str();

            this->get_trace()->msg(vp::Trace::LEVEL_DEBUG, "Creating port (name: %s)\n", port_name.c_str());

            if (this->get_port(port_name) == NULL && this->get_port(port_name) == NULL)
            {
                vp::VirtualPort *port = new vp::VirtualPort(this);
                this->add_port(port_name, port);
                port->set_name(port_name);
            }
        }
    }
}

void vp::Component::create_bindings()
{
    js::Config *config = this->get_js_config();
    js::Config *bindings = config->get("vp_bindings");
    if (bindings == NULL)
    {
        bindings = config->get("bindings");
    }

    if (bindings != NULL)
    {
        this->get_trace()->msg(vp::Trace::LEVEL_DEBUG, "Creating bindings\n");

        for (auto x : bindings->get_elems())
        {
            std::string master_binding = x->get_elem(0)->get_str();
            std::string slave_binding = x->get_elem(1)->get_str();
            int pos = master_binding.find_first_of("->");
            std::string master_comp_name = master_binding.substr(0, pos);
            std::string master_port_name = master_binding.substr(pos + 2);
            pos = slave_binding.find_first_of("->");
            std::string slave_comp_name = slave_binding.substr(0, pos);
            std::string slave_port_name = slave_binding.substr(pos + 2);

            this->get_trace()->msg(vp::Trace::LEVEL_DEBUG, "Creating binding (%s:%s -> %s:%s)\n",
                master_comp_name.c_str(), master_port_name.c_str(),
                slave_comp_name.c_str(), slave_port_name.c_str()
            );

            vp::Component *master_comp = master_comp_name == "self" ? this : this->get_childs_dict()[master_comp_name];
            vp::Component *slave_comp = slave_comp_name == "self" ? this : this->get_childs_dict()[slave_comp_name];

            vp_assert_always(master_comp != NULL, this->get_trace(),
                "Binding from invalid master (master: %s / %s, slave: %s / %s)\n",
                master_comp_name.c_str(), master_port_name.c_str(),
                slave_comp_name.c_str(), slave_port_name.c_str());

            vp_assert_always(slave_comp != NULL, this->get_trace(),
                "Binding from invalid slave (master: %s / %s, slave: %s / %s)\n",
                master_comp_name.c_str(), master_port_name.c_str(),
                slave_comp_name.c_str(), slave_port_name.c_str());

            vp::Port *master_port = master_comp->get_port(master_port_name);
            vp::Port *slave_port = slave_comp->get_port(slave_port_name);

            vp_assert_always(master_port != NULL, this->get_trace(),
                "Binding from invalid master port (master: %s / %s, slave: %s / %s)\n",
                master_comp_name.c_str(), master_port_name.c_str(),
                slave_comp_name.c_str(), slave_port_name.c_str());

            vp_assert_always(slave_port != NULL, this->get_trace(),
                "Binding from invalid slave port (master: %s / %s, slave: %s / %s)\n",
                master_comp_name.c_str(), master_port_name.c_str(),
                slave_comp_name.c_str(), slave_port_name.c_str());

            master_port->bind_to_virtual(slave_port);
        }
    }
}

void vp::Component::power_supply_sync(vp::Block *__this, int state)
{
    vp::Component *_this = (vp::Component *)__this;
    _this->power.power_supply_set_all((vp::PowerSupplyState)state);
}

void vp::Component::voltage_sync(vp::Block *__this, int voltage)
{
    vp::Component *_this = (vp::Component *)__this;
    _this->power.voltage_set_all(voltage);
}


void vp::Component::clk_reg(Component *_this, Component *clock_engine_instance)
{
    _this->clock.set_engine((ClockEngine *)clock_engine_instance);
}


void vp::Component::clk_set_frequency(Component *_this, int64_t frequency)
{
    _this->power.set_frequency(frequency);
}

vp::Port *vp::Component::get_port(std::string name)
{
    if (this->ports.find(name) == this->ports.end())
    {
        return NULL;
    }
    else
    {
        return this->ports[name];
    }
}

void vp::Component::set_launcher(gv::GvsocLauncher *launcher)
{
    this->launcher = launcher;
}