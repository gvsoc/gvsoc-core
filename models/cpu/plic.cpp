// Copyright (c) 2010-2017, The Regents of the University of California
// (Regents).  All Rights Reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
// 3. Neither the name of the Regents nor the
//    names of its contributors may be used to endorse or promote products
//    derived from this software without specific prior written permission.
//
// IN NO EVENT SHALL REGENTS BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,
// SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST PROFITS, ARISING
// OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF REGENTS HAS
// BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// REGENTS SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE. THE SOFTWARE AND ACCOMPANYING DOCUMENTATION, IF ANY, PROVIDED
// HEREUNDER IS PROVIDED "AS IS". REGENTS HAS NO OBLIGATION TO PROVIDE
// MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.

#include <vp/vp.hpp>
#include <vp/itf/io.hpp>
#include <vp/itf/wire.hpp>

#define PLIC_PRIO_BITS     4
#define PLIC_SIZE         0x01000000
#define PLIC_MAX_CONTEXTS 15872
#define PLIC_MAX_DEVICES 1024

/*
 * The PLIC consists of memory-mapped control registers, with a memory map
 * as follows:
 *
 * base + 0x000000: Reserved (interrupt source 0 does not exist)
 * base + 0x000004: Interrupt source 1 priority
 * base + 0x000008: Interrupt source 2 priority
 * ...
 * base + 0x000FFC: Interrupt source 1023 priority
 * base + 0x001000: Pending 0
 * base + 0x001FFF: Pending
 * base + 0x002000: Enable bits for sources 0-31 on context 0
 * base + 0x002004: Enable bits for sources 32-63 on context 0
 * ...
 * base + 0x0020FC: Enable bits for sources 992-1023 on context 0
 * base + 0x002080: Enable bits for sources 0-31 on context 1
 * ...
 * base + 0x002100: Enable bits for sources 0-31 on context 2
 * ...
 * base + 0x1F1F80: Enable bits for sources 992-1023 on context 15871
 * base + 0x1F1F84: Reserved
 * ...		    (higher context IDs would fit here, but wouldn't fit
 *		     inside the per-context priority vector)
 * base + 0x1FFFFC: Reserved
 * base + 0x200000: Priority threshold for context 0
 * base + 0x200004: Claim/complete for context 0
 * base + 0x200008: Reserved
 * ...
 * base + 0x200FFC: Reserved
 * base + 0x201000: Priority threshold for context 1
 * base + 0x201004: Claim/complete for context 1
 * ...
 * base + 0xFFE000: Priority threshold for context 15871
 * base + 0xFFE004: Claim/complete for context 15871
 * base + 0xFFE008: Reserved
 * ...
 * base + 0xFFFFFC: Reserved
 */

/* Each interrupt source has a priority register associated with it. */
#define PRIORITY_BASE           0
#define PRIORITY_PER_ID         4

/*
 * Each hart context has a vector of interupt enable bits associated with it.
 * There's one bit for each interrupt source.
 */
#define ENABLE_BASE             0x2000
#define ENABLE_PER_HART         0x80

/*
 * Each hart context has a set of control registers associated with it.  Right
 * now there's only two: a source priority threshold over which the hart will
 * take an interrupt, and a register to claim interrupts.
 */
#define CONTEXT_BASE            0x200000
#define CONTEXT_PER_HART        0x1000
#define CONTEXT_THRESHOLD       0
#define CONTEXT_CLAIM           4

#define REG_SIZE                0x1000000


typedef uint64_t reg_t;


struct plic_context_t {
        uint32_t num;
        int proc_id;
        bool mmode;

        uint8_t priority_threshold;
        uint32_t enable[PLIC_MAX_DEVICES/32];
        uint32_t pending[PLIC_MAX_DEVICES/32];
        uint8_t pending_priority[PLIC_MAX_DEVICES];
        uint32_t claimed[PLIC_MAX_DEVICES/32];
};



class Plic : public vp::component
{
public:
    Plic(js::config *config);

    int build();

private:
    static vp::io_req_status_e req(void *__this, vp::io_req *req);
    static void irq_sync(void *__this, bool active, int port);

