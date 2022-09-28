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
 * @brief Mx25 octospi flash model
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
#include <vp/time/time_scheduler.hpp>

// Flash sector size
#define MX25_SECTOR_SIZE (1 << 12)

// Enum for flash state
typedef enum
{
    MX25_STATE_CMD,    // Flash is receiving command header
    MX25_STATE_ADDR,   // Flash is receiving the address
    MX25_STATE_DATA,   // Flash is receiving or sending data
    MX25_STATE_DONE    // Flash is done handling the command
} mx25_state_e;

/**
 * @brief Class for Mx25 flash model
 * 
 * This model derive from the time_scheduler so that it can push events based on time since
 * the flash is not cycle-based.
 */
class Mx25 : public vp::time_scheduler
{

public:
    /**
     * @brief Construct a new Mx25 object
     * 
     * @param config JSON configuration of this component coming from system generators
     */
    Mx25(js::config *config);

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
    void cs_sync(bool value);

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
    static void cs_sync_stub(void *__this, bool value);

    /**
     * @brief Preload the specified file into the flash
     *
     * The flash array can be preloaded with the content of a file.
     * It can also be kept synced with it, so that the flash array can be accessed from the
     * workstation at the end of the simulation, or to relaunch a simulation with the flash content
     * at the end of the simulation.
     *
     * @param path   Path to the file
     * @param writeback True if the file should be kept synced with the flash array so that.
     */
    int preload_file(char *path, bool writeback);

    /**
     * @brief Send the next output data
     *
     * Take the next bits to be sent from the pending value and send them to the output
     * interface.
     * This takes care of sending with the proper line width.
     */
    void send_output_data();

    /**
     * @brief Push input bits to the pending value
     *
     * Push incoming bits to the pending value and handle the received value if a full byte has been
     * received.
     * This takes cares of receiving with the proper line width.
     * 
     * @param data The input bits.
     */
    void handle_input_data(int data);

    /**
     * @brief Handle an incoming data byte
     *
     * This is called when command header and possibly address has already been received and a data
     * byte is received, in order to process it.
     * 
     * @param byte The input data byte.
     */
    void handle_data_byte(uint8_t byte);

    /**
     * @brief Handle current command after address has been received.
     *
     * This will determine if the command does not need to send or receive data and will
     * execute it.
     * 
     * @return true if the command has been fully handled.
     * @return false if the command will receive or send data.
     */
    bool handle_command_address();

    /**
     * @brief Handle current command after header has been received.
     *
     * This will determine if the command does not need address or data and will
     * execute it.
     * 
     * @return true if the command has been fully handled.
     * @return false if the command has not been handled.
     */
    bool handle_command_header();

    /**
     * @brief Parse the current command.
     *
     * This is called once the command header has been received in order to parse it and
     * extract useful information for FSM.
     * 
     * @param addr_bits  Number of expected address bits are returned here.
     */
    void parse_command(int &addr_bits);

    /**
     * @brief Handle an access to the flash array
     *
     * This is called for both read and program operations.
     * 
     * @param address Address of the access.
     * @param read    True if the access is a read, false if it is a write.
     * @param data    Data byte in case of a write
     * @return The data byte in case of a read
     */
    uint32_t handle_array_access(int address, int read, uint8_t data);

    /**
     * @brief Erase the chip.
     */
    void erase_chip();

    /**
     * @brief Erase a sector.
     * 
     * @param address Address of the erase.
     */
    void erase_sector(unsigned int addr);

    /**
     * @brief Get the number of latency cycles out of the latency code from the register.
     * 
     * @param code The register latency code.
     * @result The number of latency cycles.
     */
    int get_latency_from_code(uint32_t code);

    /**
     * @brief Mark the flash as busy.
     * 
     * This can be used after erase or program operation to reject any other operation during
     * the specified period.
     * 
     * @param time The duration in pico-seconds of the period during which the flash is busy.
     */
    void set_busy(int64_t time);

