// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

// ATXP032 octal SPI NOR flash, at command level (wire<SpiflashCmd*>,
// devices/spiflash/spiflash_cmd.hpp): the flash controller computes the bus
// timing and hands over each command once its data phase is over.
//
// A bulk read returns the array whatever its opcode: the read commands only
// differ in their bus format, which the controller accounts for. Programs,
// erases and status register writes need a write enable first. They are done
// in the array at once, but the flash then reports busy in its status
// register for the time they take on the device (Atxp032Config), and ignores
// the other programs, erases and write enables meanwhile.

#include <vp/vp.hpp>
#include <vp/itf/wire.hpp>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <devices/spiflash/atxp032/atxp032_config.hpp>
#include "devices/spiflash/spiflash_cmd.hpp"


// Opcodes (first byte of the command)
#define OP_WRITE_ENABLE      0x06
#define OP_WRITE_DISABLE     0x04
#define OP_READ_STATUS       0x05
#define OP_READ_ID           0x9F
#define OP_ERASE_4K          0x20
#define OP_ERASE_32K         0x52
#define OP_ERASE_64K         0xD8
#define OP_ERASE_CHIP        0x60
#define OP_ERASE_CHIP_ALT    0xC7

// Status register byte 1
#define STATUS_BUSY          (1 << 0)
#define STATUS_WEL           (1 << 1)
#define OP_WRITE_STATUS      0x01

// Manufacturer and device ID of the ATXP032, in the order the bytes are sent:
// seven continuation codes, then the Adesto/Renesas ID and the device ID.
static const uint8_t atxp032_id[] = {
    0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x7F, 0x43, 0xA7, 0x00, 0x01, 0x00
};


class Atxp032 : public vp::Component
{
public:
    Atxp032(vp::ComponentConf &config);
    ~Atxp032();

    Atxp032Config cfg;

private:
    static void cmd_sync(vp::Block *__this, SpiflashCmd *cmd);
    void erase(uint32_t addr, uint32_t size, int64_t duration_ns);
    void single(SpiflashCmd *cmd, uint8_t opcode);
    bool busy();
    void start_busy(int64_t duration_ns);
    bool check_busy(uint8_t opcode);

    vp::Trace trace;
    vp::WireSlave<SpiflashCmd *> in;
    uint8_t *array;
    bool wel = false;
    // Time in ps at which the running program or erase is over
    int64_t busy_end = 0;
};


Atxp032::Atxp032(vp::ComponentConf &config)
    : vp::Component(config, this->cfg)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);

    this->in.set_sync_meth(&Atxp032::cmd_sync);
    this->new_slave_port("input", &this->in);

    // An erased NOR array reads as all ones.
    this->array = new uint8_t[this->cfg.size];
    memset(this->array, 0xFF, this->cfg.size);

    // The image is set by the board only when the flash has content.
    if (this->cfg.image != nullptr)
    {
        FILE *file = fopen(this->cfg.image, "rb");
        if (file == NULL)
        {
            this->trace.fatal("Unable to open flash image: %s, %s\n", this->cfg.image,
                strerror(errno));
            return;
        }
        size_t len = fread(this->array, 1, this->cfg.size, file);
        fclose(file);
        this->trace.msg(vp::Trace::LEVEL_INFO, "Preloaded %ld bytes from %s\n", len,
            this->cfg.image);
    }
}


Atxp032::~Atxp032()
{
    delete[] this->array;
}


bool Atxp032::busy()
{
    return (int64_t)this->time.get_time() < this->busy_end;
}


void Atxp032::start_busy(int64_t duration_ns)
{
    this->busy_end = this->time.get_time() + duration_ns * 1000;
    this->trace.msg(vp::Trace::LEVEL_DEBUG, "Busy (duration: %ld ns)\n", duration_ns);
}


// The operations which change the array or the status are ignored while
// busy: returns true if this one is
bool Atxp032::check_busy(uint8_t opcode)
{
    if (this->busy())
    {
        this->trace.force_warning("Command while busy ignored (opcode: 0x%x)\n", opcode);
        return true;
    }
    return false;
}


