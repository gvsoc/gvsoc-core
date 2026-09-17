// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

/*
 * IoV2BeatWidthAdapter — standalone component auto-inserted by the gvrun2
 * systree on io_v2 bindings whose master side declares signature
 * IoV2Beat(input_width) and whose slave side declares IoV2Beat(output_width)
 * with a different width. Both sides speak the beat sub-protocol; the adapter
 * only converts the beat granularity, repacking the per-beat streams in both
 * directions (N narrow beats <-> one wide beat, in cumulative byte order).
 *
 * Widths: the wider one must be an integer multiple of the narrower one.
 * Short terminal beats (burst size not a width multiple) are handled on both
 * sides, with beat boundaries derived from the cumulative offset within the
 * burst, per the io_v2.hpp per-beat addr convention.
 *
 * Timing / bandwidth: each side runs at its own width and at most one beat
 * per cycle per channel, so the two bindings show different per-cycle
 * occupancies — the narrow side streams every cycle while the wide side
 * carries the same bytes in fewer, wider beats (one every ratio cycles).
 * Aggregate byte bandwidth is bounded by the narrow side, exactly like a HW
 * bus-width up/down-sizer. Back-pressure keeps both sides honest:
 *
 *   - READ, output narrower: downstream beats arrive one per cycle and are
 *     packed; a wide upstream beat completes every ratio cycles.
 *   - READ, output wider: each wide downstream beat unpacks into ratio
 *     upstream beats (one per cycle); further downstream beats are refused
 *     (IO_RESP_DENIED + later resp_retry()) while the unpacked backlog is
 *     full, so the downstream is throttled to one beat per ratio cycles.
 *   - WRITE, output narrower: each upstream write request is chopped into
 *     one-per-cycle downstream beats; further upstream write requests are
 *     DENIED while the chop backlog is full and re-enabled via retry(WRITE).
 *   - WRITE, output wider: consecutive upstream write beats of one burst are
 *     packed (copied) into one wide downstream beat issued every ratio cycles.
 *
 * Both back-pressure points sit behind configurable FIFOs (read_fifo_depth /
 * write_fifo_depth, see the config): deepening them lets the adapter absorb
 * workload — a stalled upstream response channel, a burst of upstream writes —
 * before it starts denying. The auto defaults are the minimum depths that
 * sustain full streaming.
 *
 * WRITES are acknowledged once per burst (AXI B-channel semantics, see
 * io_v2.hpp "Write acknowledgement") on BOTH faces:
 *   - Upstream face: the adapter is the consumer of the master's
 *     allocator-backed write beats. Each beat's payload is copied into
 *     downstream chunks during req(), so the beat is freed right there and
 *     GRANTED is returned — non-last and last beats alike. When the whole
 *     burst has completed downstream, ONE data-less ack, drawn from the
 *     size-0 pool (our copy of the last beat was already freed, and a
 *     payload-pool object must never be recycled as an ack), is emitted
 *     upstream and freed by the master.
 *   - Downstream face: the chunks of one upstream burst are framed as one
 *     downstream burst (is_first/is_last/burst_id per chunk, same initiator
 *     — the burst record — on every chunk). A GRANTED chunk was consumed and
 *     freed by the target (non-last chunks get no resp at all); the burst
 *     completes on the last chunk's inline DONE (we keep and free the chunk)
 *     or on the target's single distinct data-less ack, correlated back via
 *     initiator and freed by us.
 *
 * Timing deviation (write acks): before the per-burst ack protocol the
 * upstream acks were scheduled per input_width stride as the covering
 * downstream chunk acks arrived. Exact reconstruction of that schedule is
 * impossible now that non-last chunks are consumed without any downstream
 * ack, so the single upstream ack is instead scheduled from the downstream
 * burst completion with the pre-existing per-ack arithmetic — ready =
 * now + max(1, latency), serialized on ack_cursor — i.e. the FINAL stride
 * only.
 *
 * Ownership follows the initiator-owned request convention:
 *   - The upstream read burst request is never freed here; read response
 *     beats emitted upstream are distinct allocator-backed objects
 *     (input_width pool) the terminal master frees. Upstream write beats are
 *     consumed and freed here at submit (see above).
 *   - The downstream read descriptor is our own allocator-backed (data-less)
 *     object, freed by us when its response stream completes; downstream
 *     read beats are freed by us as we consume them; downstream write chunks
 *     are our own allocator-backed (output_width pool) objects whose payload
 *     carries a copy of the upstream data — the downstream target consumes
 *     and frees them (an inline-DONE'd last chunk stays ours and we free
 *     it).
 *
 * No public API: private header of the component implementation. The
 * framework auto-inserts the component during the gvrun2 binding-collection
 * pass; no model includes this header.
 */