    /**
     * @brief Event handler to make the flash available.
     * 
     * This is the handler called when the busy event has expired to make the flash available.
     * This is a static method since GVSOC requires static method to handle time events.
     * 
     * @param __this The this pointer of the flash, needed as an explicit parameter as it is a
     *      static method.
     * @param event  The event associated to this handler.
     */
    static void set_available_handler(void *__this, vp::time_event *event);

    // Trace for dumping debug messages.
    vp::trace trace;
    // Input octospi interface.
    vp::hyper_slave in_itf;
    // Inut chip select interface.
    vp::wire_slave<bool> cs_itf;
    // Size of the flash, retrieved from JSON component configuration.
    int size;
    // Flash array
    uint8_t *data;
    // Flash state
    mx25_state_e octospi_state;
    // Current command being process
    uint32_t current_command;
    // Currrent address of the command being processed.
    int current_address;
    // True if the current command is a write command.
    bool is_write;
    // Number of remaining latency cycles of the current command.
    int latency_count;
    // Number of remaining bits to be processed. Used when receiving command header, address and
    // sending or receiving data to know when a byte is ready.
    int pending_bits;
    // Current byte value containing bits which has been received or which will be sent.
    uint32_t pending_value;
    // True if the flash is configured in OSPI STR or DTR mode.
    bool ospi_mode;
    // True if the flash is configured in OSPI DTR mode.
    bool dtr_mode;
    // True if the flash has received a write enable command and writing to flash is allowed.
    bool write_enable;
    // Register latency code.
    uint32_t latency_code;
    // Number of latency cycle associated to the register latency code.
    int latency;
    // True if the flash is busy in an erase or program operation and other operations should
    // be rejected.
    bool busy;
    // True if a program operation is on-going. This is used when the CS is disabled to make
    // the flash busy for a while
    bool program_ongoing;
    // Size of the program operation. This is used to estimate the duration of the program
    // operation.
    int program_size;
    // Time event used to make the flash available after a specific duration.
    vp::time_event *busy_event;
};



void Mx25::set_available_handler(void *__this, vp::time_event *event)
{
    Mx25 *_this = (Mx25 *)__this;

    _this->trace.msg(vp::trace::LEVEL_TRACE, "Set device as available\n");

    // Just make the flash as available, operations will be accepted again.
    _this->busy = false;
}



void Mx25::set_busy(int64_t time)
{
    this->trace.msg(vp::trace::LEVEL_TRACE, "Set device as busy (duration: %lld)\n", time);
    // Mark the flash as busy to reject other operations and enqueue an event which will make
    // the flash available again.
    this->busy = true;
    this->enqueue(this->busy_event, time);
}



int Mx25::get_latency_from_code(uint32_t code)
{
    return 20 - code * 2;
}



void Mx25::erase_sector(unsigned int addr)
{
    // Align the address on a sector address since we can only erase one sector at a time.
    addr &= ~(MX25_SECTOR_SIZE - 1);

    this->trace.msg(vp::trace::LEVEL_INFO, "Erasing sector (address: 0x%x)\n", addr);

    if (addr >= this->size)
    {
        this->warning.force_warning(
            "Received out-of-bound erase request (addr: 0x%x, flash_size: 0x%x)\n",
            addr, this->size);
        return;
    }

    // State of the array bytes after erase is 1 for each bit.
    memset(&this->data[addr], 0xff, MX25_SECTOR_SIZE);
}



void Mx25::erase_chip()
{
    this->trace.msg(vp::trace::LEVEL_INFO, "Erasing chip\n");
    for (unsigned int addr = 0; addr < this->size; addr += MX25_SECTOR_SIZE)
    {
        this->erase_sector(addr);
    }
}



