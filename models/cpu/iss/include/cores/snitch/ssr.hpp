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
 *          Kexin Li, ETH Zurich (likexi@ethz.ch)
 */

#pragma once

#include "cpu/iss/include/types.hpp"


class Ssr;


// The data lane in each data mover
class FifoSsr 
{
public:

    iss_freg_t lane[4] = {0, 0, 0, 0}; // Array to store the elements of the queue
    int head = 0; // Index of the first element
    int tail = 0; // Index where the next element will be inserted
    int size = 0; // Current number of elements in the queue


    // Constructor initializes an empty queue
    FifoSsr() : head(0), tail(0), size(0) {}

    // Function to add an element to fifo
    void push(iss_freg_t element) 
    {
        if (size == 4) 
        {
            printf("Fifo in data mover is full\n");
        }
        lane[tail] = element;
        tail = (tail + 1) % 4; // Circular increment
        ++size;
    }

    // Function to remove and return the first element from fifo
    iss_freg_t pop() 
    {
        if (size == 0) 
        {
            printf("Fifo in data mover is empty\n");
        }
        iss_freg_t element = lane[head];
        head = (head + 1) % 4; // Circular increment
        --size;
        return element;
    }

    // Function to check if the fifo is empty
    bool isEmpty()
    {
        return size == 0;
    }

    // Function to check if the fifo is full
    bool isFull()
    {
        return size == 4;
    }
};


// The SSR configuration registers
class CfgSsr 
{
public:

    int REG_STATUS = 0;
    int REG_REPEAT = 0;

    // Configuration of Address Generation Units with 4-dim nested loop
    int REG_BOUNDS[4] = {0, 0, 0, 0};
    int REG_STRIDES[4] = {0, 0, 0, 0};
    iss_reg_t REG_RPTR[4] = {0, 0, 0, 0};
    iss_reg_t REG_WPTR[4] = {0, 0, 0, 0};

    // Dimension of nested-loop data access pattern
    int DIM = 0;


    // Function to set the value of REG_STATUS
    void set_REG_STATUS(int value) 
    {
        REG_STATUS = value;
    }

    // Function to get the value of REG_STATUS
    int get_REG_STATUS() 
    {
        return REG_STATUS;
    }

    void set_REG_REPEAT(int value) 
    {
        REG_REPEAT = value;
    }

    int get_REG_REPEAT() 
    {
        return REG_REPEAT;
    }

    void set_REG_BOUNDS(int loop_id, int value) 
    {
        REG_BOUNDS[loop_id] = value;
    }

    int get_REG_BOUNDS(int loop_id)
    {
        return REG_BOUNDS[loop_id];
    }

    void set_REG_STRIDES(int loop_id, int value) 
    {
        REG_STRIDES[loop_id] = value;
    }

    int get_REG_STRIDES(int loop_id)
    {
        return REG_STRIDES[loop_id];
    }

    void set_REG_RPTR(int dim, iss_reg_t value) 
    {
        REG_RPTR[dim] = value;
    }

    iss_reg_t get_REG_RPTR(int dim) 
    {
        return REG_RPTR[dim];
    }

    void set_REG_WPTR(int dim, iss_reg_t value) 
    {
        REG_WPTR[dim] = value;
    }

    iss_reg_t get_REG_WPTR(int dim)
    {
        return REG_WPTR[dim];
    }
};


// The loop counters
class LoopCnt
{
public:

    // Stride counter of loop
    int stride[4] = {0, 0, 0, 0};
    // Bound counter of loop
    int bound[4] = {0, 0, 0, 0};
    // Repetition counter
    int rep = 0;
    // Status of dm counters
    int status = 0;

};


// The Credit Counter to determine whether there's need to read/write new data
class CreditCnt
{
public:

    // Maximum data of credits equals to the depth of data lane
    const int DataCredits = 4;

    // Data consumed and more empty space
    bool credit_give = false;
    // Data produced and less empty space
    bool credit_take = false;
    // Accept new data
    bool has_credit = false;
    // No space to accept new data
    bool credit_full = false;

    // Number of credits currently
    int credit_cnt = 0;
    void update_cnt();
};


