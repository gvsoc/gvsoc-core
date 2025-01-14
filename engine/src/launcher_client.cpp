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

#include <vp/vp.hpp>
#include <gv/gvsoc.hpp>
#include <vp/proxy.hpp>
#include <vp/controller.hpp>
#include <vp/proxy_client.hpp>

namespace gv
{
    gv::Controller controller;
};

gv::Logger::Logger(std::string module)
: module(module)
{
}

gv::ControllerClient::ControllerClient(gv::GvsocConf *conf, std::string name)
: logger("LAUNCHER CLIENT(" + name + ")"), name(name)
{
    this->logger.info("Creating client (this: %p)\n", this);

    // The first client is the main controller and is the only one allowed to give GVSOC
    // configuration. It can be synchronous or asynchronous but other controllers are always
    // asynchronous.
    if (gv::Controller::get().is_init && conf != NULL)
    {
        throw logic_error("Trying to open client with configuration while one has already been opened with configuration");
    }

    this->async = conf == NULL || conf->api_mode == gv::Api_mode::Api_mode_async;

    gv::Controller::get().init(conf);

    gv::Controller::get().engine_lock();
    gv::Controller::get().register_client(this);
    gv::Controller::get().engine_unlock();
}

gv::ControllerClient::~ControllerClient()
{
    gv::Controller::get().engine_lock();
    gv::Controller::get().unregister_client(this);
    gv::Controller::get().engine_unlock();
}

void gv::ControllerClient::open()
{
    this->logger.info("Open\n");
    gv::Controller::get().open(this);
}

void gv::ControllerClient::bind(gv::Gvsoc_user *user)
{
    this->logger.info("Bind (user: %p)\n", user);
    this->user = user;
    gv::Controller::get().bind(user, this);
}

void gv::ControllerClient::close()
{
    this->logger.info("Close\n");
    gv::Controller::get().close(this);
}

void gv::ControllerClient::start()
{
    this->logger.info("Start\n");
    gv::Controller::get().start(this);
}

void gv::ControllerClient::run()
{
    this->logger.info("Run\n");
    if (this->async)
    {
        gv::Controller::get().lock();
        gv::Controller::get().run_async(this);
        gv::Controller::get().unlock();
    }
    else
    {
        gv::Controller::get().run_sync();
    }
}

void gv::ControllerClient::flush()
{
    this->logger.info("Flush\n");
    gv::Controller::get().engine_lock();
    gv::Controller::get().flush(this);
    gv::Controller::get().engine_unlock();
}

int64_t gv::ControllerClient::stop()
{
    this->logger.info("Stop\n");
    gv::Controller::get().engine_lock();
    int64_t result = gv::Controller::get().stop(this);
    gv::Controller::get().engine_unlock();
    return result;
}

void gv::ControllerClient::sim_finished(int status)
{
    this->logger.info("Simulation finished\n");
    gv::Controller::get().stop(this);
    if(this->user)
    {
        this->user->has_ended();
    }
}

double gv::ControllerClient::get_instant_power(double &dynamic_power, double &static_power)
{
    return gv::Controller::get().get_instant_power(dynamic_power, static_power, this);
}

double gv::ControllerClient::get_average_power(double &dynamic_power, double &static_power)
{
    return gv::Controller::get().get_average_power(dynamic_power, static_power, this);
}

void gv::ControllerClient::report_start()
{
    gv::Controller::get().report_start(this);
}

void gv::ControllerClient::report_stop()
{
    gv::Controller::get().report_stop(this);
}

gv::PowerReport *gv::ControllerClient::report_get()
{
    return gv::Controller::get().report_get(this);
}

int64_t gv::ControllerClient::step(int64_t duration)
{
    this->logger.info("Step (duration: %lld)\n", duration);
    int64_t time;
    if (this->async)
    {
        gv::Controller::get().lock();
        time = gv::Controller::get().step_async(duration, this, NULL);
        gv::Controller::get().unlock();
    }
    else
    {
        time = gv::Controller::get().step_sync(duration, this);
    }
    return time;
}

void gv::ControllerClient::step_request(int64_t duration, void *data)
{
    this->logger.info("Step (duration: %lld)\n", duration);
    if (this->async)
    {
        gv::Controller::get().lock();
        gv::Controller::get().step_async(duration, this, data);
        gv::Controller::get().unlock();
    }
    else
    {
        gv::Controller::get().step_sync(duration, this);
    }
}


