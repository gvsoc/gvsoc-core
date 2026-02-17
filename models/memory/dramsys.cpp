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

#include <vp/vp.hpp>
#include <vp/itf/io.hpp>
#include <stdio.h>
#include <string.h>
#include <systemc.h>
#include <vector>
#include <list>
#include <queue>


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <cstdint>
#include <dlfcn.h>  // Linux specific header for dynamic loading

typedef void*   CallbackInstance_t;
typedef void    (AsynCallbackResp_Meth)(CallbackInstance_t instance, int is_write);
typedef void    (AsynCallbackUpdateReq_Meth)(CallbackInstance_t instance);

#define GRANULARITY 64
#define GRAN_CLOG 6


class ddr : public vp::Component
{

public:
    ddr(vp::ComponentConf &conf);
    ~ddr();

    void paraSendRequest(vp::IoReq *req);

    static vp::IoReqStatus req(vp::Block *__this, vp::IoReq *req);

    static void rspCallback(void *__this, int is_write);

    static void reqCallback(void *__this);

private:
    vp::Trace trace;
    vp::IoSlave in;
    void* libraryHandle;
    int dram_id;
    int (*add_dram)(char * resources_path, char * config_path);
    void (*close_dram)(int dram_id);
    int (*dram_can_accept_req)(int dram_id);
    int (*dram_has_read_rsp)(int dram_id);
    int (*dram_has_write_rsp)(int dram_id);
    int (*dram_get_write_rsp)(int dram_id);
    void (*dram_write_buffer)(int dram_id, int byte_int, int idx);
    void (*dram_write_strobe)(int dram_id, int strob_int, int idx);
    void (*dram_send_req)(int dram_id, uint64_t addr, uint64_t length , uint64_t is_write, uint64_t strob_enable);
    void (*dram_get_read_rsp)(int dram_id, uint64_t length, const void* buf);
    int (*dram_get_read_rsp_byte)(int dram_id);
    int (*dram_get_inflight_read)(int dram_id);
    void (*dram_preload_byte)(int dram_id, uint64_t dram_addr_ofst, int byte_int);
    int (*dram_check_byte)(int dram_id, uint64_t dram_addr_ofst);
    void (*dram_load_elf)(int dram_id, uint64_t dram_base_addr, char * elf_path);
    void (*dram_load_memfile)(int dram_id, uint64_t addr_ofst, char * mem_path);
    void (*dram_register_async_callback)(int dram_id, CallbackInstance_t instance, AsynCallbackResp_Meth* resp_meth, AsynCallbackUpdateReq_Meth* req_meth);

    std::queue<vp::IoReq *>  denied_req_queue;
    std::list<std::pair<vp::IoReq *, std::queue<int>>>  pending_read_req_queue;
};

