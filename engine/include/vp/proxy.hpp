/*
 * Copyright (C) 2020 GreenWaves Technologies, SAS, ETH Zurich and University of Bologna
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


#ifndef __VP_PROXY_HPP__
#define __VP_PROXY_HPP__

#include <mutex>

class Gvsoc_launcher;

class Gv_proxy : vp::Notifier
{
  public:
    Gv_proxy(vp::time_engine *engine, vp::component *top, Gvsoc_launcher *launcher, int req_pipe=-1, int reply_pipe=-1);
    int open(int port, int *out_port);
    void stop(int status);
    void notify_stop(int64_t time);
    void notify_run(int64_t time);
    bool send_payload(FILE *reply_file, std::string req, uint8_t *payload, int size);
    
  private:
 

    void listener(void);
    void proxy_loop(int, int);
    void send_reply(std::string msg);
    
    int telnet_socket;
    int socket_port;
    
    std::thread *loop_thread;
    std::thread *listener_thread;

    std::vector<int> sockets;

    vp::component *top;
    int req_pipe;
    int reply_pipe;

    std::mutex mutex;
    Gvsoc_launcher *launcher;
};

extern Gv_proxy *proxy;


#endif