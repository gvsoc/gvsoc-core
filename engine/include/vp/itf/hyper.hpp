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

#ifndef __VP_ITF_HYPER_HPP__
#define __VP_ITF_HYPER_HPP__

#include "vp/vp.hpp"

namespace vp {



  class HyperSlave;



  typedef void (HyperSyncCycleMeth)(vp::Block *, int data);
  typedef void (HyperCsSyncMeth)(vp::Block *, int cs, int active);

  typedef void (HyperSyncCycleMethMuxed)(vp::Block *, int data, int id);
  typedef void (HyperCsSyncMethMuxed)(vp::Block *, int cs, int active, int id);


  class HyperMaster : public vp::MasterPort
  {
    friend class HyperSlave;

  public:

    inline HyperMaster();

    inline void sync_cycle(int data)
    {
      return sync_cycle_meth((vp::Block *)this->get_remote_context(), data);
    }

    inline void cs_sync(int cs, int active)
    {
      return cs_sync_meth((vp::Block *)this->get_remote_context(), cs, active);
    }

    void bind_to(vp::Port *port, js::Config *config);

    inline void set_sync_cycle_meth(HyperSyncCycleMeth *meth);
    inline void set_sync_cycle_meth_muxed(HyperSyncCycleMethMuxed *meth, int id);

    bool is_bound() { return SlavePort != NULL; }

  private:

    static inline void sync_cycle_muxed_stub(HyperMaster *_this, int data);
    static inline void cs_sync_muxed_stub(HyperMaster *_this, int cs, int active);

    void (*slave_sync_cycle)(vp::Block *comp, int data);
    void (*slave_sync_cycle_mux)(vp::Block *comp, int data, int mux);

    void (*sync_cycle_meth)(vp::Block *, int data);
    void (*sync_cycle_meth_mux)(vp::Block *, int data, int mux);
    void (*cs_sync_meth)(vp::Block *, int cs, int active);
    void (*cs_sync_meth_mux)(vp::Block *, int cs, int active, int mux);

    static inline void sync_cycle_default(vp::Block *, int data);


    vp::Component *comp_mux;
    int sync_mux;
    HyperSlave *SlavePort = NULL;
    int mux_id;
  };



  class HyperSlave : public vp::SlavePort
  {

    friend class HyperMaster;

  public:

    inline HyperSlave();

    inline void sync_cycle(int data)
    {
      slave_sync_cycle_meth((vp::Block *)this->get_remote_context(), data);
    }

    inline void set_sync_cycle_meth(HyperSyncCycleMeth *meth);
    inline void set_sync_cycle_meth_muxed(HyperSyncCycleMethMuxed *meth, int id);

    inline void set_cs_sync_meth(HyperCsSyncMeth *meth);
    inline void set_cs_sync_meth_muxed(HyperCsSyncMethMuxed *meth, int id);

    inline void bind_to(vp::Port *_port, js::Config *config);

    static inline void sync_cycle_muxed_stub(HyperSlave *_this, int data);

  private:


    void (*slave_sync_cycle_meth)(vp::Block *, int data);
    void (*slave_sync_cycle_meth_mux)(vp::Block *, int data, int mux);

    void (*sync_cycle_meth)(vp::Block *comp, int data);
    void (*sync_cycle_mux_meth)(vp::Block *comp, int data, int mux);
    void (*cs_sync)(vp::Block *comp, int cs, int active);
    void (*cs_sync_mux)(vp::Block *comp, int cs, int active, int mux);

    static inline void sync_cycle_default(HyperSlave *, int data);
    static inline void cs_sync_default(HyperSlave *, int cs, int active);

    vp::Component *comp_mux;
    int sync_mux;
    int mux_id;


  };


  inline HyperMaster::HyperMaster() {
    slave_sync_cycle = &HyperMaster::sync_cycle_default;
    slave_sync_cycle_mux = NULL;
  }


  inline void HyperMaster::sync_cycle_default(vp::Block *, int data)
  {
  }


  inline void HyperMaster::set_sync_cycle_meth(HyperSyncCycleMeth *meth)
  {
    slave_sync_cycle = meth;
  }

