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
#include <cpu/iss_v2/include/types.hpp>
#include <cpu/iss_v2/include/stats/insn_duration.hpp>
#include <vp/clock/clock_event.hpp>
#include <vp/register.hpp>

class Vu;
class IssWrapper;
class PendingInsn;

#define ISS_INSN_FLAGS_VECTOR (1<< 0)

class PendingInsn
{
public:
    InsnEntry *entry;
    uint64_t timestamp;
    uint64_t reg;
    uint64_t reg_2;
    uint64_t reg_3;
    bool valid;
    bool done;
    int id;
    bool in_can_be_chained;
    bool out_can_be_chained;
    int nb_bytes_done;
    float chaining_factor;
    float out_chaining_factor;
    int8_t inreg0_index;
    int8_t inreg1_index;
    int8_t inreg2_index;
    // Cycle at which the owning block really started executing this
    // instruction (not when it was enqueued/queued). -1 until then. Used to
    // measure per-label execution duration up to Vu::insn_end.
    int64_t exec_start_cycle;
    // FPU pipeline depth of this instruction (cycles between an operand
    // word entering the unit and its result word being written), derived
    // at issue time from the fpu_lat_class and the effective element
    // width. Dependent instructions trail this many extra cycles behind
    // (chaining gate) and the scoreboard entry is released this many
    // cycles after the last chunk, without blocking the issue of the
    // next instruction in the block.
    int pipeline_latency;
    // Cycle at which the last chunk of this instruction was executed
    // (all bytes committed), -1 while still executing. Used by the
    // chaining gate to time the consumer's trailing chunks against the
    // producer's pipeline drain instead of its scoreboard release.
    int64_t commit_done_cycle;
    // Earliest cycle at which this instruction may execute its next chunk,
    // accumulated from the pipeline drain of its (possibly already
    // released) input dependencies. Carried on the consumer because the
    // scoreboard entry of the producer is released before its pipeline has
    // drained (the release also gates WAW/WAR, which the RTL frees
    // per-word).
    int64_t chain_release_cycle;
    // Handler executing the instruction, the full or fast one depending on
    // the variant of the stub which enqueued it
    iss_reg_t (*handler)(Iss *, iss_insn_t *, iss_reg_t);
};

// This represents a generic HW block where vector instructions can be forwarded
class VuBlock : public vp::Block
{
public:
    VuBlock(Block *parent, std::string name) : vp::Block(parent, name) {}
    // False if the block can accept a new instruction
    virtual bool is_full() = 0;
    // Enqueue a new instruction to the block
    virtual void enqueue_insn(PendingInsn *pending_insn) = 0;
    // Initialize ISA decoding tree. Used to attach special handlers to instructions
    virtual void isa_init() {}
};

// Generic block for computation. Used for FPU and sliding to dispatch instructions in parallel
// and assign cost based on vector length, number of lanes and lmul
class VuCompute : public VuBlock
{
public:
    VuCompute(Vu &vu, std::string name);
    void reset(bool active) override;
    bool is_full() override { return this->insns.size() == 4; }
    void enqueue_insn(PendingInsn *pending_insn) override;

private:
    // Handler for internal FSM
    static void fsm_handler(vp::Block *__this, vp::ClockEvent *event);

    Vu &vu;
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
    // Instructions whose chunks have all been executed but which are still
    // draining through the FPU pipeline (plus their completion latency).
    // They do not block the issue of the next instruction; their
    // scoreboard entry is released once their timestamp has passed.
    std::vector<PendingInsn *> draining;
    // When the instruction is chained, this indicates the minimum cyclestamp where the instruction
    // can finished, based on operation duration.
    int64_t end_cyclestamp;
    int total_size;
    int vstart;
    int vend;
    // Which unit class (0: FPU, 1: IPU) executed the previous instruction,
    // used to model the datapath switch drain. -1 until the first one.
    int last_unit_class;
    // Cycle until which each unit class drains the pipeline of its last
    // instruction. A datapath switch must wait for the departing unit to
    // drain before the incoming instruction can start.
    int64_t unit_busy_until[2];
    // Instruction already charged with the switch drain, so that it is
    // applied only once
    PendingInsn *switch_charged;
};

class VuLsuPendingInsn
{
public:
    // Cycle at which the instruction entered the block queue. Used to
    // discount the dispatch latency already absorbed while waiting
    // behind the previous instruction's request stream.
    int64_t enqueue_cycle = 0;
public:
    PendingInsn *insn;
    // Number of pending bursts. This is used to detect when the instruction is fully done.
    int nb_pending_bursts;
    // Used by some blocks to flag the termination
    bool done;
};