#pragma once

#include <cstdint>
#include <deque>
#include <vector>
#include <vp/vp.hpp>
#include <vp/itf/io_v2.hpp>
#include <vp/debug_mem.hpp>

#include <utils/io_v2_beat_width_adapter/io_v2_beat_width_adapter_config.hpp>


class IoV2BeatWidthAdapter : public vp::Component, public vp::DebugMemIf
{
public:
    IoV2BeatWidthAdapter(vp::ComponentConf &config);
    void reset(bool active) override;

    // Backdoor debug path: the adapter is invisible, forward to the downstream.
    vp::DebugMemIf *debug_mem_if() override { return this; }
    int debug_mem_access(uint64_t addr, uint8_t *data, uint64_t size,
        bool is_write) override;
    void debug_mem_regions(std::vector<vp::DebugMemRegion> &regions,
        uint64_t local_base, uint64_t window_size, uint64_t entry_base,
        int depth) override;

    IoV2BeatWidthAdapterConfig cfg;

private:
    // ---- READ path ---------------------------------------------------------
    // One downstream read beat buffered in a burst's response FIFO. The
    // downstream object itself is kept (we own it once accepted) and freed
    // when its last byte has been extracted.
    struct RxSegment
    {
        vp::IoReq *beat;
        uint64_t   size;
        uint64_t   consumed;      // bytes already extracted upstream
        int64_t    ready_cycle;   // arrival cycle + its latency
    };

    // One in-flight upstream read burst. Downstream beats are correlated back
    // to it via initiator (copied by the downstream producer from our
    // descriptor onto every beat) and buffered as they arrive; the per-cycle
    // pump extracts ONE input_width-sized upstream beat per cycle from the
    // buffered bytes, at cumulative-offset boundaries.
    struct ReadBurst
    {
        vp::IoReq *up_req;        // upstream descriptor (initiator-owned upstream)
        vp::IoReq *dn_req;        // our downstream descriptor (freed on last dn beat)
        uint64_t   burst_addr;
        uint64_t   total;
        int64_t    burst_id;
        void      *up_initiator;  // snapshot of up_req->initiator
        uint64_t   bytes_received = 0;   // taken from the downstream stream
        uint64_t   bytes_emitted = 0;    // delivered upstream
        uint64_t   rx_avail = 0;         // buffered, not yet extracted
        std::deque<RxSegment> rx;        // buffered downstream beats, in order
        // The downstream stream is over (last beat seen, or answered inline).
        bool       dn_done = false;
        // Data-less stream (zero-size read or error answered inline, or the
        // zero-size last beat): the upstream beats carry no payload and are
        // due from done_ready.
        bool       synth = false;
        int64_t    done_ready = 0;
        vp::IoRespStatus status = vp::IO_RESP_OK;
    };

    // ---- WRITE path --------------------------------------------------------
    // One upstream write burst. Opened by the is_first beat, submission-closed
    // by the is_last beat (last_seen), released when the downstream burst
    // completes and the single upstream ack is scheduled. The upstream beats
    // themselves are consumed at submit (payload copied into chunks, beat
    // freed) — this record is everything that survives them, snapshotted
    // before the free.
    struct WriteBurst
    {
        uint64_t base_addr = 0;
        uint64_t total = 0;       // bytes submitted; final once last_seen
        bool     last_seen = false;
        int64_t  burst_id = -1;   // snapshot from the opening beat
        void    *initiator = nullptr;  // same on every beat (asserted)
        // Final status latch: any chunk error (inline-DONE INVALID escape
        // hatch) or an INVALID downstream ack latches INVALID.
        vp::IoRespStatus status = vp::IO_RESP_OK;
    };

    // One downstream write beat: allocator-backed request (output_width pool)
    // whose co-allocated payload carries a copy of the covered upstream bytes.
    // The wrapper only lives until the downstream outcome of its req (GRANTED
    // or DONE): per-chunk completion is not tracked — the burst record is.
    struct WriteChunk
    {
        vp::IoReq  *req = nullptr;
        WriteBurst *burst = nullptr;
        uint64_t    addr = 0;
        uint64_t    fill = 0;
        bool        is_first = false;
        bool        is_last = false;
        int64_t     burst_id = -1;
    };

    // The single upstream per-burst write ack scheduled for emission (a
    // distinct data-less size-0-pool object is built at emit time). All
    // fields are snapshots — the burst record is already released.
    struct PendingAck
    {
        uint64_t addr;            // burst base address (informational)
        uint64_t size;            // burst total bytes (informational)
        int64_t  burst_id;
        void    *initiator;       // upstream initiator copied from the beats
        vp::IoRespStatus status;
        int64_t  ready_cycle;
    };

