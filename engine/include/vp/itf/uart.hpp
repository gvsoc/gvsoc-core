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

#ifndef __VP_ITF_UART_HPP__
#define __VP_ITF_UART_HPP__

#include "vp/vp.hpp"

namespace vp {



  class UartSlave;



  typedef void (UartSyncMeth)(void *, int data);
  typedef void (UartSyncMethMuxed)(void *, int data, int id);

  typedef void (UartSyncFullMeth)(void *, int data, int sck, int rtr, unsigned int mask);
  typedef void (UartSyncFullMethMuxed)(void *, int data, int sck, int rtr, unsigned int mask, int id);



  class UartMaster : public vp::MasterPort
  {
    friend class UartSlave;

  public:

    inline UartMaster();

    inline void sync(int data)
    {
      return sync_meth(this->get_remote_context(), data);
    }

    inline void sync_full(int data, int sck, int rtr, unsigned int mask)
    {
      if (sync_full_meth)
        return sync_full_meth(this->get_remote_context(), data, sck, rtr, mask);

      return this->sync(data);
    }

    void bind_to(vp::Port *port, js::Config *config);

    inline void set_sync_meth(UartSyncMeth *meth);
    inline void set_sync_meth_muxed(UartSyncMethMuxed *meth, int id);

    inline void set_sync_full_meth(UartSyncFullMeth *meth);
    inline void set_sync_full_meth_muxed(UartSyncFullMethMuxed *meth, int id);

    bool is_bound() { return SlavePort != NULL; }

  private:

    static inline void sync_muxed_stub(UartMaster *_this, int data);
    static inline void sync_full_muxed_stub(UartMaster *_this, int data, int sck, int rtr, unsigned int mask);

    void (*slave_sync)(void *comp, int data);
    void (*slave_sync_mux)(void *comp, int data, int mux);

    void (*slave_sync_full)(void *comp, int data, int sck, int rtr, unsigned int mask);
    void (*slave_sync_full_mux)(void *comp, int data, int sck, int rtr, unsigned int mask, int mux);

    void (*sync_meth)(void *, int data);
    void (*sync_meth_mux)(void *, int data, int mux);

    void (*sync_full_meth)(void *, int data, int sck, int rtr, unsigned int mask);
    void (*sync_full_meth_mux)(void *, int data, int sck, int rtr, unsigned int mask, int mux);

    static inline void sync_default(void *, int data);

    vp::Component *comp_mux;
    int sync_mux;
    UartSlave *SlavePort = NULL;
    int mux_id;

  };



  class UartSlave : public vp::SlavePort
  {

    friend class UartMaster;

  public:

    inline UartSlave();

    inline void sync(int data)
    {
      slave_sync_meth(this->get_remote_context(), data);
    }

    inline void sync_full(int data, int sck, int rtr, unsigned int mask)
    {
      if (slave_sync_full_meth)
      {
        slave_sync_full_meth(this->get_remote_context(), data, sck, rtr, mask);
      }
      else
      {
        this->sync(data);
      }
    }

    inline void set_sync_meth(UartSyncMeth *meth);
    inline void set_sync_meth_muxed(UartSyncMethMuxed *meth, int id);

    inline void set_sync_full_meth(UartSyncFullMeth *meth);
    inline void set_sync_full_meth_muxed(UartSyncFullMethMuxed *meth, int id);

    inline void bind_to(vp::Port *_port, js::Config *config);

  private:

    static inline void sync_muxed_stub(UartSlave *_this, int data);
    static inline void sync_full_muxed_stub(UartSlave *_this, int data, int sck, int rtr, unsigned int mask);

    void (*slave_sync_meth)(void *, int data);
    void (*slave_sync_meth_mux)(void *, int data, int mux);

    void (*slave_sync_full_meth)(void *, int data, int sck, int rtr, unsigned int mask);
    void (*slave_sync_full_meth_mux)(void *, int data, int sck, int rtr, unsigned int mask, int mux);

    void (*sync_meth)(void *comp, int data);
    void (*sync_mux_meth)(void *comp, int data, int mux);

    void (*sync_full_meth)(void *comp, int data, int sck, int rtr, unsigned int mask);
    void (*sync_full_mux_meth)(void *comp, int data, int sck, int rtr, unsigned int mask, int mux);

    static inline void sync_default(UartSlave *, int data);

    vp::Component *comp_mux;
    int sync_mux;
    int mux_id;

  };


  inline UartMaster::UartMaster() {
    slave_sync = &UartMaster::sync_default;
    slave_sync_mux = NULL;
    slave_sync_full = NULL;
    slave_sync_full_mux = NULL;
  }