    std::vector<vp::wire_slave<bool>> irq_itfs;
    vp::io_slave input_itf;
    std::vector<vp::wire_master<bool>> m_irq_itf;
    std::vector<vp::wire_master<bool>> s_irq_itf;
    int ndev;

    bool load(reg_t addr, size_t len, uint8_t* bytes);
    bool store(reg_t addr, size_t len, const uint8_t* bytes);
    size_t size() { return PLIC_SIZE; }
private:
    vp::trace trace;
    std::vector<plic_context_t> contexts;
    uint32_t num_ids;
    uint32_t num_ids_word;
    uint32_t max_prio;
    uint8_t priority[PLIC_MAX_DEVICES];
    uint32_t level[PLIC_MAX_DEVICES/32];
    uint32_t context_best_pending(const plic_context_t *c);
    void context_update(const plic_context_t *context);
    uint32_t context_claim(plic_context_t *c);
    bool priority_read(reg_t offset, uint32_t *val);
    bool priority_write(reg_t offset, uint32_t val);
    bool context_enable_read(const plic_context_t *context,
                            reg_t offset, uint32_t *val);
    bool context_enable_write(plic_context_t *context,
                                reg_t offset, uint32_t val);
    bool context_read(plic_context_t *context,
                        reg_t offset, uint32_t *val);
    bool context_write(plic_context_t *context,
                        reg_t offset, uint32_t val);

};



int Plic::build()
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);

    this->input_itf.set_req_meth(&Plic::req);
    new_slave_port("input", &this->input_itf);
    bool smode = true;
    int nb_procs = 1;

    this->ndev = this->get_js_config()->get("ndev")->get_int();

    this->irq_itfs.resize(this->ndev);
    for (int i=0; i<this->ndev; i++)
    {
        this->irq_itfs[i].set_sync_meth_muxed(&Plic::irq_sync, i);
        this->new_slave_port("irq" + std::to_string(i+1), &this->irq_itfs[i]);
    }

    this->m_irq_itf.resize(nb_procs);
    this->s_irq_itf.resize(nb_procs);
    for (int i=0; i<nb_procs; i++)
    {
        this->new_master_port("m_irq_" + std::to_string(i), &this->m_irq_itf[i]);
        this->new_master_port("s_irq_" + std::to_string(i), &this->s_irq_itf[i]);
    }

    size_t contexts_per_hart = smode ? 2 : 1;

    this->contexts.resize(contexts_per_hart * nb_procs);

    num_ids = ndev + 1;
    num_ids_word = num_ids / 32;
    if ((num_ids_word * 32) < num_ids)
        num_ids_word++;
    max_prio = (1UL << PLIC_PRIO_BITS) - 1;
    memset(priority, 0, sizeof(priority));
    memset(level, 0, sizeof(level));

    for (size_t i = 0; i < contexts.size(); i++) {
        plic_context_t* c = &contexts[i];
        c->num = i;
        c->proc_id = i / contexts_per_hart;
        if (smode) {
            c->mmode = (i % contexts_per_hart == 0);
        } else {
            c->mmode = true;
        }
        memset(&c->enable, 0, sizeof(c->enable));
        memset(&c->pending, 0, sizeof(c->pending));
        memset(&c->pending_priority, 0, sizeof(c->pending_priority));
        memset(&c->claimed, 0, sizeof(c->claimed));
    }

    return 0;
}


void Plic::irq_sync(void *__this, bool active, int port)
{
    Plic *_this = (Plic *)__this;
    int id = port + 1;

    uint8_t id_prio = _this->priority[id];
    uint32_t id_word = id / 32;
    uint32_t id_mask = 1 << (id % 32);

    if (active) {
        _this->level[id_word] |= id_mask;
    } else {
        _this->level[id_word] &= ~id_mask;
    }

    /*
    * Note: PLIC interrupts are level-triggered. As of now,
    * there is no notion of edge-triggered interrupts. To
    * handle this we auto-clear edge-triggered interrupts
    * when PLIC context CLAIM register is read.
    */
    for (size_t i = 0; i < _this->contexts.size(); i++) {
        plic_context_t* c = &_this->contexts[i];

        if (c->enable[id_word] & id_mask) {
            if (active) {
                c->pending[id_word] |= id_mask;
                c->pending_priority[id] = id_prio;
            } else {
                c->pending[id_word] &= ~id_mask;
                c->pending_priority[id] = 0;
                c->claimed[id_word] &= ~id_mask;
            }
            _this->context_update(c);
            break;
        }
    }
}


