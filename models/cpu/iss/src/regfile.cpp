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


#include "regfile.hpp"

Regfile::Regfile(Iss &iss)
: iss(iss)
{

}


void Regfile::reset(bool active)
{
    if (active)
    {
        this->regs[0] = 0;
        for (int i = 0; i < ISS_NB_TOTAL_REGS; i++)
        {
            this->regs[i] = i == 0 ? 0 : 0x57575757;
        }
        // for (int i = 0; i < ISS_NB_TOTAL_REGS; i++)
        // {
        //     if(i == 28){this->regs[i] = 0x20000;}
        //     else if(i == 11){this->regs[i] = 0x1040;}
        //     else if(i == 10){this->regs[i] = 0x1;}
        //     else if(i == 7 ){this->regs[i] = 0x70002e20;}
        //     else if(i == 6 ){this->regs[i] = 0x1038;}
        //     else {this->regs[i] = 0x0;}
        // }


    }
}
