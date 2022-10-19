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

/**
 * @brief Aps256 octospi ram model
 */

#include <vp/vp.hpp>
#include <stdio.h>
#include <string.h>

#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <vp/itf/hyper.hpp>
#include <vp/itf/wire.hpp>

// Enum for ram state
typedef enum
{
    APS_STATE_CMD,  // RAM is receiving the command header
    APS_STATE_ADDR, // RAM is receiving the address
    APS_STATE_DATA  // RAM is receiving or sending data
} Aps_state_e;

/**
 * @brief Class for Aps RAM model
 */
class Aps : public vp::component
{
public:
    Aps(js::config *config);

    // GVSOC build function overloading
    int build();

    // GVSOC reset function overloading
    void reset(bool active);

private:
    /**
     * @brief Handle octospi clock edges
     *
     * This is called everytime a usefull clock edge occurs on the octospi line.
     * This is called on both raising and falling edges in DDR mode and only on raising edge
     * in STR mode.
     * This method will enqueue the incoming data and handle any impact on the flash state, like
     * handling transmitted commands.
     * 
     * @param data   Data transmitted through the octospi data line.
     */
    void sync_cycle(int data);

    /**
     * @brief Stub for sync_cycle method
     *
     * This is just a stub method which is called when a useful clock cycle occurs on the
     * octospi line.
     * It is static since gvsoc requires static methods for handling inter-components communication.
     * It will just call the real method handling it.
     *
     * @param __this This pointer used to call the real method.
     * @param data   Data transmitted through the octospi data line.
     */
    static void sync_cycle_stub(void *__this, int data);

    /**
     * @brief Handle chip select update
     *
     * This is called everytime thechip select is updated on the octospi line.
     * It is active low.
     * 
     * @param value   Value of the chip select, 0 when it is active, 1 when it is inactive.
     */
    void cs_sync(int cs, int value);

    /**
     * @brief Stub for cs_sync method
     *
     * This is just a stub method which is called when the chip select is updated on the
     * octospi line.
     * It is static since gvsoc requires static methods for handling inter-components communication.
     * It will just call the real method handling it.
     *
     * @param __this This pointer used to call the real method.
     * @param value   Value of the chip select, 0 when it is active, 1 when it is inactive.
     */
    static void cs_sync_stub(void *__this, int cs, int value);

    /**
     * @brief Handle an access to the ram array
     *
     * This is called for both read and program operations.
     * 
     * @param address Address of the access.
     * @param read    True if the access is a read, false if it is a write.
     * @param data    Data byte in case of a write
     */
    void handle_array_access(int address, bool is_write, uint8_t data);

    /**
     * @brief Parse the current command.
     *
     * This is called once the command header has been received in order to parse it and
     * extract useful information for FSM.
     * 
     * @param addr_bits  Number of expected address bits are returned here.
     */
    void parse_command();

    /**
     * @brief Handle current command after address has been received.
     *
     * This will determine if the command does not need to send or receive data and will
     * execute it.
     */
    void handle_command_address();

    /**
     * @brief Handle an incoming data byte
     *
     * This is called when command header and possibly address has already been received and a data
     * byte is received, in order to process it.
     * 
     * @param byte The input data byte.
     */
    void handle_data_byte(uint8_t byte);

    // Trace for dumping debug messages.
    vp::trace trace;
    // Input octospi interface.
    vp::hyper_slave in_itf;
    // Size of the ram, retrieved from JSON component configuration.
    int size;
    // RAM array
    uint8_t *data;
    // Current command being process
    uint32_t current_command;
    // Currrent address of the command being processed.
    int current_address;
    // Number of remaining bytes to be processed. Used when receiving command header and address to
    // known when enough bytes are ready.
    int pending_bytes;
    // Number of remaining latency cycles of the current command.
    int latency_count;
    // True if the current command is a write command.
    bool is_write;
    // Register read latency code.
    uint32_t read_latency_code;
    // Register write latency code.
    uint32_t write_latency_code;
    // Number of read latency cycle associated to the read register latency code.
    int read_latency;
    // Number of write latency cycle associated to the write register latency code.
    int write_latency;
    // RAM state
    Aps_state_e state;
};



