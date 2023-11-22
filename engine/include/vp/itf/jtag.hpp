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

#ifndef __VP_ITF_JTAG_HPP__
#define __VP_ITF_JTAG_HPP__

#include "vp/vp.hpp"

namespace vp {



  class JtagSlave;



  typedef void (JtagSyncMeth)(vp::Block *, int tck, int tdi, int tms, int trst);
  typedef void (JtagSyncCycleMeth)(vp::Block *, int tdi, int tms, int trst);

  typedef void (JtagSyncMethMuxed)(vp::Block *, int tck, int tdi, int tms, int trst, int id);
  typedef void (JtagSyncCycleMethMuxed)(vp::Block *, int tdi, int tms, int trst, int id);

  typedef void (JtagSlaveSyncMeth)(vp::Block *, int tdo);
  typedef void (JtagSlaveSyncMethMuxed)(vp::Block *, int tdo, int id);



  class JtagMaster : public vp::MasterPort
  {
    friend class JtagSlave;

  public:

    inline JtagMaster();

    inline void sync(int tck, int tdi, int tms, int trst)
    {
      return sync_meth((vp::Block *)this->get_remote_context(), tck, tdi, tms, trst);
    }

    inline void sync_cycle(int tdi, int tms, int trst)
    {
      return sync_cycle_meth((vp::Block *)this->get_remote_context(), tdi, tms, trst);
    }

    void bind_to(vp::Port *port, js::Config *config);

    inline void set_sync_meth(JtagSlaveSyncMeth *meth);
    inline void set_sync_meth_muxed(JtagSlaveSyncMethMuxed *meth, int id);

    bool is_bound() { return SlavePort != NULL; }

    int tdo;

  private:

    static inline void sync_muxed_stub(JtagMaster *_this, int tck, int tdi, int tms, int trst);
    static inline void sync_cycle_muxed_stub(JtagMaster *_this, int tdi, int tms, int trst);

    void (*slave_sync)(vp::Block *comp, int tdo);
    void (*slave_sync_muxed)(vp::Block *comp, int tdo, int id) = NULL;

    void (*sync_meth)(vp::Block *, int tck, int tdi, int tms, int trst);
    void (*sync_meth_mux)(vp::Block *, int tck, int tdi, int tms, int trst, int mux);
    void (*sync_cycle_meth)(vp::Block *, int tdi, int tms, int trst);
    void (*sync_cycle_meth_mux)(vp::Block *, int tdi, int tms, int trst, int mux);

    static inline void sync_default(vp::Block *, int tdo);


    vp::Component *comp_mux;
    int sync_mux;
    int mux_id;
    JtagSlave *SlavePort = NULL;
  };



  class JtagSlave : public vp::SlavePort
  {

    friend class JtagMaster;

  public:

    inline JtagSlave();

    inline void sync(int tdo)
    {
      slave_sync_meth((vp::Block *)this->get_remote_context(), tdo);
    }

    inline void set_sync_meth(JtagSyncMeth *meth);
    inline void set_sync_meth_muxed(JtagSyncMethMuxed *meth, int id);

    inline void set_sync_cycle_meth(JtagSyncCycleMeth *meth);
    inline void set_sync_cycle_meth_muxed(JtagSyncCycleMethMuxed *meth, int id);

    inline void bind_to(vp::Port *_port, js::Config *config);

  private:

    static inline void sync_muxed_stub(JtagSlave *_this, int tdo);
    

    void (*slave_sync_meth)(vp::Block *, int tdo);
    void (*slave_sync_mux_meth)(vp::Block *, int tdo, int id);

    void (*sync_meth)(vp::Block *comp, int tck, int tdi, int tms, int trst);
    void (*sync_mux_meth)(vp::Block *comp, int tck, int tdi, int tms, int trst, int mux);
    void (*sync_cycle_meth)(vp::Block *comp, int tdi, int tms, int trst);
    void (*sync_cycle_mux_meth)(vp::Block *comp, int tdi, int tms, int trst, int mux);

    static inline void sync_default(JtagSlave *, int tck, int tdi, int tms, int trst);
    static inline void sync_cycle_default(JtagSlave *, int tdi, int tms, int trst);

    int mux_id;
    vp::Component *comp_mux;
    int sync_mux;


  };