uint32_t Mx25::handle_array_access(int address, int is_write, uint8_t data)
{
    if (address >= this->size)
    {
        this->warning.force_warning(
            "Received out-of-bound request (addr: 0x%x, flash_size: 0x%x)\n", address, this->size);
        return 0;
    }

    if (!is_write)
    {
        uint8_t data;
        data = this->data[address];
        this->trace.msg(vp::trace::LEVEL_TRACE,
            "Sending data byte (address: 0x%x, value: 0x%x)\n", address, data);
        return data;
    }
    else
    {
        this->trace.msg(vp::trace::LEVEL_TRACE,
            "[Word Programming] Writing to flash (address: 0x%x, value: 0x%x)\n", address, data);

        // The program operation can only switch bits from 1 to 0.
        // Check if some bits can not be set to zero.
        uint8_t new_value = this->data[address] & data;

        if (new_value != data)
        {
            this->warning.force_warning(
                "Failed to program specified location (addr: 0x%x, flash_val: 0x%2.2x, "
                "program_val: 0x%2.2x)\n", address, this->data[address], data);
        }

        this->data[address] &= data;
        this->program_ongoing = true;
        this->write_enable = false;
        this->program_size++;
    }

    return 0;
}


void Mx25::parse_command(int &addr_bits)
{
    this->trace.msg(vp::trace::LEVEL_TRACE,
        "Handling command (cmd: 0x%x)\n", this->current_command);

    // Init to default values so that command can override when expecting a specific value.
    addr_bits = 0;
    this->is_write = false;
    this->latency_count = 0;

    // Store the current command as it will be used througout the whole command
    this->current_command = this->pending_value;

    // Write enable
    if (this->current_command == 0x06 || this->current_command == 0x06f9)
    {
    }
    // Write configuration register 2
    else if (this->current_command == 0x72 || this->current_command == 0x728d)
    {
        this->is_write = true;
        addr_bits = 32;
    }
    // Erase sector
    else if (this->current_command == 0x21de || this->current_command == 0x21)
    {
        this->is_write = true;
        addr_bits = 32;
        if (this->busy)
        {
            this->trace.force_warning("Trying to erase while flash is busy\n");
        }
    }
    // Erase chip
    else if (this->current_command == 0x609f)
    {
        this->is_write = true;
        if (this->busy)
        {
            this->trace.force_warning("Trying to erase while flash is busy\n");
        }
        this->erase_chip();
    }
    // Read status
    else if (this->current_command == 0x05fa || this->current_command == 0x05)
    {
        // Latency depends on the spi mode
        if (this->current_command == 0x05)
        {
        }
        else
        {
            addr_bits = 32;
            if (this->dtr_mode)
            {
                this->latency_count = 4;
            }
            else
            {
                this->latency_count = 5;
            }
        }
    }
    // Program
    else if (this->current_command == 0x12ed || this->current_command == 0x12)
    {
        this->is_write = true;
        addr_bits = 32;
        this->program_size = 0;
        if (this->busy)
        {
            this->trace.force_warning("Trying to program while flash is busy\n");
        }
    }
    // Read
    else if (this->current_command == 0xee11 || this->current_command == 0xc ||
        this->current_command == 0xec13)
    {
        addr_bits = 32;
        if (this->current_command == 0xec13)
        {
            // OSPI STR mode is adding one more latency cycle
            this->latency_count = this->latency + 1;
        }
        else if (this->current_command == 0xee11)
        {
            this->latency_count = this->latency;
        }
        else
        {
            // SPI mode has a fixed latency count
            this->latency_count = 8;
        }
        if (this->busy)
        {
            this->trace.force_warning("Trying to read while flash is busy\n");
        }
    }
    else
    {
        this->trace.force_warning(
            "Received unknown flash command (cmd: 0x%x)\n", this->current_command);
    }

    // Since the latency is a number of raising edges, we need to double it in DDR mode
    // since the model is called at each edge.
    if (this->dtr_mode)
    {
        this->latency_count *= 2;
    }
}



bool Mx25::handle_command_header()
{
    // Write enable
    if (this->current_command == 0x06 || this->current_command == 0x06f9)
    {
        this->write_enable = true;
        return true;
    }
    // Erase chip
    else if (this->current_command == 0x609f)
    {
        this->erase_chip();
        this->write_enable = false;

        // Duration of the erase has been determined with calibration
        this->set_busy(1209778219000);
        return true;
    }

    return false;
}



bool Mx25::handle_command_address()
{
    // In case the command has not been handled, store the address so that it can be used later on.
    this->current_address = this->pending_value;

    if (this->current_command == 0x21de || this->current_command == 0x21)
    {
        // Erase sector do not need any data and can be handled now
        if (!this->busy)
        {
            this->erase_sector(this->current_address);
            this->write_enable = false;

            // The duration of the erase has been estimated with calibration
            this->set_busy(18086488739);

            return true;
        }
    }

    return false;
}



