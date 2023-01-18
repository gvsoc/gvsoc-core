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
    struct gv_conf gv_conf;

    gv_conf.open_proxy = false;
    gv_conf.proxy_socket = &conf->proxy_socket;
    gv_conf.req_pipe = -1;
    gv_conf.reply_pipe = -1;

    this->handler = vp::__gv_create(conf->config_path, &gv_conf);

    this->instance = ((vp::top *)this->handler)->top_instance;

    this->instance->pre_pre_build();
    this->instance->pre_build();
    this->instance->build();

    if (this->instance->gv_conf.open_proxy || instance->get_vp_config()->get_child_bool("proxy/enabled"))
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
}

void Gvsoc_launcher::run()
{
    if (!proxy)
    {
        this->instance->step(0);
    }
}

int Gvsoc_launcher::join()
{
    this->retval = this->instance->join();
    return this->retval;
}

int64_t Gvsoc_launcher::stop()
{
    this->instance->stop_exec();
    return this->instance->get_time();
}

int64_t Gvsoc_launcher::step(int64_t duration)
{
    if (!proxy)
    {
        this->instance->step(duration);
    }
    return 0;
}

gv::Io_binding *Gvsoc_launcher::io_bind(gv::Io_user *user, std::string comp_name, std::string itf_name)
{
    return (gv::Io_binding *)this->instance->external_bind(comp_name, itf_name, (void *)user);
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


void *Gvsoc_launcher::get_component(std::string path)
{
    return this->instance->get_component(split(path, '/'));
}

extern "C" void *gv_chip_pad_bind(void *handle, char *name, int ext_handle)
{
    Gvsoc_launcher *gvsoc = (Gvsoc_launcher *)handle;
    return gvsoc->instance->external_bind(name, "", (void *)(long)ext_handle);
}

