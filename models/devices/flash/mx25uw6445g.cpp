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
#include <stdio.h>
#include <string.h>

#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <vp/itf/hyper.hpp>
#include <vp/itf/wire.hpp>

#define REGS_AREA_SIZE 1024

#define FLASH_STATE_IDLE 0
#define FLASH_STATE_WRITE_BUFFER_WAIT_SIZE 1
#define FLASH_STATE_WRITE_BUFFER 2
#define FLASH_STATE_WRITE_BUFFER_WAIT_CONFIRM 3
#define FLASH_STATE_CMD_0 4
#define FLASH_STATE_CMD_1 5
#define FLASH_STATE_LOAD_VCR 6

#define FLASH_SECTOR_SIZE (1 << 18)

typedef enum
{
    MX25UW6445G_STATE_WAIT_CMD0,
    MX25UW6445G_STATE_WAIT_CMD1,
    MX25UW6445G_STATE_WAIT_CMD2,
    MX25UW6445G_STATE_WAIT_CMD3,
    MX25UW6445G_STATE_WAIT_CMD4,
    MX25UW6445G_STATE_WAIT_CMD5,
    MX25UW6445G_STATE_PROGRAM_START,
    MX25UW6445G_STATE_PROGRAM,
    MX25UW6445G_STATE_GET_STATUS_REG
} mx25uw6445g_state_e;

typedef enum
{
    OCTOSPI_STATE_CMD,
    OCTOSPI_STATE_ADDR,
    OCTOSPI_STATE_LATENCY,
    OCTOSPI_STATE_DATA
} octospi_state_e;

class Mx25uw6445g : public vp::component
{

public:
    int build();

    Mx25uw6445g(js::config *config);

    void handle_access(int address, int read, uint8_t data);
    int preload_file(char *path);
    void erase_sector(unsigned int addr);
    void erase_chip();
    int setup_writeback_file(const char *path);

    static void sync_cycle(void *_this, int data);
    static void cs_sync(void *__this, bool value);

    int get_nb_word() { return nb_word; }

private:
    int get_latency_from_code(uint32_t code);

    vp::trace trace;
    vp::hyper_slave in_itf;
    vp::wire_slave<bool> cs_itf;

    int size;
    uint8_t *data;
    bool data_is_mmapped;
    uint8_t *reg_data;

    mx25uw6445g_state_e state;
    octospi_state_e octospi_state;
    int pending_bytes;
    uint16_t pending_cmd;

    uint32_t cmd;
    uint32_t addr;
    int cmd_count;
    int addr_count;

    int current_address;
    int reg_access;

    bool burst_write = false;
    int nb_word = -1;
    int sector;

    bool ospi_mode;
    bool dtr_mode;
    bool write_enable;

    bool is_register_access;
    bool is_write;

    int latency;
    uint32_t latency_code;
    int latency_count;

    uint32_t confreg_2_0;
};

void Mx25uw6445g::erase_sector(unsigned int addr)
{
    addr &= ~(FLASH_SECTOR_SIZE - 1);

    this->trace.msg(vp::trace::LEVEL_INFO, "Erasing sector (address: 0x%x)\n", addr);

    if (addr >= this->size)
    {
        this->warning.force_warning("Received out-of-bound erase request (addr: 0x%x, flash_size: 0x%x)\n", addr, this->size);
        return;
    }

    memset(&this->data[addr], 0xff, FLASH_SECTOR_SIZE);
}

void Mx25uw6445g::erase_chip()
{
    this->trace.msg(vp::trace::LEVEL_INFO, "Erasing chip\n");
    for (unsigned int addr = 0; addr < addr + this->size; addr += FLASH_SECTOR_SIZE)
    {
        this->erase_sector(addr);
    }
}

