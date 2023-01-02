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
#include <gv/gvsoc_proxy.hpp>
#include <gv/gvsoc.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <vp/time/time_scheduler.hpp>
#include <vp/proxy.hpp>
#include <vp/queue.hpp>
#include <vp/signal.hpp>
#include <sys/stat.h>


extern "C" long long int dpi_time_ps();


#ifdef __VP_USE_SYSTEMC
#include <systemc.h>
#endif


#ifdef __VP_USE_SYSTEMV
extern "C" void dpi_raise_event();
#endif

char vp_error[VP_ERROR_SIZE];



static Gv_proxy *proxy = NULL;






void vp::component::get_trace(std::vector<vp::trace *> &traces, std::string path)
{
    if (this->get_path() != "" && path.find(this->get_path()) != 0)
    {
        return;
    }

    for (vp::component *component: this->get_childs())
    {
        component->get_trace(traces, path);
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

void vp::component::reg_step_pre_start(std::function<void()> callback)
{
    this->pre_start_callbacks.push_back(callback);
}

void vp::component::register_build_callback(std::function<void()> callback)
{
    this->build_callbacks.push_back(callback);
}

void vp::component::post_post_build()
{
    traces.post_post_build();
}

void vp::component::final_bind()
{
    for (auto port : this->slave_ports)
    {
        port.second->final_bind();
    }

    for (auto port : this->master_ports)
    {
        port.second->final_bind();
    }

    for (auto &x : this->childs)
    {
        x->final_bind();
    }
}



void vp::component::set_vp_config(js::config *config)
{
    this->vp_config = config;
}



void vp::component::set_gv_conf(struct gv_conf *gv_conf)
{
    if (gv_conf)
    {
        memcpy(&this->gv_conf, gv_conf, sizeof(struct gv_conf));
    }
    else
    {
        memset(&this->gv_conf, 0, sizeof(struct gv_conf));
        gv_init(&this->gv_conf);
    }
}



js::config *vp::component::get_vp_config()
{
    if (this->vp_config == NULL)
    {
        if (this->parent != NULL)
        {
            this->vp_config = this->parent->get_vp_config();
        }
    }

    vp_assert_always(this->vp_config != NULL, NULL, "No VP config found\n");

    return this->vp_config;
}



void vp::component::load_all()
{
    for (auto &x : this->childs)
    {
        x->load_all();
    }

    this->load();
}



int vp::component::build_new()
{
    this->bind_comps();

    this->post_post_build_all();

    this->pre_start_all();

    this->start_all();

    this->final_bind();

    return 0;
}



void vp::component::post_post_build_all()
{
    for (auto &x : this->childs)
    {
        x->post_post_build_all();
    }

    this->post_post_build();
}



void vp::component::start_all()
{
    for (auto &x : this->childs)
    {
        x->start_all();
    }

    this->start();
}



void vp::component::stop_all()
{
    for (auto &x : this->childs)
    {
        x->stop_all();
    }

    this->stop();
}


void vp::component::flush_all()
{
    for (auto &x : this->childs)
    {
        x->flush_all();
    }

    this->flush();
}



void vp::component::pre_start_all()
{
    for (auto &x : this->childs)
    {
        x->pre_start_all();
    }

    this->pre_start();
    for (auto x : pre_start_callbacks)
    {
        x();
    }
}



void vp::component_clock::clk_reg(component *_this, component *clock)
{
    _this->clock = (clock_engine *)clock;
    for (auto &x : _this->childs)
    {
        x->clk_reg(x, clock);
    }
    for (clock_event *event: _this->events)
    {
        event->set_clock(_this->clock);
    }
}


void vp::component::reset_all(bool active, bool from_itf)
{
    // Small hack to not propagate the reset from top level if the reset has
    // already been done through the interface from another component.
    // This should be all implemented with interfaces to better control
    // the reset propagation.
    this->reset_done_from_itf |= from_itf;

    if (from_itf || !this->reset_done_from_itf)
    {
        this->get_trace()->msg("Reset\n");

        this->pre_reset();

        for (auto reg : this->regs)
        {
            reg->reset(active);
        }

        this->block::reset_all(active);

        if (active)
        {
            for (clock_event *event: this->events)
            {
                this->event_cancel(event);
            }
        }

        for (auto &x : this->childs)
        {
            x->reset_all(active);
        }
    }

}

void vp::component_clock::reset_sync(void *__this, bool active)
{
    component *_this = (component *)__this;
    _this->reset_done_from_itf = true;
    _this->reset_all(active, true);
}

void vp::component_clock::pre_build(component *comp)
{
    clock_port.set_reg_meth((vp::clk_reg_meth_t *)&component_clock::clk_reg);
    comp->new_slave_port("clock", &clock_port);

    reset_port.set_sync_meth(&component_clock::reset_sync);
    comp->new_slave_port("reset", &reset_port);

    comp->traces.new_trace("comp", comp->get_trace(), vp::DEBUG);
    comp->traces.new_trace("warning", &comp->warning, vp::WARNING);

}


int64_t vp::time_engine::get_next_event_time()
{
    if (this->first_client)
    {
        return this->first_client->next_event_time;
    }

    return this->time;
}


bool vp::time_engine::dequeue(time_engine_client *client)
{
    if (!client->is_enqueued)
        return false;

    client->is_enqueued = false;

    time_engine_client *current = this->first_client, *prev = NULL;
    while (current && current != client)
    {
        prev = current;
        current = current->next;
    }
    if (prev)
        prev->next = client->next;
    else
        this->first_client = client->next;

    return true;
}

bool vp::time_engine::enqueue(time_engine_client *client, int64_t time)
{
    vp_assert(time >= 0, NULL, "Time must be positive\n");

    int64_t full_time = this->get_time() + time;

#ifdef __VP_USE_SYSTEMC
    // Notify to the engine that something has been pushed in case it is done
    // by an external systemC component and the engine needs to be waken up
    if (started)
        sync_event.notify();
#endif

#ifdef __VP_USE_SYSTEMV
    dpi_raise_event();
#endif

    if (client->is_running())
        return false;

    if (client->is_enqueued)
    {
        if (client->next_event_time <= full_time)
            return false;
        this->dequeue(client);
    }

    client->is_enqueued = true;

    time_engine_client *current = first_client, *prev = NULL;
    client->next_event_time = full_time;
    while (current && current->next_event_time < client->next_event_time)
    {
        prev = current;
        current = current->next;
    }
    if (prev)
        prev->next = client;
    else
        first_client = client;
    client->next = current;

    return true;
}

vp::clock_event::clock_event(component_clock *comp, clock_event_meth_t *meth)
    : comp(comp), _this((void *)static_cast<vp::component *>((vp::component_clock *)(comp))), meth(meth),
    enqueued(false), stall_cycle(0)
{
    comp->add_clock_event(this);
    this->clock = comp->get_clock();
}

vp::clock_event::clock_event(component_clock *comp, void *_this, clock_event_meth_t *meth)
: comp(comp), _this(_this), meth(meth), enqueued(false)
{
    comp->add_clock_event(this);
    this->clock = comp->get_clock();
}

void vp::component_clock::add_clock_event(clock_event *event)
{
    this->events.push_back(event);
}

vp::time_engine *vp::component::get_time_engine()
{
    if (this->time_engine_ptr == NULL)
    {
        this->time_engine_ptr = (vp::time_engine*)this->get_service("time");
    }

    return this->time_engine_ptr;
}



vp::master_port::master_port(vp::component *owner)
    : vp::port(owner)
{
}



void vp::component::new_master_port(std::string name, vp::master_port *port)
{
    this->get_trace()->msg(vp::trace::LEVEL_DEBUG, "New master port (name: %s, port: %p)\n", name.c_str(), port);

    port->set_owner(this);
    port->set_context(this);
    port->set_name(name);
    this->add_master_port(name, port);
}



void vp::component::new_master_port(void *comp, std::string name, vp::master_port *port)
{
    this->get_trace()->msg(vp::trace::LEVEL_DEBUG, "New master port (name: %s, port: %p)\n", name.c_str(), port);

    port->set_owner(this);
    port->set_context(comp);
    port->set_name(name);
    this->add_master_port(name, port);
}



void vp::component::add_slave_port(std::string name, vp::slave_port *port)
{
    vp_assert_always(port != NULL, this->get_trace(), "Adding NULL port\n");
    //vp_assert_always(this->slave_ports[name] == NULL, this->get_trace(), "Adding already existing port\n");
    this->slave_ports[name] = port;
}



void vp::component::add_master_port(std::string name, vp::master_port *port)
{
    vp_assert_always(port != NULL, this->get_trace(), "Adding NULL port\n");
    //vp_assert_always(this->master_ports[name] == NULL, this->get_trace(), "Adding already existing port\n");
    this->master_ports[name] = port;
}



void vp::component::new_slave_port(std::string name, vp::slave_port *port)
{
    this->get_trace()->msg(vp::trace::LEVEL_DEBUG, "New slave port (name: %s, port: %p)\n", name.c_str(), port);

    port->set_owner(this);
    port->set_context(this);
    port->set_name(name);
    this->add_slave_port(name, port);
}



void vp::component::new_slave_port(void *comp, std::string name, vp::slave_port *port)
{
    this->get_trace()->msg(vp::trace::LEVEL_DEBUG, "New slave port (name: %s, port: %p)\n", name.c_str(), port);

    port->set_owner(this);
    port->set_context(comp);
    port->set_name(name);
    this->add_slave_port(name, port);
}



void vp::component::add_service(std::string name, void *service)
{
    if (this->parent)
        this->parent->add_service(name, service);
    else if (all_services[name] == NULL)
        all_services[name] = service;
}



void vp::component::new_service(std::string name, void *service)
{
    this->get_trace()->msg(vp::trace::LEVEL_DEBUG, "New service (name: %s, service: %p)\n", name.c_str(), service);

    if (this->parent)
        this->parent->add_service(name, service);

    all_services[name] = service;
}

void *vp::component::get_service(string name)
{
    if (all_services[name])
        return all_services[name];

    if (this->parent)
        return this->parent->get_service(name);

    return NULL;
}

std::vector<std::string> split_name(const std::string &s, char delimiter)
{
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter))
    {
        tokens.push_back(token);
    }
    return tokens;
}

