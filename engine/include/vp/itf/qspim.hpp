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

#ifndef __VP_ITF_QSPIM_HPP__
#define __VP_ITF_QSPIM_HPP__

#include "vp/vp.hpp"

namespace vp {



  class QspimSlave;



  typedef void (QspimSyncMeth)(vp::Block *, int sck, int data_0, int data_1, int data_2, int data_3, int mask);
  typedef void (QspimCsSyncMeth)(vp::Block *, int cs, int active);

  typedef void (QspimSyncMethMuxed)(vp::Block *, int sck, int data_0, int data_1, int data_2, int data_3, int mask, int id);
  typedef void (QspimCsSyncMethMuxed)(vp::Block *, int cs, int active, int id);

  typedef void (QspimSlaveSyncMeth)(vp::Block *, int sck, int data_0, int data_1, int data_2, int data_3, int mask);
  typedef void (QspimSlaveSyncMethMuxed)(vp::Block *, int sck, int data_0, int data_1, int data_2, int data_3, int mask, int id);

  typedef void (QspimSlaveCsSyncMeth)(vp::Block *, int cs, int active);
  typedef void (QspimSlaveCsSyncMethMuxed)(vp::Block *, int cs, int active, int id);



  class QspimMaster : public vp::MasterPort
  {
    friend class QspimSlave;

  public:

    inline QspimMaster();

    inline void sync(int sck, int data_0, int data_1, int data_2, int data_3, int mask)
    {
      return sync_meth((vp::Block *)this->get_remote_context(), sck, data_0, data_1, data_2, data_3, mask);
    }

    inline void cs_sync(int cs, int active)
    {
      return cs_sync_meth((vp::Block *)this->get_remote_context(), cs, active);
    }

    void bind_to(vp::Port *port, js::Config *config);

    inline void set_sync_meth(QspimSlaveSyncMeth *meth);

    inline void set_sync_meth_muxed(QspimSlaveSyncMethMuxed *meth, int id);

    inline void set_cs_sync_meth(QspimSlaveCsSyncMeth *meth);

    inline void set_cs_sync_meth_muxed(QspimSlaveCsSyncMethMuxed *meth, int id);

    bool is_bound() { return SlavePort != NULL; }

  private:

    static inline void sync_muxed_stub(QspimMaster *_this, int sck, int data_0, int data_1, int data_2, int data_3, int mask);
    static inline void cs_sync_muxed_stub(QspimMaster *_this, int cs, int active);

    void (*slave_sync)(vp::Block *comp, int sck, int data_0, int data_1, int data_2, int data_3, int mask);
    void (*slave_sync_mux)(vp::Block *comp, int sck, int data_0, int data_1, int data_2, int data_3, int mask, int id);

    void (*slave_cs_sync)(vp::Block *comp, int cs, int active);
    void (*slave_cs_sync_mux)(vp::Block *comp, int cs, int active, int id);

    void (*sync_meth)(vp::Block *, int sck, int data_0, int data_1, int data_2, int data_3, int mask);
    void (*sync_meth_mux)(vp::Block *, int sck, int data_0, int data_1, int data_2, int data_3, int mask, int mux);
    void (*cs_sync_meth)(vp::Block *, int cs, int active);
    void (*cs_sync_meth_mux)(vp::Block *, int cs, int active, int mux);

    static inline void sync_default(vp::Block *, int sck, int data_0, int data_1, int data_2, int data_3, int mask);
    static inline void cs_sync_default(vp::Block *, int cs, int active);


    vp::Component *comp_mux;
    int sync_mux;
    QspimSlave *SlavePort = NULL;

    int mux_id;
  };



  class QspimSlave : public vp::SlavePort
  {

    friend class QspimMaster;

  public:

    inline QspimSlave();

    inline void sync(int sck, int data_0, int data_1, int data_2, int data_3, int mask)
    {
      slave_sync_meth((vp::Block *)this->get_remote_context(), sck, data_0, data_1, data_2, data_3, mask);
    }

    inline void cs_sync(int cs, int active)
    {
      slave_cs_sync_meth((vp::Block *)this->get_remote_context(), cs, active);
    }

    inline void set_sync_meth(QspimSyncMeth *meth);
    inline void set_sync_meth_muxed(QspimSyncMethMuxed *meth, int id);

    inline void set_cs_sync_meth(QspimCsSyncMeth *meth);
    inline void set_cs_sync_meth_muxed(QspimCsSyncMethMuxed *meth, int id);

    inline void bind_to(vp::Port *_port, js::Config *config);

  private:

    static inline void sync_muxed_stub(QspimSlave *_this, int sck, int data_0, int data_1, int data_2, int data_3, int mask);
    static inline void cs_sync_muxed_stub(QspimSlave *_this, int cs, int active);


    void (*slave_sync_meth)(vp::Block *, int sck, int data_0, int data_1, int data_2, int data_3, int mask);
    void (*slave_sync_meth_mux)(vp::Block *, int sck, int data_0, int data_1, int data_2, int data_3, int mask, int mux);
    void (*slave_cs_sync_meth)(vp::Block *, int cs, int active);
    void (*slave_cs_sync_meth_mux)(vp::Block *, int cs, int active, int mux);

    void (*sync_meth)(vp::Block *comp, int sck, int data_0, int data_1, int data_2, int data_3, int mask);
    void (*sync_mux_meth)(vp::Block *comp, int sck, int data_0, int data_1, int data_2, int data_3, int mask, int mux);
    void (*cs_sync_meth)(vp::Block *comp, int cs, int active);
    void (*cs_sync_mux_meth)(vp::Block *comp, int cs, int active, int mux);

