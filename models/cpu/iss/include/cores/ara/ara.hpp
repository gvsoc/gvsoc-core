/*
 * Copyright (C) 2020 SAS, ETH Zurich and University of Bologna
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
 * Authors: Germain Haugou (germain.haugou@gmail.com)
 */

#pragma once

#include <queue>
#include "cpu/iss/include/types.hpp"
#include "vp/clock/clock_event.hpp"
#include "vp/register.hpp"

class Ara;
class IssWrapper;
class PendingInsn;

// This represents a generic HW block where vector instructions can be forwarded
class AraBlock : public vp::Block
{
public:
    AraBlock(Block *parent, std::string name) : vp::Block(parent, name) {}
    // False if the block can accept a new instruction
    virtual bool is_full() = 0;
    // Enqueue a new instruction to the block
    virtual void enqueue_insn(PendingInsn *pending_insn) = 0;
    // Initialize ISA decoding tree. Used to attach special handlers to instructions
    virtual void isa_init() {}
};

// Generic block for computation. Used for FPU and sliding to dispatch instructions in parallel
// and assign cost based on vector length, number of lanes and lmul
class AraVcompute : public AraBlock
{
public:
    AraVcompute(Ara &ara, std::string name);
    void reset(bool active) override;
    bool is_full() override { return this->insns.size() == 4; }
    void enqueue_insn(PendingInsn *pending_insn) override;

private:
    // Handler for internal FSM
    static void fsm_handler(vp::Block *__this, vp::ClockEvent *event);

    Ara &ara;
    // Used for this block system traces
    vp::Trace trace;
    // Event for active state
    vp::Trace event_active;
    // Event for PC of instruction being processed
    vp::Trace event_pc;
    // Event for label of instruction being processed
    vp::Trace event_label;
    // Clock event used for scheduling FSM handler when at least one instruction has to be processed
    vp::ClockEvent fsm_event;
    // Queue of pending instructions to be processed by this block
    // The block process them in-order
    std::queue<PendingInsn *> insns;
    // Current instruction being processed
    PendingInsn *pending_insn;
    // When the instruction is chained, this indicates the minimum cyclestamp where the instruction
    // can finished, based on operation duration.
    int64_t end_cyclestamp;
};

class AraVlsuPendingInsn
{
public:
    PendingInsn *insn;
    // Number of pending bursts. This is used to detect when the instruction is fully done.
    int nb_pending_bursts;
};

#if defined(CONFIG_GVSOC_ISS_USE_SPATZ)

// Block processing load/store vector instructions
class AraVlsu : public AraBlock
{
public:

    void reset(bool active) override;
    void start() override;
    AraVlsu(Ara &ara, IssWrapper &top);
    bool is_full() override { return this->nb_pending_insn.get() == AraVlsu::queue_size; }
    void enqueue_insn(PendingInsn *pending_insn) override;
    void isa_init() override;

private:
    // Handler for internal FSM
    static void fsm_handler(vp::Block *__this, vp::ClockEvent *event);
    // Handler called when a load instruction starts to be processed, in order to initialize the FSM
    // for a read burst
    static void handle_insn_load(AraVlsu *vlsu, iss_insn_t *insn);
    // Handler called when a write instruction starts to be processed, in order to initialize the FSM
    // for a write burst
    static void handle_insn_store(AraVlsu *vlsu, iss_insn_t *insn);
    // Handler called when a stride load instruction starts to be processed, in order to initialize
    // the FSM for a read burst
    static void handle_insn_load_strided(AraVlsu *vlsu, iss_insn_t *insn);
    // Handler called when a strided write instruction starts to be processed, in order to
    // initialize the FSM for a write burst
    static void handle_insn_store_strided(AraVlsu *vlsu, iss_insn_t *insn);

    void handle_access(iss_insn_t *insn, bool is_write, int reg, bool do_stride=false, iss_reg_t stride=0);

    // Number of instruction that can be enqueued at the same time
    static constexpr int queue_size = 4;