vp::config *vp::config::create_config(jsmntok_t *tokens, int *_size)
{
    jsmntok_t *current = tokens;
    config *config = NULL;

    switch (current->type)
    {
    case JSMN_PRIMITIVE:
        if (strcmp(current->str, "True") == 0 || strcmp(current->str, "False") == 0 || strcmp(current->str, "true") == 0 || strcmp(current->str, "false") == 0)
        {
            config = new config_bool(current);
        }
        else
        {
            config = new config_number(current);
        }
        current++;
        break;

    case JSMN_OBJECT:
    {
        int size;
        config = new config_object(current, &size);
        current += size;
        break;
    }

    case JSMN_ARRAY:
    {
        int size;
        config = new config_array(current, &size);
        current += size;
        break;
    }

    case JSMN_STRING:
        config = new config_string(current);
        current++;
        break;

    case JSMN_UNDEFINED:
        break;
    }

    if (_size)
    {
        *_size = current - tokens;
    }

    return config;
}

vp::config *vp::config_string::get_from_list(std::vector<std::string> name_list)
{
    if (name_list.size() == 0)
        return this;
    return NULL;
}

vp::config *vp::config_number::get_from_list(std::vector<std::string> name_list)
{
    if (name_list.size() == 0)
        return this;
    return NULL;
}

