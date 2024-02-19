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


#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>

#include <vp/vp.hpp>
#include <gv/gvsoc.hpp>
#include <gv/testbench.hpp>
#include <vp/proxy.hpp>
#include <vp/proxy_client.hpp>



Gvsoc_proxy_client::Gvsoc_proxy_client(gv::GvsocConf *conf)
{
    this->conf = conf;
}

void Gvsoc_proxy_client::reader_routine()
{
    FILE *sock = fdopen(this->proxy_socket, "r");

    while(1)
    {
        char line_array[1024];

        if (!fgets(line_array, 1024, sock))
            return ;

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

        int req;
        int64_t is_stop=-1, is_run=-1;
        std::string msg = "";

        for (auto x: tokens)
        {
            int start = 0;
            int index = x.find("=");
            std::string name = x.substr(start, index - start);
            std::string value = x.substr(index + 1, x.size());

            if (name == "req")
            {
                req = strtol(value.c_str(), NULL, 0);
            }
            else if (name == "exit")
            {
                this->retval = std::stoi(value);
                this->has_exited = true;
            }
            else if (name == "msg")
            {
                msg = value;
                int pos = msg.find('=') + 1;
                if (msg.find("stopped") != -1)
                {
                    is_stop = std::stol(msg.substr(pos, msg.size()));
                }
                else if (msg.find("running") != -1)
                {
                    is_stop = std::stol(msg.substr(pos, msg.size()));
                }
            }
            else if (name == "payload")
            {
                if (this->payload_callbacks.count(req) > 0)
                {
                    int size = std::stoi(value);
                    uint8_t data[size];
                    if (fread(data, 1, size, sock) == size)
                    {
                        std::function<void(uint8_t *, int)> callback = this->payload_callbacks[req];
                        callback(data, size);
                    }
                }
            }
        }

        std::unique_lock<std::mutex> lock(this->mutex);

        if (is_stop != -1)
        {
            this->timestamp = is_stop;
            this->running = false;
        }
        else if (is_stop != -1)
        {
            this->running = true;
        }

        this->replies[req] = msg;
        this->cond.notify_all();
        lock.unlock();
    }


}

void Gvsoc_proxy_client::bind(gv::Gvsoc_user *user)
{
}

