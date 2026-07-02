# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

"""Control script for the router_proxy_mem testbench.

Exercises proxy mem_read / mem_write through the backdoor debug-memory
path while the simulation is PAUSED — proxy.run() is never called, so
any access still relying on the timed path (clock events) would hang.
"""

import sys

import gvsoc.gvsoc_control

MEM0_BASE = 0x1000_0000
# Entry address of mem1, reached through the router2 cascade
MEM1_BASE = 0x2000_1000


def target_control(proxy):
    router = gvsoc.gvsoc_control.Router(proxy, path='**/router')

    # Level-1: write/read roundtrip into mem0
    data = bytes((i * 7 + 3) & 0xFF for i in range(64))
    router.mem_write(MEM0_BASE + 0x100, len(data), data)
    readback = router.mem_read(MEM0_BASE + 0x100, len(data))
    if readback != data:
        print(f"[control] FAIL: mem0 readback mismatch", file=sys.stderr)
        return 1

    # Level-2: roundtrip through the router cascade with address rebasing
    router.mem_write_int(MEM1_BASE + 0x10, 4, 0xDEADBEEF)
    value = router.mem_read_int(MEM1_BASE + 0x10, 4)
    if value != 0xDEADBEEF:
        print(f"[control] FAIL: mem1 readback got 0x{value:08x}", file=sys.stderr)
        return 2

    # Hole inside router2 (0x2000_0000..0x2000_0FFF has no level-2 mapping)
    try:
        router.mem_read(0x2000_0100, 4)
        print("[control] FAIL: read in router2 hole did not error", file=sys.stderr)
        return 3
    except Exception:
        pass

    # Unmapped at level 1
    try:
        router.mem_read(0x3000_0000, 4)
        print("[control] FAIL: unmapped read did not error", file=sys.stderr)
        return 4
    except Exception:
        pass

    print("[control] OK")
    return 0