void Mx25::handle_data_byte(uint8_t data)
{
    if (this->current_command == 0x5fa || this->current_command == 0x5)
    {
        // Return status register
        this->pending_value = (this->write_enable << 1) | this->busy;
    }
    else if (this->current_command == 0x72 || this->current_command == 0x728d)
    {
        if (this->current_address == 0)
        {
            // Return configuration register 2 address 0 value
            int spi_mode = this->pending_value & 0x3;

            this->ospi_mode = spi_mode != 0;
            this->dtr_mode = spi_mode == 2;

            this->trace.msg(vp::trace::LEVEL_INFO,
                "Writing configuration register 2 (ospi_mode: %d, dtr mode: %d)\n",
                this->ospi_mode, this->dtr_mode);
        }
        else
        {
            this->trace.force_warning(
                "Unsupported address for configuration register 2 (address: 0x%x)\n",
                this->current_address);
        }
    }
    else if (this->current_command == 0x12ed || this->current_command == 0x12 ||
        this->current_command == 0xee11 || this->current_command == 0xc ||
        this->current_command == 0xec13)
    {
        // Read or program operation
        if (!this->busy)
        {
                this->pending_value = this->handle_array_access(
                    this->current_address, this->is_write, this->pending_value);
            this->current_address++;
        }
    }
    else
    {
        this->trace.force_warning("Received data while command does not expect data\n");
    }
}



void Mx25::handle_input_data(int data)
{
    // Push the input bits to the pending value.
    // The flash can receive either in 8 bits or 1 bit mode
    if (this->ospi_mode)
    {
        this->pending_bits -= 8;
        this->pending_value = (this->pending_value << 8) | data;
    }
    else
    {
        this->pending_bits--;
        this->pending_value = (this->pending_value << 1) | data;
    }
}



void Mx25::send_output_data()
{
    // Send the next chunk of data from the pending value
    // The flash send either in 8 bits or 1 bit mode.
    if (this->ospi_mode)
    {
        this->in_itf.sync_cycle(this->pending_value);
        this->pending_bits -= 8;
    }
    else
    {
        this->in_itf.sync_cycle((this->pending_value >> 7) & 1);
        this->pending_bits--;
        this->pending_value <<= 1;
    }

    // Since the flash will send data until the CS becomes inactive, we need to rearm the send.
    if (this->pending_bits == 0)
    {
        this->pending_bits = 8;
    }
}