#if defined(CONFIG_GVSOC_ISS_USE_SPATZ)

#if defined(CONFIG_GVSOC_ISS_VLSU_V2)

// Largest number of bytes a single VLSU request can carry. A request never
// spans more than one lane word, and the widest lane the model supports is
// 8 bytes; the per-request masking buffers are sized from this.
#define VLSU_MAX_REQ_SIZE 8


// Block processing load/store vector instructions, io_v2 variant.
//
// Functionally mirrors the v1 ``VuLsu`` (same FSM, same instruction handling)
// but talks to the TCDM through master ports built on ``vp/itf/io_v2.hpp``:
// status codes are ``IO_REQ_DONE``/``GRANTED``/``DENIED``, error reporting
// rides on the response status (``IO_RESP_OK``/``IO_RESP_INVALID``), and the
// retry/resp callbacks are mandatory (passed at port construction time).
//
// All three downstream forms are supported, so the VLSU ports can face a
// deny/retry arbiter such as ``interco.log_ico_v2``:
//   - DONE: completion is scheduled ``get_full_latency()`` cycles later
//     through the delayed-bursts priority queue.
//   - GRANTED: completion arrives through the resp() callback.
//   - DENIED: the request is parked on its port and re-issued synchronously
//     inside the retry() callback (mandatory io_v2 contract — the log ico
//     keeps its accept window open only for the duration of the retry call).
//     Only the denied port stalls; the other ports keep streaming.
class VuLsu : public VuBlock
{
public:

    void reset(bool active) override;
    void start() override;
    VuLsu(Vu &vu, Iss &iss);
    bool is_full() override { return this->nb_pending_insn.get() == VuLsu::queue_size; }
    void enqueue_insn(PendingInsn *pending_insn) override;
    void isa_init() override;

private:
    // Handler for internal FSM
    static void fsm_handler(vp::Block *__this, vp::ClockEvent *event);
    static void handle_insn_load(VuLsu *vlsu, iss_insn_t *insn);
    static void handle_insn_store(VuLsu *vlsu, iss_insn_t *insn);
    static void handle_insn_load_strided(VuLsu *vlsu, iss_insn_t *insn);
    static void handle_insn_store_strided(VuLsu *vlsu, iss_insn_t *insn);
    static void handle_insn_load_indexed(VuLsu *vlsu, iss_insn_t *insn);
    static void handle_insn_store_indexed(VuLsu *vlsu, iss_insn_t *insn);

    void handle_access(iss_insn_t *insn, bool is_write, int reg, bool do_stride=false, iss_reg_t stride=0, int reg_indexed=-1);

    // io_v2 master callbacks — pure ready/valid signal (no request) on retry,
    // and response notification on resp.
    static void port_retry_muxed(vp::Block *__this, int id, vp::IoRetryChannel);
    static vp::IoRespAck port_resp_muxed(vp::Block *__this, vp::IoReq *req, int id);

    // Called when a request has been accepted with DONE: either complete it
    // now or push it to the delayed-bursts queue for get_full_latency()
    // cycles.
    void handle_done(vp::IoReq *req);
    // Terminate one burst: commit its elements to the VRF (chaining) and
    // retire it from the per-port in-order ROB.
    void burst_done(vp::IoReq *req);

    // Number of instruction that can be enqueued at the same time
    static constexpr int queue_size = 4;

