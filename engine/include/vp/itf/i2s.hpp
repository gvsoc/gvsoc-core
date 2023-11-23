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

#ifndef __VP_ITF_I2S_HPP__
#define __VP_ITF_I2S_HPP__

#include "vp/vp.hpp"

namespace vp {



  class I2sSlave;



  typedef void (I2sSyncMeth)(vp::Block *, int sck, int ws, int sd, bool full_duplex);
  typedef void (I2sSyncMethMuxed)(vp::Block *, int sck, int ws, int sd, bool full_duplex, int id);



  class I2sMaster : public vp::MasterPort
  {
    friend class I2sSlave;

  public:

    inline I2sMaster();

    inline void sync(int sck, int ws, int sd, bool full_duplex);

    void bind_to(vp::Port *port, js::Config *config);

    inline void set_sync_meth(I2sSyncMeth *meth);
    inline void set_sync_meth_muxed(I2sSyncMethMuxed *meth, int id);

    bool is_bound() { return SlavePort != NULL; }

  private:

    static inline void sync_muxed_stub(I2sMaster *_this, int sck, int ws, int sd, bool full_duplex);

    void (*slave_sync)(vp::Block *comp, int sck, int ws, int sd, bool full_duplex);
    void (*slave_sync_mux)(vp::Block *comp, int sck, int ws, int sd, bool full_duplex, int mux);

    void (*sync_meth)(vp::Block *, int sck, int ws, int sd, bool full_duplex);
    void (*sync_meth_mux)(vp::Block *, int sck, int ws, int sd, bool full_duplex, int mux);

    static inline void sync_default(vp::Block *, int sck, int ws, int sd, bool full_duplex);

    vp::Component *comp_mux;
    int sync_mux;
    I2sSlave *SlavePort = NULL;
    int mux_id;

    int sd;

  };



  class I2sSlave : public vp::SlavePort
  {

    friend class I2sMaster;

  public:

    inline I2sSlave();

    inline void sync(int sck, int ws, int sd, bool full_duplex)
    {
      if (next) next->sync(sck, ws, sd, full_duplex);
      slave_sync_meth((vp::Block *)this->get_remote_context(), sck, ws, sd, full_duplex);
    }

    inline void set_sync_meth(I2sSyncMeth *meth);
    inline void set_sync_meth_muxed(I2sSyncMethMuxed *meth, int id);

    inline void bind_to(vp::Port *_port, js::Config *config);

    bool is_bound() { return remote_port != NULL; }

    inline int get_sd();

  private:

    static inline void sync_muxed_stub(I2sSlave *_this, int sck, int ws, int sd, bool full_duplex);

    void (*slave_sync_meth)(vp::Block *, int sck, int ws, int sd, bool full_duplex);
    void (*slave_sync_meth_mux)(vp::Block *, int sck, int ws, int sd, bool full_duplex, int mux);

    void (*sync_meth)(vp::Block *comp, int sck, int ws, int sd, bool full_duplex);
    void (*sync_mux_meth)(vp::Block *comp, int sck, int ws, int sd, bool full_duplex, int mux);

    static inline void sync_default(I2sSlave *, int sck, int ws, int sd, bool full_duplex);

    vp::Component *comp_mux;
    int sync_mux;
    int mux_id;
    I2sMaster *master_port = NULL;
    I2sSlave *next = NULL;

  };


  inline I2sMaster::I2sMaster() {
    slave_sync = &I2sMaster::sync_default;
    slave_sync_mux = NULL;
    this->sd = 2;
  }




  inline void I2sMaster::sync_muxed_stub(I2sMaster *_this, int sck, int ws, int sd, bool full_duplex)
  {
    return _this->sync_meth_mux(_this->comp_mux, sck, ws, sd, full_duplex, _this->sync_mux);
  }

