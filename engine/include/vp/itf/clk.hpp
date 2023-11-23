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

#ifndef __VP_ITF_CLK_HPP__
#define __VP_ITF_CLK_HPP__

#include "vp/ports.hpp"

namespace vp {

  class Component;

  typedef void (ClkRegMeth)(Component *_this, Component *clock);
  typedef void (ClkSetFrequencyMeth)(Component *_this, int64_t frequency);

  class ClkSlave;

  class ClkMaster : public MasterPort
  {
  public:

    ClkMaster() : ports(NULL) {}

    void reg(Component *clock);
    void set_frequency(int64_t frequency);

    void bind_to(vp::Port *port, js::Config *config);

  private:
    void (*reg_meth)(Component *, Component *clock);

    ClkSlave *ports;
  };



  class ClkSlave : public SlavePort
  {

    friend class ClkMaster;

  public:

    ClkSlave();

    void set_reg_meth(ClkRegMeth *meth);
    void set_set_frequency_meth(ClkSetFrequencyMeth *meth);


  private:

    void (*req)(Component *comp, Component *clock);
    void (*set_frequency)(Component *comp, int64_t frequency);
    static void reg_default(ClkSlave *);
    static void set_frequency_default(int64_t frequency);

    ClkSlave *next;

  };


};

#endif