    static vp::IoReqStatus req_handler(vp::Block *__this, vp::IoReq *req);
    static vp::IoRespAck   resp_handler(vp::Block *__this, vp::IoReq *req);
    static void            retry_handler(vp::Block *__this, vp::IoRetryChannel channel);
    static void            resp_retry_in_handler(vp::Block *__this, vp::IoRetryChannel channel);
    static void            fsm_handler(vp::Block *__this, vp::ClockEvent *event);

    // Read path.
    vp::IoReqStatus submit_read(vp::IoReq *req);
    vp::IoRespAck   consume_read_beat(vp::IoReq *beat);
    void complete_read_inline(ReadBurst *burst, int64_t latency);
    // Cycle from which `burst` can deliver its next upstream beat, INT64_MAX
    // while its bytes have not all arrived.
    int64_t next_read_beat_ready(ReadBurst *burst);
    // Extract and send upstream the next beat of `burst` (one per cycle).
    void emit_read_beat(ReadBurst *burst);
    void retire_read_burst(ReadBurst *burst);

    // Write path.
    vp::IoReqStatus submit_write(vp::IoReq *req);
    void finish_chunk(bool is_last);
    void issue_pending_chunks();
    void complete_chunk(WriteChunk *chunk, vp::IoReqStatus st);
    void complete_write_burst(WriteBurst *burst, int64_t latency_cycles);
    void emit_ack(const PendingAck &ack);
    void maybe_unblock_write();

    void reschedule_fsm();

    vp::DebugMemIf *resolve_debug_mem();

    // Tiny freelist pools (process-lifetime, like IoReqAllocator).
    ReadBurst  *alloc_read_burst();
    void        free_read_burst(ReadBurst *burst);
    WriteBurst *alloc_write_burst();
    void        free_write_burst(WriteBurst *burst);
    WriteChunk *alloc_chunk();
    void        free_chunk(WriteChunk *chunk);

    int input_width;
    int output_width;
    // Read response FIFO depth (config read_fifo_depth, in input_width
    // beats): repacked upstream read beats buffered before the downstream
    // producer is back-pressured. Auto (0) sizes it to 2 downstream beats'
    // worth (double-buffering) — the minimum for continuous streaming;
    // larger depths absorb workload (e.g. an upstream response stall)
    // without stalling the downstream.
    size_t read_pending_limit;
    // Write FIFO depth (config write_fifo_depth, in output_width chunks):
    // complete, un-issued downstream write chunks buffered before upstream
    // write requests are refused. Auto (0) = 2 — just enough for seamless
    // chunk streaming; larger depths absorb a burst of upstream writes at
    // the upstream rate before throttling the writer.
    size_t write_chunk_limit;

    // Allocator pools: input_width-sized upstream read beats, output_width-
    // sized downstream write chunks, data-less downstream read descriptors.
    vp::IoReqAllocator *in_beat_allocator;
    vp::IoReqAllocator *out_beat_allocator;
    vp::IoReqAllocator *desc_allocator;

    vp::IoSlave in;
    vp::IoMaster out;
    vp::ClockEvent fsm_event;

    // Read state.
    std::vector<ReadBurst *> live_reads;        // for reset cleanup
    size_t rx_bytes = 0;                        // buffered downstream read bytes
    int64_t read_cursor = -1;                   // <=1 upstream read beat / cycle
    bool dn_read_blocked = false;               // we denied a downstream beat

    // Write state.
    std::vector<WriteBurst *> live_bursts;      // awaiting completion (+ reset cleanup)
    WriteBurst *open_burst = nullptr;           // currently accepting beats
    WriteChunk *cur_chunk = nullptr;            // under construction
    std::deque<WriteChunk *> chunk_queue;       // complete, waiting to issue
    WriteChunk *held_chunk = nullptr;           // DENIED downstream, re-send on retry
    int64_t chunk_issue_cursor = -1;            // <=1 downstream chunk / cycle
    bool up_write_blocked = false;              // we denied an upstream write
    std::deque<PendingAck> ack_pending;
    int64_t ack_cursor = -1;                    // serializes upstream write acks

    // Response-path back-pressure from the upstream master: the refused beat
    // (a read beat or a write burst ack — both allocator-backed and ours until
    // accepted) is held here and re-sent from resp_retry_in_handler; nothing
    // else is emitted until it is accepted.
    bool resp_held = false;
    vp::IoReq *held_req = nullptr;

    // Freelists.
    std::vector<ReadBurst *> read_burst_pool;
    std::vector<WriteBurst *> write_burst_pool;
    std::vector<WriteChunk *> chunk_pool;

    vp::Trace trace;
};
