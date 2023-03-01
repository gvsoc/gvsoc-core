/*
 * Copyright (C) 2018 ETH Zurich and University of Bologna
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
 * Authors: Andreas Traber
 */

#ifndef __GDB_SERVER_RSP_HPP__
#define __GDB_SERVER_RSP_HPP__

#include <thread>
#include <mutex>
#include "rsp-packet-codec.hpp"
#include "gdbserver.hpp"


class Gdb_server;


class Rsp {
public:
    Rsp(Gdb_server *top);
    void start(int port);
    bool signal(int signal=-1);
    bool signal_from_core(vp::Gdbserver_core *core, int signal, std::string reason="", int info=0);
    bool send_exit(int status);

private:
    void proxy_listener();
    void proxy_loop(int sock);
    int open_proxy(int port);
    bool decode(char* data, size_t len);
    bool send(const char *data, size_t len);
    bool send_str(const char *data);
    bool query(char* data, size_t len);
    bool v_packet(char* data, size_t len);
    bool multithread(char *data, size_t len);
    bool regs_send();
    bool mem_write(char *data, size_t len);
    bool mem_write_ascii(char *data, size_t len);
    bool reg_read(char *data, size_t);
    bool reg_write(char *data, size_t);
    bool mem_read(char *data, size_t);
    bool bp_insert(char *data, size_t len);
    bool bp_remove(char *data, size_t len);
    void stop();
    void stop_all_cores();
    void stop_all_cores_safe();

    Gdb_server *top;
    int sock;
    int proxy_socket_in;
    int port;
    std::thread *listener_thread;
    std::thread *loop_thread = NULL;
    RspPacketCodec *codec;
    CircularCharBuffer *out_buffer;
    std::mutex mutex;
    bool proxy_loop_stop;
    int client_socket;
};

#endif