void Mx25uw6445g::handle_access(int address, int is_write, uint8_t data)
{
    if (address >= this->size)
    {
        this->warning.force_warning("Received out-of-bound request (addr: 0x%x, flash_size: 0x%x)\n", address, this->size);
    }
    else
    {
        if (!is_write)
        {
            uint8_t data;
            if (this->state == MX25UW6445G_STATE_GET_STATUS_REG)
            {
                data = this->pending_cmd;
                this->pending_bytes--;
                this->pending_cmd >>= 8;
                if (this->pending_bytes == 0)
                    this->state = MX25UW6445G_STATE_WAIT_CMD0;
                this->trace.msg(vp::trace::LEVEL_TRACE, "Sending data byte (value: 0x%x)\n", data);
            }
            else
            {
                data = this->data[address];
                this->trace.msg(vp::trace::LEVEL_TRACE, "Sending data byte (address: 0x%x, value: 0x%x)\n", address, data);
            }
            this->in_itf.sync_cycle(data);
        }
        else
        {
            this->trace.msg(vp::trace::LEVEL_TRACE, "[Word Programming] Writing to flash (address: 0x%x, value: 0x%x)\n", address, data);

            uint8_t new_value = this->data[address] & data;

            if (new_value != data)
            {
                this->warning.force_warning("Failed to program specified location (addr: 0x%x, flash_val: 0x%2.2x, program_val: 0x%2.2x)\n", address, this->data[address], data);
            }

            this->data[address] &= data;
        }
    }
}