vp::config *vp::config_bool::get_from_list(std::vector<std::string> name_list)
{
    if (name_list.size() == 0)
        return this;
    return NULL;
}

vp::config *vp::config_array::get_from_list(std::vector<std::string> name_list)
{
    if (name_list.size() == 0)
        return this;
    return NULL;
}

vp::config *vp::config_object::get_from_list(std::vector<std::string> name_list)
{
    if (name_list.size() == 0)
        return this;

    vp::config *result = NULL;
    std::string name;
    int name_pos = 0;

    for (auto &x : name_list)
    {
        if (x != "*" && x != "**")
        {
            name = x;
            break;
        }
        name_pos++;
    }

    for (auto &x : childs)
    {

        if (name == x.first)
        {
            result = x.second->get_from_list(std::vector<std::string>(name_list.begin() + name_pos + 1, name_list.begin() + name_list.size()));
            if (name_pos == 0 || result != NULL)
                return result;
        }
        else if (name_list[0] == "*")
        {
            result = x.second->get_from_list(std::vector<std::string>(name_list.begin() + 1, name_list.begin() + name_list.size()));
            if (result != NULL)
                return result;
        }
        else if (name_list[0] == "**")
        {
            result = x.second->get_from_list(name_list);
            if (result != NULL)
                return result;
        }
    }

    return result;
}

vp::config *vp::config_object::get(std::string name)
{
    return get_from_list(split_name(name, '/'));
}

vp::config_string::config_string(jsmntok_t *tokens)
{
    value = tokens->str;
}

vp::config_number::config_number(jsmntok_t *tokens)
{
    value = atof(tokens->str);
}

vp::config_bool::config_bool(jsmntok_t *tokens)
{
    value = strcmp(tokens->str, "True") == 0 || strcmp(tokens->str, "true") == 0;
}

vp::config_array::config_array(jsmntok_t *tokens, int *_size)
{
    jsmntok_t *current = tokens;
    jsmntok_t *top = current++;

    for (int i = 0; i < top->size; i++)
    {
        int child_size;
        elems.push_back(create_config(current, &child_size));
        current += child_size;
    }

    if (_size)
    {
        *_size = current - tokens;
    }
}

vp::config_object::config_object(jsmntok_t *tokens, int *_size)
{
    jsmntok_t *current = tokens;
    jsmntok_t *t = current++;

    for (int i = 0; i < t->size; i++)
    {
        jsmntok_t *child_name = current++;
        int child_size;
        config *child_config = create_config(current, &child_size);
        current += child_size;

        if (child_config != NULL)
        {
            childs[child_name->str] = child_config;
        }
    }

    if (_size)
    {
        *_size = current - tokens;
    }
}

