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

#ifndef __VP_ITF_IMPLEM_WIRE_CLASS_HPP__
#define __VP_ITF_IMPLEM_WIRE_CLASS_HPP__

#include "vp/vp.hpp"

namespace vp {

  class Component;
  class Block;

  template<class T>
  class WireSlave;

  template<class T>
  class WireMaster : public MasterPort
  {
    friend class WireSlave<T>;
  public:

    WireMaster();

    void set_sync_meth(void (*)(vp::Block *_this, T value));
    void set_sync_meth_muxed(void (*)(vp::Block *_this, T value, int), int id);

    inline void sync(T value)
    {
      if (next) next->sync(value);
      sync_meth((vp::Block *)this->get_remote_context(), value);
    }

    inline void sync_back(T *value)
    {
      if (next) next->sync_back(value);
      sync_back_meth((vp::Block *)this->get_remote_context(), value);
    }

    void bind_to(vp::Port *port, js::Config *config);

    bool is_bound() { return SlavePort != NULL; }

    void finalize();

  private:
    static inline void sync_muxed(WireMaster *_this, T value);
    static inline void sync_freq_cross_stub(WireMaster *_this, T value);
    static inline void sync_back_freq_cross_stub(WireMaster *_this, T *value);
    static inline void sync_back_muxed(WireMaster *_this, T *value);
    void (*sync_meth)(vp::Block *, T value);
    void (*sync_meth_mux)(vp::Block *, T value, int id);
    void (*sync_back_meth)(vp::Block *, T *value);
    void (*sync_back_meth_mux)(vp::Block *, T *value, int id);

    void (*sync_meth_freq_cross)(vp::Block *, T value);
    void (*sync_back_meth_freq_cross)(vp::Block *, T *value);

    void (*master_sync_meth)(vp::Block *comp, T value);
    void (*master_sync_meth_mux)(vp::Block *comp, T value, int id);

    // Default sync callback, just do nothing.
    static inline void sync_default(vp::Block *, T value) {}

    vp::Component *comp_mux;
    int sync_mux;
    int sync_back_mux;
    WireSlave<T> *SlavePort = NULL;
    WireMaster<T> *next = NULL;

    vp::Block *slave_context_for_freq_cross;

    int master_sync_mux_id;
  };



  template<class T>
  class WireSlave : public SlavePort
  {

    friend class WireMaster<T>;

  public:

    inline WireSlave();

    inline void sync(T value)
    {
      this->master_sync_meth((vp::Block *)this->get_remote_context(), value);
    }

    void set_sync_meth(void (*)(vp::Block *_this, T value));
    void set_sync_meth_muxed(void (*)(vp::Block *_this, T value, int), int id);

    void set_sync_back_meth(void (*)(vp::Block *_this, T *value));
    void set_sync_back_meth_muxed(void (*)(vp::Block *_this, T *value, int), int id);

    inline void bind_to(vp::Port *_port, js::Config *config);

    bool is_bound() { return master_port != NULL; }


  private:

    static inline void sync_muxed(WireSlave *_this, T value);
    
    static inline void sync_muxed_stub(WireSlave *_this, T value);

    void (*sync_meth)(vp::Block *comp, T value);
    void (*sync_meth_mux)(vp::Block *comp, T value, int id);

    void (*sync_back)(vp::Block *comp, T *value);
    void (*sync_back_mux)(vp::Block *comp, T *value, int id);

    void (*master_sync_meth)(vp::Block *comp, T value);
    void (*master_sync_meth_mux)(vp::Block *comp, T value, int id);

    int sync_mux_id;

    int sync_back_mux_id;

    vp::Component *master_comp_mux;
    int master_sync_mux;

    WireMaster<T> *master_port = NULL;
  };

};

#endif