void Mx25::sync_cycle(int data)
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

    if (this->octospi_state == MX25_STATE_CMD)
    {
        // State when we are sampling the command header

        this->trace.msg(vp::trace::LEVEL_TRACE,
            "Received command byte (value: 0x%x, pending_bits: %d)\n",
            data, this->pending_bits);

        // Queue the incoming bits, this function will take care of the line width
        this->handle_input_data(data);

        // Only do something once the full command is received
        if (this->pending_bits == 0)
        {
            // Parse the command. This will in particular compute the number of address bits
            // to be received
            int addr_bits;
            
            this->parse_command(addr_bits);

            // Check that we are not trying to write without having enabled the write
            if (this->is_write && !this->write_enable)
            {
                this->trace.force_warning("Trying to write without write enable\n");
            }

            bool command_done = this->handle_command_header();

            // Now compute the next state
            if (command_done)
            {
                this->octospi_state = MX25_STATE_DONE;
            }
            else
            {
                // There are commands with and without an address
                if (addr_bits)
                {
                    this->pending_bits = addr_bits;
                    this->octospi_state = MX25_STATE_ADDR;
                }
                else
                {
                    this->pending_bits = 8;
                    this->octospi_state = MX25_STATE_DATA;
                }
            }
        }
    }
    else if (this->octospi_state == MX25_STATE_ADDR)
    {
        // State where we are sampling the command address

        this->trace.msg(vp::trace::LEVEL_TRACE,
            "Received address byte (value: 0x%x, addr_count: %d)\n",
            data, this->pending_bits);

        // Queue the incoming bits, this function will take care of the line width
        this->handle_input_data(data);

        // Only do something once the full address is received
        if (this->pending_bits == 0)
        {
            // Handle the command now that we have the address
            bool command_done = this->handle_command_address();

            // Now compute the next state
            if (command_done)
            {
                this->octospi_state = MX25_STATE_DONE;
            }
            else
            {
                this->pending_bits = 8;
                this->octospi_state = MX25_STATE_DATA;
            }
        }

    }
    else if (this->octospi_state == MX25_STATE_DATA)
    {
        // State where we are receiving or sending data

        // First check if we need to skip data due to ongoing latency
        if (this->latency_count > 0)
        {
            // The latency count has already been multipled by 2 in case we are in DDR mode
            this->latency_count--;
        }
        else
        {
            // Otherwise take care of the data

            // In case we have a write operation, we need to queue the input data so that
            // we can handle it once we have received a full byte
            if (this->is_write)
            {
                // Queue the incoming bits, this function will take care of the line width
                this->handle_input_data(data);
            }

            // We need the command to handle the data either if we have received a full byte with
            // a write operation, or when we start sending a byte with a read operation
            if (this->is_write && this->pending_bits == 0 ||
                !this->is_write && this->pending_bits == 8)
            {
                if (this->is_write)
                {
                    this->pending_bits = 8;
                }

                this->handle_data_byte(data);
            }

            // For read operation the command handling should have prepared the byte to be sent,
            // now need to stream it to the interface at each clock edge
            if (!this->is_write)
            {
                this->send_output_data();
            }

        }
    }
    else if (this->octospi_state == MX25_STATE_DONE)
    {
        // This state is just here to catch errors from the SW where the command is longer
        // than expected. Only chip select update can make use leave this state.
        this->trace.force_warning("Received clock edge while command is over\n");
    }
}



void Mx25::cs_sync(bool value)
{
    // This method is called whenever the chip select is updated.
    // The chip select value is active low.

    this->trace.msg(vp::trace::LEVEL_TRACE, "Received CS sync (active: %d)\n", !value);

    if (value == 0)
    {
        // Case where chip select is active and we start a command.
        // Do all the required initializations.
        this->octospi_state = MX25_STATE_CMD;
        this->pending_value = 0;

        // Commands are 16bits in octospi mode and 8bits in single spi mode.
        if (this->ospi_mode)
        {
            this->pending_bits = 16;
        }
        else
        {
            this->pending_bits = 8;
        }
    }
    else
    {
        // Chip select is inactive, take care of properly timing the program operation.
        // This is done now since we need to know the size of the program operation, and we know it
        // only once the chip select becomes inactive.

        if (this->program_ongoing)
        {
            this->program_ongoing = false;

            // Timing has been determined with calibration.
            int fixed = 8533036;
            int duration = fixed + (float)(192025544 - fixed) * this->program_size / 256;
            if (this->program_size <= 16)
            {
                duration += 2000000;
            }
            if (this->program_size <= 4)
            {
                duration += 3000000;
            }

            // Set the flash as busy during the estimated duration so that no other operation is
            // done.
            this->set_busy(duration);
        }
    }
}



