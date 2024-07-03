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
#include <vp/proxy.hpp>
#include <vp/launcher.hpp>
#include "vp/top.hpp"


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


void gv::GvProxy::notify_stop(int64_t time)
{
    this->send_reply("req=-1;msg=stopped=" + std::to_string(time) + '\n');
}

void gv::GvProxy::notify_run(int64_t time)
{
    this->send_reply("req=-1;msg=running=" + std::to_string(time) + '\n');
}


void gv::GvProxy::send_reply(std::string msg)
{
    std::unique_lock<std::mutex> lock(this->mutex);
    for (auto x: this->sockets)
    {
        dprintf(x, "%s", msg.c_str());
    }
    lock.unlock();
}


bool gv::GvProxy::send_payload(FILE *reply_file, std::string req, uint8_t *payload, int size)
{
    fprintf(reply_file, "req=%s;payload=%d\n", req.c_str(), size);
    if (size > 0)
    {
        int write_size = fwrite(payload, 1, size, reply_file);
        fflush(reply_file);
        return write_size != size;
    }
    else
    {
        fflush(reply_file);
        return false;
    }
}


void gv::GvProxy::proxy_loop(int socket_fd, int reply_fd)
{
    FILE *sock = fdopen(socket_fd, "r");
    FILE *reply_sock = fdopen(reply_fd, "w");
    gv::GvsocLauncher *launcher = this->launcher;
    vp::TimeEngine *engine = launcher->top_get()->get_time_engine();

    if (!this->is_async)
    {
        engine->lock();
    }

    while(1)
    {
        char line_array[1024];

        if (!this->is_async)
        {
            engine->unlock();
        }

        if (!fgets(line_array, 1024, sock))
        {
            if (!this->is_async)
            {
                engine->lock();
                launcher->release();
                engine->critical_notify();
                engine->unlock();
            }
            return ;
        }

        if (!this->is_async)
        {
            engine->lock();
        }

        std::string line = std::string(line_array);

        int start = 0;
        int end = line.find(";");
        std::vector<std::string> tokens;
        while (end != -1) {
            tokens.push_back(line.substr(start, end - start));
            start = end + 1;
            end = line.find(";", start);
        }
        tokens.push_back(line.substr(start, end - start));

        std::string req = "";
        std::string cmd = "";

        for (auto x: tokens)
        {
            int start = 0;
            int index = x.find("=");
            std::string name = x.substr(start, index - start);
            std::string value = x.substr(index + 1, x.size());

            if (name == "req")
            {
                req = value;
            }
            else if (name == "cmd")
            {
                cmd = value;
            }
        }

        std::regex regex{R"([\s]+)"};
        std::sregex_token_iterator it{cmd.begin(), cmd.end(), regex, -1};
        std::vector<std::string> words{it, {}};

        if (words.size() > 0)
        {
            if (words[0] == "release")
            {
                if (!this->is_async)
                {
                    launcher->release();
                    engine->critical_notify();
                }
                std::unique_lock<std::mutex> lock(this->mutex);
                dprintf(reply_fd, "req=%s\n", req.c_str());
                lock.unlock();
            }
            else if (words[0] == "retain")
            {
                if (!this->is_async)
                {
                    launcher->retain();
                }
                std::unique_lock<std::mutex> lock(this->mutex);
                dprintf(reply_fd, "req=%s\n", req.c_str());
                lock.unlock();
            }
            else if (words[0] == "run")
            {
                launcher->run_internal();
                std::unique_lock<std::mutex> lock(this->mutex);
                dprintf(reply_fd, "req=%s\n", req.c_str());
                lock.unlock();
            }
            else if (words[0] == "step")
            {
                if (words.size() != 2)
                {
                    fprintf(stderr, "This command requires 1 argument: step timestamp");
                }
                else
                {
                    int64_t duration = strtol(words[1].c_str(), NULL, 0);
                    int64_t timestamp = engine->get_time() + duration;
                    launcher->step_internal(duration);
                    std::unique_lock<std::mutex> lock(this->mutex);
                    dprintf(reply_fd, "req=%s;msg=%ld\n", req.c_str(), timestamp);
                    lock.unlock();
                }
            }
            else if (words[0] == "stop")
            {
                launcher->stop();
                std::unique_lock<std::mutex> lock(this->mutex);
                dprintf(reply_fd, "req=%s\n", req.c_str());
                lock.unlock();
            }
            else if (words[0] == "quit")
            {
                if (this->is_async)
                {
                    engine->lock();
                }
                engine->quit(strtol(words[1].c_str(), NULL, 0));
                if (this->is_async)
                {
                    engine->unlock();
                }
                std::unique_lock<std::mutex> lock(this->mutex);
                dprintf(reply_fd, "req=%s;msg=quit\n", req.c_str());
                lock.unlock();
            }
            else
            {
                // Before interacting with the engine, we must lock it since our requests will come
                // from a different thread.
                if (this->is_async)
                {
                    engine->lock();
                }

                if (words[0] == "get_component")
                {
                    vp::Block *comp = this->top->get_block_from_path(split(words[1], '/'));
                    std::unique_lock<std::mutex> lock(this->mutex);
                    if (comp)
                    {
                        dprintf(reply_fd, "req=%s;msg=%p\n", req.c_str(), comp);
                    }
                    else
                    {
                        dprintf(reply_fd, "req=%s;msg=0x0\n", req.c_str());
                    }
                    lock.unlock();
                }
                else if (words[0] == "component")
                {
                    vp::Component *comp = (vp::Component *)strtoll(words[1].c_str(), NULL, 0);
                    std::string retval = comp->handle_command(this, sock, reply_sock, {words.begin() + 2, words.end()}, req);
                    std::unique_lock<std::mutex> lock(this->mutex);
                    fprintf(reply_sock, "req=%s;msg=%s\n", req.c_str(), retval.c_str());
                    fflush(reply_sock);
                    lock.unlock();
                }
                else if (words[0] == "trace")
                {
                    if (words.size() != 3)
                    {
                        fprintf(stderr, "This command requires 2 arguments: trace [add|remove] regexp");
                    }
                    else
                    {
                        if (words[1] == "add")
                        {
                            this->top->traces.get_trace_engine()->add_trace_path(0, words[2]);
                            this->top->traces.get_trace_engine()->check_traces();
                        }
                        else if (words[1] == "level")
                        {
                            this->top->traces.get_trace_engine()->set_trace_level(words[2].c_str());
                            this->top->traces.get_trace_engine()->check_traces();
                        }
                        else
                        {
                            this->top->traces.get_trace_engine()->add_exclude_trace_path(0, words[2]);
                            this->top->traces.get_trace_engine()->check_traces();
                        }
                        fprintf(reply_sock, "req=%s\n", req.c_str());
                        fflush(reply_sock);
                    }
                }
                else if (words[0] == "event")
                {
                    if (words.size() != 3)
                    {
                        fprintf(stderr, "This command requires 2 arguments: event [add|remove] regexp");
                    }
                    else
                    {
                        if (words[1] == "add")
                        {
                            // TODO regular expressions are too slow for the profiler, should be moved
                            // to a new command
                            //this->top->traces.get_trace_engine()->add_trace_path(1, words[2]);
                            //this->top->traces.get_trace_engine()->check_traces();
                            this->top->traces.get_trace_engine()->conf_trace(1, words[2], 1);
                        }
                        else
                        {
                            //this->top->traces.get_trace_engine()->add_exclude_trace_path(1, words[2]);
                            //this->top->traces.get_trace_engine()->check_traces();
                            this->top->traces.get_trace_engine()->conf_trace(1, words[2], 0);
                        }
                        fprintf(reply_sock, "req=%s\n", req.c_str());
                    }
                }
                else
                {
                    printf("Ignoring2 invalid command: %s\n", words[0].c_str());
                }
                if (this->is_async)
                {
                    engine->unlock();
                }
            }
        }
    }
}

