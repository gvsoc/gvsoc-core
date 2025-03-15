// #include "spatz.hpp"

// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// //                                                            VECTOR REGISTER FILE
// /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "cpu/iss/include/iss.hpp"

Iss::Iss(IssWrapper &top)
    : prefetcher(*this), exec(top, *this), insn_cache(*this), decode(*this), timing(*this),
    core(*this), irq(*this), gdbserver(*this), lsu(*this), dbgunit(*this), syscalls(top, *this),
    trace(*this), csr(*this), regfile(top, *this), mmu(*this), pmp(*this), exception(*this),
    spatz(*this), memcheck(top, *this), top(top), vector(*this)
{
}