int Mx25::preload_file(char *path, bool writeback)
{
    this->get_trace()->msg(vp::trace::LEVEL_INFO,
        "Preloading memory with stimuli file (path: %s)\n", path);

    if (writeback)
    {
        // Writeback mode where the file is kept synced with the flash array so that it is still
        // available at the end of the simulation.

        // Create a read/write file and map it in the process to directly access the file
        // from the flash array
        int fd = open(path, O_RDWR | O_CREAT, 0600);
        if (fd < 0)
        {
            this->trace.force_warning("Unable to open writeback file (path: %s, error: %s)\n",
                path, strerror(errno));
            return -1;
        }

        if (ftruncate(fd, this->size) < 0)
        {
            this->trace.force_warning("Unable to truncate writeback file (path: %s, error: %s)\n",
                path, strerror(errno));
            close(fd);
            return -1;
        }
        this->data = (uint8_t *)mmap(NULL, this->size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (!this->data)
        {
            this->trace.force_warning("Unable to mmap writeback file (path: %s, error: %s)\n",
                path, strerror(errno));
            close(fd);
            return -1;
        }

        // fd is even not useful anymore and can be closed.
        // Data are automatically write back to the file
        // at random time during execution (depending of the kernel cache behavior)
        // and, anyway, at the termination of the application.
        close(fd);
    }
    else
    {
        // Classic mode where the file is just used as an input file for the flash array.
        FILE *file = fopen(path, "r");
        if (file == NULL)
        {
            this->trace.force_warning("Unable to open preload file (path: %s, error: %s)\n",
                path, strerror(errno));
            return -1;
        }

        if (fread(this->data, 1, this->size, file) == 0)
        {
            this->trace.force_warning
                ("Unable to read from preload file file (path: %s, error: %s)\n",
                path, strerror(errno));
            return -1;
        }
    }

    return 0;
}



void Mx25::sync_cycle_stub(void *__this, int data)
{
    // Stub for real method, just forward the call
    Mx25 *_this = (Mx25 *)__this;
    _this->sync_cycle(data);
}



void Mx25::cs_sync_stub(void *__this, bool value)
{
    // Stub for real method, just forward the call
    Mx25 *_this = (Mx25 *)__this;
    _this->cs_sync(value);
}


void Mx25::reset(bool active)
{
    // This function is called everytime the flash is reset.
    // Both active and inactive levels trigger a call.
    if (active)
    {
        // The time engine parent will take care of cleaning any ongoing activity with the events
        vp::time_scheduler::reset(active);

        // When reset is active, we must put back the flash into the initial state
        this->ospi_mode = false;
        this->dtr_mode = false;
        this->write_enable = false;
        this->busy = false;
        this->program_ongoing = false;
        this->latency_code = 0;
        this->latency = this->get_latency_from_code(this->latency_code);
    }
}


int Mx25::build()
{
    // This method is called when the simulated system is built.
    // We just need here to take care of anything which must be done once at platform startup.

    // Retrieve time engine so that we can post time events
    this->engine = (vp::time_engine*)this->get_service("time");

    // Trace for outputting debug messages
    traces.new_trace("trace", &trace, vp::DEBUG);

    // Input interface for exchanging octospi data
    in_itf.set_sync_cycle_meth(&Mx25::sync_cycle_stub);
    new_slave_port("input", &in_itf);

    // Input interface for chip select update
    cs_itf.set_sync_meth(&Mx25::cs_sync_stub);
    new_slave_port("cs", &cs_itf);

    js::config *conf = this->get_js_config();

    // Prepare the event for managing the periods where the flash is busy (program and erase).
    // It will be pushed when flash is busy and will put the flash back to available state.
    this->busy_event = this->time_event_new(Mx25::set_available_handler);

    // Now take care of the flash content
    js::config *preload_file_conf = conf->get("preload_file");
    bool writeback = this->get_js_config()->get_child_bool("writeback");
    this->size = conf->get("size")->get_int();

    this->trace.msg(vp::trace::LEVEL_INFO, "Building flash (size: 0x%x)\n", this->size);

    // If there is no preload file or if the preload file is a classi input file,
    // Allocate an array for the flash and fill it with clean state which is 1 everywhere so that
    //. the whole flash can be programmed without being erased.
    if (!preload_file_conf || !writeback)
    {
        this->data = new uint8_t[this->size];
        memset(this->data, 0xff, this->size);
    }

    // The preload file can be either input file, or memory-mapped file to keep flash content
    // on workstation
    if (preload_file_conf)
    {
        if (this->preload_file((char *)preload_file_conf->get_str().c_str(), writeback))
        {
            return -1;
        }
    }

    return 0;
}



Mx25::Mx25(js::config *config)
    : vp::time_scheduler(config)
{
}



// Constructor function needed by GVSOC to instantiate this module.
// Just instantiate the flash class.
extern "C" vp::component *vp_constructor(js::config *config)
{
    return new Mx25(config);
}
