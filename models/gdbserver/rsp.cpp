/*
 * Copyright (C) 2018 ETH Zurich and University of Bologna and GreenWaves Technologies SA
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

#include "rsp.hpp"
#include "rsp.hpp"
#include "gdbserver.hpp"
#include <sys/socket.h>
#include <sys/prctl.h>
#include <netinet/in.h>
#include <unistd.h>

#define RSP_PACKET_MAX_SIZE (16*1024)
#define REPLY_BUF_LEN 256


#define UNAVAILABLE10 "00000000" \
                      "00000000" \
                      "00000000" \
                      "00000000" \
                      "00000000" \
                      "00000000" \
                      "00000000" \
                      "00000000" \
                      "00000000" \
                      "00000000"
#define UNAVAILABLE33 UNAVAILABLE10 UNAVAILABLE10 UNAVAILABLE10 "00000000" \
                                                                "00000000" \
                                                                "00000000"


Rsp::Rsp(Gdb_server *top) : top(top)
{

}


void Rsp::proxy_loop(int socket)
{
    this->top->trace.msg(vp::trace::LEVEL_INFO, "Entering proxy loop\n");

    this->sock = socket;

    CircularCharBuffer *in_buffer = new CircularCharBuffer(RSP_PACKET_MAX_SIZE);
    this->out_buffer = new CircularCharBuffer(RSP_PACKET_MAX_SIZE);

    // Packet codec
    this->codec = new RspPacketCodec();

    this->codec->on_ack([this]() {
        this->top->trace.msg(vp::trace::LEVEL_TRACE, "RSP: received ack\n");
        // TODO - Should timeout on no ACK
    });

    this->codec->on_error([this](const char *err_str) 
    {
        this->top->trace.msg(vp::trace::LEVEL_ERROR, "RSP: packet error: %s\n", err_str);
        this->stop();
    });

    this->codec->on_packet([this](char *pkt, size_t pkt_len)
    {
        this->codec->encode_ack(this->out_buffer);

        if (!this->decode(pkt, pkt_len))
        {
            this->stop();
        }
    });

    this->codec->on_ctrlc([this]()
    {
        this->stop_all_cores();
    });

    this->stop_all_cores();

    while(1)
    {
        char * buf;
        size_t len;
        in_buffer->write_block((void**)&buf, &len);

        int ret = ::recv(socket, buf, len, 0);

        if (this->proxy_loop_stop)
        {
            return;
        }

        if((ret == -1 && errno != EWOULDBLOCK) || (ret == 0))
        {
            this->top->trace.msg(vp::trace::LEVEL_WARNING, "Error receiving, leaving proxy loop\n");
            return;
        }

        if(ret == -1 && errno == EWOULDBLOCK)
        {
            // no data available
            continue;
        }

        in_buffer->commit_write(ret);

        this->top->get_time_engine()->lock();
        this->codec->decode(in_buffer);
        this->top->get_time_engine()->unlock();
    }
}

void Rsp::stop()
{
    this->proxy_loop_stop = true;
    close(this->client_socket);
    this->loop_thread = NULL;
}


void Rsp::stop_all_cores()
{
    this->top->get_time_engine()->lock();
    this->stop_all_cores_safe();
    this->top->get_time_engine()->unlock();
}

void Rsp::stop_all_cores_safe()
{
    for (auto core: this->top->get_cores())
    {
        if (core->gdbserver_state() == vp::Gdbserver_core::state::running)
        {
            core->gdbserver_stop();
        }
    }
}

bool Rsp::send(const char *data, size_t len)
{
    std::unique_lock<std::mutex> lock(this->mutex);

    this->top->trace.msg(vp::trace::LEVEL_TRACE, "Sending message (text: \"%s\", size: %ld)\n", data, len);

    // disabled run length encoding since GDB didn't seem to like it
    this->codec->encode(data, len, this->out_buffer, true);

    void *buf;
    size_t size;

    this->out_buffer->read_block(&buf, &size);

    if (::send(this->sock, buf, size, 0) != (int)size)
    {
        this->top->trace.msg(vp::trace::LEVEL_INFO, "Unable to send data to client\n");
        return false;
    }

    this->out_buffer->commit_read(size);

    lock.unlock();

    return true;
}


bool Rsp::send_str(const char *data)
{
    return this->send(data, strlen(data));
}


bool Rsp::signal_from_core(vp::Gdbserver_core *core, int signal, std::string reason, uint64_t info)
{
    char str[128];
    int len;
    len = snprintf(str, 128, "T%02xthread:%x;", signal, core->gdbserver_get_id()+1);
    if (reason != "")
    {
        this->top->set_active_core(core->gdbserver_get_id());
        this->top->set_active_core_for_other(core->gdbserver_get_id());
        len += snprintf(str + len, 128 - len, "%s:%x;", reason.c_str(), info);
    }
    if (!this->send(str, len))
    {
        return false;
    }

    this->stop_all_cores_safe();

    return true;
}

bool Rsp::send_exit(int status)
{
    char str[128];
    int len;
    len = snprintf(str, 128, "W%02x", (uint32_t)status);
    return send(str, len);
}

bool Rsp::signal(int signal)
{
    char str[128];
    int len;

    auto core = this->top->get_core();
    int state = core->gdbserver_state();
    this->active_core_for_other = core->gdbserver_get_id();

    if (signal == -1)
    {
        if (state == vp::Gdbserver_core::state::running)
        {
            signal = vp::Gdbserver_engine::SIGNAL_NONE;
        }
        else
        {
            signal = vp::Gdbserver_engine::SIGNAL_STOP;
        }
    }

    len = snprintf(str, 128, "S%02x", signal);

    return send(str, len);
}


bool Rsp::reg_write(char *data, size_t)
{
    uint32_t addr;
    uint32_t wdata;

    if (sscanf(data, "%x=%08x", &addr, &wdata) != 2)
    {
        this->top->trace.msg(vp::trace::LEVEL_ERROR, "Could not parse packet\n");
        return false;
    }

    wdata = ntohl(wdata);
    auto core = this->top->get_core(this->active_core_for_other);

    if (core->gdbserver_reg_set(addr, (uint8_t *)&wdata))
    {
        return send_str("E01");
    }
    else
    {
        return send_str("OK");
    }
}


bool Rsp::reg_read(char *data, size_t)
{
    uint32_t addr;
    uint32_t rdata = 0;
    char data_str[10];

    if (sscanf(data, "%x", &addr) != 1)
    {
        fprintf(stderr, "Could not parse packet\n");
        return false;
    }

    auto core = this->top->get_core(this->active_core_for_other);

    if (core->gdbserver_reg_get(addr, (uint8_t *)&rdata))
    {
        return send_str("E01");
    }
    else
    {
        rdata = htonl(rdata);
        snprintf(data_str, 9, "%08x", rdata);
        return send_str(data_str);
    }
}

bool Rsp::mem_write(char *data, size_t len)
{
    uint32_t addr;
    unsigned int length;
    size_t i;

    if (sscanf(data, "%x,%x:", &addr, &length) != 2)
    {
        this->top->trace.msg(vp::trace::LEVEL_INFO, "Could not parse packet\n");
        return false;
    }

    for (i = 0; i < len; i++)
    {
        if (data[i] == ':')
        {
            break;
        }
    }

    if (i == len)
        return false;

    // align to hex data
    data = &data[i + 1];
    len = len - i - 1;

    if (this->top->io_access(addr, len,(uint8_t *)data, true))
    {
        return send_str("E03");
    }

    return send_str("OK");
}


bool Rsp::mem_write_ascii(char *data, size_t len)
{
    uint32_t addr;
    int length;
    uint32_t wdata;
    size_t i, j;

    char *buffer;
    int buffer_len;

    if (sscanf(data, "%x,%d:", &addr, &length) != 2)
    {
        this->top->trace.msg(vp::trace::LEVEL_ERROR, "Could not parse packet\n");
        return false;
    }

    for (i = 0; i < len; i++)
    {
        if (data[i] == ':')
        {
            break;
        }
    }

    if (i == len)
        return false;

    // align to hex data
    data = &data[i + 1];
    len = len - i - 1;

    buffer_len = len / 2;
    buffer = (char *)malloc(buffer_len);
    if (buffer == NULL)
    {
        this->top->trace.msg(vp::trace::LEVEL_ERROR, "Failed to allocate buffer\n");
        return false;
    }

    for (j = 0; j < len / 2; j++)
    {
        wdata = 0;
        for (i = 0; i < 2; i++)
        {
            char c = data[j * 2 + i];
            uint32_t hex = 0;
            if (c >= '0' && c <= '9')
                hex = c - '0';
            else if (c >= 'a' && c <= 'f')
                hex = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F')
                hex = c - 'A' + 10;

            wdata |= hex << (4 * i);
        }

        buffer[j] = wdata;
    }

    if (this->top->io_access(addr, buffer_len, (uint8_t *)buffer, true))
    {
        return send_str("E03");
    }

    free(buffer);

    return send_str("OK");
}

bool Rsp::multithread(char *data, size_t len)
{
    int thread_id;

    if (len < 1)
        return false;

    switch (data[0])
    {
    case 'c':
    case 'g':
        if (sscanf(&data[1], "%x", &thread_id) != 1)
            return false;

        if (thread_id == -1) // affects all threads
            return send_str("OK");

        if (thread_id != 0)
            thread_id = thread_id - 1;
        else
            thread_id = this->top->default_hartid;

        if (data[0] == 'c')
        {
            if (this->top->set_active_core(thread_id))
            {
                return send_str("E01");
            }
        }
        else
        {
            if (this->top->set_active_core_for_other(thread_id))
            {
                return send_str("E01");
            }
            this->active_core_for_other = thread_id;
        }

        return send_str("OK");
    }

    return false;
}



bool Rsp::query(char* data, size_t len)
{
    int ret;
    char reply[REPLY_BUF_LEN];

    if (strncmp ("qSupported", data, strlen("qSupported")) == 0)
    {
        snprintf(reply, REPLY_BUF_LEN, "PacketSize=%x;vContSupported+;hwbreak+;QNonStop-", RSP_PACKET_MAX_SIZE);
        return send_str(reply);
    }
    else if (strncmp ("qTStatus", data, strlen ("qTStatus")) == 0)
    {
        // not supported, send empty packet
        return send_str("");
    }
    else if (strncmp ("qfThreadInfo", data, strlen ("qfThreadInfo")) == 0)
    {
        reply[0] = 'm';
        ret = 1;
        for (auto &core : this->top->get_cores())
        {
            ret += snprintf(&reply[ret], REPLY_BUF_LEN - ret, "%x,", core->gdbserver_get_id() + 1);
        }

        return send(reply, ret-1);
    }
    else if (strncmp ("qsThreadInfo", data, strlen ("qsThreadInfo")) == 0)
    {
        return send_str("l");
    }
    else if (strncmp ("qAttached", data, strlen ("qAttached")) == 0)
    {
        return send_str("1");
    }
    else if (strncmp ("qC", data, strlen ("qC")) == 0)
    {
        return send_str("");
        snprintf(reply, 64, "QC %d", this->top->get_core()->gdbserver_get_id() + 1);
        return send_str(reply);
    }
    else if (strncmp ("qOffsets", data, strlen ("qOffsets")) == 0)
    {
        return send_str("Text=0;Data=0;Bss=0");
    }
    else if (strncmp ("qSymbol", data, strlen ("qSymbol")) == 0)
    {
        return send_str("OK");
    }
    else if (strncmp ("qThreadExtraInfo", data, strlen ("qThreadExtraInfo")) == 0)
    {
        const char* str_default = "Unknown Core";
        char str[REPLY_BUF_LEN];
        unsigned int thread_id;
        if (sscanf(data, "qThreadExtraInfo,%x", &thread_id) != 1)
        {
            this->top->trace.msg(vp::trace::LEVEL_WARNING, "Could not parse qThreadExtraInfo packet\n");
            return send_str("E01");
        }
        auto core = this->top->get_core(thread_id - 1);

        if (core != NULL)
        {
            std::string name = core->gdbserver_get_name();
            strcpy(str, name.c_str());
        }
        else
            strcpy(str, str_default);

        ret = 0;
        for(size_t i = 0; i < strlen(str); i++)
            ret += snprintf(&reply[ret], REPLY_BUF_LEN - ret, "%02X", str[i]);

        return send(reply, ret);
    }
    else if (strncmp ("qT", data, strlen ("qT")) == 0)
    {
        // not supported, send empty packet
        return send_str("");
    }

    this->top->trace.msg(vp::trace::LEVEL_ERROR, "Unknown query packet\n");

    return send_str("");
}


bool Rsp::v_packet(char* data, size_t len)
{
    if (strncmp ("vCont?", data, std::min(strlen ("vCont?"), len)) == 0)
    {
        return send_str("vCont;c;s;C;S");
    }
    else if (strncmp ("vCont", data, std::min(strlen ("vCont"), len)) == 0)
    {
        // vCont can contains several commands, handle them in sequence
        char *str = strtok(&data[6], ";");
        bool thread_is_started=false;
        int stepped_tid = -1;
        while(str != NULL)
        {
            // Extract command and thread ID
            int tid = -1;
            char *delim = strchr(str, ':');
            if (delim != NULL)
            {
                tid = atoi(delim+1);
                if (tid != 0)
                {
                    this->top->set_active_core(tid - 1);
                }
                *delim = 0;
            }

            if (str[0] == 'C' || str[0] == 'c')
            {
                for (auto core: this->top->get_cores())
                {
                    core->gdbserver_cont();
                }
            }
            else if (str[0] == 'S' || str[0] == 's')
            {
                this->top->get_core()->gdbserver_stepi();
            }
            else
            {
                this->top->trace.msg(vp::trace::LEVEL_ERROR, "Unsupported command in vCont packet: %s\n", str);
            }

            str = strtok(NULL, ";");
        }

        bool result = this->send_str("OK");

        return result;
    }

    return this->send_str("");
}


bool Rsp::regs_send()
{
    int nb_regs, reg_size;
    auto core = this->top->get_core(this->active_core_for_other);
    core->gdbserver_regs_get(&nb_regs, &reg_size, NULL);

    if (reg_size == 4)
    {
        uint32_t regs[nb_regs];
        char regs_str[512];

        core->gdbserver_regs_get(NULL, NULL, (uint8_t *)regs);

        for (int i=0; i<nb_regs; i++)
        {
            snprintf(&regs_str[i * 8], 9, "%08x", (unsigned int)htonl(regs[i]));

        }

        return this->send_str(regs_str);
    }
    else
    {
        this->top->trace.msg(vp::trace::LEVEL_ERROR, "Unsupported register size (size: %d)\n", reg_size);
    }

    return this->send_str(UNAVAILABLE33);
}


bool Rsp::decode(char* data, size_t len)
{
    this->top->trace.msg(vp::trace::LEVEL_TRACE, "Received packet (text: '%s', len: %ld)\n", data, len);

    switch (data[0])
    {
        case 'q':
            return this->query(data, len);

        case 'v':
            return this->v_packet(data, len);

        case 'H':
            return this->multithread(&data[1], len-1);

        case '?':
            return this->signal();

        case 'g':
            return this->regs_send();

        case 'X':
            return this->mem_write(&data[1], len-1);

        case 'p':
            return this->reg_read(&data[1], len-1);

        case 'P':
            return this->reg_write(&data[1], len-1);

        case 'c':
        case 'C': {
            auto core = this->top->get_core();
            int ret = core->gdbserver_cont();
            return ret;
        }

        case 'm':
            return this->mem_read(&data[1], len-1);

        case 'z':
            return this->bp_remove(&data[0], len);

        case 'Z':
            return this->bp_insert(&data[0], len);

        case 's':
        case 'S':
            this->top->get_core()->gdbserver_stepi();
            return this->send_str("OK");

        case 'D':
            this->top->get_core()->gdbserver_cont();
            this->send_str("OK");
            return false;

        case 'T':
            return send_str("OK"); // threads are always alive

        case '!':
            return send_str(""); // extended mode not supported

        case 'M':
            return mem_write_ascii(&data[1], len-1);

        default:
            this->top->trace.msg(vp::trace::LEVEL_ERROR, "Unknown packet: starts with %c\n", data[0]);
            break;
    }

    return false;
}


void Rsp::proxy_listener()
{
    int client;

    fprintf(stderr, "GDB server listening on port %d\n", this->port);

    while(1)
    {
        if((client = accept(this->proxy_socket_in, NULL, NULL)) == -1)
        {
            if(errno == EAGAIN)
                continue;

            this->top->trace.msg(vp::trace::LEVEL_ERROR, "Unable to accept connection: %s\n", strerror(errno));
            return;
        }

        if (this->loop_thread)
        {
            this->top->trace.msg(vp::trace::LEVEL_WARNING, "Client already connected\n");
        }
        else
        {
            this->top->trace.msg(vp::trace::LEVEL_INFO, "Client connected\n");

            this->proxy_loop_stop = false;
            this->client_socket = client;
            this->loop_thread = new std::thread(&Rsp::proxy_loop, this, client);
        }
    }

    return;
}


int Rsp::open_proxy(int port)
{
    struct sockaddr_in addr;
    int yes = 1;
    int first_port = port;

    while (port < first_port + 10000)
    {
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = INADDR_ANY;
        memset(addr.sin_zero, '\0', sizeof(addr.sin_zero));

        this->proxy_socket_in = socket(PF_INET, SOCK_STREAM, 0);
        if(this->proxy_socket_in < 0)
        {
            port++;
            continue;
        }

        if(setsockopt(this->proxy_socket_in, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
        {
            port++;
            continue;
        }

        if(bind(this->proxy_socket_in, (struct sockaddr *)&addr, sizeof(addr)) == -1)
        {
            port++;
            continue;
        }

        if(listen(this->proxy_socket_in, 1) == -1)
        {
            port++;
            continue;
        }

        break;
    };

    if (port == first_port + 10000)
    {
        this->top->trace.msg(vp::trace::LEVEL_ERROR, "Didn't manage to open socket\n");
        return -1;
    }

    this->port = port;
    this->listener_thread = new std::thread(&Rsp::proxy_listener, this);

    return 0;
}


void Rsp::start(int port)
{
    if (this->open_proxy(port))
    {
        this->top->trace.msg(vp::trace::LEVEL_ERROR, "FAILED to open proxy\n");
    }
}

bool Rsp::mem_read(char *data, size_t)
{
    unsigned char buffer[512];
    char reply[1024];
    uint32_t addr;
    uint32_t length;
    uint32_t i;

    if (sscanf(data, "%x,%x", &addr, &length) != 2)
    {
        this->top->trace.msg(vp::trace::LEVEL_ERROR, "Could not parse packet\n");
        return false;
    }

    if (length >= 512)
    {
        return send_str("E01");
    }

    if (1) //m_top->target->check_mem_access(addr, length))
    {
        if (this->top->io_access(addr, length, (uint8_t *)buffer, false))
        {
            return send_str("E03");
        }

        for (i = 0; i < length; i++)
        {
            snprintf(&reply[i * 2], 3, "%02x", (uint32_t)buffer[i]);
        }
    }
    else
    {
        this->top->trace.msg(vp::trace::LEVEL_ERROR, "Filtered memory read attempt - area is inaccessible\n");
        memset(reply, (int)'0', length * 2);
        reply[length * 2] = '\0';
    }

    return send(reply, length * 2);
}


bool Rsp::bp_insert(char *data, size_t len)
{
    int type;
    uint32_t addr;
    int bp_len;

    if (len < 1)
        return false;

    if (3 != sscanf(data, "Z%1d,%x,%1d", (int *)&type, &addr, &bp_len))
    {
        this->top->trace.msg(vp::trace::LEVEL_ERROR, "Could not get three arguments\n");
        return send_str("E01");
    }

    if (type == 0 || type == 1)
    {
        this->top->trace.msg(vp::trace::LEVEL_DEBUG, "Inserting breakpoint (addr: 0x%x)\n", addr);
        this->top->breakpoint_insert(addr);
    }
    else if ( type == 2 || type == 3)
    {
        this->top->trace.msg(vp::trace::LEVEL_DEBUG, "Inserting watchpoint (addr: 0x%x)\n", addr);
        this->top->watchpoint_insert(type == 2, addr, bp_len);
    }

    return send_str("OK");
}

bool Rsp::bp_remove(char *data, size_t len)
{
    int type;
    uint32_t addr;
    int bp_len;

    data[len] = 0;

    if (3 != sscanf(data, "z%1d,%x,%1d", (int *)&type, &addr, &bp_len))
    {
        this->top->trace.msg(vp::trace::LEVEL_ERROR, "Could not get three arguments\n");
        return false;
    }

    if (type == 0 || type == 1)
    {
        this->top->trace.msg(vp::trace::LEVEL_DEBUG, "Removing breakpoint (addr: 0x%x)\n", addr);
        this->top->breakpoint_remove(addr);
    }
    else if ( type == 2 || type == 3)
    {
        this->top->trace.msg(vp::trace::LEVEL_DEBUG, "Removing watchpoint (addr: 0x%x)\n", addr);
        this->top->watchpoint_remove(type == 2, addr, bp_len);
    }

    return send_str("OK");
}