  inline void HyperMaster::set_sync_cycle_meth_muxed(HyperSyncCycleMethMuxed *meth, int id)
  {
    slave_sync_cycle_mux = meth;
    slave_sync_cycle = NULL;
    mux_id = id;
  }



  inline void HyperMaster::sync_cycle_muxed_stub(HyperMaster *_this, int data)
  {
    return _this->sync_cycle_meth_mux(_this->comp_mux, data, _this->sync_mux);
  }



  inline void HyperMaster::cs_sync_muxed_stub(HyperMaster *_this, int cs, int active)
  {
    return _this->cs_sync_meth_mux(_this->comp_mux, cs, active, _this->sync_mux);
  }



  inline void HyperMaster::bind_to(vp::Port *_port, js::Config *config)
  {
    HyperSlave *port = (HyperSlave *)_port;
    if (port->sync_cycle_mux_meth == NULL)
    {
      sync_cycle_meth = port->sync_cycle_meth;
      cs_sync_meth = port->cs_sync;
      this->set_remote_context(port->get_context());
    }
    else
    {
      sync_cycle_meth_mux = port->sync_cycle_mux_meth;
      sync_cycle_meth = (HyperSyncCycleMeth *)&HyperMaster::sync_cycle_muxed_stub;

      cs_sync_meth_mux = port->cs_sync_mux;
      cs_sync_meth = (HyperCsSyncMeth *)&HyperMaster::cs_sync_muxed_stub;

      this->set_remote_context(this);
      comp_mux = (vp::Component *)port->get_context();
      sync_mux = port->mux_id;
    }
  }

  inline void HyperSlave::sync_cycle_muxed_stub(HyperSlave *_this, int data)
  {
    return _this->slave_sync_cycle_meth_mux(_this->comp_mux, data, _this->sync_mux);
  }

  inline void HyperSlave::bind_to(vp::Port *_port, js::Config *config)
  {
    SlavePort::bind_to(_port, config);
    HyperMaster *port = (HyperMaster *)_port;
    port->SlavePort = this;
    if (port->slave_sync_cycle_mux == NULL)
    {
      this->slave_sync_cycle_meth = port->slave_sync_cycle;
      this->set_remote_context(port->get_context());
    }
    else
    {
      this->slave_sync_cycle_meth_mux = port->slave_sync_cycle_mux;
      this->slave_sync_cycle_meth = (HyperSyncCycleMeth *)&HyperSlave::sync_cycle_muxed_stub;

      set_remote_context(this);
      comp_mux = (vp::Component *)port->get_context();
      sync_mux = port->mux_id;
    }
  }

  inline HyperSlave::HyperSlave() : sync_cycle_meth(NULL), sync_cycle_mux_meth(NULL) {
    sync_cycle_meth = (HyperSyncCycleMeth *)&HyperSlave::sync_cycle_default;
    cs_sync = (HyperCsSyncMeth *)&HyperSlave::cs_sync_default;
  }

  inline void HyperSlave::set_sync_cycle_meth(HyperSyncCycleMeth *meth)
  {
    sync_cycle_meth = meth;
    sync_cycle_mux_meth = NULL;
  }

  inline void HyperSlave::set_cs_sync_meth(HyperCsSyncMeth *meth)
  {
    cs_sync = meth;
    cs_sync_mux = NULL;
  }

  inline void HyperSlave::set_sync_cycle_meth_muxed(HyperSyncCycleMethMuxed *meth, int id)
  {
    sync_cycle_mux_meth = meth;
    sync_cycle_meth = NULL;
    mux_id = id;
  }

  inline void HyperSlave::set_cs_sync_meth_muxed(HyperCsSyncMethMuxed *meth, int id)
  {
    cs_sync_mux = meth;
    cs_sync = NULL;
    mux_id = id;
  }

  inline void HyperSlave::sync_cycle_default(HyperSlave *, int data)
  {
  }


  inline void HyperSlave::cs_sync_default(HyperSlave *, int cs, int active)
  {
  }



};

#endif