void Gvsoc_proxy_client::open()
{
    struct sockaddr_in addr;
    int yes = 1;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(this->conf->proxy_socket);
    addr.sin_addr.s_addr = INADDR_ANY;
    socklen_t size = sizeof(addr.sin_zero);
    memset(addr.sin_zero, '\0', sizeof(addr.sin_zero));

    this->proxy_socket = socket(PF_INET, SOCK_STREAM, 0);
    if (this->proxy_socket < 0)
    {
        throw std::runtime_error("Unable to create comm socket: " + std::string(strerror(errno)));
    }

    if (setsockopt(this->proxy_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
    {
        throw std::runtime_error("Unable to setsockopt on the socket: " + std::string(strerror(errno)));
    }

    if (connect(this->proxy_socket, (struct sockaddr*)&addr, sizeof(addr)) == -1)
    {
        throw std::runtime_error("Unable to setsockopt on the socket: " + std::string(strerror(errno)));
    }

    this->reader_thread = new std::thread(&Gvsoc_proxy_client::reader_routine, this);
}


std::string Gvsoc_proxy_client::wait_reply(int req)
{
    std::unique_lock<std::mutex> lock(this->mutex);

    while (this->replies.count(req) == 0)
    {
        this->cond.wait(lock);
    }

    std::string result = this->replies[req];
    this->replies.erase(req);

    lock.unlock();

    return result;
}

int Gvsoc_proxy_client::post_command(std::string command, bool keep_lock)
{
    this->mutex.lock();

    int req = this->current_req_id++;
    dprintf(this->proxy_socket, "req=%d;cmd=%s\n", req, command.c_str());

    if (!keep_lock)
    {
        this->mutex.unlock();
    }

    return req;
}

std::string Gvsoc_proxy_client::send_command(std::string command, bool keep_lock)
{
    int req = this->post_command(command, keep_lock);
    return this->wait_reply(req);
}

void Gvsoc_proxy_client::start()
{
}

void Gvsoc_proxy_client::close()
{
}

void Gvsoc_proxy_client::flush()
{
}

void Gvsoc_proxy_client::run()
{
    this->send_command("run");
}

int Gvsoc_proxy_client::join()
{
    std::unique_lock<std::mutex> lock(this->mutex);

    while (!this->has_exited)
    {
        this->cond.wait(lock);
    }

    lock.unlock();

    return this->retval;
}

int64_t Gvsoc_proxy_client::stop()
{
    return 0;
}

void Gvsoc_proxy_client::wait_stopped()
{
}

int64_t Gvsoc_proxy_client::step(int64_t duration)
{
    return 0;
}

int64_t Gvsoc_proxy_client::step_until(int64_t duration)
{
    return 0;
}

gv::Io_binding *Gvsoc_proxy_client::io_bind(gv::Io_user *user, std::string comp_name, std::string itf_name)
{
    return NULL;
}

gv::Wire_binding *Gvsoc_proxy_client::wire_bind(gv::Wire_user *user, std::string comp_name, std::string itf_name)
{
    return NULL;
}

void Gvsoc_proxy_client::update(int64_t timestamp)
{

}

void Gvsoc_proxy_client::vcd_bind(gv::Vcd_user *user)
{
}

void Gvsoc_proxy_client::vcd_enable()
{
}

void Gvsoc_proxy_client::vcd_disable()
{
}

void Gvsoc_proxy_client::event_add(std::string path, bool is_regex)
{
}

void Gvsoc_proxy_client::event_exclude(std::string path, bool is_regex)
{
}


void *Gvsoc_proxy_client::get_component(std::string path)
{
    std::string result = this->send_command("get_component " + path);
    return (void *)strtol(result.c_str(), NULL, 0);
}

void Gvsoc_proxy_client::unlock_command()
{
    this->mutex.unlock();
}

void Gvsoc_proxy_client::send(uint8_t *buffer, int size)
{
    ::send(this->proxy_socket, buffer, size, 0);
}

void Gvsoc_proxy_client::receive(uint8_t *buffer, int size)
{
    ::recv(this->proxy_socket, buffer, size, 0);
}

int Gvsoc_proxy_client::get_req()
{
    std::unique_lock<std::mutex> lock(this->mutex);
    int req = this->current_req_id++;
    lock.unlock();
    return req;
}

void Gvsoc_proxy_client::register_payload_callback(int req, std::function<void(uint8_t *, int)> callback)
{
    std::unique_lock<std::mutex> lock(this->mutex);
    this->payload_callbacks[req] = callback;
    lock.unlock();
}

void Gvsoc_proxy_client::unregister_payload_callback(int req)
{
    std::unique_lock<std::mutex> lock(this->mutex);
    this->payload_callbacks.erase(req);
    lock.unlock();
}

gv::Testbench::Testbench(gv::Gvsoc *gvsoc, std::string path)
{
    this->gvsoc = gvsoc;
    this->component = this->gvsoc->get_component(path);
    if (this->component == NULL)
    {
        throw std::runtime_error("Didn't find testbench component (path: " + path + ")");
    }
}

gv::TestbenchUart *gv::Testbench::get_uart(int id)
{
    return new gv::TestbenchUart(this->gvsoc, this, id);
}

gv::TestbenchUart::TestbenchUart(Gvsoc *gvsoc, gv::Testbench *testbench, int id)
{
    this->testbench = testbench;
    this->gvsoc = gvsoc;
    this->id = id;
}

void gv::TestbenchUart::open(TestbenchUartConf *conf)
{
    char command_header[1024];

    sprintf(command_header, "component %p uart setup ", this->testbench->component);

    std::string command = std::string(command_header);
    command += " itf=" + std::to_string(this->id);
    command += " enabled=1";
    command += " baudrate=" + std::to_string(conf->baudrate);
    command += " word_size=" + std::to_string(conf->word_size);
    command += " stop_bits=" + std::to_string(conf->stop_bits);
    command += " parity_mode=" + std::to_string(conf->parity_mode);
    command += " ctrl_flow=" + std::to_string(conf->ctrl_flow);
    command += " is_usart=" + std::to_string(conf->is_usart);
    command += " usart_polarity=" + std::to_string(conf->usart_polarity);
    command += " usart_phase=" + std::to_string(conf->usart_phase);

    ((Gvsoc_proxy_client *)this->gvsoc)->send_command(command);
}

void gv::TestbenchUart::close()
{
    char command_header[1024];

    sprintf(command_header, "component %p uart setup ", this->testbench->component);

    std::string command = std::string(command_header);
    command += " itf=" + std::to_string(this->id);
    command += " enabled=0";

    ((Gvsoc_proxy_client *)this->gvsoc)->send_command(command);
    
}

void gv::TestbenchUart::tx(uint8_t *buffer, int size)
{
    char command_header[1024];

    sprintf(command_header, "component %p uart tx %d %d ", this->testbench->component, this->id, size);

    std::string command = std::string(command_header);

    int req = ((Gvsoc_proxy_client *)this->gvsoc)->post_command(command, true);
    ((Gvsoc_proxy_client *)this->gvsoc)->send(buffer, size);
    ((Gvsoc_proxy_client *)this->gvsoc)->unlock_command();
    ((Gvsoc_proxy_client *)this->gvsoc)->wait_reply(req);
}

void gv::TestbenchUart::rx_enable()
{
    int req = ((Gvsoc_proxy_client *)this->gvsoc)->get_req();
    ((Gvsoc_proxy_client *)this->gvsoc)->register_payload_callback(req,
        std::bind(&gv::TestbenchUart::handle_rx, this, std::placeholders::_1, std::placeholders::_2));
    char command_header[1024];

    sprintf(command_header, "component %p uart rx %d 1 %d ", this->testbench->component, this->id, req);

    std::string command = std::string(command_header);

    ((Gvsoc_proxy_client *)this->gvsoc)->send_command(command);
}

void gv::TestbenchUart::rx_attach_callback(std::function<void(uint8_t *, int)> callback)
{
    this->rx_callback = callback;
}

void gv::TestbenchUart::handle_rx(uint8_t *data, int size)
{
    if (this->rx_callback)
    {
        this->rx_callback(data, size);
    }
}

double Gvsoc_proxy_client::get_average_power(double &dynamic_power, double &static_power)
{
    return 0;
}

double Gvsoc_proxy_client::get_instant_power(double &dynamic_power, double &static_power)
{
    return 0;
}

void Gvsoc_proxy_client::report_start()
{

}

void Gvsoc_proxy_client::report_stop()
{

}

gv::PowerReport *Gvsoc_proxy_client::report_get()
{
    return NULL;
}
