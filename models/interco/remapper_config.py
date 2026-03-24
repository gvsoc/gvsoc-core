# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

from config_tree import Config, cfg_field

class RemapperConfig(Config):

    base: int = cfg_field(default=0, fmt="hex", dump=True, desc=(
        "Base addresse of the area to remap"
    ))
    size: int = cfg_field(default=0, fmt="hex", dump=True, desc=(
        "Size of the area to remap"
    ))
    target_base: int = cfg_field(default=0, fmt="hex", dump=True, desc=(
        "Base address of the target area"
    ))