vp::config *vp::component::import_config(const char *config_string)
{
    if (config_string == NULL)
        return NULL;

    jsmn_parser parser;

    jsmn_init(&parser);
    int nb_tokens = jsmn_parse(&parser, config_string, strlen(config_string), NULL, 0);

    jsmntok_t tokens[nb_tokens];

    jsmn_init(&parser);
    nb_tokens = jsmn_parse(&parser, config_string, strlen(config_string), tokens, nb_tokens);

    char *str = strdup(config_string);
    for (int i = 0; i < nb_tokens; i++)
    {
        jsmntok_t *tok = &tokens[i];
        tok->str = &str[tok->start];
        str[tok->end] = 0;
    }

    return new vp::config_object(tokens);
}

void vp::component::conf(string name, string path, vp::component *parent)
{
    this->name = name;
    this->parent = parent;
    this->path = path;
    if (parent != NULL)
    {
        parent->add_child(name, this);
    }
}

void vp::component::add_child(std::string name, vp::component *child)
{
    this->childs.push_back(child);
    this->childs_dict[name] = child;
}




vp::component *vp::component::get_component(std::vector<std::string> path_list)
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

    for (auto x:this->childs)
    {
        vp::component *comp;
        if (name == x->get_name())
        {
            comp = x->get_component({ path_list.begin() + name_pos + 1, path_list.end() });
        }
        else if (path_list[0] == "**")
        {
            comp = x->get_component(path_list);
        }
        else if (path_list[0] == "*")
        {
            comp = x->get_component({ path_list.begin() + 1, path_list.end() });
        }
        if (comp)
        {
            return comp;
        }
    }

    return NULL;
}

void vp::component::elab()
{
    for (auto &x : this->childs)
    {
        x->elab();
    }
}

void vp::component::new_reg(std::string name, vp::reg_1 *reg, uint8_t reset_val, bool reset)
{
    this->get_trace()->msg(vp::trace::LEVEL_DEBUG, "New register (name: %s, width: %d, reset_val: 0x%x, reset: %d)\n",
        name.c_str(), 1, reset_val, reset
    );

    reg->init(this, name, reset ? (uint8_t *)&reset_val : NULL);
    this->regs.push_back(reg);
}

void vp::component::new_reg(std::string name, vp::reg_8 *reg, uint8_t reset_val, bool reset)
{
    this->get_trace()->msg(vp::trace::LEVEL_DEBUG, "New register (name: %s, width: %d, reset_val: 0x%x, reset: %d)\n",
        name.c_str(), 8, reset_val, reset
    );

    reg->init(this, name, reset ? (uint8_t *)&reset_val : NULL);
    this->regs.push_back(reg);
}

void vp::component::new_reg(std::string name, vp::reg_16 *reg, uint16_t reset_val, bool reset)
{
    this->get_trace()->msg(vp::trace::LEVEL_DEBUG, "New register (name: %s, width: %d, reset_val: 0x%x, reset: %d)\n",
        name.c_str(), 16, reset_val, reset
    );

    reg->init(this, name, reset ? (uint8_t *)&reset_val : NULL);
    this->regs.push_back(reg);
}

void vp::component::new_reg(std::string name, vp::reg_32 *reg, uint32_t reset_val, bool reset)
{
    this->get_trace()->msg(vp::trace::LEVEL_DEBUG, "New register (name: %s, width: %d, reset_val: 0x%x, reset: %d)\n",
        name.c_str(), 32, reset_val, reset
    );

    reg->init(this, name, reset ? (uint8_t *)&reset_val : NULL);
    this->regs.push_back(reg);
}

void vp::component::new_reg(std::string name, vp::reg_64 *reg, uint64_t reset_val, bool reset)
{
    this->get_trace()->msg(vp::trace::LEVEL_DEBUG, "New register (name: %s, width: %d, reset_val: 0x%x, reset: %d)\n",
        name.c_str(), 64, reset_val, reset
    );

    reg->init(this, name, reset ? (uint8_t *)&reset_val : NULL);
    this->regs.push_back(reg);
}

void vp::master_port::final_bind()
{
    if (this->is_bound)
    {
        this->finalize();
    }
}

void vp::slave_port::final_bind()
{
    if (this->is_bound)
        this->finalize();
}

extern "C" char *vp_get_error()
{
    return vp_error;
}

extern "C" void vp_port_bind_to(void *_master, void *_slave, const char *config_str)
{
    vp::master_port *master = (vp::master_port *)_master;
    vp::slave_port *slave = (vp::slave_port *)_slave;

    vp::config *config = NULL;
    if (config_str != NULL)
    {
        config = master->get_comp()->import_config(config_str);
    }

    master->bind_to(slave, config);
    slave->bind_to(master, config);
}


void vp::component::throw_error(std::string error)
{
    throw std::invalid_argument("[\033[31m" + this->get_path() + "\033[0m] " + error);
}