    Vu &vu;
    vp::Trace trace;
    vp::Trace event_active;
    std::vector<vp::Trace> event_addr;
    std::vector<vp::Trace> event_size;
    std::vector<vp::Trace> event_is_write;
    vp::Trace event_queue;
    vp::Trace event_pc;
    vp::Event event_label;
    vp::ClockEvent fsm_event;
    std::vector<VuLsuPendingInsn> insns;
    iss_addr_t pending_addr;
    iss_addr_t pending_size;
    bool pending_is_write;
    uint8_t *pending_velem;
    int pending_vreg;
    int insn_first;
    int insn_first_waiting;
    int insn_last;
    vp::Register<uint8_t> nb_pending_insn;
    int nb_waiting_insn;
    // Ports to the TCDM, used by VLSU for vector load and store operations.
    // io_v2 master ports require retry/resp callbacks at construction time;
    // we use the muxed variants so a single pair of callbacks dispatches by
    // port id.
    std::vector<vp::IoMaster> ports;
    // Queues of requests. Each port has its own queue to model limited
    // outstanding requests.
    std::vector<vp::Queue *> req_queues;
    // Whole list of requests for all ports
    std::vector<vp::IoReq> requests;
    // Per-port request denied by the downstream and waiting for its retry()
    // signal. A port with a parked request issues nothing else.
    std::vector<vp::IoReq *> denied_reqs;
    int nb_ports;
    iss_reg_t stride;
    bool strided;
    // Whether a unit-stride access is issued one element per request because
    // its base address is not aligned on the lane width
    bool single_element;
    // Single-element mode request sequencing. Like on RTL, each port owns
    // full lane-width words of the vector so that two elements sharing a
    // word are issued on the same port on consecutive cycles, instead of
    // conflicting on the same memory bank in the same cycle.
    iss_addr_t se_base_addr;
    uint8_t *se_base_velem;
    int se_base_vstart;
    int se_nb_elems;
    std::vector<int> se_port_count;
    int elem_size;
    int reg_indexed;
    int pending_elem;
    int inst_elem_size;
    int64_t op_timestamp;
    bool prev_is_write;
    // Whether the previous VLSU instruction was an indexed access, used to
    // model the index-fetch startup of back-to-back indexed streams
    bool prev_is_indexed;
    bool started;
    int vstart;
    // The on-going access is masked by v0 (its vm bit is clear). On RTL the
    // mask never suppresses a memory request -- it is applied as a byte
    // strobe on stores (mem_req_strb) and as a write byte enable on the VRF
    // write-back of loads (vrf_req_d.wbe), see spatz_vlsu.sv. The model does
    // the same, so masking costs no cycles and the request stream is
    // identical to the unmasked one.
    bool masked;


    // Ongoing instruction
    int insn_ongoing;

    // Bursts which have been handled synchronously with a delay. They are
    // held here until their delay has elapsed.
    struct DelayedBurst
    {
        vp::IoReq *req;
        uint64_t timestamp;
    };

    struct DelayedBurstCompare
    {
        bool operator()(const DelayedBurst &a, const DelayedBurst &b) const
        {
            return a.timestamp > b.timestamp;
        }
    };

    std::priority_queue<DelayedBurst, std::vector<DelayedBurst>, DelayedBurstCompare> delayed_bursts;

    // Per-port in-order reorder buffer entry. The request keeps a pointer to
    // its entry in ``initiator`` (io_v2 has no arg stack).
    struct VlsuRobEntry
    {
        // Port and id
        int port = 0;
        int rob_id = 0;

        // If the entry is allocated to a request
        bool allocated = false;

        // If the response is valid
        bool valid = false;

        // Request itself
        vp::IoReq *req = nullptr;

        // Instruction slot which issued the request
        VuLsuPendingInsn *slot = nullptr;

        // Vector register
        int vreg = 0;

        int elem_size = 0;
        int vstart = 0;
        int size = 0;

        // Masked access support. A masked LOAD lands in `scratch` instead of
        // straight in the vector register file, and the active elements are
        // merged into `vrf_dest` when the entry retires, so the inactive ones
        // keep their previous value (mask-undisturbed). A masked STORE points
        // the request's byte strobe at `strb`, so the target commits only the
        // active elements. `masked` is false for the common unmasked case, in
        // which neither buffer is touched.
        bool masked = false;
        uint8_t *vrf_dest = nullptr;
        // Sized for one request, which never exceeds the lane width.
        uint8_t scratch[VLSU_MAX_REQ_SIZE];
        uint8_t strb[VLSU_MAX_REQ_SIZE];
    };

    // Reorder buffer
    std::vector<std::vector<VlsuRobEntry>> rob;
    // Next available entry in the ROB for each port
    std::vector<int> rob_next;
    // The first entry in the ROB which is waiting for response for each port
    std::vector<int> rob_first;
    // Number of allocated entries in the ROB for each port
    std::vector<int> rob_count;
};

#else

// Block processing load/store vector instructions
class VuLsu : public VuBlock
{
public:

