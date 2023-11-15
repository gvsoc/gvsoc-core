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

#ifndef __VP_ITF_CPI_HPP__
#define __VP_ITF_CPI_HPP__

#include "vp/vp.hpp"

namespace vp {



  class CpiSlave;



  typedef void (CpiSyncMeth)(void *, int pclk, int href, int vsync, int data);
  typedef void (CpiSyncMethMuxed)(void *, int pclk, int href, int vsync, int data, int id);

  typedef void (CpiSyncCycleMeth)(void *, int href, int vsync, int data);
  typedef void (CpiSyncCycleMethMuxed)(void *, int href, int vsync, int data, int id);


  class CpiMaster : public vp::MasterPort
  {
    friend class CpiSlave;

  public:

    inline CpiMaster();

    inline void sync(int pclk, int href, int vsync, int data)
    {
      return sync_meth(this->get_remote_context(), pclk, href, vsync, data);
    }

    inline void sync_cycle(int href, int vsync, int data)
    {
      return sync_cycle_meth(this->get_remote_context(), href, vsync, data);
    }

    void bind_to(vp::Port *port, js::Config *config);

    bool is_bound() { return SlavePort != NULL; }

  private:

    static inline void sync_muxed_stub(CpiMaster *_this, int pclk, int href, int vsync, int data);
    static inline void sync_cycle_muxed_stub(CpiMaster *_this, int href, int vsync, int data);

    void (*sync_meth)(void *, int pclk, int href, int vsync, int data);
    void (*sync_meth_mux)(void *, int pclk, int href, int vsync, int data, int mux);

    void (*sync_cycle_meth)(void *, int href, int vsync, int data);
    void (*sync_cycle_meth_mux)(void *, int href, int vsync, int data, int mux);

    vp::Component *comp_mux;
    int sync_mux;
    CpiSlave *SlavePort = NULL;

  };



  class CpiSlave : public vp::SlavePort
  {

    friend class CpiMaster;

  public:

    inline CpiSlave();

    inline void set_sync_meth(CpiSyncMeth *meth);
    inline void set_sync_meth_muxed(CpiSyncMethMuxed *meth, int id);

    inline void set_sync_cycle_meth(CpiSyncCycleMeth *meth);
    inline void set_sync_cycle_meth_muxed(CpiSyncCycleMethMuxed *meth, int id);

    inline void bind_to(vp::Port *_port, js::Config *config);

  private:

    void (*sync_meth)(void *comp, int pclk, int href, int vsync, int data);
    void (*sync_mux_meth)(void *comp, int pclk, int href, int vsync, int data, int mux);

    void (*sync_cycle_meth)(void *comp, int href, int vsync, int data);
    void (*sync_cycle_mux_meth)(void *comp, int href, int vsync, int data, int mux);

    static inline void sync_default(CpiSlave *, int pclk, int href, int vsync, int data);
    static inline void sync_cycle_default(CpiSlave *, int href, int vsync, int data);

    int mux_id;

  };


  inline CpiMaster::CpiMaster() {
  }




  inline void CpiMaster::sync_muxed_stub(CpiMaster *_this, int pclk, int href, int vsync, int data)
  {
    return _this->sync_meth_mux(_this->comp_mux, pclk, href, vsync, data, _this->sync_mux);
  }

  inline void CpiMaster::sync_cycle_muxed_stub(CpiMaster *_this, int href, int vsync, int data)
  {
    return _this->sync_cycle_meth_mux(_this->comp_mux, href, vsync, data, _this->sync_mux);
  }

  inline void CpiMaster::bind_to(vp::Port *_port, js::Config *config)
  {
    CpiSlave *port = (CpiSlave *)_port;
    if (port->sync_mux_meth == NULL)
    {
      sync_meth = port->sync_meth;
      sync_cycle_meth = port->sync_cycle_meth;
      set_remote_context(port->get_context());
    }
    else
    {
      sync_meth_mux = port->sync_mux_meth;
      sync_meth = (CpiSyncMeth *)&CpiMaster::sync_muxed_stub;

      sync_cycle_meth_mux = port->sync_cycle_mux_meth;
      sync_cycle_meth = (CpiSyncCycleMeth *)&CpiMaster::sync_cycle_muxed_stub;

      set_remote_context(this);
      comp_mux = (vp::Component *)port->get_context();
      sync_mux = port->mux_id;
    }
  }

  inline void CpiSlave::bind_to(vp::Port *_port, js::Config *config)
  {
    SlavePort::bind_to(_port, config);
  }

  inline CpiSlave::CpiSlave() : sync_meth(NULL), sync_mux_meth(NULL) {
    sync_meth = (CpiSyncMeth *)&CpiSlave::sync_default;
    sync_cycle_meth = (CpiSyncCycleMeth *)&CpiSlave::sync_cycle_default;
  }

  inline void CpiSlave::set_sync_meth(CpiSyncMeth *meth)
  {
    sync_meth = meth;
    sync_mux_meth = NULL;
  }

  inline void CpiSlave::set_sync_meth_muxed(CpiSyncMethMuxed *meth, int id)
  {
    sync_mux_meth = meth;
    sync_meth = NULL;
    mux_id = id;
  }

  inline void CpiSlave::set_sync_cycle_meth(CpiSyncCycleMeth *meth)
  {
    sync_cycle_meth = meth;
    sync_cycle_mux_meth = NULL;
  }

  inline void CpiSlave::set_sync_cycle_meth_muxed(CpiSyncCycleMethMuxed *meth, int id)
  {
    sync_cycle_mux_meth = meth;
    sync_cycle_meth = NULL;
    mux_id = id;
  }

  inline void CpiSlave::sync_default(CpiSlave *, int pclk, int href, int vsync, int data)
  {
  }

  inline void CpiSlave::sync_cycle_default(CpiSlave *, int href, int vsync, int data)
  {
  }



};

#endif
