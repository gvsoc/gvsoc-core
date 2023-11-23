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
 */

#ifndef __VP_ITF_CLOCK_HPP__
#define __VP_ITF_CLOCK_HPP__

#include "vp/vp.hpp"

namespace vp {

  class Component;

  class ClockSlave;

  class ClockMaster : public MasterPort
  {
    friend class ClockSlave;
  public:

    ClockMaster();

    inline void sync(bool value)
    {
      if (next) next->sync(value);
      sync_meth((vp::Block *)this->get_remote_context(), value);
    }

    inline void set_frequency(int64_t frequency)
    {
      if (next) next->set_frequency(frequency);
      set_frequency_meth((vp::Block *)this->get_remote_context(), frequency);
    }

    void bind_to(vp::Port *port, js::Config *config);

    bool is_bound() { return SlavePort != NULL; }

    void finalize();

  private:
    static inline void sync_muxed(ClockMaster *_this, bool value);
    static inline void set_frequency_muxed(ClockMaster *_this, int64_t frequency);
    static inline void sync_freq_cross_stub(ClockMaster *_this, bool value);

    static inline void sync_default(vp::Block *, bool value);
    static inline void set_frequency_default(vp::Block *, int64_t value);
    static inline void set_frequency_freq_cross_stub(ClockMaster *_this, int64_t value);

    void (*sync_meth)(vp::Block *, bool value);
    void (*sync_meth_mux)(vp::Block *, bool value, int id);
    void (*sync_meth_freq_cross)(vp::Block *, bool value);

    void (*set_frequency_meth)(vp::Block *, int64_t frequency);
    void (*set_frequency_meth_mux)(vp::Block *, int64_t frequency, int id);
    void (*set_frequency_meth_freq_cross)(vp::Block *, int64_t value);

    vp::Component *comp_mux;
    int sync_mux;
    ClockSlave *SlavePort = NULL;
    ClockMaster *next = NULL;

    vp::Block *slave_context_for_freq_cross;
  };


  class ClockSlave : public SlavePort
  {

    friend class ClockMaster;

  public:

    inline ClockSlave();

    void set_sync_meth(void (*)(vp::Block *_this, bool value));
    void set_sync_meth_muxed(void (*)(vp::Block *_this, bool value, int), int id);

    void set_set_frequency_meth(void (*)(vp::Block *_this, int64_t frequency));
    void set_set_frequency_meth_muxed(void (*)(vp::Block *_this, int64_t, int), int id);

    inline void bind_to(vp::Port *_port, js::Config *config);


  private:

    void (*sync)(vp::Block *comp, bool value);
    void (*sync_mux)(vp::Block *comp, bool value, int id);

    void (*set_frequency)(vp::Block *comp, int64_t frequency);
    void (*set_frequency_mux)(vp::Block *comp, int64_t frequency, int id);

    int sync_mux_id;
  };

};

#endif