gv::GvProxy::GvProxy(vp::TimeEngine *engine, vp::Component *top, gv::GvsocLauncher *launcher, bool is_async, int req_pipe, int reply_pipe)
  : top(top), launcher(launcher), req_pipe(req_pipe), reply_pipe(reply_pipe)
{
    this->is_async = is_async;
    launcher->register_exec_notifier(this);
    if (!this->is_async)
    {
        launcher->retain();
    }
}

void gv::GvProxy::listener(void)
{
    int client_fd;
    while(1)
    {
        if ((client_fd = accept(this->telnet_socket, NULL, NULL)) == -1)
        {
            if(errno == EAGAIN) continue;
            fprintf(stderr, "Failed to accept connection: %s\n", strerror(errno));
            return;
        }

        this->sockets.push_back(client_fd);
        this->loop_thread = new std::thread(&gv::GvProxy::proxy_loop, this, client_fd, client_fd);
    }
}



int gv::GvProxy::open(int port, int *out_port)
{
    if (this->req_pipe == -1)
    {
        struct sockaddr_in addr;
        int yes = 1;

        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;
        socklen_t size = sizeof(addr.sin_zero);
        memset(addr.sin_zero, '\0', sizeof(addr.sin_zero));

        this->telnet_socket = socket(PF_INET, SOCK_STREAM, 0);

        if (this->telnet_socket < 0)
        {
            fprintf(stderr, "Unable to create comm socket: %s\n", strerror(errno));
            return -1;
        }

        if (setsockopt(this->telnet_socket, SOL_SOCKET, SO_REUSEADDR, &yes,
                sizeof(int)) == -1) {
            fprintf(stderr, "Unable to setsockopt on the socket: %s\n", strerror(errno));
            return -1;
        }

        if (::bind(this->telnet_socket, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
            fprintf(stderr, "Unable to bind the socket: %s\n", strerror(errno));
            return -1;
        }

        if (listen(this->telnet_socket, 1) == -1) {
            fprintf(stderr, "Unable to listen: %s\n", strerror(errno));
            return -1;
        }

        getsockname(this->telnet_socket, (sockaddr*)&addr, &size);

        if (out_port)
        {
            *out_port = ntohs(addr.sin_port);
        }

        this->listener_thread = new std::thread(&gv::GvProxy::listener, this);
    }
    else
    {
        this->loop_thread = new std::thread(&gv::GvProxy::proxy_loop, this, this->req_pipe, this->reply_pipe);
    }

    return 0;
}



void gv::GvProxy::stop(int status)
{
    for (auto x: this->sockets)
    {
        dprintf(x, "req=-1;exit=%d\n", status);
        shutdown(x, SHUT_RDWR);
    }
}
