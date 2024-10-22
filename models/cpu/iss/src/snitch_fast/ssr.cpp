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
 * Authors: Germain Haugou, ETH (germain.haugou@iis.ee.ethz.ch)
 *          Kexin Li, ETH Zurich (likexi@ethz.ch)
 */

 #include <cpu/iss/include/cores/snitch_fast/ssr.hpp>
 #include "cpu/iss/include/iss.hpp"
 #include ISS_CORE_INC(class.hpp)


 Ssr::Ssr(Iss &iss)
 : iss(iss)
 {
     iss.csr.declare_csr(&this->csr_ssr, "ssr", 0x7C0);
     this->csr_ssr.register_callback(std::bind(&Ssr::csr_ssr_access, this, std::placeholders::_1,
         std::placeholders::_2));
 }


 void Ssr::reset(bool active)
 {
     if (active)
     {
     }
 }


 void Ssr::build()
 {
 }

 bool Ssr::csr_ssr_access(bool is_write, iss_reg_t &value)
 {
    return false;
}
