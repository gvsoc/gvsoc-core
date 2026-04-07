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

#ifdef CONFIG_REGFILE_FI
#include <vp/fault_injector.hpp>
#endif
#include "cpu/iss/include/iss.hpp"

Vector::Vector(Iss &iss)
{
}

void Vector::build()
{
}

void Vector::reset(bool active)
{
    if (active)
    {
        for (int i = 0; i < ISS_NB_VREGS; i++){
            for (int j = 0; j < CONFIG_ISS_VLEN/8; j++){
                this->vregs[i][j] = 0;
            }
        }
#ifdef CONFIG_REGFILE_FI
		if (!this->registered_with_fic)
		{
			bool fic_enabled = this->iss.top.get_js_config()->get_child_bool("regfile_fi");
			if (fic_enabled)
			{
				vp::FIC_registrator *fic = (vp::FIC_registrator *) this->iss.top.get_service("FIC");
				fic->register_regfile(&this->iss.top, VP_FI_VREG, (void *) this->vregs, 
					ISS_NB_VREGS, CONFIG_ISS_VLEN / 8);
			}
			this->registered_with_fic = true;
		}
#endif
    }
}
