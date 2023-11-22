/*
 * Copyright (C) 2020  GreenWaves Technologies, SAS, SAS, ETH Zurich and University of Bologna
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


#include <vp/vp.hpp>
#include <vp/itf/io.hpp>
#include <vp/signal.hpp>
#include <queue>


class Mem_plug_req;
class Mem_plug_port;
class Mem_plug;


/**
 * @brief Implementation part of a mem_plug
 * 
 * This contains all the internals methods and variables that do not need to be
 * exposed to the model developper.
 */
class Mem_plug_implem : public vp::Block
{
    friend class Mem_plug_port;
    friend class Mem_plug;

public:
    /**
     * @brief Construct a new Mem_plug_implem object
     * 
     * @param parent Parent block containing this block.
     * @param comp Component containing this block (used for tracing and events).
     * @param path Path of the block in the component (for trace declaration).
     * @param mem_itf Memory output interface.
     * @param nb_ports Number of ports to the memory interface.
     * @param output_latency Latency in cycles on each output port.
     */
    Mem_plug_implem(vp::Block *parent, std::string name, vp::Component *comp, std::string path, vp::IoMaster *mem_itf,
        int nb_ports, int output_latency);

    void handle_request_done(Mem_plug_req *req);

protected:
    vp::Signal<int> nb_pending_reqs;   // Number of pending input requests
    int output_latency;

private:
    void check_state();
    std::vector<Mem_plug_port *> ports;       // Output ports, each one can handle a request.
    vp::Queue waiting_reqs;  // Pending input requests waiting for available
        // output put
    int nb_ports;                             // Number of output ports
    vp::Trace         trace;                  // Plug trace
};


class Mem_plug_port : public vp::Block
{
    friend class Mem_plug_implem;
public:
    Mem_plug_port(Mem_plug_implem *top, std::string name, vp::Component *comp, std::string path, vp::IoMaster *out_itf);
    bool enqueue(Mem_plug_req *req);

protected:
    void check_state();
    static void event_handler(vp::Block *__this, vp::ClockEvent *event);

    Mem_plug_implem *top;
    vp::IoReq out_req;
    vp::Queue pending_req;
    vp::ClockEvent *event;
    vp::Trace         trace;
    vp::Component *comp;
    vp::IoMaster *out_itf;
    int64_t ready_time=-1;
};