ddr::ddr(vp::ComponentConf &config)
    : vp::Component(config)
{
    traces.new_trace("trace", &trace, vp::DEBUG);

    in.set_req_meth(&ddr::req);
    new_slave_port("input", &in);

    libraryHandle = dlopen("libDRAMSys_Simulator.so", RTLD_LAZY);
    add_dram = (int (*)(char*, char*))dlsym(libraryHandle, "add_dram");
    close_dram = (void (*)(int))dlsym(libraryHandle, "close_dram");
    dram_can_accept_req = (int (*)(int))dlsym(libraryHandle, "dram_can_accept_req");
    dram_has_read_rsp = (int (*)(int))dlsym(libraryHandle, "dram_has_read_rsp");
    dram_has_write_rsp = (int (*)(int))dlsym(libraryHandle, "dram_has_write_rsp");
    dram_get_write_rsp = (int (*)(int))dlsym(libraryHandle, "dram_get_write_rsp");
    dram_write_buffer = (void (*)(int, int, int))dlsym(libraryHandle, "dram_write_buffer");
    dram_write_strobe = (void (*)(int, int, int))dlsym(libraryHandle, "dram_write_strobe");
    dram_send_req = (void (*)(int, uint64_t, uint64_t, uint64_t, uint64_t))dlsym(libraryHandle, "dram_send_req");
    dram_get_read_rsp = (void (*)(int, uint64_t, const void*))dlsym(libraryHandle, "dram_get_read_rsp");
    dram_get_read_rsp_byte = (int (*)(int))dlsym(libraryHandle, "dram_get_read_rsp_byte");
    dram_get_inflight_read = (int (*)(int))dlsym(libraryHandle, "dram_get_inflight_read");
    dram_preload_byte = (void (*)(int, uint64_t, int))dlsym(libraryHandle, "dram_preload_byte");
    dram_check_byte = (int (*)(int, uint64_t))dlsym(libraryHandle, "dram_check_byte");
    dram_load_elf = (void (*)(int, uint64_t, char*))dlsym(libraryHandle, "dram_load_elf");
    dram_load_memfile = (void (*)(int, uint64_t, char*))dlsym(libraryHandle, "dram_load_memfile");
    dram_register_async_callback = (void (*)(int, CallbackInstance_t, AsynCallbackResp_Meth*, AsynCallbackUpdateReq_Meth*))dlsym(libraryHandle, "dram_register_async_callback");

#ifdef DRAMSYS_PATH
    std::cout << "DRAMSYS_PATH is defined!: " << DRAMSYS_PATH << std::endl;
#else
    std::cout << "DRAMSYS_PATH is not defined." << std::endl;
#endif

    std::string current_path = DRAMSYS_PATH;
    std::string resources_path = current_path + "/dramsys_configs";
    std::string dram_type = get_js_config()->get("dram-type")->get_str();
    std::string simulationJson_path;
    if (dram_type == "")    dram_type = "hbm2-example.json";  // Default DRAM type if not specified in the configuration
    simulationJson_path = resources_path + "/" + dram_type;


    dram_id = add_dram((char*)resources_path.c_str(), (char*)simulationJson_path.c_str());
    dram_register_async_callback(dram_id, (CallbackInstance_t)this, (AsynCallbackResp_Meth *)&ddr::rspCallback, (AsynCallbackUpdateReq_Meth*)&ddr::reqCallback);

}

void ddr::paraSendRequest(vp::IoReq *req){
    uint64_t offset = req->get_addr();
    uint8_t *data = req->get_data();
    uint64_t size = req->get_size();

    //Basic information
    uint64_t req_start_addr = (offset >> GRAN_CLOG) << GRAN_CLOG;
    uint64_t num_req = 1 + (((offset + size - 1) - req_start_addr) >> GRAN_CLOG);
    trace.msg("---- Req Info: aligned addr->0x%x, #DRAM requests->%d \n",req_start_addr, num_req);

    //Generate masks and data queue
    std::queue<int> mask_q, mask_d;
    std::queue<uint8_t> data_q;
    for (int i = 0; i < GRANULARITY * num_req; ++i)
    {
        if (i >= (offset - req_start_addr) && i< (offset + size - req_start_addr))
        {
            mask_q.push(1);
            mask_d.push(1);
            data_q.push(data[ i - (offset - req_start_addr)]);
        } else {
            mask_q.push(0);
            mask_d.push(0);
            data_q.push(0);
        }
    }

    //splite up for each request
    for (int iter = 0; iter < num_req; ++iter)
    {
        if (req->get_is_write()) trace.msg("---- Write Data List of DRAM REQ #%d \n", iter);
        //Write mask and data to buffer
        for (int i = 0; i < GRANULARITY; ++i)
        {
            if (req->get_is_write()) trace.msg("-------- iter %d Put byte %d valid %d \n", i, (int)data_q.front(), mask_q.front());
            dram_write_buffer(dram_id, data_q.front(), i);
            dram_write_strobe(dram_id, mask_q.front(), i);
            data_q.pop();
            mask_q.pop();
        }

        //Send req to dram
        dram_send_req(dram_id, req_start_addr + iter*GRANULARITY, GRANULARITY, req->get_is_write(), req->get_is_write());
    }

    //queue read request
    std::pair<vp::IoReq *, std::queue<int>> pair;
    pair.first = req;
    pair.second = mask_d;
    if (!req->get_is_write())
    {
        pending_read_req_queue.push_back(pair);
        trace.msg("---- Add in Pending list: mask length->%d \n",pair.second.size());
    }

}

