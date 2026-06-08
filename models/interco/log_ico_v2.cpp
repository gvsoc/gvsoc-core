// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)
//
// Logarithmic (bank-interleaved) crossbar on the io_v2 protocol with
// round-robin arbitration. M masters -> N banks. The crossbar never
// queues request pointers: an incoming request is DENIED in the normal
// (idle) state and the master is expected to retry it later. The
// crossbar only remembers *which* input wants to talk to *which* bank
// in one bit per (input, bank) pair.
//
// Forward path:
//   1. Master M calls req() for bank B. We decode B from the address,
//      set bit M in banks[B].pending_mask, schedule the FSM (0 delay),
//      and return DENIED.
//   2. The FSM iterates the banks. For each bank with pending bits it
//      picks a single winner via round-robin (find-first-set on a
//      rotated bitmask), clears the bit, and calls retry() on that
//      master. The FSM raises an "in_election" flag for the whole
//      iteration: while it is set, any request landing on input_req is
//      forwarded inline to its bank instead of being denied -- so the
//      master's synchronous retry handler just re-issues and the
//      crossbar serves it within the same tick.
//
//      This relies on the io_v2 synchronous-retry constraint: a master
//      MUST re-issue inside its retry() callback (same cycle), not defer
//      it. The in_election window is open only for the duration of the
//      retry() call here; a master that re-sends a cycle later misses it
//      and live-locks. See vp/itf/io_v2.hpp (IoSlave::retry) and
//      interfaces/io_v2.rst.
//   3. The bank is IoV2Sync and answers DONE inline, so input_req
//      returns DONE to the re-issuing master. By the time retry()
//      returns to the FSM the bank has already been hit this cycle.
//   4. If any bank still has bits after the iteration, the FSM re-arms
//      for the next cycle so each bank serves at most one master per
//      cycle.
//
// Output side (IoV2Sync): the bank must answer inline with
// IO_REQ_DONE and never drives resp()/retry(). Bind only to a sync
// slave such as memory.memory_v3.
//
// Address decoding: the input address is expected to be in
// ``[0, nb_slaves * <bank_size>)`` (the upstream router or remapper
// strips any region base before reaching the crossbar):
//
//     bank_id     = (addr >> interleaving_width) & (nb_slaves - 1)
//     bank_offset = ((addr >> (slave_bits + interleaving_width)) << interleaving_width)
//                   | (addr & ((1 << interleaving_width) - 1))

#include <climits>
#include <memory>
#include <vector>
#include <vp/vp.hpp>
#include <vp/itf/io_v2.hpp>
#include <interco/log_ico_v2/log_ico_config.hpp>


class LogIco;

// One per master input port. Just a thin wrapper around its IoSlave --
// the crossbar keeps no per-input state in the new design.
struct InputState
{
    InputState(LogIco *top, int id);
    LogIco *top;
    int id;
    vp::IoSlave itf;
};

struct BankState
{
    BankState(LogIco *top, int id);
    LogIco *top;
    int id;
    vp::IoMaster itf;
    // Bit i set means input i has a pending (denied) request to this bank.
    uint64_t pending_mask = 0;
    // Next round-robin scan start (in [0, nb_masters)).
    int rr_next = 0;
};


class LogIco : public vp::Component
{
    friend struct InputState;
    friend struct BankState;

public:
    LogIco(vp::ComponentConf &conf);
    void reset(bool active) override;

    LogIcoConfig cfg;
    vp::Trace trace;

private:
    static vp::IoReqStatus input_req   (vp::Block *__this, vp::IoReq *req, int id);
    static void            output_resp (vp::Block *__this, vp::IoReq *req, int id);
    static void            output_retry(vp::Block *__this, int id);
    static void            fsm_handler (vp::Block *__this, vp::ClockEvent *event);

    int       decode_bank   (uint64_t offset) const;
    uint64_t  decode_offset (uint64_t offset) const;
    // Find the first set bit at or after `rr_next` in a `nb`-wide mask,
    // wrapping. Returns the bit index in [0, nb); precondition: mask != 0.
    int       pick_winner   (uint64_t mask, int rr_next, int nb) const;

    int slave_bits = 0;
    std::vector<std::unique_ptr<InputState>> inputs;
    std::vector<std::unique_ptr<BankState>>  banks;
    // True while the FSM is calling retry() on the elected winners.
    // Any incoming request seen during this window is forwarded inline.
    bool in_election = false;

    vp::ClockEvent fsm_event;
};


static int ceil_log2_u(unsigned int n)
{
    if (n <= 1) return 0;
    return 32 - __builtin_clz(n - 1);
}


//
// Per-port state ctors
//

InputState::InputState(LogIco *top, int id)
    : top(top), id(id),
      itf(id, &LogIco::input_req)
{
}

BankState::BankState(LogIco *top, int id)
    : top(top), id(id),
      itf(id, &LogIco::output_retry, &LogIco::output_resp)
{
}


//
// LogIco
//

LogIco::LogIco(vp::ComponentConf &config)
    : vp::Component(config, this->cfg),
      fsm_event(this, &LogIco::fsm_handler)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);

    int nb_masters = (int)this->cfg.nb_masters;
    int nb_slaves  = (int)this->cfg.nb_slaves;
    this->slave_bits = ceil_log2_u((unsigned int)nb_slaves);

    vp_assert_always(nb_masters > 0 && nb_masters <= 64, &this->trace,
        "nb_masters must be in (0, 64], got %d\n", nb_masters);

    this->banks.reserve(nb_slaves);
    for (int i = 0; i < nb_slaves; i++)
    {
        std::string name = "output_" + std::to_string(i);
        auto b = std::make_unique<BankState>(this, i);
        this->new_master_port(name, &b->itf);
        this->banks.push_back(std::move(b));
    }

    this->inputs.reserve(nb_masters);
    for (int i = 0; i < nb_masters; i++)
    {
        std::string name = "input_" + std::to_string(i);
        auto in = std::make_unique<InputState>(this, i);
        this->new_slave_port(name, &in->itf);
        this->inputs.push_back(std::move(in));
    }
}

