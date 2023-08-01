/*
 * Copyright (C) 2020 GreenWaves Technologies, SAS, SAS, ETH Zurich and University of Bologna
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
#include <gv/gvsoc.hpp>
#include <vp/proxy.hpp>
#include <vp/launcher.hpp>

Gvsoc_launcher::Gvsoc_launcher(gv::GvsocConf *conf)
{
    this->conf = conf;
}

void Gvsoc_launcher::open()
{
    this->handler = new vp::top(conf->config_path, conf->api_mode == gv::Api_mode::Api_mode_async);

    this->instance = ((vp::top *)this->handler)->top_instance;
    this->engine = ((vp::top *)this->handler)->time_engine;

    if (instance->get_vp_config()->get_child_bool("proxy/enabled"))
    {
        int in_port = instance->get_vp_config()->get_child_int("proxy/port");
        int out_port;
        proxy = new Gv_proxy(this->engine, instance);

        if (proxy->open(in_port, &out_port))
        {
            instance->throw_error("Failed to start proxy");
        }
    }
}

void Gvsoc_launcher::bind(gv::Gvsoc_user *user)
{
    this->user = user;
    this->engine->bind_to_launcher(user);
}

void Gvsoc_launcher::start()
{
    this->instance->build_new();
    this->instance->reset_all(true);
    this->instance->reset_all(false);
}

void Gvsoc_launcher::close()
{
    if (proxy)
    {
        proxy->stop(this->retval);
    }

    this->instance->stop_all();

    vp::top *top = (vp::top *)this->handler;

    delete top;
}

void Gvsoc_launcher::run()
{
    if (!proxy)
    {
        this->engine->step(0);
    }
}

int Gvsoc_launcher::join()
{
    this->retval = this->engine->join();
    return this->retval;
}

int64_t Gvsoc_launcher::stop()
{
    this->engine->stop_exec();
    return this->engine->get_time();
}

void Gvsoc_launcher::wait_stopped()
{
    this->engine->wait_stopped();
}

int64_t Gvsoc_launcher::step(int64_t duration)
{
    if (!proxy)
    {
        this->engine->step(duration);
    }
    return 0;
}

int64_t Gvsoc_launcher::step_until(int64_t timestamp)
{
    if (!proxy)
    {
        return this->engine->step_until(timestamp);
    }
    return 0;
}

gv::Io_binding *Gvsoc_launcher::io_bind(gv::Io_user *user, std::string comp_name, std::string itf_name)
{
    return (gv::Io_binding *)this->instance->external_bind(comp_name, itf_name, (void *)user);
}

gv::Wire_binding *Gvsoc_launcher::wire_bind(gv::Wire_user *user, std::string comp_name, std::string itf_name)
{
    return (gv::Wire_binding *)this->instance->external_bind(comp_name, itf_name, (void *)user);
}

void Gvsoc_launcher::vcd_bind(gv::Vcd_user *user)
{
    this->instance->traces.get_trace_manager()->set_vcd_user(user);
}

void Gvsoc_launcher::event_add(std::string path, bool is_regex)
{
    this->instance->traces.get_trace_manager()->conf_trace(1, path, 1);
}

void Gvsoc_launcher::event_exclude(std::string path, bool is_regex)
{
    this->instance->traces.get_trace_manager()->conf_trace(1, path, 0);
}


static std::vector<std::string> split(const std::string& s, char delimiter)
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



void Gvsoc_launcher::update(int64_t timestamp)
{
    this->engine->time_engine_update(timestamp);
}



void *Gvsoc_launcher::get_component(std::string path)
{
    return this->instance->get_component(split(path, '/'));
}
