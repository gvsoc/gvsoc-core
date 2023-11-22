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

#ifndef __VP_ITF_IMPLEMEN_CLOCK_HPP__
#define __VP_ITF_IMPLEMEN_CLOCK_HPP__

namespace vp {

  inline ClockMaster::ClockMaster()
  {
    this->sync_meth = &ClockMaster::sync_default;
    this->set_frequency_meth = &ClockMaster::set_frequency_default;
  }

  inline void ClockMaster::bind_to(vp::Port *_port, js::Config *config)
  {
    this->remote_port = _port;

    if (SlavePort != NULL)
    {
      // Case where a slave port is already connected.
      // Just connect the new one to the list so that all are called.
      ClockMaster *master = new ClockMaster;
      master->set_owner(this->get_owner());
      master->bind_to(_port, config);
      master->next = this->next;
      this->next = master;
    }
    else
    {
      ClockSlave *port = (ClockSlave *)_port;
      if (port->sync_mux == NULL)
      {
        sync_meth = port->sync;
        set_frequency_meth = port->set_frequency;
        set_remote_context(port->get_context());
      }
      else
      {
        sync_meth_mux = port->sync_mux;
        sync_meth = (void (*)(vp::Block *, bool))&ClockMaster::sync_muxed;
        set_frequency_meth_mux = port->set_frequency_mux;
        set_frequency_meth = (void (*)(vp::Block *, int64_t))&ClockMaster::set_frequency_muxed;
        set_remote_context(this);
        comp_mux = (vp::Component *)port->get_context();
        sync_mux = port->sync_mux_id;
      }

    }
  }

  inline void ClockMaster::sync_default(vp::Block *, bool value)
  {
  }

  inline void ClockMaster::set_frequency_default(vp::Block *, int64_t value)
  {
  }

  inline void ClockMaster::sync_muxed(ClockMaster *_this, bool value)
  {
    return _this->sync_meth_mux(_this->comp_mux, value, _this->sync_mux);
  }

  inline void ClockMaster::sync_freq_cross_stub(ClockMaster *_this, bool value)
  {
    // The normal callback was tweaked in order to get there when the master is sending a
    // request. 
    // First synchronize the target engine in case it was left behind,
    // and then generate the normal call with the mux ID using the saved 
    _this->remote_port->get_owner()->clock.get_engine()->sync();
    return _this->sync_meth_freq_cross((Component *)_this->slave_context_for_freq_cross, value);
  }

  inline void ClockMaster::set_frequency_freq_cross_stub(ClockMaster *_this, int64_t value)
  {
    // The normal callback was tweaked in order to get there when the master is sending a
    // request. 
    // First synchronize the target engine in case it was left behind,
    // and then generate the normal call with the mux ID using the saved handler
    if (_this->remote_port->get_owner()->clock.get_engine())
      _this->remote_port->get_owner()->clock.get_engine()->sync();
    return _this->set_frequency_meth_freq_cross((Component *)_this->slave_context_for_freq_cross, value);
  }

  inline void ClockMaster::set_frequency_muxed(ClockMaster *_this, int64_t frequency)
  {
    return _this->set_frequency_meth_mux(_this->comp_mux, frequency, _this->sync_mux);
  }

  inline void ClockMaster::finalize()
  {
    ClockMaster *port = this;

    while(port)
    {
      // We have to instantiate a stub in case the binding is crossing different
      // frequency domains in order to resynchronize the target engine.
      if (port->get_owner()->clock.get_engine() != port->remote_port->get_owner()->clock.get_engine())
      {
        // Just save the normal handler and tweak it to enter the stub when the
        // master is pushing the request.
        port->sync_meth_freq_cross = port->sync_meth;
        port->sync_meth = (void (*)(vp::Block *, bool))&ClockMaster::sync_freq_cross_stub;

        port->set_frequency_meth_freq_cross = port->set_frequency_meth;
        port->set_frequency_meth = (void (*)(vp::Block *, int64_t))&ClockMaster::set_frequency_freq_cross_stub;

        port->slave_context_for_freq_cross = (vp::Block *)port->get_remote_context();
        port->set_remote_context(port);
      }

      port = port->next;
    }
  }



  inline void ClockSlave::bind_to(vp::Port *_port, js::Config *config)
  {
    SlavePort::bind_to(_port, config);
    ClockMaster *port = (ClockMaster *)_port;
    ClockSlave *SlavePort = new ClockSlave();
    port->SlavePort = SlavePort;
    SlavePort->set_owner(this->get_owner());
    SlavePort->set_context(port->get_context());
    SlavePort->set_remote_context(port->get_context());
  }

  inline void ClockSlave::set_sync_meth(void (*meth)(vp::Block *, bool))
  {
    sync = meth;
    sync_mux = NULL;
  }

  inline void ClockSlave::set_sync_meth_muxed(void (*meth)(vp::Block *, bool, int), int id)
  {
    sync = NULL;
    sync_mux = meth;
    sync_mux_id = id;
  }

  inline void ClockSlave::set_set_frequency_meth(void (*meth)(vp::Block *, int64_t))
  {
    set_frequency = meth;
    set_frequency_mux = NULL;
  }

  inline void ClockSlave::set_set_frequency_meth_muxed(void (*meth)(vp::Block *, int64_t, int), int id)
  {
    set_frequency = NULL;
    set_frequency_mux = meth;
    sync_mux_id = id;
  }

  inline ClockSlave::ClockSlave() : sync(NULL), sync_mux(NULL), set_frequency(NULL), set_frequency_mux(NULL)
  {
    this->sync = &ClockMaster::sync_default;
    this->set_frequency = &ClockMaster::set_frequency_default;
  }

  inline void vp::ClkMaster::bind_to(vp::Port *_port, js::Config *config)
  {
    ClkSlave *port = (ClkSlave *)_port;

    port->next = ports;
    ports = port;
  }



  inline void vp::ClkMaster::reg(Component *clock)
  {
    ClkSlave *port = ports;
    while (port)
    {
      port->req((Component *)port->get_context(), clock);
      port = port->next;
    }
  }

  inline void vp::ClkMaster::set_frequency(int64_t frequency)
  {
    ClkSlave *port = ports;
    while (port)
    {
      port->set_frequency((Component *)port->get_context(), frequency);
      port = port->next;
    }
  }

  inline vp::ClkSlave::ClkSlave() : req(NULL) {
    req = (ClkRegMeth *)&ClkSlave::reg_default;
    set_frequency = (ClkSetFrequencyMeth *)&ClkSlave::set_frequency_default;
  }

  inline void vp::ClkSlave::set_reg_meth(ClkRegMeth *meth)
  {
    req = meth;
  }

  inline void vp::ClkSlave::set_set_frequency_meth(ClkSetFrequencyMeth *meth)
  {
    set_frequency = meth;
  }

  inline void vp::ClkSlave::reg_default(ClkSlave *)
  {
  }

  inline void vp::ClkSlave::set_frequency_default(int64_t)
  {
  }

};

#endif