    Ara &ara;
    vp::Trace trace;
    // Event for active state
    vp::Trace event_active;
    // Event for address of current AXI burst
    std::vector<vp::Trace> event_addr;
    // Event for size of current AXI burst
    std::vector<vp::Trace> event_size;
    // Event for write or read opcode of current AXI burst
    std::vector<vp::Trace> event_is_write;
    // Event for PC of enqueued instructions
    vp::Trace event_queue;
    // Event for PC of instruction being processed
    vp::Trace event_pc;
    // Event for label of instruction being processed
    vp::Trace event_label;
    // Clock event used for scheduling FSM handler when at least one instruction has to be processed
    vp::ClockEvent fsm_event;
    // Queue of pending instructions to be processed by this block
    // The block process them in-order
    std::vector<AraVlsuPendingInsn> insns;
    // Address of the next burst to be sent
    iss_addr_t pending_addr;
    // Remaining size of the current load/store operation
    iss_addr_t pending_size;
    // Write or read of the current load/store operation
    bool pending_is_write;
    // Pointer to vector register file where next burst should read or written
    uint8_t *pending_velem;
    // Thsi indicates the vector register involved in the load/sotre operation.
    // Used for vector chaining to commit elements to correct register.
    int pending_vreg;
    // First valid instruction in the queue.
    int insn_first;
    // First valid instruction in the queue waiting to be started
    int insn_first_waiting;
    // Index in the queue where the next instruction should be pushed.
    int insn_last;
    // Number of enqueued instructions
    vp::Register<uint8_t> nb_pending_insn;
    // Number of instructions waiting to be started
    int nb_waiting_insn;
    // Ports to the TCDM, used by VLSU for vector load and store operations
    std::vector<vp::IoMaster> ports;
    // Queues of requests. Each port has its own queue to model limited oustanding requests
    std::vector<vp::Queue *> req_queues;
    // Whole list of requests for all ports
    std::vector<vp::IoReq> requests;
    // Number of TCDM ports
    int nb_ports;
    iss_reg_t stride;
    bool strided;
    int elem_size;
};

#else

// Block processing load/store vector instructions
class AraVlsu : public AraBlock
{
public:

    void reset(bool active) override;
    AraVlsu(Ara &ara, IssWrapper &top);
    bool is_full() override { return this->nb_pending_insn.get() == AraVlsu::queue_size; }
    void enqueue_insn(PendingInsn *pending_insn) override;
    void isa_init() override;

private:
    // Handler for internal FSM
    static void fsm_handler(vp::Block *__this, vp::ClockEvent *event);
    // Handler called when a load instruction starts to be processed, in order to initialize the FSM
    // for a read burst
    static void handle_insn_load(AraVlsu *vlsu, iss_insn_t *insn);
    // Handler called when a write instruction starts to be processed, in order to initialize the FSM
    // for a write burst
    static void handle_insn_store(AraVlsu *vlsu, iss_insn_t *insn);
    // Handler for asynchronous burst grants
    static void data_grant(vp::Block *__this, vp::IoReq *req);
    // Handler for asynchronous burst responses
    static void data_response(vp::Block *__this, vp::IoReq *req);
    void handle_burst_end(vp::IoReq *req);

    // Number of bursts which can be sent at the same time
    static constexpr int nb_burst = 8;
    // Number of instruction that can be enqueued at the same time
    static constexpr int queue_size = 4;

    Ara &ara;
    vp::Trace trace;
    // Event for active state
    vp::Trace event_active;
    // Event for address of current AXI burst
    vp::Trace event_addr;
    // Event for size of current AXI burst
    vp::Trace event_size;
    // Event for write or read opcode of current AXI burst
    vp::Trace event_is_write;
    // Event for PC of enqueued instructions
    vp::Trace event_queue;
    // Event for PC of instruction being processed
    vp::Trace event_pc;
    // Event for label of instruction being processed
    vp::Trace event_label;
    // Clock event used for scheduling FSM handler when at least one instruction has to be processed
    vp::ClockEvent fsm_event;
    // Queue of pending instructions to be processed by this block
    // The block process them in-order
    std::vector<AraVlsuPendingInsn> insns;
    // Address of the next burst to be sent
    iss_addr_t pending_addr;
    // Remaining size of the current load/store operation
    iss_addr_t pending_size;
    // Write or read of the current load/store operation
    bool pending_is_write;
    // Pointer to vector register file where next burst should read or written
    uint8_t *pending_velem;
    // Queue of available bursts for load/store
    std::queue<vp::IoReq *> free_bursts;
    // Memory interface for load/store bursts
    vp::IoMaster io_itf;
    // True if one burst was not granted. Once it is true, the block can not send any burst
    // anymore until the last one is granted
    bool stalled;
    // Bursts which have been handled synchronously with a delay. There are hold here until their
    // delay has elapsed
    std::queue<vp::IoReq *> delayed_bursts;
    // Timestamps where burst in delayed_bursts can be released
    std::queue<int64_t> delayed_bursts_timestamps;
    // First valid instruction in the queue.
    int insn_first;
    // First valid instruction in the queue waiting to be started
    int insn_first_waiting;
    // Index in the queue where the next instruction should be pushed.
    int insn_last;
    // Number of enqueued instructions
    vp::Register<uint8_t> nb_pending_insn;
    // Number of instructions waiting to be started
    int nb_waiting_insn;
};

#endif

// Ara top block
class Ara : public vp::Block
{
    friend class AraVcompute;

public:
    // List of sub-blocks processing instructions
    typedef enum
    {
        vlsu_id,
        vfpu_id,
        vslide_id,
        nb_blocks
    } blocks_e;

    Ara(IssWrapper &top, Iss &iss);