int Mx25uw6445g::preload_file(char *path)
{
    this->get_trace()->msg(vp::trace::LEVEL_INFO, "Preloading memory with stimuli file (path: %s)\n", path);

    if (this->get_js_config()->get_child_bool("writeback"))
    {
        int fd = open(path, O_RDWR | O_CREAT, 0600);
        if (fd < 0)
        {
            printf("Unable to open writeback file (path: %s, error: %s)\n", path, strerror(errno));
            return 0;
        }

        if (ftruncate(fd, this->size) < 0)
        {
            printf("Unable to truncate writeback file (path: %s, error: %s)\n", path, strerror(errno));
            close(fd);
            return -1;
        }
        this->data = (uint8_t *)mmap(NULL, this->size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (!this->data)
        {
            printf("Unable to mmap writeback file (path: %s, error: %s)\n", path, strerror(errno));
            close(fd);
            return -1;
        }

        /*
         * fd is even not useful anymore and can be closed.
         * Data are automatically write back to the file
         * at random time during execution (depending of the kernel cache behavior)
         * and, anyway, at the termination of the application.
         */
        close(fd);
    }
    else
    {
        FILE *file = fopen(path, "r");
        if (file == NULL)
        {
            printf("Unable to open stimulus file (path: %s, error: %s)\n", path, strerror(errno));
            return -1;
        }

        if (fread(this->data, 1, this->size, file) == 0)
            return -1;
    }

    return 0;
}

/*
 * Bback the data memory to a mmap file to provide access to the mx25uw6445g content
 * at the end of the execution.
 */
int Mx25uw6445g::setup_writeback_file(const char *path)
{
    this->get_trace()->msg("writeback memory to an output file (path: %s)\n", path);
    int fd = open(path, O_RDWR | O_CREAT, 0600);
    if (fd < 0)
    {
        printf("Unable to open writeback file (path: %s, error: %s)\n", path, strerror(errno));
        return 0;
    }

    if (ftruncate(fd, this->size) < 0)
    {
        printf("Unable to truncate writeback file (path: %s, error: %s)\n", path, strerror(errno));
        close(fd);
        return -1;
    }
    uint8_t *mmapped_data = (uint8_t *)mmap(NULL, this->size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (!mmapped_data)
    {
        printf("Unable to mmap writeback file (path: %s, error: %s)\n", path, strerror(errno));
        close(fd);
        return -1;
    }

    /* copy the current data content into the mmap area and replace data pointer with the mmap pointer */
    memcpy(mmapped_data, this->data, this->size);
    free(this->data);
    this->data = mmapped_data;
    this->data_is_mmapped = true;

    /*
     * fd is even not useful anymore and can be closed.
     * Data are automatically write back to the file
     * at random time during execution (depending of the kernel cache behavior)
     * and, anyway, at the termination of the application.
     */
    close(fd);
    return 0;
}

Mx25uw6445g::Mx25uw6445g(js::config *config)
    : vp::component(config)
{
}

void Mx25uw6445g::sync_cycle(void *__this, int data)
{
    Mx25uw6445g *_this = (Mx25uw6445g *)__this;

    if (_this->octospi_state == OCTOSPI_STATE_CMD)
    {
        _this->trace.msg(vp::trace::LEVEL_TRACE, "Received command byte (value: 0x%x)\n", data);

        if (_this->ospi_mode)
        {
            _this->cmd_count -= 8;
            _this->cmd = (_this->cmd << 8) | data;
        }
        else
        {
            _this->cmd_count--;
            _this->cmd = (_this->cmd << 1) | data;
        }

        if (_this->cmd_count == 0)
        {
            _this->trace.msg(vp::trace::LEVEL_TRACE, "Handling command (cmd: 0x%x)\n", _this->cmd);

            _this->is_write = false;
            _this->is_register_access = false;

            // Write enable
            if (_this->cmd == 0x06 || _this->cmd == 0x06f9)
            {
                _this->write_enable = true;
                _this->addr_count = 0;
            }
            // Write configuration register 2
            else if (_this->cmd == 0x72)
            {
                _this->is_register_access = true;
                _this->is_write = true;
                _this->addr_count = 32;
            }
            // Erase sector
            else if (_this->cmd == 0x21de)
            {
                _this->addr_count = 32;
            }
            // Read status
            else if (_this->cmd == 0x05fa)
            {
                _this->addr_count = 32;
                _this->latency_count = 4;
            }
            // Program
            else if (_this->cmd == 0x12ed)
            {
                _this->is_write = true;
                _this->addr_count = 32;
            }
            // Read
            else if (_this->cmd == 0xee11)
            {
                _this->addr_count = 32;
                _this->latency_count = _this->latency;
            }
            else
            {
                _this->trace.fatal("Received unknown flash command (cmd: 0x%x)\n", _this->cmd);
            }

            if (_this->is_write && !_this->write_enable)
            {
                _this->trace.fatal("Trying to write without write enable\n");
            }


            if (_this->addr_count)
            {
                _this->octospi_state = OCTOSPI_STATE_ADDR;
            }
            else
            {
                _this->octospi_state = OCTOSPI_STATE_DATA;
            }
        }
    }
    else if (_this->octospi_state == OCTOSPI_STATE_ADDR)
    {
        if (_this->ospi_mode)
        {
            _this->addr_count -= 8;
            _this->addr = (_this->addr << 8) | data;
        }
        else
        {
            _this->addr_count--;
            _this->addr = (_this->addr << 1) | data;
        }

        if (_this->addr_count == 0)
        {
            _this->current_address = _this->addr;

            if (_this->cmd == 0x21de)
            {
                _this->erase_sector(_this->addr);
                _this->write_enable = false;
            }
            _this->octospi_state = OCTOSPI_STATE_DATA;
        }

        _this->latency_count *= 2;
    }
    else if (_this->octospi_state == OCTOSPI_STATE_DATA)
    {
        if (_this->latency_count > 0)
        {
            _this->latency_count--;
        }
        else
        {
            if (_this->cmd == 0x5fa)
            {
                uint32_t status = _this->write_enable << 1;
                _this->in_itf.sync_cycle(status);
            }
            else if (_this->cmd == 0x72)
            {
                if (_this->addr == 0)
                {
                    _this->confreg_2_0 = data;
                    int spi_mode = data & 0x3;

                    _this->ospi_mode = spi_mode != 0;
                    _this->dtr_mode = spi_mode == 2;

                    _this->trace.msg(vp::trace::LEVEL_INFO, "Writing configuration register 2 (value: 0x%x, ospi_mode: %d, dtr mode: %d)\n",
                        _this->confreg_2_0, _this->ospi_mode, _this->dtr_mode);
                }
                else
                {
                    _this->trace.fatal("Unsupported address for configuration register 2 (address: 0x%x)\n", _this->addr);
                }
            }
            else
            {
                _this->handle_access(_this->current_address, _this->is_write, data);
                _this->current_address++;
            }
        }
    }
}

void Mx25uw6445g::cs_sync(void *__this, bool value)
{
    Mx25uw6445g *_this = (Mx25uw6445g *)__this;
    _this->trace.msg(vp::trace::LEVEL_TRACE, "Received CS sync (value: %d)\n", value);

    _this->octospi_state = OCTOSPI_STATE_CMD;
    _this->cmd = 0;

    // Commands are 16bits in octospi mode and 8bits in single spi mode
    if (_this->ospi_mode)
    {
        _this->cmd_count = 16;
    }
    else
    {
        _this->cmd_count = 8;
    }

    if (value == 0)
    {
        if (_this->state == MX25UW6445G_STATE_PROGRAM_START)
        {
            _this->state = MX25UW6445G_STATE_PROGRAM;
        }
        else if (_this->state == MX25UW6445G_STATE_PROGRAM)
        {
            _this->trace.msg(vp::trace::LEVEL_DEBUG, "End of program command (addr: 0x%x)\n", _this->current_address);

            if (_this->get_nb_word() < 0)
            {
                _this->state = MX25UW6445G_STATE_WAIT_CMD0;
            }
            else
            {
                _this->state = MX25UW6445G_STATE_PROGRAM;
            }
        }
    }
}

int Mx25uw6445g::get_latency_from_code(uint32_t code)
{
    return 20 - code * 2;
}

int Mx25uw6445g::build()
{
    traces.new_trace("trace", &trace, vp::DEBUG);

    in_itf.set_sync_cycle_meth(&Mx25uw6445g::sync_cycle);
    new_slave_port("input", &in_itf);

    cs_itf.set_sync_meth(&Mx25uw6445g::cs_sync);
    new_slave_port("cs", &cs_itf);

    js::config *conf = this->get_js_config();

    this->size = conf->get("size")->get_int();
    this->trace.msg(vp::trace::LEVEL_INFO, "Building flash (size: 0x%x)\n", this->size);

    this->data = new uint8_t[this->size];
    memset(this->data, 0xff, this->size);
    this->data_is_mmapped = false;

    this->reg_data = new uint8_t[REGS_AREA_SIZE];
    memset(this->reg_data, 0x57, REGS_AREA_SIZE);
    ((uint16_t *)this->reg_data)[0] = 0x8F1F;

    this->state = MX25UW6445G_STATE_WAIT_CMD0;
    this->pending_bytes = 0;
    this->pending_cmd = 0;
    this->ospi_mode = false;
    this->dtr_mode = false;
    this->write_enable = false;
    this->confreg_2_0 = 0;
    this->latency_count = 0;

    js::config *preload_file_conf = conf->get("preload_file");
    if (preload_file_conf == NULL)
    {
        preload_file_conf = conf->get("content/image");
    }

    if (preload_file_conf)
    {
        if (this->preload_file((char *)preload_file_conf->get_str().c_str()))
            return -1;
    }

    js::config *writeback_file_conf = conf->get("writeback_file");

    if (writeback_file_conf)
    {
        if (this->setup_writeback_file(writeback_file_conf->get_str().c_str()))
            return -1;
    }

    this->latency_code = 0;
    this->latency = this->get_latency_from_code(this->latency_code);

    return 0;
}

extern "C" vp::component *vp_constructor(js::config *config)
{
    return new Mx25uw6445g(config);
}
