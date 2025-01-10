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


#include <sstream>
#include <string>
#include <stdio.h>
#include <vp/vp.hpp>
#include <stdio.h>
#include "string.h"
#include <sstream>
#include <string>
#include <dlfcn.h>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <poll.h>
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


gv::GvProxy::GvProxy(vp::TimeEngine *engine, vp::Component *top, gv::GvsocLauncher *launcher, int req_pipe, int reply_pipe)
  : top(top), launcher(launcher), req_pipe(req_pipe), reply_pipe(reply_pipe), logger("PROXY")
{
    this->logger.info("Instantiating proxy\n");
}

void gv::GvProxy::listener(void)
{
    int client_fd;
    while(1)
    {
        this->logger.info("Waiting for connection\n");

        if ((client_fd = accept(this->telnet_socket, NULL, NULL)) == -1)
        {
            if(errno == EAGAIN) continue;
            fprintf(stderr, "Failed to accept connection: %s\n", strerror(errno));
            return;
        }

        this->logger.info("Client connected, creating new session\n");

        this->sockets.push_back(client_fd);
        std::unique_lock<std::mutex> lock(this->mutex);
        this->sessions.push_back(new GvProxySession(this, client_fd, client_fd));
        this->cond.notify_all();
        lock.unlock();
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
        this->sessions.push_back(new GvProxySession(this, this->req_pipe, this->reply_pipe));
    }

    return 0;
}


void gv::GvProxy::wait_connected()
{
    std::unique_lock<std::mutex> lock(this->mutex);
    while(this->sessions.size() == 0)
    {
        this->cond.wait(lock);
    }
    lock.unlock();
}

int gv::GvProxy::join()
{
    // TODO not used anymore
    std::unique_lock<std::mutex> lock(this->mutex);
    while(!this->has_finished)
    {
        this->cond.wait(lock);
    }
    lock.unlock();

    for (auto x: this->sockets)
    {
        shutdown(x, SHUT_RDWR);
    }

    for (auto x: this->sessions)
    {
        x->wait();
    }

    return this->exit_status;
}

void gv::GvProxy::send_quit(int status)
{
    for (auto x: this->sockets)
    {
        dprintf(x, "req=-1;exit=%d\n", status);
    }
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

gv::GvProxySession::GvProxySession(GvProxy *proxy, int req_fd, int reply_fd)
: gv::GvsocLauncherClient(NULL, "PROXY_SESSION(" + std::to_string(req_fd) + ")"),
    logger("PROXY_SESSION(" + std::to_string(req_fd) + ")"), proxy(proxy),
    socket_fd(req_fd), reply_fd(reply_fd)
{
    this->retain();

    this->loop_thread = new std::thread(&gv::GvProxySession::proxy_loop, this);
}

void gv::GvProxySession::wait()
{
    this->loop_thread->join();
}

void gv::GvProxySession::proxy_loop()
{
    FILE *sock = fdopen(socket_fd, "r");
    FILE *reply_sock = fdopen(reply_fd, "w");
    gv::GvsocLauncher *launcher = this->proxy->launcher;
    vp::TimeEngine *engine = launcher->top_get()->get_time_engine();

    while(1)
    {
        std::string line;

        char buffer[1024];
        this->logger.info("Wait for command\n");
        while (fgets(buffer, sizeof(buffer), sock))
        {
            line += buffer;
            if (line.back() == '\n') {
                break;
            }
        }

        this->logger.info("Got command (cmd: %s)\n", buffer);

        // Check for errors or exit conditions
        if (line.empty())
        {
            return;
        }

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
                std::unique_lock<std::mutex> lock(this->proxy->mutex);
                dprintf(reply_fd, "req=%s\n", req.c_str());
                lock.unlock();
            }
            else if (words[0] == "retain")
            {
                std::unique_lock<std::mutex> lock(this->proxy->mutex);
                dprintf(reply_fd, "req=%s\n", req.c_str());
                lock.unlock();
            }
            else if (words[0] == "run")
            {
                this->run();
                std::unique_lock<std::mutex> lock(this->proxy->mutex);
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
                    this->step(duration);
                    std::unique_lock<std::mutex> lock(this->proxy->mutex);
                    dprintf(reply_fd, "req=%s;msg=%ld\n", req.c_str(), timestamp);
                    lock.unlock();
                }
            }
            else if (words[0] == "stop")
            {
                this->stop();
                std::unique_lock<std::mutex> lock(this->proxy->mutex);
                dprintf(reply_fd, "req=%s\n", req.c_str());
                lock.unlock();
            }
            else if (words[0] == "quit")
            {
                std::unique_lock<std::mutex> lock(this->proxy->mutex);

                this->proxy->exit_status = strtol(words[1].c_str(), NULL, 0);
                this->proxy->has_finished = true;
                this->proxy->cond.notify_all();

                dprintf(reply_fd, "req=%s;msg=quit\n", req.c_str());
                lock.unlock();

                this->quit(this->proxy->exit_status);
            }
            else
            {
                // Before interacting with the engine, we must lock it since our requests will come
                // from a different thread.
                this->lock();

                if (words[0] == "get_component")
                {
                    vp::Block *comp = this->proxy->top->get_block_from_path(split(words[1], '/'));
                    std::unique_lock<std::mutex> lock(this->proxy->mutex);
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
                    std::string retval = comp->handle_command(this->proxy, sock, reply_sock, {words.begin() + 2, words.end()}, req);
                    std::unique_lock<std::mutex> lock(this->proxy->mutex);
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
                            this->proxy->top->traces.get_trace_engine()->add_trace_path(0, words[2]);
                            this->proxy->top->traces.get_trace_engine()->check_traces();
                        }
                        else if (words[1] == "level")
                        {
                            this->proxy->top->traces.get_trace_engine()->set_trace_level(words[2].c_str());
                            this->proxy->top->traces.get_trace_engine()->check_traces();
                        }
                        else
                        {
                            this->proxy->top->traces.get_trace_engine()->add_exclude_trace_path(0, words[2]);
                            this->proxy->top->traces.get_trace_engine()->check_traces();
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
                            this->proxy->top->traces.get_trace_engine()->conf_trace(1, words[2], 1);
                        }
                        else
                        {
                            //this->top->traces.get_trace_engine()->add_exclude_trace_path(1, words[2]);
                            //this->top->traces.get_trace_engine()->check_traces();
                            this->proxy->top->traces.get_trace_engine()->conf_trace(1, words[2], 0);
                        }
                        fprintf(reply_sock, "req=%s\n", req.c_str());
                    }
                }
                else
                {
                    printf("Ignoring invalid command: %s\n", words[0].c_str());
                }

                this->unlock();
            }
        }
    }
}

void gv::GvProxySession::sim_finished(int status)
{
    this->proxy->send_quit(status);
}