  inline void I2sMaster::bind_to(vp::Port *_port, js::Config *config)
  {
    I2sSlave *port = (I2sSlave *)_port;
    if (port->sync_mux_meth == NULL)
    {
      sync_meth = port->sync_meth;
      set_remote_context(port->get_context());
    }
    else
    {
      sync_meth_mux = port->sync_mux_meth;
      sync_meth = (I2sSyncMeth *)&I2sMaster::sync_muxed_stub;

      set_remote_context(this);
      comp_mux = (vp::Component *)port->get_context();
      sync_mux = port->mux_id;
    }
  }

  inline void I2sMaster::set_sync_meth(I2sSyncMeth *meth)
  {
    slave_sync = meth;
  }

  inline void I2sMaster::set_sync_meth_muxed(I2sSyncMethMuxed *meth, int id)
  {
    slave_sync_mux = meth;
    slave_sync = NULL;
    mux_id = id;
  }

  inline void I2sMaster::sync_default(vp::Block *, int sck, int ws, int sd, bool full_duplex)
  {
  }

  inline void I2sMaster::sync(int sck, int ws, int sd, bool full_duplex)
  {
    this->sd = sd;
    return sync_meth((vp::Block *)this->get_remote_context(), sck, ws, this->SlavePort->get_sd(), full_duplex);
  }

  inline void I2sSlave::sync_muxed_stub(I2sSlave *_this, int sck, int ws, int sd, bool full_duplex)
  {
    return _this->slave_sync_meth_mux(_this->comp_mux, sck, ws, sd, full_duplex, _this->sync_mux);
  }

  inline int I2sSlave::get_sd()
  {
    int sdi = this->master_port->sd & 3;
    int sdo = (this->master_port->sd >> 2) & 3;
    I2sSlave *current = this->next;

    while(current)
    {
      int next_sd = current->master_port->sd;
      int next_data = next_sd & 3;
      if (next_data != 2)
      {
        if (sdi == 2 || next_data == sdi)
          sdi = next_data;
        else
          sdi = 3;
      }
      next_data = (next_sd >> 2) & 3;
      if (next_data != 2)
      {
        if (sdo == 2 || next_data == sdo)
          sdo = next_data;
        else
          sdo = 3;
      }
      current = current->next;
    }

    return sdi | (sdo << 2);
  }

  inline void I2sSlave::bind_to(vp::Port *_port, js::Config *config)
  {
    I2sMaster *port = (I2sMaster *)_port;

    if (this->master_port != NULL)
    {
      I2sSlave *slave = new I2sSlave;
      port->SlavePort = slave;
      slave->bind_to(_port, config);
      slave->next = this->next;
      this->next = slave;
    }
    else
    {
      SlavePort::bind_to(_port, config);
      this->master_port = port;
      port->SlavePort = this;
      if (port->slave_sync_mux == NULL)
      {
        this->slave_sync_meth = port->slave_sync;
        this->set_remote_context(port->get_context());
      }
      else
      {
        this->slave_sync_meth_mux = port->slave_sync_mux;
        this->slave_sync_meth = (I2sSyncMeth *)&I2sSlave::sync_muxed_stub;

        set_remote_context(this);
        comp_mux = (vp::Component *)port->get_context();
        sync_mux = port->mux_id;
      }
    }
  }

  inline I2sSlave::I2sSlave() : sync_meth(NULL), sync_mux_meth(NULL) {
    sync_meth = (I2sSyncMeth *)&I2sSlave::sync_default;
  }

  inline void I2sSlave::set_sync_meth(I2sSyncMeth *meth)
  {
    sync_meth = meth;
    sync_mux_meth = NULL;
  }

  inline void I2sSlave::set_sync_meth_muxed(I2sSyncMethMuxed *meth, int id)
  {
    sync_mux_meth = meth;
    sync_meth = NULL;
    mux_id = id;
  }

  inline void I2sSlave::sync_default(I2sSlave *, int sck, int ws, int sd, bool full_duplex)
  {
  }



};

#endif