vp::io_req_status_e Plic::req(void *__this, vp::io_req *req)
{
    Plic *_this = (Plic *)__this;
    bool status;

    _this->trace.msg(vp::trace::LEVEL_TRACE, "Received request (offset: 0x%x, size: 0x%d, is_write: %d)\n",
        req->get_addr(), req->get_size(), req->get_is_write());

    if (req->get_is_write())
    {
        status = _this->store(req->get_addr(), req->get_size(), req->get_data());
    }
    else
    {
        status = _this->load(req->get_addr(), req->get_size(), req->get_data());
    }
    return status ? vp::IO_REQ_OK : vp::IO_REQ_INVALID;
}



uint32_t Plic::context_best_pending(const plic_context_t *c)
{
  uint8_t best_id_prio = 0;
  uint32_t best_id = 0;

  for (uint32_t i = 0; i < num_ids_word; i++) {
    if (!c->pending[i]) {
      continue;
    }

    for (uint32_t j = 0; j < 32; j++) {
      uint32_t id = i * 32 + j;
      if ((num_ids <= id) ||
          !(c->pending[i] & (1 << j)) ||
          (c->claimed[i] & (1 << j))) {
        continue;
      }

      if (!best_id ||
          (best_id_prio < c->pending_priority[id])) {
        best_id = id;
        best_id_prio = c->pending_priority[id];
      }
    }
  }

  return best_id;
}

void Plic::context_update(const plic_context_t *c)
{
    uint32_t best_id = context_best_pending(c);
    bool active = best_id != 0;
    if (c->mmode)
    {
        this->m_irq_itf[c->proc_id].sync(active);
    }
    else
    {
        this->s_irq_itf[c->proc_id].sync(active);
    }
}

uint32_t Plic::context_claim(plic_context_t *c)
{
  uint32_t best_id = context_best_pending(c);
  uint32_t best_id_word = best_id / 32;
  uint32_t best_id_mask = (1 << (best_id % 32));

  if (best_id) {
    c->claimed[best_id_word] |= best_id_mask;
  }

  context_update(c);
  return best_id;
}

bool Plic::priority_read(reg_t offset, uint32_t *val)
{
  uint32_t id = (offset >> 2);

  if (id > 0 && id < num_ids)
    *val = priority[id];
  else
    *val = 0;

  return true;
}

bool Plic::priority_write(reg_t offset, uint32_t val)
{
  uint32_t id = (offset >> 2);

  if (id > 0 && id < num_ids) {
    val &= ((1 << PLIC_PRIO_BITS) - 1);
    priority[id] = val;
  }

  return true;
}

bool Plic::context_enable_read(const plic_context_t *c,
                                 reg_t offset, uint32_t *val)
{
  uint32_t id_word = offset >> 2;

  if (id_word < num_ids_word)
    *val = c->enable[id_word];
  else
    *val = 0;

  return true;
}

bool Plic::context_enable_write(plic_context_t *c,
                                  reg_t offset, uint32_t val)
{
  uint32_t id_word = offset >> 2;

  if (id_word >= num_ids_word)
    return true;

  uint32_t old_val = c->enable[id_word];
  uint32_t new_val = id_word == 0 ? val & ~(uint32_t)1 : val;
  uint32_t xor_val = old_val ^ new_val;

  c->enable[id_word] = new_val;

  for (uint32_t i = 0; i < 32; i++) {
    uint32_t id = id_word * 32 + i;
    uint32_t id_mask = 1 << i;
    uint8_t id_prio = priority[id];
    if (!(xor_val & id_mask)) {
      continue;
    }
    if ((new_val & id_mask) &&
        (level[id_word] & id_mask)) {
      c->pending[id_word] |= id_mask;
      c->pending_priority[id] = id_prio;
    } else if (!(new_val & id_mask)) {
      c->pending[id_word] &= ~id_mask;
      c->pending_priority[id] = 0;
      c->claimed[id_word] &= ~id_mask;
    }
  }

  context_update(c);
  return true;
}