void Aps::handle_array_access(int address, bool is_write, uint8_t data)
{
    if (address >= this->size)
    {
        this->warning.force_warning
            ("Received out-of-bound request (addr: 0x%x, ram_size: 0x%x)\n", address, this->size);
    }
    else
    {
        if (!is_write)
        {
            uint8_t data = this->data[address];
            this->trace.msg(vp::trace::LEVEL_TRACE, "Sending data byte (value: 0x%x)\n", data);
            this->in_itf.sync_cycle(data);
        }
        else
        {
            this->trace.msg(vp::trace::LEVEL_TRACE, "Received data byte (value: 0x%x)\n", data);
            this->data[address] = data;
        }
    }
}



void Aps::parse_command()
{
    this->trace.msg(vp::trace::LEVEL_TRACE,
        "Handling command (cmd: 0x%x)\n", this->current_command);

    if (this->current_command == 0x40)
    {
        // Register read
        this->is_write = false;
        this->latency_count = this->read_latency;
    }
    else if (this->current_command == 0xc0)
    {
        // Register write
        this->is_write = true;
        this->latency_count = 1;
    }
    else if (this->current_command == 0x20)
    {
        // Burst read
        this->is_write = false;
        this->latency_count = this->read_latency;
    }
    else if (this->current_command == 0xA0)
    {
        // Burst write
        this->is_write = true;
        this->latency_count = this->write_latency;
    }
    else
    {
        this->trace.force_warning("Received unknown RAM command (cmd: 0x%x)\n", this->current_command);
    }

    // Since the latency is a number of raising edges, we need to double it in DDR mode
    // since the model is called at each edge.
    this->latency_count *= 2;
}



void Aps::handle_command_address()
{
    if (this->current_command == 0x20 || this->current_command == 0xa0)
    {
        this->trace.msg(vp::trace::LEVEL_DEBUG, 
            "Received burst command (command: 0x%x, address: 0x%x, is_write: %d)\n",
            this->current_command, this->current_address, this->is_write);
    }
}



void Aps::handle_data_byte(uint8_t data)
{
    // Register read
    if (this->current_command == 0x40)
    {
        uint32_t value = 0;

        if (this->current_address == 0)
        {
            value = this->read_latency_code << 2;
        }
        else if (this->current_address == 4)
        {
            value = this->write_latency_code << 4;
        }
        
        this->trace.msg(vp::trace::LEVEL_INFO,
            "Reading ram register (addr: %d, data: 0x%x)\n",
            this->current_address, value);

        this->in_itf.sync_cycle(value);
    }
    // Register write
    else if (this->current_command == 0xc0)
    {
        this->trace.msg(vp::trace::LEVEL_INFO,
            "Writing ram register (addr: %d, data: 0x%x)\n",
            this->current_address, data);

        if (this->current_address == 0)
        {
            // Compute the number of latency cycle out of the register code
            this->read_latency_code = (data >> 2) & 0x7;
            this->read_latency = this->read_latency_code + 3;
            this->trace.msg(vp::trace::LEVEL_INFO,
                "Set read latency (latency: %d)\n",
                this->read_latency);
        }
        else if (this->current_address == 4)
        {
            // Compute the number of latency cycle out of the register code
            this->write_latency_code = (data >> 5) & 0x7;
            switch (this->write_latency_code)
            {
                case 0x0: this->write_latency = 3; break;
                case 0x4: this->write_latency = 4; break;
                case 0x2: this->write_latency = 5; break;
                case 0x6: this->write_latency = 6; break;
                case 0x1: this->write_latency = 7; break;
            }
            this->trace.msg(vp::trace::LEVEL_INFO, "Set write latency (latency: %d)\n",
                this->write_latency);
        }
    }
    // Read or write burst
    else
    {
        this->handle_array_access(this->current_address, this->is_write, data);
        this->current_address++;
    }
}