void Atxp032::erase(uint32_t addr, uint32_t size, int64_t duration_ns)
{
    if (!this->wel)
    {
        this->trace.force_warning("Erase without write enable (addr: 0x%x)\n", addr);
        return;
    }

    addr &= ~(size - 1);
    if (addr + size <= this->cfg.size)
    {
        this->trace.msg(vp::Trace::LEVEL_DEBUG, "Erase (addr: 0x%x, size: 0x%x)\n", addr, size);
        memset(this->array + addr, 0xFF, size);
    }
    this->start_busy(duration_ns);
}


void Atxp032::single(SpiflashCmd *cmd, uint8_t opcode)
{
    if (cmd->is_write)
    {
        if (this->check_busy(opcode))
        {
            return;
        }
        switch (opcode)
        {
            case OP_WRITE_ENABLE:  this->wel = true; break;
            case OP_WRITE_DISABLE: this->wel = false; break;
            case OP_ERASE_4K:      this->erase(cmd->addr, 4 * 1024, this->cfg.erase_4k_ns); break;
            case OP_ERASE_32K:     this->erase(cmd->addr, 32 * 1024, this->cfg.erase_32k_ns); break;
            case OP_ERASE_64K:     this->erase(cmd->addr, 64 * 1024, this->cfg.erase_64k_ns); break;
            case OP_ERASE_CHIP:
            case OP_ERASE_CHIP_ALT:
                this->erase(0, this->cfg.size, this->cfg.erase_chip_ns);
                break;
            case OP_WRITE_STATUS:
                // The status itself is not modelled, only its write time
                if (this->wel)
                {
                    this->start_busy(this->cfg.write_status_ns);
                }
                break;
            default:
                // Status/control register writes, resets, mode switches: the
                // array and the bus format do not depend on them here.
                this->trace.msg(vp::Trace::LEVEL_DEBUG, "Ignored command (opcode: 0x%x)\n",
                    opcode);
                break;
        }
        return;
    }

    memset(cmd->data, 0, cmd->size);
    switch (opcode)
    {
        case OP_READ_STATUS:
        {
            bool busy = this->busy();
            uint8_t status = (this->wel ? STATUS_WEL : 0) | (busy ? STATUS_BUSY : 0);
            for (uint32_t i = 0; i < cmd->size; i++)
            {
                cmd->data[i] = status;
            }
            break;
        }
        case OP_READ_ID:
            for (uint32_t i = 0; i < cmd->size && i < sizeof(atxp032_id); i++)
            {
                cmd->data[i] = atxp032_id[i];
            }
            break;
        default:
            this->trace.msg(vp::Trace::LEVEL_DEBUG, "Register read returns 0 (opcode: 0x%x)\n",
                opcode);
            break;
    }
}


void Atxp032::cmd_sync(vp::Block *__this, SpiflashCmd *cmd)
{
    Atxp032 *_this = (Atxp032 *)__this;
    uint8_t opcode = cmd->opcode >> 8;

    // The write enable latch is cleared at the end of the operation
    if (_this->busy_end != 0 && !_this->busy())
    {
        _this->wel = false;
        _this->busy_end = 0;
    }

    _this->trace.msg(vp::Trace::LEVEL_DEBUG,
        "Command (opcode: 0x%x, single: %d, is_write: %d, addr: 0x%x, size: 0x%x)\n",
        cmd->opcode, cmd->single, cmd->is_write, cmd->addr, cmd->size);

    if (cmd->single)
    {
        _this->single(cmd, opcode);
        return;
    }

    if ((uint64_t)cmd->addr + cmd->size > _this->cfg.size)
    {
        _this->trace.force_warning("Out-of-bound access (addr: 0x%x, size: 0x%x)\n",
            cmd->addr, cmd->size);
        cmd->error = true;
        return;
    }

    if (!cmd->is_write)
    {
        memcpy(cmd->data, _this->array + cmd->addr, cmd->size);
        return;
    }

    // Page program: a NOR cell only goes from 1 to 0.
    if (_this->check_busy(opcode))
    {
        return;
    }
    if (!_this->wel)
    {
        _this->trace.force_warning("Program without write enable (addr: 0x%x)\n", cmd->addr);
        return;
    }
    for (uint32_t i = 0; i < cmd->size; i++)
    {
        _this->array[cmd->addr + i] &= cmd->data[i];
    }
    int64_t duration = (int64_t)_this->cfg.program_byte_ns * cmd->size;
    if (duration > _this->cfg.program_page_ns)
    {
        duration = _this->cfg.program_page_ns;
    }
    _this->start_busy(duration);
}


extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new Atxp032(config);
}