  inline void UartMaster::sync_muxed_stub(UartMaster *_this, int data)
  {
    return _this->sync_meth_mux(_this->comp_mux, data, _this->sync_mux);
  }

  inline void UartMaster::sync_full_muxed_stub(UartMaster *_this, int data, int sck, int rtr, unsigned int mask)
  {
    return _this->sync_full_meth_mux(_this->comp_mux, data, sck, rtr, rtr, _this->sync_mux);
  }

  inline void UartMaster::bind_to(vp::Port *_port, js::Config *config)
  {
    UartSlave *port = (UartSlave *)_port;
    if (port->sync_mux_meth == NULL && port->sync_full_mux_meth == NULL)
    {
      sync_meth = port->sync_meth;
      sync_full_meth = port->sync_full_meth;
      set_remote_context(port->get_context());
    }
    else
    {
      sync_meth_mux = port->sync_mux_meth;
      sync_meth = (UartSyncMeth *)&UartMaster::sync_muxed_stub;
      sync_full_meth_mux = port->sync_full_mux_meth;
      sync_full_meth = (UartSyncFullMeth *)&UartMaster::sync_full_muxed_stub;

      set_remote_context(this);
      comp_mux = (vp::Component *)port->get_context();
      sync_mux = port->mux_id;
    }
  }

  inline void UartMaster::set_sync_meth(UartSyncMeth *meth)
  {
    slave_sync = meth;
  }

  inline void UartMaster::set_sync_meth_muxed(UartSyncMethMuxed *meth, int id)
  {
    slave_sync_mux = meth;
    slave_sync = NULL;
    mux_id = id;
  }

  inline void UartMaster::set_sync_full_meth(UartSyncFullMeth *meth)
  {
    slave_sync_full = meth;
  }

  inline void UartMaster::set_sync_full_meth_muxed(UartSyncFullMethMuxed *meth, int id)
  {
    slave_sync_full_mux = meth;
    slave_sync_full = NULL;
    mux_id = id;
  }

  inline void UartMaster::sync_default(void *, int data)
  {
  }

  inline void UartSlave::sync_muxed_stub(UartSlave *_this, int data)
  {
    return _this->slave_sync_meth_mux(_this->comp_mux, data, _this->sync_mux);
  }

  inline void UartSlave::sync_full_muxed_stub(UartSlave *_this, int data, int sck,int rtr, unsigned int mask)
  {
    return _this->slave_sync_full_meth_mux(_this->comp_mux, data, sck, rtr, mask, _this->sync_mux);
  }

  inline void UartSlave::bind_to(vp::Port *_port, js::Config *config)
  {
    SlavePort::bind_to(_port, config);
    UartMaster *port = (UartMaster *)_port;
    port->SlavePort = this;
    if (port->slave_sync_mux == NULL)
    {
      this->slave_sync_meth = port->slave_sync;
      this->slave_sync_full_meth = port->slave_sync_full;
      this->set_remote_context(port->get_context());
    }
    else
    {
      this->slave_sync_meth_mux = port->slave_sync_mux;
      this->slave_sync_meth = (UartSyncMeth *)&UartSlave::sync_muxed_stub;

      this->slave_sync_full_meth_mux = port->slave_sync_full_mux;
      if (port->slave_sync_full_mux == NULL)
        this->slave_sync_full_meth = NULL;
      else
        this->slave_sync_full_meth = (UartSyncFullMeth *)&UartSlave::sync_full_muxed_stub;

      set_remote_context(this);
      comp_mux = (vp::Component *)port->get_context();
      sync_mux = port->mux_id;
    }
  }

  inline UartSlave::UartSlave() : sync_meth(NULL), sync_mux_meth(NULL), sync_full_meth(NULL), sync_full_mux_meth(NULL) {
    sync_meth = (UartSyncMeth *)&UartSlave::sync_default;
    sync_full_meth = NULL;
  }

  inline void UartSlave::set_sync_meth(UartSyncMeth *meth)
  {
    sync_meth = meth;
    sync_mux_meth = NULL;
  }

  inline void UartSlave::set_sync_meth_muxed(UartSyncMethMuxed *meth, int id)
  {
    sync_mux_meth = meth;
    sync_meth = NULL;
    mux_id = id;
  }

  inline void UartSlave::sync_default(UartSlave *, int data)
  {
  }

  inline void UartSlave::set_sync_full_meth(UartSyncFullMeth *meth)
  {
    sync_full_meth = meth;
    sync_full_mux_meth = NULL;
  }

  inline void UartSlave::set_sync_full_meth_muxed(UartSyncFullMethMuxed *meth, int id)
  {
    sync_full_mux_meth = meth;
    sync_full_meth = NULL;
    mux_id = id;
  }



};

#endif