void vp::component::build_instance(std::string name, vp::component *parent)
{
    std::string comp_path = parent->get_path() != "" ? parent->get_path() + "/" + name : name == "" ? "" : "/" + name;

    this->conf(name, comp_path, parent);
    this->pre_pre_build();
    this->pre_build();
    this->build();
    this->power.build();

    for (auto x : build_callbacks)
    {
        x();
    }
}


vp::component *vp::component::new_component(std::string name, js::config *config, std::string module_name)
{
    if (module_name == "")
    {
        module_name = config->get_child_str("vp_component");

        if (module_name == "")
        {
            module_name = "utils.composite_impl";
        }
    }

#ifdef __M32_MODE__
    if (this->get_vp_config()->get_child_bool("sv-mode"))
    {
        module_name = "sv_m32." + module_name;
    }
    else if (this->get_vp_config()->get_child_bool("debug-mode"))
    {
        module_name = "debug_m32." + module_name;
    }
    else
    {
        module_name = "m32." + module_name;
    }
#else
    if (this->get_vp_config()->get_child_bool("sv-mode"))
    {
        module_name = "sv." + module_name;
    }
    else if (this->get_vp_config()->get_child_bool("debug-mode"))
    {
        module_name = "debug." + module_name;
    }
#endif

    this->get_trace()->msg(vp::trace::LEVEL_DEBUG, "New component (name: %s, module: %s)\n", name.c_str(), module_name.c_str());

    std::replace(module_name.begin(), module_name.end(), '.', '/');

    std::string path = __gv_get_component_path(this->get_vp_config(), module_name);

    void *module = dlopen(path.c_str(), RTLD_NOW | RTLD_GLOBAL | RTLD_DEEPBIND);
    if (module == NULL)
    {
        this->throw_error("ERROR, Failed to open periph model (module: " + module_name + ", error: " + std::string(dlerror()) + ")");
    }

    vp::component *(*constructor)(js::config *) = (vp::component * (*)(js::config *)) dlsym(module, "vp_constructor");
    if (constructor == NULL)
    {
        this->throw_error("ERROR, couldn't find vp_constructor in loaded module (module: " + module_name + ")");
    }

    vp::component *instance = constructor(config);

    instance->build_instance(name, this);

    return instance;
}

vp::component::component(js::config *config)
    : block(NULL), traces(*this), power(*this), reset_done_from_itf(false)
{
    this->comp_js_config = config;

    //this->conf(path, parent);
}

void vp::component::create_ports()
{
    js::config *config = this->get_js_config();
    js::config *ports = config->get("vp_ports");
    if (ports == NULL)
    {
        ports = config->get("ports");
    }

    if (ports != NULL)
    {
        this->get_trace()->msg(vp::trace::LEVEL_DEBUG, "Creating ports\n");

        for (auto x : ports->get_elems())
        {
            std::string port_name = x->get_str();

            this->get_trace()->msg(vp::trace::LEVEL_DEBUG, "Creating port (name: %s)\n", port_name.c_str());

            if (this->get_master_port(port_name) == NULL && this->get_slave_port(port_name) == NULL)
                this->add_master_port(port_name, new vp::virtual_port(this));
        }
    }
}

