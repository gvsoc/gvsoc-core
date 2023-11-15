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

#ifndef __VP_ITF_I2C_HPP__
#define __VP_ITF_I2C_HPP__

#include "vp/vp.hpp"

namespace vp {



  class I2cSlave;



  typedef void (I2cSyncMeth)(void *, int scl, int sda);
  typedef void (I2cSyncMethMuxed)(void *, int scl, int sda, int id);
  typedef void (I2cSyncMethDemuxed)(void *, int scl, int sda, int id);

  typedef void (I2cSlaveSyncMeth)(void *, int scl, int sda);
  typedef void (I2cSlaveSyncMethMuxed)(void *, int scl, int sda, int id);



  class I2cMaster : public vp::MasterPort
  {
    friend class I2cSlave;

  public:

    inline I2cMaster();

    inline void sync(int scl, int sda)
    {
      return sync_meth(this->get_remote_context(), scl, sda);
    }

    void bind_to(vp::Port *port, js::Config *config);

    inline void set_sync_meth(I2cSlaveSyncMeth *meth);

    inline void set_sync_meth_muxed(I2cSlaveSyncMethMuxed *meth, int id);

    bool is_bound() { return SlavePort != NULL; }

  private:

    static inline void sync_muxed_stub(I2cMaster *_this, int scl, int sda);

    void (*slave_sync)(void *comp, int scl, int sda);
    void (*slave_sync_mux)(void *comp, int scl, int sda, int id);

    void (*sync_meth)(void *, int scl, int sda);
    void (*sync_meth_mux)(void *, int scl, int sda, int mux);

    static inline void sync_default(void *, int scl, int sda);


    vp::Component *comp_mux;
    int sync_mux;
    I2cSlave *SlavePort = NULL;

    int mux_id;
  };



  class I2cSlave : public vp::SlavePort
  {

    friend class I2cMaster;

  public:

    inline I2cSlave();

    inline void sync(int scl, int sda)
    {
      if (next)
      {
        next->sync(scl, sda);
      }
      slave_sync_meth(this->get_remote_context(), scl, sda);
    }

    inline void set_sync_meth(I2cSyncMeth *meth);
    inline void set_sync_meth_muxed(I2cSyncMethMuxed *meth, int id);
    inline void set_sync_meth_demuxed(I2cSyncMethDemuxed *meth);

    inline void bind_to(vp::Port *_port, js::Config *config);

  private:

    static inline void sync_muxed_stub(I2cSlave *_this, int scl, int sda);


    void (*slave_sync_meth)(void *, int scl, int sda);
    void (*slave_sync_meth_mux)(void *, int scl, int sda, int mux);

    void (*sync_meth)(void *comp, int scl, int sda);
    void (*sync_mux_meth)(void *comp, int scl, int sda, int mux);

    static inline void sync_default(I2cSlave *, int scl, int sda);

    vp::Component *comp_mux;
    int sync_mux;
    int mux_id;
    int demux_id;
    I2cSlave *next = NULL;
  };


  inline I2cMaster::I2cMaster() {
    slave_sync = &I2cMaster::sync_default;
    slave_sync_mux = NULL;
  }



  inline void I2cMaster::sync_muxed_stub(I2cMaster *_this, int scl, int sda)
  {
    return _this->sync_meth_mux(_this->comp_mux, scl, sda, _this->sync_mux);
  }





  inline void I2cMaster::bind_to(vp::Port *_port, js::Config *config)
  {
    I2cSlave *port = (I2cSlave *)_port;
    if (port->sync_mux_meth == NULL)
    {
      sync_meth = port->sync_meth;
      this->set_remote_context(port->get_context());
    }
    else
    {
      sync_meth_mux = port->sync_mux_meth;
      this->set_remote_context(this);
      comp_mux = (vp::Component *)port->get_context();
      sync_meth = (I2cSyncMeth *)&I2cMaster::sync_muxed_stub;
      if (port->demux_id >= 0)
      {
        sync_mux = port->demux_id;
        port->demux_id++;
      }
      else
      {
        this->set_remote_context(this);
        comp_mux = (vp::Component *)port->get_context();
        sync_mux = port->mux_id;
      }
    }
  }

  inline void I2cMaster::set_sync_meth(I2cSlaveSyncMeth *meth)
  {
    slave_sync = meth;
  }

  inline void I2cMaster::set_sync_meth_muxed(I2cSlaveSyncMethMuxed *meth, int id)
  {
    slave_sync_mux = meth;
    slave_sync = NULL;
    mux_id = id;
  }

  inline void I2cMaster::sync_default(void *, int scl, int sda)
  {
  }

  inline void I2cSlave::sync_muxed_stub(I2cSlave *_this, int scl, int sda)
  {
    return _this->slave_sync_meth_mux(_this->comp_mux, scl, sda, _this->sync_mux);
  }

  inline void I2cSlave::bind_to(vp::Port *_port, js::Config *config)
  {
    I2cMaster *port = (I2cMaster *)_port;

    if (this->remote_port)
    {
      I2cSlave *slave = new I2cSlave;
      port->SlavePort = slave;
      slave->bind_to(_port, config);
      slave->next = this->next;
      this->next = slave;
    }
    else
    {
      SlavePort::bind_to(_port, config);
      port->SlavePort = this;
      if (port->slave_sync_mux == NULL)
      {
        this->slave_sync_meth = port->slave_sync;
        this->set_remote_context(port->get_context());
      }
      else
      {
        this->slave_sync_meth_mux = port->slave_sync_mux;
        this->slave_sync_meth = (I2cSyncMeth *)&I2cSlave::sync_muxed_stub;

        set_remote_context(this);
        comp_mux = (vp::Component *)port->get_context();
        sync_mux = port->mux_id;
      }
    }
  }

  inline I2cSlave::I2cSlave() : sync_meth(NULL), sync_mux_meth(NULL) {
    sync_meth = (I2cSyncMeth *)&I2cSlave::sync_default;
    demux_id = -1;
  }

  inline void I2cSlave::set_sync_meth(I2cSyncMeth *meth)
  {
    sync_meth = meth;
    sync_mux_meth = NULL;
  }

  inline void I2cSlave::set_sync_meth_muxed(I2cSyncMethMuxed *meth, int id)
  {
    sync_mux_meth = meth;
    sync_meth = NULL;
    mux_id = id;
  }

  inline void I2cSlave::set_sync_meth_demuxed(I2cSyncMethMuxed *meth)
  {
    sync_mux_meth = meth;
    sync_meth = NULL;
    demux_id = 0;
  }

  inline void I2cSlave::sync_default(I2cSlave *, int scl, int sda)
  {
  }




};

#endif