// The Data Mover 
class DataMover
{
    friend class FifoSsr;
    friend class CfgSsr;
    friend class LoopCnt;
    friend class CreditCnt;

public:

    // Data Lane with depth of 4
    FifoSsr fifo;
    // Configuration registers
    CfgSsr config;
    // Credit counter for FIFO
    CreditCnt credit;
    // Generate loop counters
    LoopCnt counter;

    // Properties of data mover
    // Whether data mover is read/write
    bool is_write = false;
    // Whether data mover is active
    bool is_config = false;

    // Temporary variable used to get load/store result
    iss_freg_t temp = 0x0;

    // Address generation unit, compute address of memory access
    iss_reg_t temp_addr = 0x0;
    iss_reg_t inc_addr = 0x0;
    bool rep_done = true;
    bool ssr_done = false;
    iss_reg_t addr_gen_unit(bool is_write);

    // Update loop counters after every memory access
    bool dm_read = false;
    bool dm_write = false;
    void update_cnt();

    // When the fifo is full, data can't be written from computation unit to fifo any more.
    // Under this condition, the core need to stall and wait for empty space to write again.
    bool dm_write_full = false;

    // When the data is not preloaded, set the flag to true.
    bool dm_not_preload = false;
};
        

class Ssr
{
    friend class FifoSsr;
    friend class CfgSsr;
    friend class LoopCnt;
    friend class CreditCnt;
    friend class DataMover;

public:

    Ssr(Iss &iss);

    void build();
    void reset(bool active);

    vp::Trace trace;

    // Switch mapping 3 read streams and 1 write streams from fp regfile to 3 data movers
    // ft0 (ssr_fregs[0]) <-> data mover 0
    // ft1 (ssr_fregs[1]) <-> data mover 1
    // ft2 (ssr_fregs[2]) <-> data mover 2

    // Three data movers
    DataMover dm0;
    DataMover dm1;
    DataMover dm2;

    // Three memory ports
    // ssr_dm0 shares datapath with int core lsu and fp subsystem lsu.
    // ssr_dm1 and ssr_dm2 are private.
    vp::IoMaster ssr_dm0;
    vp::IoMaster ssr_dm1;
    vp::IoMaster ssr_dm2;
    
    // Data fetch request
    vp::IoReq io_req;

    // Three operands used in this round
    // Temporary variable stored for traces
    iss_freg_t ssr_fregs[3] = {0x0, 0x0, 0x0};

    // ssr enable and disable flag controlled by ssrcfg
    bool ssr_enable = false;
    // Enable or disable SSR in core
    void enable();
    void disable();

    // Set or get configuration registers in data mover
    void cfg_write(iss_insn_t *insn, int reg, int ssr, iss_reg_t value);
    iss_reg_t cfg_read(iss_insn_t *insn, int reg, int ssr);

    // Data request sent from SSR to memory, fetch operands 
    int data_req(iss_addr_t addr, uint8_t *data_ptr, int size, bool is_write, int64_t &latency, int dm);
    // Load or store fp data from or to memory
    inline bool load_float(iss_addr_t addr, uint8_t *data_ptr, int size, int dm);
    inline bool store_float(iss_addr_t addr, uint8_t *data_ptr, int size, int dm);

    // Read or write in data mover
    iss_freg_t dm_read(int dm);
    bool dm_write(iss_freg_t value, int dm);
    bool dm_write_again();

    // Update SSR after one instruction
    void clear_flags();
    void update_ssr();

    // Clear SSR counters and configurations after we have new configs coming
    void clear_ssr(int dm);

    // Each data lane has its own private event for data preloading and storing,
    // because they have their own memory ports.
    static void dm_event_0(vp::Block *__this, vp::ClockEvent *event);
    static void dm_event_1(vp::Block *__this, vp::ClockEvent *event);
    static void dm_event_2(vp::Block *__this, vp::ClockEvent *event);

    // Definition of three clock events
    vp::ClockEvent *event_0;
    vp::ClockEvent *event_1;
    vp::ClockEvent *event_2;

private:
    Iss &iss;

    vp::ClockEngine *engine;

};