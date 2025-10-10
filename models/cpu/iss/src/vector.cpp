/*
 * Copyright (C) 2020 ETH Zurich and University of Bologna
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
 * Authors: Germain Haugou (germain.haugou@gmail.com)
 */

#include "cpu/iss/include/iss.hpp"


Vector::Vector(Iss &iss)
{
printf("building Vector in vector.cpp \n");

}

void Vector::build()
{

}

void Vector::reset(bool active)
{
    if (active)
    {
    	for (int k = 0; k < Vector::N_BANK; k++){
		    for (int i = 0; i < ISS_NB_VREGS; i++){
		        for (int j = 0; j < NB_VEL; j++){
		            this->vregs[k][i][j] = 0;
		        }
		    }
		    this->Bank_status[k]= true;
		}
    }
}