    void reset(bool active) override;
    void start() override;
    VuLsu(Vu &vu, Iss &iss);
    bool is_full() override { return this->nb_pending_insn.get() == VuLsu::queue_size; }
    void enqueue_insn(PendingInsn *pending_insn) override;
    void isa_init() override;

private:
    // Handler for internal FSM
    static void fsm_handler(vp::Block *__this, vp::ClockEvent *event);
    // Handler called when a load instruction starts to be processed, in order to initialize the FSM
    // for a read burst
    static void handle_insn_load(VuLsu *vlsu, iss_insn_t *insn);
    // Handler called when a write instruction starts to be processed, in order to initialize the FSM
    // for a write burst
    static void handle_insn_store(VuLsu *vlsu, iss_insn_t *insn);
    // Handler called when a stride load instruction starts to be processed, in order to initialize
    // the FSM for a read burst
    static void handle_insn_load_strided(VuLsu *vlsu, iss_insn_t *insn);
    // Handler called when a strided write instruction starts to be processed, in order to
    // initialize the FSM for a write burst
    static void handle_insn_store_strided(VuLsu *vlsu, iss_insn_t *insn);
    // Handler called when an indexed load instruction starts to be processed, in order to initialize
    // the FSM for a read burst
    static void handle_insn_load_indexed(VuLsu *vlsu, iss_insn_t *insn);
    // Handler called when an indexed write instruction starts to be processed, in order to
    // initialize the FSM for a write burst
    static void handle_insn_store_indexed(VuLsu *vlsu, iss_insn_t *insn);

    void handle_access(iss_insn_t *insn, bool is_write, int reg, bool do_stride=false, iss_reg_t stride=0, int reg_indexed=-1);

    // Handler for asynchronous burst grants
    static void data_grant(vp::Block *__this, vp::IoReq *req);
    // Handler for asynchronous burst responses
    static void data_response(vp::Block *__this, vp::IoReq *req);

    // Number of instruction that can be enqueued at the same time
    static constexpr int queue_size = 4;

    Vu &vu;
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
    vp::Event event_label;
    // Clock event used for scheduling FSM handler when at least one instruction has to be processed
    vp::ClockEvent fsm_event;
    // Queue of pending instructions to be processed by this block
    // The block process them in-order
    std::vector<VuLsuPendingInsn> insns;
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
    // Whether a unit-stride access is issued one element per request because
    // its base address is not aligned on the lane width
    bool single_element;
    // Single-element mode request sequencing. Like on RTL, each port owns
    // full lane-width words of the vector so that two elements sharing a
    // word are issued on the same port on consecutive cycles, instead of
    // conflicting on the same memory bank in the same cycle.
    iss_addr_t se_base_addr;
    uint8_t *se_base_velem;
    int se_base_vstart;
    int se_nb_elems;
    std::vector<int> se_port_count;
    int elem_size;
    int reg_indexed;
    int pending_elem;
    int inst_elem_size;
    int64_t op_timestamp;
    bool prev_is_write;
    // Whether the previous VLSU instruction was an indexed access, used to
    // model the index-fetch startup of back-to-back indexed streams
    bool prev_is_indexed;
    bool started;
    int vstart;
    // Instruction currently active in the VLSU. pending_insn->timestamp is reused across phases:
    // 1. as an enqueue-cycle guard, 2. as the request issuing start time after instruction latency, 
    // and 3. for memory-response/retirement timing. Keeping this index separate from insn_first_waiting 
    // makes the phase explicit and prevents queued instructions from consuming their instruction latency 
    // before they become active.
    int insn_ongoing;
    
    // True if one burst was not granted. Once it is true, the block can not send any burst
    // anymore until the last one is granted
    bool stalled;

    // Bursts which have been handled synchronously with a delay. There are hold here until their
    // delay has elapsed
    struct DelayedBurst
    {
        vp::IoReq *req;
        uint64_t timestamp;
    };

    struct DelayedBurstCompare
    {
        bool operator()(const DelayedBurst &a, const DelayedBurst &b) const
        {
            return a.timestamp > b.timestamp;
        }
    };

    std::priority_queue<DelayedBurst, std::vector<DelayedBurst>, DelayedBurstCompare> delayed_bursts;

    // Reorder Buffer for mempool configuration
    struct VlsuRobEntry
    {
        // Port and id
        int port = 0;
        int rob_id = 0;

        // If the entry is allocated to a request
        bool allocated = false;

        // If the response is valid
        bool valid = false;

        // Request itself
        vp::IoReq *req = nullptr;

        // Instruction slot issued the request
        VuLsuPendingInsn *slot = nullptr;

        // Vector register
        int vreg = 0;

        int elem_size = 0;
        int vstart = 0;
        int size = 0;
    };

    // Reorder buffer
    std::vector<std::vector<VlsuRobEntry>> rob;
    // Next available entry in the ROB for each port
    std::vector<int> rob_next;
    // The first entry in the ROB which is waiting for response for each port
    std::vector<int> rob_first;
    // Number of allocated entries in the ROB for each port
    std::vector<int> rob_count;
};