    static inline void sync_default(QspimSlave *, int sck, int data_0, int data_1, int data_2, int data_3, int mask);
    static inline void cs_sync_default(QspimSlave *, int cs, int active);

    vp::Component *comp_mux;
    int sync_mux;
    int mux_id;


  };


  inline QspimMaster::QspimMaster() {
    slave_sync = &QspimMaster::sync_default;
    slave_cs_sync = &QspimMaster::cs_sync_default;
    slave_sync_mux = NULL;
  }



  inline void QspimMaster::sync_muxed_stub(QspimMaster *_this, int sck, int data_0, int data_1, int data_2, int data_3, int mask)
  {
    return _this->sync_meth_mux(_this->comp_mux, sck, data_0, data_1, data_2, data_3, mask, _this->sync_mux);
  }




  inline void QspimMaster::cs_sync_muxed_stub(QspimMaster *_this, int cs, int active)
  {
    return _this->cs_sync_meth_mux(_this->comp_mux, cs, active, _this->sync_mux);
  }



  inline void QspimMaster::bind_to(vp::Port *_port, js::Config *config)
  {
    QspimSlave *port = (QspimSlave *)_port;
    if (port->sync_mux_meth == NULL)
    {
      sync_meth = port->sync_meth;
      cs_sync_meth = port->cs_sync_meth;
      this->set_remote_context(port->get_context());
    }
    else
    {
      sync_meth_mux = port->sync_mux_meth;
      sync_meth = (QspimSyncMeth *)&QspimMaster::sync_muxed_stub;

      cs_sync_meth_mux = port->cs_sync_mux_meth;
      cs_sync_meth = (QspimCsSyncMeth *)&QspimMaster::cs_sync_muxed_stub;

      this->set_remote_context(this);
      comp_mux = (vp::Component *)port->get_context();
      sync_mux = port->mux_id;
    }
  }

  inline void QspimMaster::set_sync_meth(QspimSlaveSyncMeth *meth)
  {
    slave_sync = meth;
  }

  inline void QspimMaster::set_sync_meth_muxed(QspimSlaveSyncMethMuxed *meth, int id)
  {
    slave_sync_mux = meth;
    slave_sync = NULL;
    mux_id = id;
  }

  inline void QspimMaster::set_cs_sync_meth(QspimSlaveCsSyncMeth *meth)
  {
    slave_cs_sync = meth;
  }

  inline void QspimMaster::set_cs_sync_meth_muxed(QspimSlaveCsSyncMethMuxed *meth, int id)
  {
    slave_cs_sync_mux = meth;
    slave_cs_sync = NULL;
    mux_id = id;
  }

  inline void QspimMaster::sync_default(vp::Block *, int sck, int data_0, int data_1, int data_2, int data_3, int mask)
  {
  }

  inline void QspimMaster::cs_sync_default(vp::Block *, int cs, int active)
  {
  }

  inline void QspimSlave::sync_muxed_stub(QspimSlave *_this, int sck, int data_0, int data_1, int data_2, int data_3, int mask)
  {
    return _this->slave_sync_meth_mux(_this->comp_mux, sck, data_0, data_1, data_2, data_3, mask, _this->sync_mux);
  }

  inline void QspimSlave::cs_sync_muxed_stub(QspimSlave *_this, int cs, int active)
  {
    return _this->slave_cs_sync_meth_mux(_this->comp_mux, cs, active, _this->sync_mux);
  }

  inline void QspimSlave::bind_to(vp::Port *_port, js::Config *config)
  {
    SlavePort::bind_to(_port, config);
    QspimMaster *port = (QspimMaster *)_port;
    port->SlavePort = this;
    if (port->slave_sync_mux == NULL)
    {
      this->slave_sync_meth = port->slave_sync;
      this->slave_cs_sync_meth = port->slave_cs_sync;
      this->set_remote_context(port->get_context());
    }
    else
    {
      this->slave_sync_meth_mux = port->slave_sync_mux;
      this->slave_sync_meth = (QspimSlaveSyncMeth *)&QspimSlave::sync_muxed_stub;
      this->slave_cs_sync_meth_mux = port->slave_cs_sync_mux;
      this->slave_cs_sync_meth = (QspimSlaveCsSyncMeth *)&QspimSlave::cs_sync_muxed_stub;

      set_remote_context(this);
      comp_mux = (vp::Component *)port->get_context();
      sync_mux = port->mux_id;
    }
  }

  inline QspimSlave::QspimSlave() : sync_meth(NULL), sync_mux_meth(NULL) {
    sync_meth = (QspimSyncMeth *)&QspimSlave::sync_default;
    cs_sync_meth = (QspimCsSyncMeth *)&QspimSlave::cs_sync_default;
  }

  inline void QspimSlave::set_sync_meth(QspimSyncMeth *meth)
  {
    sync_meth = meth;
    sync_mux_meth = NULL;
  }

  inline void QspimSlave::set_cs_sync_meth(QspimCsSyncMeth *meth)
  {
    cs_sync_meth = meth;
    cs_sync_mux_meth = NULL;
  }

  inline void QspimSlave::set_sync_meth_muxed(QspimSyncMethMuxed *meth, int id)
  {
    sync_mux_meth = meth;
    sync_meth = NULL;
    mux_id = id;
  }

  inline void QspimSlave::set_cs_sync_meth_muxed(QspimCsSyncMethMuxed *meth, int id)
  {
    cs_sync_mux_meth = meth;
    cs_sync_meth = NULL;
    mux_id = id;
  }

  inline void QspimSlave::sync_default(QspimSlave *, int sck, int data_0, int data_1, int data_2, int data_3, int mask)
  {
  }


  inline void QspimSlave::cs_sync_default(QspimSlave *, int cs, int active)
  {
  }



};

#endif