void vp::component::create_bindings()
{
    js::config *config = this->get_js_config();
    js::config *bindings = config->get("vp_bindings");
    if (bindings == NULL)
    {
        bindings = config->get("bindings");
    }

    if (bindings != NULL)
    {
        this->get_trace()->msg(vp::trace::LEVEL_DEBUG, "Creating bindings\n");

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

            this->get_trace()->msg(vp::trace::LEVEL_DEBUG, "Creating binding (%s:%s -> %s:%s)\n",
                master_comp_name.c_str(), master_port_name.c_str(),
                slave_comp_name.c_str(), slave_port_name.c_str()
            );

            vp::component *master_comp = master_comp_name == "self" ? this : this->get_childs_dict()[master_comp_name];
            vp::component *slave_comp = slave_comp_name == "self" ? this : this->get_childs_dict()[slave_comp_name];

            vp_assert_always(master_comp != NULL, this->get_trace(),
                "Binding from invalid master (master: %s / %s, slave: %s / %s)\n",
                master_comp_name.c_str(), master_port_name.c_str(),
                slave_comp_name.c_str(), slave_port_name.c_str());

            vp_assert_always(slave_comp != NULL, this->get_trace(),
                "Binding from invalid slave (master: %s / %s, slave: %s / %s)\n",
                master_comp_name.c_str(), master_port_name.c_str(),
                slave_comp_name.c_str(), slave_port_name.c_str());

            vp::port *master_port = master_comp->get_master_port(master_port_name);
            vp::port *slave_port = slave_comp->get_slave_port(slave_port_name);

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



std::vector<vp::slave_port *> vp::slave_port::get_final_ports()
{
    return { this };
}



std::vector<vp::slave_port *> vp::master_port::get_final_ports()
{
    std::vector<vp::slave_port *> result;

    for (auto x : this->slave_ports)
    {
        std::vector<vp::slave_port *> slave_ports = x->get_final_ports();
        result.insert(result.end(), slave_ports.begin(), slave_ports.end());
    }

    return result;
}



void vp::master_port::bind_to_slaves()
{
    for (auto x : this->slave_ports)
    {
        for (auto y : x->get_final_ports())
        {
            this->get_owner()->get_trace()->msg(vp::trace::LEVEL_DEBUG, "Creating final binding (%s:%s -> %s:%s)\n",
                this->get_owner()->get_path().c_str(), this->get_name().c_str(),
                y->get_owner()->get_path().c_str(), y->get_name().c_str()
            );

            this->bind_to(y, NULL);
            y->bind_to(this, NULL);
        }
    }
}

void vp::component::bind_comps()
{
    this->get_trace()->msg(vp::trace::LEVEL_DEBUG, "Creating final bindings\n");

    for (auto x : this->get_childs())
    {
        x->bind_comps();
    }

    for (auto x : this->master_ports)
    {
        if (!x.second->is_virtual())
        {
            x.second->bind_to_slaves();
        }
    }
}


void *vp::component::external_bind(std::string comp_name, std::string itf_name, void *handle)
{
    for (auto &x : this->childs)
    {
        void *result = x->external_bind(comp_name, itf_name, handle);
        if (result != NULL)
            return result;
    }

    return NULL;
}


void vp::master_port::bind_to_virtual(vp::port *port)
{
    vp_assert_always(port != NULL, this->get_comp()->get_trace(), "Trying to bind master port to NULL\n");
    this->slave_ports.push_back(port);
}

void vp::component::create_comps()
{
    js::config *config = this->get_js_config();
    js::config *comps = config->get("vp_comps");
    if (comps == NULL)
    {
        comps = config->get("components");
    }

    if (comps != NULL)
    {
        this->get_trace()->msg(vp::trace::LEVEL_DEBUG, "Creating components\n");

        for (auto x : comps->get_elems())
        {
            std::string comp_name = x->get_str();

            js::config *comp_config = config->get(comp_name);

            std::string vp_component = comp_config->get_child_str("vp_component");

            if (vp_component == "")
                vp_component = "utils.composite_impl";

            this->new_component(comp_name, comp_config, vp_component);
        }
    }
}



void vp::component::dump_traces_recursive(FILE *file)
{
    this->dump_traces(file);

    for (auto& x: this->get_childs())
    {
        x->dump_traces_recursive(file);
    }
}

std::string vp::__gv_get_component_path(js::config *gv_config, std::string relpath)
{
    js::config *inc_dirs = gv_config->get("include_dirs");
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


vp::component *vp::__gv_create(std::string config_path, struct gv_conf *gv_conf)
{
    setenv("PULP_CONFIG_FILE", config_path.c_str(), 1);

    js::config *js_config = js::import_config_from_file(config_path);
    if (js_config == NULL)
    {
        fprintf(stderr, "Invalid configuration.");
        return NULL;
    }

    js::config *gv_config = js_config->get("target/gvsoc");

    std::string module_name = "vp.trace_domain_impl";

#ifdef __M32_MODE__
    if (gv_config->get_child_bool("sv-mode"))
    {
        module_name = "sv_m32." + module_name;
    }
    else if (gv_config->get_child_bool("debug-mode"))
    {
        module_name = "debug_m32." + module_name;
    }
    else
    {
        module_name = "m32." + module_name;
    }
#else
    if (gv_config->get_child_bool("sv-mode"))
    {
        module_name = "sv." + module_name;
    }
    else if (gv_config->get_child_bool("debug-mode"))
    {
        module_name = "debug." + module_name;
    }
#endif


    std::replace(module_name.begin(), module_name.end(), '.', '/');

    std::string path = __gv_get_component_path(gv_config, module_name);

    void *module = dlopen(path.c_str(), RTLD_NOW | RTLD_GLOBAL | RTLD_DEEPBIND);
    if (module == NULL)
    {
        throw std::invalid_argument("ERROR, Failed to open periph model (module: " + module_name + ", error: " + std::string(dlerror()) + ")");
    }

    vp::component *(*constructor)(js::config *) = (vp::component * (*)(js::config *)) dlsym(module, "vp_constructor");
    if (constructor == NULL)
    {
        throw std::invalid_argument("ERROR, couldn't find vp_constructor in loaded module (module: " + module_name + ")");
    }

    vp::component *instance = constructor(js_config);

    vp::top *top = new vp::top();

    top->top_instance = instance;
    top->power_engine = new vp::power::engine(instance);

    instance->set_vp_config(gv_config);
    instance->set_gv_conf(gv_conf);

    return (vp::component *)top;
}


extern "C" void *gv_create(const char *config_path, struct gv_conf *gv_conf)
{
    return (void *)vp::__gv_create(config_path, gv_conf);
}


extern "C" void gv_destroy(void *arg)
{
    vp::top *top = (vp::top *)arg;
    vp::component *instance = (vp::component *)top->top_instance;
    instance->stop_all();
}


extern "C" void gv_start(void *arg)
{
    vp::top *top = (vp::top *)arg;
    vp::component *instance = (vp::component *)top->top_instance;

    instance->pre_pre_build();
    instance->pre_build();
    instance->build();
    instance->build_new();

    if (instance->gv_conf.open_proxy || instance->get_vp_config()->get_child_bool("proxy/enabled"))
    {
        int in_port = instance->gv_conf.open_proxy ? 0 : instance->get_vp_config()->get_child_int("proxy/port");
        int out_port;
        proxy = new Gv_proxy(instance, instance->gv_conf.req_pipe, instance->gv_conf.reply_pipe);
        if (proxy->open(in_port, &out_port))
        {
            instance->throw_error("Failed to start proxy");
        }

        if (instance->gv_conf.proxy_socket)
        {
            *instance->gv_conf.proxy_socket = out_port;
        }
    }

}


extern "C" void gv_reset(void *arg, bool active)
{
    vp::top *top = (vp::top *)arg;
    vp::component *instance = (vp::component *)top->top_instance;
    instance->reset_all(active);
}


extern "C" void gv_step(void *arg, int64_t timestamp)
{
    vp::top *top = (vp::top *)arg;
    vp::component *instance = (vp::component *)top->top_instance;

    instance->step(timestamp);
}


extern "C" int64_t gv_time(void *arg)
{
    vp::top *top = (vp::top *)arg;
    vp::component *instance = (vp::component *)top->top_instance;

    return instance->get_time_engine()->get_next_event_time();
}


extern "C" void *gv_open(const char *config_path, bool open_proxy, int *proxy_socket, int req_pipe, int reply_pipe)
{
    struct gv_conf gv_conf;

    gv_conf.open_proxy = open_proxy;
    gv_conf.proxy_socket = proxy_socket;
    gv_conf.req_pipe = req_pipe;
    gv_conf.reply_pipe = reply_pipe;

    void *instance = gv_create(config_path, &gv_conf);
    if (instance == NULL)
        return NULL;

    gv_start(instance);

    return instance;
}


Gvsoc_proxy::Gvsoc_proxy(std::string config_path)
    : config_path(config_path)
{

}



void Gvsoc_proxy::proxy_loop()
{
    FILE *sock = fdopen(this->reply_pipe[0], "r");

    while(1)
    {
        char line[1024];

        if (!fgets(line, 1024, sock))
            return ;

        std::string s = std::string(line);
        std::regex regex{R"([\s]+)"};
        std::sregex_token_iterator it{s.begin(), s.end(), regex, -1};
        std::vector<std::string> words{it, {}};

        if (words.size() > 0)
        {
            if (words[0] == "stopped")
            {
                int64_t timestamp = std::atoll(words[1].c_str());
                printf("GOT STOP AT %ld\n", timestamp);
                this->mutex.lock();
                this->stopped_timestamp = timestamp;
                this->running = false;
                this->cond.notify_all();
                this->mutex.unlock();
            }
            else if (words[0] == "running")
            {
                int64_t timestamp = std::atoll(words[1].c_str());
                printf("GOT RUN AT %ld\n", timestamp);
                this->mutex.lock();
                this->running = true;
                this->cond.notify_all();
                this->mutex.unlock();
            }
            else if (words[0] == "req=")
            {
            }
            else
            {
                printf("Ignoring invalid command: %s\n", words[0].c_str());
            }
        }
    }
}


int Gvsoc_proxy::open()
{
    pid_t ppid_before_fork = getpid();

    if (pipe(this->req_pipe) == -1)
        return -1;

    if (pipe(this->reply_pipe) == -1)
        return -1;

    pid_t child_id = fork();

    if(child_id == -1)
    {
        return -1;
    }
    else if (child_id == 0)
    {
        int r = prctl(PR_SET_PDEATHSIG, SIGTERM);
        if (r == -1) { perror(0); exit(1); }
        // test in case the original parent exited just
        // before the prctl() call
        if (getppid() != ppid_before_fork) exit(1);

        void *instance = gv_open(this->config_path.c_str(), true, NULL, this->req_pipe[0], this->reply_pipe[1]);

        int retval = gv_run(instance);

        gv_stop(instance, retval);

        return retval;
    }
    else
    {
        this->running = false;
        this->loop_thread = new std::thread(&Gvsoc_proxy::proxy_loop, this);
    }

    return 0;
}



void Gvsoc_proxy::run()
{
    dprintf(this->req_pipe[1], "cmd=run\n");
}



int64_t Gvsoc_proxy::pause()
{
    int64_t result;
    dprintf(this->req_pipe[1], "cmd=stop\n");
    std::unique_lock<std::mutex> lock(this->mutex);
    while (this->running)
    {
        this->cond.wait(lock);
    }
    result = this->stopped_timestamp;
    lock.unlock();
    return result;
}



void Gvsoc_proxy::close()
{
    dprintf(this->req_pipe[1], "cmd=quit\n");
}



void Gvsoc_proxy::add_event_regex(std::string regex)
{
    dprintf(this->req_pipe[1], "cmd=event add %s\n", regex.c_str());

}



void Gvsoc_proxy::remove_event_regex(std::string regex)
{
    dprintf(this->req_pipe[1], "cmd=event remove %s\n", regex.c_str());
}



void Gvsoc_proxy::add_trace_regex(std::string regex)
{
    dprintf(this->req_pipe[1], "cmd=trace add %s\n", regex.c_str());
}



void Gvsoc_proxy::remove_trace_regex(std::string regex)
{
    dprintf(this->req_pipe[1], "cmd=trace remove %s\n", regex.c_str());
}


vp::time_scheduler::time_scheduler(js::config *config)
    : time_engine_client(config), first_event(NULL)
{

}


int64_t vp::time_scheduler::exec()
{
    vp::time_event *current = this->first_event;

    while (current && current->time == this->get_time())
    {
        this->first_event = current->next;
        current->set_enqueued(false);

        current->meth(current->_this, current);

        current = this->first_event;
    }

    if (this->first_event == NULL)
    {
        return -1;
    }
    else
    {
        return this->first_event->time - this->get_time();
    }
}


vp::time_event::time_event(time_scheduler *comp, time_event_meth_t *meth)
    : comp(comp), _this((void *)static_cast<vp::component *>((vp::time_scheduler *)(comp))), meth(meth), enqueued(false)
{
    comp->add_event(this);
}


void vp::time_scheduler::reset(bool active)
{
    if (active)
    {
        for (time_event *event: this->events)
        {
            this->cancel(event);
        }
    }
}

void vp::time_scheduler::cancel(vp::time_event *event)
{
    if (!event->is_enqueued())
        return;

    vp::time_event *current = this->first_event, *prev = NULL;

    while (current && current != event)
    {
        prev = current;
        current = current->next;
    }

    if (prev)
    {
        prev->next = event->next;
    }
    else
    {
        this->first_event = event->next;
    }

    event->set_enqueued(false);

    if (this->first_event == NULL)
    {
        this->dequeue_from_engine();
    }
}

void vp::time_scheduler::add_event(time_event *event)
{
    this->events.push_back(event);
}


vp::time_event *vp::time_scheduler::enqueue(time_event *event, int64_t time)
{
    vp::time_event *current = this->first_event, *prev = NULL;
    int64_t full_time = time + this->get_time();

    event->set_enqueued(true);

    while (current && current->time < full_time)
    {
        prev = current;
        current = current->next;
    }

    if (prev)
        prev->next = event;
    else
        this->first_event = event;
    event->next = current;
    event->time = full_time;

    this->enqueue_to_engine(time);

    return event;
}


extern "C" int gv_run(void *arg)
{
    vp::top *top = (vp::top *)arg;
    vp::component *instance = (vp::component *)top->top_instance;

    if (!proxy)
    {
        instance->run();
    }

    return instance->join();
}


extern "C" void gv_init(struct gv_conf *gv_conf)
{
    gv_conf->open_proxy = 0;
    if (gv_conf->proxy_socket)
    {
        *gv_conf->proxy_socket = -1;
    }
    gv_conf->req_pipe = 0;
    gv_conf->reply_pipe = 0;
}


extern "C" void gv_stop(void *arg, int retval)
{
    vp::top *top = (vp::top *)arg;
    vp::component *instance = (vp::component *)top->top_instance;

    if (proxy)
    {
        proxy->stop(retval);
    }

    instance->stop_all();

    delete top->power_engine;
}



#ifdef __VP_USE_SYSTEMC

static void *(*sc_entry)(void *);
static void *sc_entry_arg;

void set_sc_main_entry(void *(*entry)(void *), void *arg)
{
    sc_entry = entry;
    sc_entry_arg = arg;
}

int sc_main(int argc, char *argv[])
{
    sc_entry(sc_entry_arg);
    return 0;
}
#endif


void vp::fatal(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    if (vfprintf(stderr, fmt, ap) < 0) {}
    va_end(ap);
    abort();
}


extern "C" void *gv_chip_pad_bind(void *handle, char *name, int ext_handle)
{
    vp::top *top = (vp::top *)handle;
    vp::component *instance = (vp::component *)top->top_instance;
    return instance->external_bind(name, "", (void *)(long)ext_handle);
}
