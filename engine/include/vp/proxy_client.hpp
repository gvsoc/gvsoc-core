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

#pragma once

#include <condition_variable>
#include <vp/vp.hpp>

class Gvsoc_proxy_client : public gv::Gvsoc
{
public:
    Gvsoc_proxy_client(gv::GvsocConf *conf);

    void open() override;
    void bind(gv::Gvsoc_user *user) override;
    void close() override;

    void run() override;

    void start() override;

    void flush() override;

    int64_t stop() override;

    double get_average_power(double &dynamic_power, double &static_power) override;
    double get_instant_power(double &dynamic_power, double &static_power) override;
    void report_start() override;
    void report_stop() override;
    gv::PowerReport *report_get() override;

    void wait_stopped() override;
    void update(int64_t timestamp) override;

    int64_t step(int64_t duration) override;
    int64_t step_until(int64_t timestamp) override;

    int join() override;

    gv::Io_binding *io_bind(gv::Io_user *user, std::string comp_name, std::string itf_name) override;
    gv::Wire_binding *wire_bind(gv::Wire_user *user, std::string comp_name, std::string itf_name) override;

    void vcd_bind(gv::Vcd_user *user) override;
    void vcd_enable() override;
    void vcd_disable() override;
    void event_add(std::string path, bool is_regex) override;
    void event_exclude(std::string path, bool is_regex) override;
    void *get_component(std::string path) override;
    std::string send_command(std::string command, bool keep_lock=false);
    int post_command(std::string command, bool keep_lock=false);
    void unlock_command();
    void send(uint8_t *buffer, int size);
    void receive(uint8_t *buffer, int size);
    std::string wait_reply(int req);
    int get_req();
    void register_payload_callback(int req, std::function<void(uint8_t *, int)> callback);
    void unregister_payload_callback(int req);


private:
    void reader_routine();

    gv::GvsocConf *conf;
    int proxy_socket;
    std::thread *reader_thread;
    std::mutex mutex;
    std::condition_variable cond;
    int current_req_id = 0;
    std::unordered_map<int, std::string> replies;
    bool running = false;
    int64_t timestamp = -1;
    bool has_exited = false;
    int retval;
    std::unordered_map<int, std::function<void(uint8_t *, int)>> payload_callbacks;
};