void Aps::sync_cycle(int data)
{
    // This method is called whenever there is a usefull clock edge on the octospi line.
    // This is called on both raising and falling edges in DDR mode and only on raising edge
    // in STR mode.
    // Most of the time, the model does not have to care if we are in DDR or STR mode since this
    // method is called only for useful edges. Only the handling of latency needs to care since it
    // is a number of full cycles, so special care needs to be taken in DDR mode.

    // This method mostly implements an FSM, which is first sampling command, address and data, and
    // is then handling the commands. It can also send back data on the octospi line for read
    // commands.

    if (this->state == APS_STATE_CMD)
    {
        // State when we are sampling the command header

        // Queue the incoming byte
        this->pending_bytes--;
        this->current_command = (this->current_command >> 8) | (data << 8);

        this->trace.msg(vp::trace::LEVEL_TRACE,
            "Received command byte (byte: 0x%x, command: 0x%x, pending_bytes: %d)\n",
            data, this->current_command, this->pending_bytes);

        // Only do something once the full command is received
        if (this->pending_bytes == 0)
        {
            // Parse the command
            this->parse_command();

            // And then wait for the address since all commands expect an address
            this->state = APS_STATE_ADDR;
            this->pending_bytes = 4;
        }
    }
    else if (this->state == APS_STATE_ADDR)
    {
        // State where we are sampling the command address

        // Queue the incoming byte
        this->pending_bytes--;
        this->current_address = (this->current_address << 8) | data;
        this->current_address = this->current_address;

        this->trace.msg(vp::trace::LEVEL_TRACE,
            "Received address byte (byte: 0x%x, address: 0x%x, pending_bytes: %d)\n",
            data, this->current_address, this->pending_bytes);

        // Only do something once the full address is received
        if (this->pending_bytes == 0)
        {
            // Handle the command now that we have the address
            this->handle_command_address();

            // Now wait for data since all commands deal with data.
            this->state = APS_STATE_DATA;
        }
    }
    else if (this->state == APS_STATE_DATA)
    {
        // State where we are receiving or sending data

        // First check if we need to skip data due to ongoing latency
        if (this->latency_count > 0)
        {
            this->latency_count--;

            this->trace.msg(vp::trace::LEVEL_TRACE, "Accounting latency (remaining: %d)\n",
                this->latency_count);
        }
        else
        {
            // Otherwise take care of the data

            if (this->is_write)
            {
                this->trace.msg(vp::trace::LEVEL_TRACE, "Received data (data: 0x%x)\n",
                    data);
            }

            this->handle_data_byte(data);
        }
    }
}



void Aps::sync_cycle_stub(void *__this, int data)
{
    // Stub for real method, just forward the call
    Aps *_this = (Aps *)__this;
    _this->sync_cycle(data);
}



void Aps::cs_sync(int cs, int value)
{
    this->trace.msg(vp::trace::LEVEL_TRACE, "Received CS sync (value: %d)\n", value);

    this->state = APS_STATE_CMD;
    this->pending_bytes = 2;
}



void Aps::cs_sync_stub(void *__this, int cs, int value)
{
    // Stub for real method, just forward the call
    Aps *_this = (Aps *)__this;
    _this->cs_sync(cs, value);
}



void Aps::reset(bool active)
{
    // This function is called everytime the flash is reset.
    // Both active and inactive levels trigger a call.
    if (active)
    {
        // When reset is active, we must put back the ram into the initial state
        this->read_latency = 5;
        this->write_latency = 5;
    }
}



int Aps::build()
{
    // This method is called when the simulated system is built.
    // We just need here to take care of anything which must be done once at platform startup.

    // Trace for outputting debug messages
    traces.new_trace("trace", &trace, vp::DEBUG);

    // Input interface for exchanging octospi data
    in_itf.set_cs_sync_meth(&Aps::cs_sync_stub);
    in_itf.set_sync_cycle_meth(&Aps::sync_cycle_stub);
    new_slave_port("input", &in_itf);

    js::config *conf = this->get_js_config();

    // Allocate an array for the ram and fill it with special value to help SW detecting reads
    // to non-initialized data.
    this->size = conf->get("size")->get_int();

    this->data = new uint8_t[this->size];
    memset(this->data, 0x57, this->size);

    return 0;
}



Aps::Aps(js::config *config)
    : vp::component(config)
{
}



// Constructor function needed by GVSOC to instantiate this module.
// Just instantiate the flash class.
extern "C" vp::component *vp_constructor(js::config *config)
{
    return new Aps(config);
}
