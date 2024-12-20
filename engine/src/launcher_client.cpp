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


#include <pthread.h>
#include <signal.h>

#include <vp/vp.hpp>
#include <gv/gvsoc.hpp>
#include <vp/proxy.hpp>
#include <vp/launcher.hpp>
#include <vp/proxy_client.hpp>
#include "vp/top.hpp"

static gv::GvsocLauncher *launcher = NULL;

gv::Logger::Logger(std::string module)
: module(module)
{
}

gv::GvsocLauncherClient::GvsocLauncherClient(gv::GvsocConf *conf, std::string name)
: logger("LAUNCHER CLIENT(" + name + ")")
{
    this->logger.info("Creating client (this: %p)\n", this);

    if (launcher != NULL && conf != NULL)
    {
        throw logic_error("Trying to open client with configuration while one has already been opened with configuration");
    }

    if (launcher == NULL)
    {
        launcher = new gv::GvsocLauncher(conf);
    }

    launcher->register_client(this);
}

gv::GvsocLauncherClient::~GvsocLauncherClient()
{
    launcher->unregister_client(this);
}

void gv::GvsocLauncherClient::open()
{
    this->logger.info("Open\n");
    launcher->open(this);
}

void gv::GvsocLauncherClient::bind(gv::Gvsoc_user *user)
{
    this->logger.info("Bind (user: %p)\n", user);
    launcher->bind(user, this);
}

void gv::GvsocLauncherClient::close()
{
    this->logger.info("Close\n");
    launcher->close(this);
}

void gv::GvsocLauncherClient::start()
{
    this->logger.info("Start\n");
    launcher->start(this);
}

void gv::GvsocLauncherClient::run()
{
    this->logger.info("Run\n");
    launcher->run(this);
}

void gv::GvsocLauncherClient::flush()
{
    this->logger.info("Flush\n");
    launcher->flush(this);
}

int64_t gv::GvsocLauncherClient::stop()
{
    this->logger.info("Stop\n");
    return launcher->stop(this);
}

void gv::GvsocLauncherClient::wait_stopped()
{
    this->logger.info("Wait stopped\n");
    launcher->wait_stopped(this);
}

double gv::GvsocLauncherClient::get_instant_power(double &dynamic_power, double &static_power)
{
    return launcher->get_instant_power(dynamic_power, static_power, this);
}

double gv::GvsocLauncherClient::get_average_power(double &dynamic_power, double &static_power)
{
    return launcher->get_average_power(dynamic_power, static_power, this);
}

void gv::GvsocLauncherClient::report_start()
{
    launcher->report_start(this);
}

void gv::GvsocLauncherClient::report_stop()
{
    launcher->report_stop(this);
}

gv::PowerReport *gv::GvsocLauncherClient::report_get()
{
    return launcher->report_get(this);
}

int64_t gv::GvsocLauncherClient::step(int64_t duration)
{
    this->logger.info("Step (duration: %lld)\n", duration);
    return launcher->step(duration, this);
}

int64_t gv::GvsocLauncherClient::step_until(int64_t timestamp)
{
    this->logger.info("Step until (timestamp: %lld)\n", timestamp);
    return launcher->step_until(timestamp, this);
}

int64_t gv::GvsocLauncherClient::step_and_wait(int64_t duration)
{
    this->logger.info("Step and wait (duration: %lld)\n", duration);
    return launcher->step_and_wait(duration, this);
}

int64_t gv::GvsocLauncherClient::step_until_and_wait(int64_t timestamp)
{
    this->logger.info("Step until and wait (timestamp: %lld)\n", timestamp);
    return launcher->step_until_and_wait(timestamp, this);
}

int gv::GvsocLauncherClient::join()
{
    this->logger.info("Join\n");
    this->quit(0);
    return launcher->join(this);
}

void gv::GvsocLauncherClient::retain()
{
    this->logger.info("Retain\n");
    launcher->retain(this);
}

void gv::GvsocLauncherClient::release()
{
    this->logger.info("Release\n");
    launcher->release(this);
}

void gv::GvsocLauncherClient::lock()
{
    this->logger.info("Lock\n");
    launcher->lock(this);
}

void gv::GvsocLauncherClient::unlock()
{
    this->logger.info("Unlock\n");
    launcher->unlock(this);
}

void gv::GvsocLauncherClient::update(int64_t timestamp)
{
    this->logger.info("Timestamp (timestamp: %lld)\n", timestamp);
    launcher->update(timestamp, this);
}

gv::Io_binding *gv::GvsocLauncherClient::io_bind(gv::Io_user *user,
    std::string comp_name, std::string itf_name)
{
    this->logger.info("IO bind (user: %p, comp: %s, itf: %s)\n", user, comp_name.c_str(),
        itf_name.c_str());
    return launcher->io_bind(user, comp_name, itf_name, this);
}

gv::Wire_binding *gv::GvsocLauncherClient::wire_bind(gv::Wire_user *user,
    std::string comp_name, std::string itf_name)
{
    this->logger.info("Wire bind (user: %p, comp: %s, itf: %s)\n", user, comp_name.c_str(),
            itf_name.c_str());
    return launcher->wire_bind(user, comp_name, itf_name, this);
}

void gv::GvsocLauncherClient::vcd_bind(gv::Vcd_user *user)
{
    this->logger.info("VCD bind (user: %p)\n", user);
    launcher->vcd_bind(user, this);
}

void gv::GvsocLauncherClient::vcd_enable()
{
    this->logger.info("VCD enable\n");
    launcher->vcd_enable(this);
}

void gv::GvsocLauncherClient::vcd_disable()
{
    this->logger.info("VCD disable\n");
    launcher->vcd_disable(this);
}

void gv::GvsocLauncherClient::event_add(std::string path, bool is_regex)
{
    this->logger.info("Event add (path: %s, is_regex: %d)\n", path.c_str(), is_regex);
    launcher->event_add(path, is_regex, this);
}

void gv::GvsocLauncherClient::event_exclude(std::string path, bool is_regex)
{
    this->logger.info("Event Exclude (path: %s, is_regex: %d)\n", path.c_str(), is_regex);
    launcher->event_exclude(path, is_regex, this);
}

void *gv::GvsocLauncherClient::get_component(std::string path)
{
    return launcher->get_component(path, this);
}

void gv::GvsocLauncherClient::quit(int status)
{
    this->has_quit = true;
    this->status = status;
    launcher->client_quit(this);
}
