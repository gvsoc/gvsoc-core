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

#include "vp/vp_data.hpp"
#include "vp/ports.hpp"

namespace vp {

  class component;

  typedef void (clk_reg_meth_t)(component *_this, component *clock);
  typedef void (clk_set_frequency_meth_t)(component *_this, int64_t frequency);

  class clk_slave;

  class clk_master : public master_port
  {
  public:

    clk_master() : ports(NULL) {}

    void reg(component *clock);
    void set_frequency(int64_t frequency);

    void bind_to(vp::port *port, vp::config *config);

  private:
    void (*reg_meth)(component *, component *clock);

    clk_slave *ports;
  };



  class clk_slave : public slave_port
  {

    friend class clk_master;

  public:

    clk_slave();

    void set_reg_meth(clk_reg_meth_t *meth);
    void set_set_frequency_meth(clk_set_frequency_meth_t *meth);


  private:

    void (*req)(component *comp, component *clock);
    void (*set_frequency)(component *comp, int64_t frequency);
    static void reg_default(clk_slave *);
    static void set_frequency_default(int64_t frequency);

    clk_slave *next;

  };


};

#endif