int64_t gv::ControllerClient::step_until(int64_t timestamp)
{
    this->logger.info("Step until (timestamp: %lld)\n", timestamp);
    int64_t time;
    if (this->async)
    {
        gv::Controller::get().lock();
        time = gv::Controller::get().step_until_async(timestamp, this, NULL);
        gv::Controller::get().unlock();
    }
    else
    {
        time = gv::Controller::get().step_until_sync(timestamp, this);
    }
    return time;
}

int64_t gv::ControllerClient::step_and_wait(int64_t duration)
{
    this->logger.info("Step and wait (duration: %lld)\n", duration);
    int64_t time;
    if (this->async)
    {
        gv::Controller::get().lock();
        time = gv::Controller::get().step_and_wait_async(duration, this);
        gv::Controller::get().unlock();
    }
    else
    {
        time = gv::Controller::get().step_sync(duration, this);
    }
    return time;
}

int64_t gv::ControllerClient::step_until_and_wait(int64_t timestamp)
{
    this->logger.info("Step until and wait (timestamp: %lld)\n", timestamp);
    int64_t time;
    if (this->async)
    {
        gv::Controller::get().lock();
        time = gv::Controller::get().step_until_and_wait_async(timestamp, this);
        gv::Controller::get().unlock();
    }
    else
    {
        time = gv::Controller::get().step_until_sync(timestamp, this);
    }
    return time;
}

int gv::ControllerClient::join()
{
    this->logger.info("Join\n");
    int status;
    if (this->async)
    {
        gv::Controller::get().lock();
        status = gv::Controller::get().join(this);
        gv::Controller::get().unlock();
    }
    else
    {
        status = gv::Controller::get().join(this);
    }
    return status;
}

void gv::ControllerClient::retain()
{
}

void gv::ControllerClient::wait_runnable()
{
    gv::Controller::get().wait_runnable();
}

void gv::ControllerClient::release()
{
}

void gv::ControllerClient::lock()
{
    this->logger.info("Lock\n");
    gv::Controller::get().engine_lock();
}

void gv::ControllerClient::unlock()
{
    this->logger.info("Unlock\n");
    gv::Controller::get().engine_unlock();
}

void gv::ControllerClient::update(int64_t timestamp)
{
    this->logger.info("Update (timestamp: %lld)\n", timestamp);
    gv::Controller::get().update(timestamp, this);
}

gv::Io_binding *gv::ControllerClient::io_bind(gv::Io_user *user,
    std::string comp_name, std::string itf_name)
{
    this->logger.info("IO bind (user: %p, comp: %s, itf: %s)\n", user, comp_name.c_str(),
        itf_name.c_str());
    return gv::Controller::get().io_bind(user, comp_name, itf_name, this);
}

gv::Wire_binding *gv::ControllerClient::wire_bind(gv::Wire_user *user,
    std::string comp_name, std::string itf_name)
{
    this->logger.info("Wire bind (user: %p, comp: %s, itf: %s)\n", user, comp_name.c_str(),
            itf_name.c_str());
    return gv::Controller::get().wire_bind(user, comp_name, itf_name, this);
}

void gv::ControllerClient::vcd_bind(gv::Vcd_user *user)
{
    this->logger.info("VCD bind (user: %p)\n", user);
    gv::Controller::get().vcd_bind(user, this);
}

void gv::ControllerClient::vcd_enable()
{
    this->logger.info("VCD enable\n");
    gv::Controller::get().vcd_enable(this);
}

void gv::ControllerClient::vcd_disable()
{
    this->logger.info("VCD disable\n");
    gv::Controller::get().vcd_disable(this);
}

void gv::ControllerClient::event_add(std::string path, bool is_regex)
{
    this->logger.info("Event add (path: %s, is_regex: %d)\n", path.c_str(), is_regex);
    gv::Controller::get().event_add(path, is_regex, this);
}

void gv::ControllerClient::event_exclude(std::string path, bool is_regex)
{
    this->logger.info("Event Exclude (path: %s, is_regex: %d)\n", path.c_str(), is_regex);
    gv::Controller::get().event_exclude(path, is_regex, this);
}

void *gv::ControllerClient::get_component(std::string path)
{
    return gv::Controller::get().get_component(path, this);
}

void gv::ControllerClient::terminate()
{
    this->logger.info("Forcing simulation termination\n");
    gv::Controller::get().sim_finished(0);
}

void gv::ControllerClient::quit(int status)
{
    if (this->async)
    {
        gv::Controller::get().engine_lock();
        this->has_quit = true;
        this->status = status;
        gv::Controller::get().client_quit(this);
        gv::Controller::get().engine_unlock();
    }
    else
    {
        this->has_quit = true;
        this->status = status;
        gv::Controller::get().client_quit(this);
    }
}