#endif // CONFIG_GVSOC_ISS_VLSU_V2

#else

// Block processing load/store vector instructions
class VuLsu : public VuBlock
{
public:

    void reset(bool active) override;
    VuLsu(Vu &vu, Iss &iss);
    bool is_full() override { return this->nb_pending_insn.get() == VuLsu::queue_size; }
    void enqueue_insn(PendingInsn *pending_insn) override;
    void isa_init() override;

private:
    // Handler for internal FSM
    static void fsm_handler(vp::Block *__this, vp::ClockEvent *event);
    // Handler called when a load instruction starts to be processed, in order to initialize the FSM
    // for a read burst
    static void handle_insn_load(VuLsu *vlsu, iss_insn_t *insn);
    // Handler called when a write instruction starts to be processed, in order to initialize the FSM
    // for a write burst
    static void handle_insn_store(VuLsu *vlsu, iss_insn_t *insn);
    // Handler for asynchronous burst grants
    static void data_grant(vp::Block *__this, vp::IoReq *req);
    // Handler for asynchronous burst responses
    static void data_response(vp::Block *__this, vp::IoReq *req);
    void handle_burst_end(vp::IoReq *req);

    // Number of bursts which can be sent at the same time
    static constexpr int nb_burst = 8;
    // Number of instruction that can be enqueued at the same time
    static constexpr int queue_size = 4;

    Vu &vu;
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
    vp::Event event_label;
    // Clock event used for scheduling FSM handler when at least one instruction has to be processed
    vp::ClockEvent fsm_event;
    // Queue of pending instructions to be processed by this block
    // The block process them in-order
    std::vector<VuLsuPendingInsn> insns;
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
    int elem_size;
    int vstart;

    // Instruction currently active in the VLSU. It is either waiting for its instruction latency to elapse 
    // or already actively issuing memory requests. This is kept separate from insn_first_waiting for the following reason. 
    // pending_insn->timestamp has multiple purposes:
    // 1. At the instruction enqueue time, it is set to (enqueue cycle + 1) to avoid starting to execute the instruction immediately in the same cycle;
    // 2. At the instruction execution start, it is set to (current cycle + instruction latency), to prevent the instruction from issuing memory requests before its instruction latency elapses;
    // 3. During the instruction execution, it is used to track the synchronous memory access latency (current cycle + request latency) and instruction retirement time.
    // In order to distinguish between the first two cases in the FSM, and ensure that instruction latency is only modeled once,
    // we seperate index insn_ongoing (with timestamp of purpose 2 or 3) from insn_first_waiting (with timestamp of purpose 1).
    // This also prevents the younger instructions waiting in the queue from consuming its latency while the older instruction is still active.
    int insn_ongoing;
};

#endif

// Vu top block
class Vu : public vp::Block
{
    friend class VuCompute;
    friend class VuLsu;

public:
    // List of sub-blocks processing instructions
    typedef enum
    {
        vlsu_id,
        vfpu_id,
        vslide_id,
        nb_blocks
    } blocks_e;

    Vu(Iss &iss);

    void reset(bool reset);
    // Called by ISS to initialize vector instructions in the ISA decoding tree to attach needed
    // handlers
    void isa_init();
    // Called by sub-blocks to notify the end of processing of an instruction. The instruction
    // The instruction handler is called, the instruction is removed ffrom the sequencer, and the
    // registers involved is updated
    void insn_end(PendingInsn *insn);
    void insn_commit(PendingInsn *pending_insn, int nb_elems);
    bool insn_ready(PendingInsn *insn);
    // Called by the ISS to offload an instruction to vu. The instruction is pushed to the queue
    // of pending instruction, analyzed, and push to a processing block for executing once
    // dependencies are resolved. Can be called only when queue is not full.
    void insn_enqueue(InsnEntry *insn, iss_reg_t (*handler)(Iss *, iss_insn_t *, iss_reg_t));
    // Return true when queue if full and vu can not accept new instructions
    bool queue_is_full() { return this->queue_full.get(); }
    bool queue_is_empty() { return this->nb_pending_insn == 0; }
    // Return the CVA6 register value associated to the instruction being executed
    inline uint64_t current_insn_reg_get() { return current_insn_reg; }
    inline uint64_t current_insn_reg_2_get() { return current_insn_reg_2; }