void LogIco::reset(bool active)
{
    if (active)
    {
        this->in_election = false;
        for (auto &b : this->banks)
        {
            b->pending_mask = 0;
            b->rr_next = 0;
        }
    }
}


int LogIco::decode_bank(uint64_t offset) const
{
    if (this->slave_bits == 0) return 0;
    int mask = (1 << this->slave_bits) - 1;
    return (int)((offset >> this->cfg.interleaving_width) & (uint64_t)mask);
}

uint64_t LogIco::decode_offset(uint64_t offset) const
{
    uint64_t iw = (uint64_t)this->cfg.interleaving_width;
    uint64_t iw_mask  = (iw == 0) ? 0 : ((((uint64_t)1) << iw) - 1);
    uint64_t hi_shift = (uint64_t)this->slave_bits + iw;
    return ((offset >> hi_shift) << iw) | (offset & iw_mask);
}

int LogIco::pick_winner(uint64_t mask, int rr_next, int nb) const
{
    // Valid-bit mask so an over-wide rotation doesn't pull stale bits in.
    uint64_t valid = (nb == 64) ? ~0ULL : ((1ULL << nb) - 1);
    mask &= valid;

    // Rotate the mask right by rr_next so the scan start lands at bit 0,
    // then ctz gives the offset of the first set bit at-or-after rr_next.
    uint64_t rotated;
    if (rr_next == 0)
    {
        rotated = mask;
    }
    else
    {
        rotated = ((mask >> rr_next) | (mask << (nb - rr_next))) & valid;
    }
    int rel = __builtin_ctzll(rotated);
    return (rr_next + rel) % nb;
}


//
// Forward path
//

vp::IoReqStatus LogIco::input_req(vp::Block *__this, vp::IoReq *req, int id)
{
    LogIco *_this = (LogIco *)__this;

    uint64_t addr   = req->get_addr();
    int bank_id    = _this->decode_bank(addr);

    if (_this->in_election)
    {
        // FSM is dispatching retries; any request arriving in this
        // window (typically the re-issue triggered by the retry call
        // we just made) is forwarded inline to the IoV2Sync bank.
        uint64_t bank_offset = _this->decode_offset(addr);

        _this->trace.msg(vp::Trace::LEVEL_DEBUG,
            "Forwarding (input: %d, addr: 0x%llx -> bank %d bank_addr: 0x%llx)\n",
            id,
            (unsigned long long)addr,
            bank_id,
            (unsigned long long)bank_offset);

        req->set_addr(bank_offset);
        vp::IoReqStatus st = _this->banks[bank_id]->itf.req(req);
        vp_assert_always(st == vp::IO_REQ_DONE, &_this->trace,
            "IoV2Sync output returned a non-DONE status (%d)\n", (int)st);
        return vp::IO_REQ_DONE;
    }

    // Idle state: record that this input wants this bank, schedule the
    // arbiter, and tell the master to retry later.
    _this->trace.msg(vp::Trace::LEVEL_DEBUG,
        "Req arrived, denying (input: %d, addr: 0x%llx, size: 0x%llx, write: %d, bank: %d)\n",
        id,
        (unsigned long long)addr,
        (unsigned long long)req->get_size(),
        req->get_is_write() ? 1 : 0,
        bank_id);

    _this->banks[bank_id]->pending_mask |= (1ULL << id);
    _this->fsm_event.enqueue(0);
    return vp::IO_REQ_DENIED;
}

void LogIco::fsm_handler(vp::Block *__this, vp::ClockEvent *event)
{
    LogIco *_this = (LogIco *)__this;
    int nb = (int)_this->cfg.nb_masters;
    bool any_remaining = false;

    _this->in_election = true;
    for (auto &bank : _this->banks)
    {
        if (bank->pending_mask == 0) continue;

        int winner = _this->pick_winner(bank->pending_mask, bank->rr_next, nb);
        bank->pending_mask &= ~(1ULL << winner);
        bank->rr_next = (winner + 1) % nb;

        _this->trace.msg(vp::Trace::LEVEL_DEBUG,
            "Round-robin pick (bank: %d, winner: %d, remaining_mask: 0x%llx)\n",
            bank->id, winner,
            (unsigned long long)bank->pending_mask);

        // Retry runs the master's retry handler synchronously: the
        // master re-issues, input_req (with in_election=true) forwards
        // inline to the bank and returns DONE.
        _this->inputs[winner]->itf.retry();

        if (bank->pending_mask != 0) any_remaining = true;
    }
    _this->in_election = false;

    if (any_remaining)
    {
        // Re-arm next cycle so each bank serves at most one master per
        // cycle.
        _this->fsm_event.enqueue(1);
    }
}


//
// Response path: the IoV2Sync bank must never drive these.
//

void LogIco::output_resp(vp::Block *__this, vp::IoReq * /*req*/, int id)
{
    LogIco *_this = (LogIco *)__this;
    _this->trace.fatal("Unexpected async resp() from IoV2Sync bank %d "
                       "(the synchronous sub-protocol forbids it)\n", id);
}

void LogIco::output_retry(vp::Block *__this, int id)
{
    LogIco *_this = (LogIco *)__this;
    _this->trace.fatal("Unexpected retry() from IoV2Sync bank %d "
                       "(the synchronous sub-protocol forbids it)\n", id);
}


extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new LogIco(config);
}
