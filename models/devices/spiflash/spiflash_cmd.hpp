// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

// Command-level interface between a SPI flash controller and the flash
// (wire<SpiflashCmd*>).
//
// One command per call, synchronous: the controller computes the bus timing
// itself and hands each command to the flash when its data phase is over.
// The flash executes it at once, reads into or writes from `data`.

#pragma once

#include <stdint.h>

struct SpiflashCmd
{
    bool     is_write;
    bool     single;      // single access (register, command) rather than bulk
    uint16_t opcode;      // first byte in bits 15:8 (second byte of a 2-byte opcode in 7:0)
    int      cmd_size;    // opcode bytes sent (0 to 2)
    uint32_t addr;        // flash byte address, right-aligned
    uint8_t *data;
    uint32_t size;        // data bytes, 0 for a command without data phase
    bool     error;       // set by the flash if it cannot execute the command
};