    void dump_regs_to_trace(iss_insn_t *insn, PendingInsn *pending_insn, int nb_elem, bool is_out);
    inline void exec_insn_chunk(iss_insn_t *insn, PendingInsn *pending_insn, int vstart,
        int vend, int nb_elem);

    // Access to upper ISS
    Iss &iss;
    // Number of <lane_width> bits lanes in vu.
    int nb_lanes;
    // Number of <lane_width> bits integer units in vu. Integer computational
    // instructions are processed at this rate instead of the lane one.
    int nb_ipus;
    // Width in bits of one lane
    int lane_width;

    // Number of pending vector loads and stores in spatz. Use to synchronize with snitch memory
    // accesses
    int nb_pending_vaccess;
    // Number of pending vector stores in spatz. Use to synchronize with snitch memory accesses
    int nb_pending_vstore;

    int vstart;
    int vend;
    vp::Trace trace;
    uint64_t saved_value;
    void insn_handle_reduction();

private:
    void insn_commit(PendingInsn *pending_insn);
    template<bool fast>
    static iss_reg_t vector_insn_stub_handler(Iss *iss, iss_insn_t *insn, iss_reg_t pc);

    template<bool fast>
    static iss_reg_t load_store_handler(Iss *iss, iss_insn_t *insn, iss_reg_t pc);
    // Handler for internal FSM
    static void fsm_handler(vp::Block *__this, vp::ClockEvent *event);
    // Allocate a slot for an instruction being enqueued. This is used to duplicate the cva6
    // pending instruction to commit it early so that cva6 can use the slot for another instruction.
    PendingInsn *pending_insn_alloc(InsnEntry *entry);
    inline int alloc_id();
    inline void free_id(int id);

    // Size of the queue holding pending instructions. Once full, vu can not accept instructions
    // from CVA6 anymore. Spatz can handle 4 instructions at a time.
    static constexpr int queue_size = 4;

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
    // Number of instructions currently being processed by vu. This is increased when an
    // instruction is enqueued, and decreased when it ends
    vp::Register<uint8_t> nb_pending_insn;
    // List of instructions currently being processed by vu. This contains both the instructions
    // waiting for the resolution of their dependencies, and the instructions already dispatched
    // to the processing blocks.
    std::vector<PendingInsn> pending_insns;
    // True if the queue is full and vu can not accept any other instruction
    vp::Register<bool> queue_full;
    // List of processing blocks
    std::vector<VuBlock *> blocks;
    // CVA6 register associated to the instruction being executed. This is used by instruction
    // handlers when they are executed. This needs to be buffered because CVA6 might have executed
    // following instructions overriding the register
    uint64_t current_insn_reg;
    uint64_t current_insn_reg_2;
    uint64_t insn_id_table;

    std::queue<PendingInsn *> stalled_insns;
    std::vector<uint64_t> insns_in_deps;
    // Writers AND readers of this instruction's destination registers: both
    // write-after-write and write-after-read block until the dependency
    // retires, through the same gate in Vu::insn_ready. See the note there
    // about the RTL chaining this does not model.
    std::vector<uint64_t> insns_out_deps;
    uint64_t writing_insns[32];
    uint64_t reading_insns[32];
    int insn_latency;

#ifdef CONFIG_GVSOC_STATS_ACTIVE
    // Per-label execution-duration statistics for vector instructions, dumped
    // under the "vinsn_duration" group. A vector instruction waits in the Vu
    // queue and in its block before really executing, so duration is measured
    // from the block's real start (exec_start_cycle) to Vu::insn_end.
    bool stats_enabled = false;
    InsnDurationStats insn_durations;
#endif
};

inline int Vu::alloc_id()
{
    int id = __builtin_ctzll(this->insn_id_table);
    this->insn_id_table &= ~(1ULL << id);
    return id;
}

inline void Vu::free_id(int id)
{
    this->insn_id_table |= (1ULL << id);
}

inline void Vu::exec_insn_chunk(iss_insn_t *insn, PendingInsn *pending_insn, int vstart,
    int vend, int nb_elem)
{
    this->vstart = vstart;
    this->vend = vend;

#ifdef VP_TRACE_ACTIVE
    this->dump_regs_to_trace(insn, pending_insn, nb_elem, false);
#endif

    pending_insn->handler(&this->iss, insn, insn->addr);

#ifdef VP_TRACE_ACTIVE
    this->dump_regs_to_trace(insn, pending_insn, nb_elem, true);
#endif
}

inline void Vu::insn_handle_reduction()
{
    this->insn_latency = 3;
}