  inline JtagMaster::JtagMaster() {
    slave_sync = &JtagMaster::sync_default;
  }



  inline void JtagMaster::sync_muxed_stub(JtagMaster *_this, int tck, int tdi, int tms, int trst)
  {
    return _this->sync_meth_mux(_this->comp_mux, tck, tdi, tms, trst, _this->sync_mux);
  }



  inline void JtagMaster::sync_cycle_muxed_stub(JtagMaster *_this, int tdi, int tms, int trst)
  {
    return _this->sync_cycle_meth_mux(_this->comp_mux, tdi, tms, trst, _this->sync_mux);
  }




  inline void JtagMaster::bind_to(vp::Port *_port, js::Config *config)
  {
    JtagSlave *port = (JtagSlave *)_port;
    if (port->sync_mux_meth == NULL)
    {
      sync_meth = port->sync_meth;
      sync_cycle_meth = port->sync_cycle_meth;
      this->set_remote_context(port->get_context());
    }
    else
    {
      sync_meth_mux = port->sync_mux_meth;
      sync_meth = (JtagSyncMeth *)&JtagMaster::sync_muxed_stub;

      sync_cycle_meth_mux = port->sync_cycle_mux_meth;
      sync_cycle_meth = (JtagSyncCycleMeth *)&JtagMaster::sync_cycle_muxed_stub;

      this->set_remote_context(this);
      comp_mux = (vp::Component *)port->get_context();
      sync_mux = port->mux_id;
    }
  }

  inline void JtagMaster::set_sync_meth(JtagSlaveSyncMeth *meth)
  {
    slave_sync = meth;
    slave_sync_muxed = NULL;
  }

  inline void JtagMaster::set_sync_meth_muxed(JtagSlaveSyncMethMuxed *meth, int id)
  {
    slave_sync = NULL;
    slave_sync_muxed = meth;
    mux_id = id;
  }

  inline void JtagMaster::sync_default(vp::Block *, int tdo)
  {
  }

  inline void JtagSlave::sync_muxed_stub(JtagSlave *_this, int tdo)
  {
    return _this->slave_sync_mux_meth(_this->comp_mux, tdo, _this->sync_mux);
  }

  inline void JtagSlave::bind_to(vp::Port *_port, js::Config *config)
  {
    SlavePort::bind_to(_port, config);
    JtagMaster *port = (JtagMaster *)_port;
    port->SlavePort = this;
    if (port->slave_sync_muxed == NULL)
    {
      this->slave_sync_meth = port->slave_sync;
      this->set_remote_context(port->get_context());
    }
    else
    {
      this->slave_sync_mux_meth = port->slave_sync_muxed;  
      this->slave_sync_meth = (JtagSlaveSyncMeth *)&JtagSlave::sync_muxed_stub;
      this->set_remote_context(this);
      comp_mux = (vp::Component *)port->get_context();
      sync_mux = port->mux_id;
    }
  }

  inline JtagSlave::JtagSlave() : sync_meth(NULL), sync_mux_meth(NULL) {
    sync_meth = (JtagSyncMeth *)&JtagSlave::sync_default;
    sync_cycle_meth = (JtagSyncCycleMeth *)&JtagSlave::sync_cycle_default;
  }

  inline void JtagSlave::set_sync_meth(JtagSyncMeth *meth)
  {
    sync_meth = meth;
    sync_mux_meth = NULL;
  }

  inline void JtagSlave::set_sync_cycle_meth(JtagSyncCycleMeth *meth)
  {
    sync_cycle_meth = meth;
    sync_cycle_mux_meth = NULL;
  }

  inline void JtagSlave::set_sync_meth_muxed(JtagSyncMethMuxed *meth, int id)
  {
    sync_mux_meth = meth;
    sync_meth = NULL;
    mux_id = id;
  }

  inline void JtagSlave::set_sync_cycle_meth_muxed(JtagSyncCycleMethMuxed *meth, int id)
  {
    sync_cycle_mux_meth = meth;
    sync_cycle_meth = NULL;
    mux_id = id;
  }

  inline void JtagSlave::sync_default(JtagSlave *, int tck, int tdi, int tms, int trst)
  {
  }

  inline void JtagSlave::sync_cycle_default(JtagSlave *, int tdi, int tms, int trst)
  {
  }




};

#endif