    void build();
    void reset(bool reset);
    // Called by ISS to initialize vector instructions in the ISA decoding tree to attach needed
    // handlers
    void isa_init();
    // Called by sub-blocks to notify the end of processing of an instruction. The instruction
    // The instruction handler is called, the instruction is removed ffrom the sequencer, and the
    // registers involved is updated
    void insn_end(PendingInsn *insn);
    // Called by the ISS to offload an instruction to Ara. The instruction is pushed to the queue
    // of pending instruction, analyzed, and push to a processing block for executing once
    // dependencies are resolved. Can be called only when queue is not full.
    void insn_enqueue(PendingInsn *insn);
    // Called by a unit to notify that part of the vector result has been produced.
    // Used for vector chaining to start another operation before the result is fully produced.
    void insn_commit(int reg, int size);
    // Return true when queue if full and ara can not accept new instructions
    bool queue_is_full() { return this->queue_full.get(); }
    // Return the CVA6 register value associated to the instruction being executed
    inline uint64_t current_insn_reg_get() { return current_insn_reg; }
    inline uint64_t current_insn_reg_2_get() { return current_insn_reg_2; }

    // Access to upper ISS
    Iss &iss;
    // Number of 64 bits lanes in Ara.
    int nb_lanes;

#if defined(CONFIG_GVSOC_ISS_USE_SPATZ)
    // Number of pending vector loads and stores in spatz. Use to synchronize with snitch memory
    // accesses
    int nb_pending_vaccess;
    // Number of pending vector stores in spatz. Use to synchronize with snitch memory accesses
    int nb_pending_vstore;
#endif

private:
    // Handler for internal FSM
    static void fsm_handler(vp::Block *__this, vp::ClockEvent *event);
    // Allocate a slot for an instruction being enqueued. This is used to duplicate the cva6
    // pending instruction to commit it early so that cva6 can use the slot for another instruction.
    PendingInsn *pending_insn_alloc(PendingInsn *cva6_pending_insn);

    // Size of the queue holding pending instructions. Once full, Ara can not accept instructions
    // from CVA6 anymore
    static constexpr int queue_size = 8;

    vp::Trace trace;
    // Event for active state
    vp::Trace event_active;
    // Event for PC of instruction being enqueued
    vp::Trace event_pc;
    // Event for PC of enqueued instructions
    vp::Trace event_queue;
    // Event for label of instruction being enqueued
    vp::Trace event_label;
    // Clock event used for scheduling FSM handler when at least one instruction has to be processed
    vp::ClockEvent fsm_event;
    // Number of instructions currently being processed by Ara. This is increased when an
    // instruction is enqueued, and decreased when it ends
    vp::Register<uint8_t> nb_pending_insn;
    // Number of instructions currently being processed by Ara which have not yet been dispatched
    // to their processing block, which means their dependencies have not been resolved yet
    vp::Register<uint8_t> nb_waiting_insn;
    // List of instructions currently being processed by Ara. This contains both the instructions
    // waiting for the resolution of their dependencies, and the instructions already dispatched
    // to the processing blocks.
    std::vector<PendingInsn> pending_insns;
    // Index of the first pending instruction in the queue
    int insn_first;
    // Index of the first pending instruction waiting for symbol resolution. They might be pending
    // instructions before this one which has been resolved, forwarded to their processing block,
    // and waiting for completion.
    int insn_first_waiting;
    // Index where the next pending instruction should be enqueued.
    int insn_last;
    // True if the queue is full and ara can not accept any other instruction
    vp::Register<bool> queue_full;
    // List of processing blocks
    std::vector<AraBlock *> blocks;
    // Scoreboard describing which registers are ready to be used as inputs.
    // This indicates the cyclestamp at which the register is ready. The register is available only
    // if the cyclestamp is lower or equal than the current cycle.
    // Any instruction which needs a non-available register as input is blocked until it becomes
    // available.
    int64_t scoreboard_valid_ts[ISS_NB_VREGS];
    // Scoreboard describing the number of instructions using each register.
    // Any instruction which needs to write a register which is being used is blocked until the
    // register is not used anymore
    unsigned int scoreboard_in_use[ISS_NB_VREGS];
    // Scoreboard describing the number of instructions using each register as output.
    unsigned int scoreboard_out_use[ISS_NB_VREGS];
    // Scoreboard describing which registers are currently used in vector chaining
    PendingInsn *scoreboard_chained[ISS_NB_VREGS];
    // Scoreboard describing for each vector how many elements are committed. Used for vector
    // chaining.
    unsigned int scoreboard_committed[ISS_NB_VREGS];
    // CVA6 register associated to the instruction being executed. This is used by instruction
    // handlers when they are executed. This needs to be buffered because CVA6 might have executed
    // following instructions overriding the register
    uint64_t current_insn_reg;
    uint64_t current_insn_reg_2;
};