vp::IoReqStatus ddr::req(vp::Block *__this, vp::IoReq *req)
{
    ddr *_this = (ddr *)__this;

    uint64_t offset = req->get_addr();
    uint8_t *data = req->get_data();
    uint64_t size = req->get_size();

    _this->trace.msg("IO access (offset: 0x%x, size: 0x%x, is_write: %d)\n", offset, size, req->get_is_write());

    if (_this->dram_can_accept_req(_this->dram_id))
    {
        _this->paraSendRequest(req);
        if (req->get_is_write()) return vp::IO_REQ_OK;
        return vp::IO_REQ_PENDING;
    }else{
        _this->denied_req_queue.push(req);
        _this->trace.msg("---- Add in denied request list \n");
        return vp::IO_REQ_DENIED;
    }
}

void ddr::rspCallback(void *__this, int is_write){
    ddr *_this = (ddr *)__this;
    int rep_byte_int, mask;
    _this->trace.msg("---- Response Callback Triggered \n");

    if (is_write)
    {
        while(_this->dram_has_write_rsp(_this->dram_id)) rep_byte_int = _this->dram_get_write_rsp(_this->dram_id);
    } else {
        while(_this->dram_has_read_rsp(_this->dram_id) && _this->pending_read_req_queue.size() != 0){
            vp::IoReq *req = _this->pending_read_req_queue.front().first;
            uint64_t offset = req->get_addr();
            uint8_t *data = req->get_data();
            uint64_t size = req->get_size();

            //Basic information
            uint64_t req_start_addr = (offset >> GRAN_CLOG) << GRAN_CLOG;
            uint64_t num_req = 1 + (((offset + size - 1) - req_start_addr) >> GRAN_CLOG);
            uint64_t read_ptr = num_req*GRANULARITY - _this->pending_read_req_queue.front().second.size();
            uint64_t start_ptr = offset - req_start_addr;
            uint64_t end_ptr = offset - req_start_addr + size;

            //Get byte and pop mask list
            rep_byte_int = _this->dram_get_read_rsp_byte(_this->dram_id);
            mask = _this->pending_read_req_queue.front().second.front();
            _this->pending_read_req_queue.front().second.pop();

            _this->trace.msg("-------- iter %d Get byte %d valid %d \n", read_ptr, rep_byte_int, (read_ptr >= start_ptr && read_ptr < end_ptr));

            //Fill read data
            if (read_ptr >= start_ptr && read_ptr < end_ptr)
            {
                data[read_ptr - start_ptr] = mask? (uint8_t)rep_byte_int: 0;
            }

            //Check mask list size, response request, pop pending req in list
            if (_this->pending_read_req_queue.front().second.size() == 0)
            {
                _this->trace.msg("---- Response read \n");
                req->get_resp_port()->resp(req);
                _this->pending_read_req_queue.pop_front();
            }
        }
    }
}

void ddr::reqCallback(void *__this){
    ddr *_this = (ddr *)__this;

    while(_this->dram_can_accept_req(_this->dram_id) && _this->denied_req_queue.size() != 0)
    {
        vp::IoReq *req = _this->denied_req_queue.front();
        _this->paraSendRequest(req);
        req->get_resp_port()->grant(req);
        if (req->get_is_write()) req->get_resp_port()->resp(req);
        _this->denied_req_queue.pop();
    }
}

ddr::~ddr(){
    close_dram(dram_id);
}


extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new ddr(config);
}
