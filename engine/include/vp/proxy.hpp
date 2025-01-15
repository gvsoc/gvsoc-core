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
#include <vp/controller.hpp>

namespace gv {

class GvProxy;

class GvProxySession : public gv::Gvsoc_user
{
public:
    GvProxySession(GvProxy *proxy, int req_fd, int reply_fd);
    void wait();
    void has_ended(int status) override;
    void handle_step_end(void *data) override;
    void handle_syscall_stop() override;

private:
    void proxy_loop();

    Logger logger;
    GvProxy *proxy;
    std::thread *loop_thread;
    int socket_fd;
    int reply_fd;
    Gvsoc *gvsoc;
};

class GvProxy
{
public:
    GvProxy(vp::TimeEngine *engine, vp::Component *top, int req_pipe=-1, int reply_pipe=-1);
    int open(int port, int *out_port);
    void send_quit(int status);
    int join();
    bool send_payload(FILE *reply_file, std::string req, uint8_t *payload, int size);
    void wait_connected();

    // Use to notify to loop thread to exit
    std::mutex mutex;
    // Set to true qhen proxy side has finished, which means engine can exit
    bool has_finished = false;
    // Exit status sent by proxy to be returned to main
    int exit_status;
    std::condition_variable cond;
    vp::Component *top;


  private:
    void send_reply(std::string msg);

    void listener(void);

    int telnet_socket;
    int socket_port;

    std::thread *listener_thread;

    std::vector<int> sockets;

    Logger logger;
    std::vector<GvProxySession *> sessions;
    int req_pipe;
    int reply_pipe;
};
}

extern gv::GvProxy *proxy;


#endif
