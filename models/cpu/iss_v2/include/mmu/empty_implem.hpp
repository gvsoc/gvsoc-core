// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

#pragma once

// Without TLB, the memory accessed directly is found from the address. The LSU
// only asks for aligned accesses, which are fully in the memory if their first
// byte is.

inline bool MmuEmpty::load_virt_to_phys(iss_addr_t virt_addr, iss_addr_t &phys_addr,
    bool &use_mem_array)
{
    phys_addr = virt_addr;
#ifdef CONFIG_GVSOC_ISS_MEMORY
    use_mem_array = this->iss.lsu.mem_array_contains(phys_addr, 1);
#endif
    return false;
}

inline bool MmuEmpty::store_virt_to_phys(iss_addr_t virt_addr, iss_addr_t &phys_addr,
    bool &use_mem_array)
{
    phys_addr = virt_addr;
#ifdef CONFIG_GVSOC_ISS_MEMORY
    use_mem_array = this->iss.lsu.mem_array_contains(phys_addr, 1);
#endif
    return false;
}