bool Plic::context_read(plic_context_t *c,
                          reg_t offset, uint32_t *val)
{
  switch (offset) {
    case CONTEXT_THRESHOLD:
      *val = c->priority_threshold;
      return true;
    case CONTEXT_CLAIM:
      *val = context_claim(c);
      return true;
    default:
      return true;
  };
}

bool Plic::context_write(plic_context_t *c,
                           reg_t offset, uint32_t val)
{
  bool ret = true, update = false;

  switch (offset) {
    case CONTEXT_THRESHOLD:
      val &= ((1 << PLIC_PRIO_BITS) - 1);
      if (val <= max_prio)
        c->priority_threshold = val;
      else
        update = true;
      break;
    case CONTEXT_CLAIM: {
      uint32_t id_word = val / 32;
      uint32_t id_mask = 1 << (val % 32);
      if ((val < num_ids) &&
          (c->enable[id_word] & id_mask)) {
        c->claimed[id_word] &= ~id_mask;
        update = true;
      }
      break;
    }
    default:
      ret = false;
      update = true;
      break;
  };

  if (update) {
    context_update(c);
  }

  return ret;
}

bool Plic::load(reg_t addr, size_t len, uint8_t* bytes)
{
  bool ret = false;
  uint32_t val = 0;

  switch (len) {
    case 4:
      break;
    case 8:
      // Implement double-word loads as a pair of word loads
      return load(addr, 4, bytes) && load(addr + 4, 4, bytes + 4);
    default:
      // Subword loads are not supported
      return false;
  }

  if (PRIORITY_BASE <= addr && addr < ENABLE_BASE) {
    ret = priority_read(addr, &val);
  } else if (ENABLE_BASE <= addr && addr < CONTEXT_BASE) {
    uint32_t cntx = (addr - ENABLE_BASE) / ENABLE_PER_HART;
    addr -= cntx * ENABLE_PER_HART + ENABLE_BASE;
    if (cntx < contexts.size()) {
      ret = context_enable_read(&contexts[cntx], addr, &val);
    }
  } else if (CONTEXT_BASE <= addr && addr < REG_SIZE) {
    uint32_t cntx = (addr - CONTEXT_BASE) / CONTEXT_PER_HART;
    addr -= cntx * CONTEXT_PER_HART + CONTEXT_BASE;
    if (cntx < contexts.size()) {
      ret = context_read(&contexts[cntx], addr, &val);
    }
  }

  if (ret) {
    memcpy(bytes, (uint8_t *)&val, len);
  }

  return ret;
}

bool Plic::store(reg_t addr, size_t len, const uint8_t* bytes)
{
  bool ret = false;
  uint32_t val;

  switch (len) {
    case 4:
      break;
    case 8:
      // Implement double-word stores as a pair of word stores
      return store(addr, 4, bytes) && store(addr + 4, 4, bytes + 4);
    default:
      // Subword stores are not supported
      return false;
  }

  memcpy((uint8_t *)&val, bytes, len);

  if (PRIORITY_BASE <= addr && addr < ENABLE_BASE) {
    ret = priority_write(addr, val);
  } else if (ENABLE_BASE <= addr && addr < CONTEXT_BASE) {
    uint32_t cntx = (addr - ENABLE_BASE) / ENABLE_PER_HART;
    addr -= cntx * ENABLE_PER_HART + ENABLE_BASE;
    if (cntx < contexts.size())
      ret = context_enable_write(&contexts[cntx], addr, val);
  } else if (CONTEXT_BASE <= addr && addr < REG_SIZE) {
    uint32_t cntx = (addr - CONTEXT_BASE) / CONTEXT_PER_HART;
    addr -= cntx * CONTEXT_PER_HART + CONTEXT_BASE;
    if (cntx < contexts.size())
      ret = context_write(&contexts[cntx], addr, val);
  }

  return ret;
}




Plic::Plic(js::config *config)
    : vp::component(config)
{
}


extern "C" vp::component *vp_constructor(js::config *config)
{
    return new Plic(config);
}
