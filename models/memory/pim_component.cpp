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

#include <vp/vp.hpp>
#include <vp/itf/io.hpp>
#include <pim.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <cstdint>

/**
 * @brief PIM Component
 * 
 * This component models a basic Processing-In-Memory (PIM) unit that can perform
 * operations directly within the memory system. It communicates with the DRAM system
 * to notify and send data for PIM operations. On its basic configuration, it triples 
 * the content of the data received from DRAM and sends it back.
 */
class PimComponent : public vp::Component
{
public:
    PimComponent(vp::ComponentConf &conf);

    static void setMemspec(vp::Block *__this, GvsocMemspec _memspec);

    static void receivePimNotify(vp::Block *__this, PimStride *pim_stride);
    
private:
    vp::Trace trace;
    vp::WireSlave<GvsocMemspec> rcv_memspec_itf;
    vp::WireSlave<PimStride*> pim_notify_itf;
    vp::WireMaster<PimStride*> pim_data_itf;

    GvsocMemspec memspec = {0};
    uint nb_channels;
    // Length of the access from PIM component to DRAM
    uint64_t access_length;
    // Stride of the accesses from PIM component to DRAM
    // (parallel access to DRAM elements can be modeled 
    // with multiple accesses with a given stride)
    uint64_t access_stride; 
    // Number of accesses to perform from PIM component to DRAM
    uint64_t access_count;  

    // BufferS to store data for each channel
    std::vector<char*> pim_channel_data;
};

PimComponent::PimComponent(vp::ComponentConf &config) 
    : vp::Component(config)
{
    this->traces.new_trace("trace", &trace, vp::DEBUG);

    rcv_memspec_itf.set_sync_meth(&PimComponent::setMemspec);   // Interface to receive memory specification from DRAMSys wrapper
    new_slave_port("rcv_memspec", &rcv_memspec_itf);
    pim_notify_itf.set_sync_meth(&PimComponent::receivePimNotify);  // Interface to receive PIM operation notification from DRAMSys wrapper
    new_slave_port("pim_notify", &pim_notify_itf);
    new_master_port("pim_data", &pim_data_itf);                 // Interface to access data from DRAMSys wrapper for PIM operation
}

void PimComponent::setMemspec(vp::Block *__this, GvsocMemspec _memspec)
{
    PimComponent *_this = (PimComponent *)__this;

    _this->memspec = _memspec;
    _this->trace.msg("Memory specification received: nb_channels=%d\n", _this->memspec.nb_channels);
    
    // Check that the memory specification wasn't already set
    if (_this->pim_channel_data.size() != 0) {
        _this->trace.msg("Error: Memory specification already set, cannot reallocate PIM buffers\n");
        exit(1);
    }

    // Current parallel access settings: for now considering bank parallelism within a channel
    _this->nb_channels = _this->memspec.nb_channels;
    _this->access_length = _this->memspec.access_size;  // Column size
    _this->access_stride = _this->memspec.bank_stride;  // Stride to access different banks within the same channel
    _this->access_count = _this->memspec.nb_banks * _this->memspec.nb_bank_groups;  // Accessing all banks within the channel in parallel

    // Allocate PIM buffer for each channel based on the memory specification
    for (uint i = 0; i < _this->nb_channels; i++) {
        // For now considering bank parallelism within a channel
        _this->pim_channel_data.push_back(
            new char[_this->access_length * _this->access_count]
        );
    }    
}

void PimComponent::receivePimNotify(vp::Block *__this, PimStride *pimStride)
{
    PimComponent *_this = (PimComponent *)__this;

    PimInfo pimInfo = pimStride->pimInfo;  

    // Fill pimStride with data to send back to DRAM

    _this->trace.msg("PIM Callback Triggered by a %s to address 0x%lx\n", pimInfo.is_write ? "write" : "read", pimStride->base_addr);
    uint64_t old_base_addr = pimStride->base_addr;
    // For bank parallel access, set the base address to bank 0 and bank group 
    uint64_t lower_bits = pimStride->base_addr % _this->access_stride;
    // Set to zero all bits lower than the highest stride address
    uint64_t higher_bits = pimStride->base_addr & ~(_this->access_stride * _this->access_count - 1);
    pimStride->base_addr = higher_bits + lower_bits;

    pimStride->length = _this->access_length;
    pimStride->stride = _this->access_stride;
    pimStride->count = _this->access_count;
    pimStride->buf = _this->pim_channel_data[pimInfo.channel];

    if (pimInfo.is_write) {
        _this->trace.msg("---- Notified write on channel %d, address 0x%lx, new base 0x%lx\n", pimInfo.channel, old_base_addr, pimStride->base_addr);
        // Copy data in the internal buffer to DRAM
        _this->pim_data_itf.sync(pimStride);
    } else {
        _this->trace.msg("---- Notified read on channel %d, address 0x%lx, new base 0x%lx\n", pimInfo.channel, old_base_addr, pimStride->base_addr);
        // Request data from DRAM through pim_data_itf
        _this->pim_data_itf.sync(pimStride);
        // Process data (for now, just triple the data as an example)
        for (uint i = 0; i < _this->access_length * _this->access_count; i++) {
            _this->pim_channel_data[pimInfo.channel][i] *= 3;
        }
    }
}

extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new PimComponent(config);
}