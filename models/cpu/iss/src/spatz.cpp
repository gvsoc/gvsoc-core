// #include "spatz.hpp"

// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// //                                                            VECTOR REGISTER FILE
// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "cpu/iss/include/iss.hpp"


Spatz::Spatz(Iss &iss)
    : vregfile(iss), vlsu(iss), last_stamp(0), runtime(0), max_vlsu_latency(0)
{
    iss.top.traces.new_trace("spatz/trace", &this->trace, vp::DEBUG);
    for (int i = 0; i < ISS_NB_VREGS; ++i)
    {
        this->reg_score_board[i] = 0;
    }
    for (int i = 0; i < 3; ++i)
    {
        this->unit_score_board[i] = 0;
    }
}

void Spatz::build()
{
    this->vlsu.build();
}

void Spatz::reset(bool active)
{
}

VRegfile::VRegfile(Iss &iss) : iss(iss){
    VRegfile::reset(true);
}

uint64_t Spatz::timing_insn(iss_insn_t *insn, int uint_id, int vd, int vs1, int vs2, uint64_t latency, uint64_t timestamp)
{
    uint64_t meet_point = timestamp;
    uint64_t future_point = 0;
    uint64_t delay = 0;
    if (uint_id >= 0)
    {
        meet_point = (this->unit_score_board[uint_id] > meet_point)? this->unit_score_board[uint_id] : meet_point;
    }

    if (vs1 >= 0)
    {
        meet_point = (this->reg_score_board[vs1] > meet_point)? this->reg_score_board[vs1] : meet_point;
    }

    if (vs2 >= 0)
    {
        meet_point = (this->reg_score_board[vs2] > meet_point)? this->reg_score_board[vs2] : meet_point;
    }

    delay = meet_point - timestamp;
    future_point = meet_point + latency;

    uint64_t add_runtime = (this->last_stamp >= future_point)? 0: (this->last_stamp >= meet_point)? future_point - this->last_stamp : future_point - meet_point;
    this->runtime += add_runtime;
    this->last_stamp = future_point;
    this->trace.msg("[Spatz] Finished : %0d ns ---> %0d ns | period = %0d ns | runtime = %0d ns | Instruction = %s\n", meet_point, future_point, future_point - meet_point, this->runtime, insn->decoder_item->u.insn.label);

    this->unit_score_board[uint_id] = future_point;
    if (vd >= 0)
    {
        this->reg_score_board[vd] = future_point;
    }

    return delay;
}



void Vlsu::data_response(vp::Block *__this, vp::IoReq *req)
{
}


Vlsu::Vlsu(Iss &iss) : iss(iss)
{
    this->next_io = 0;
}

void Vlsu::build()
{
    for (int i=0; i<CONFIG_GVSOC_ISS_SPATZ_VLSU; i++)
    {
        this->io_itf[i].set_resp_meth(&Vlsu::data_response);
        this->iss.top.new_master_port("vlsu_" + std::to_string(i), &this->io_itf[i], (vp::Block *)this);
    }
    this->next_io = 0;
}
inline void VRegfile::reset(bool active){
    if (active){
        for (int i = 0; i < ISS_NB_VREGS; i++){
            for (int j = 0; j < NB_VEL; j++){
                this->vregs[i][j] = i == 0 ? 0 : 0x57575757;
            }
        }
    }
}

// Iss::Iss(IssWrapper &top)
//     : prefetcher(*this), exec(top, *this), insn_cache(*this), decode(*this), timing(*this), core(*this), irq(*this),
//       gdbserver(*this), lsu(*this), dbgunit(*this), syscalls(top, *this), trace(*this), csr(*this),
//       regfile(top, *this), mmu(*this), pmp(*this), exception(*this), spatz(*this), memcheck(top, *this), top(top)
// {
// }
