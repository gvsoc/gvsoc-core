# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

from dataclasses import dataclass
from gvrun.config import Config, cfg_field

@dataclass(repr=False)
class DemuxConfig(Config):

    offset: int = cfg_field(default=0, fmt="hex", dump=True, desc=(
        "First bit used to extract the target index"
    ))
    width: int = cfg_field(default=0, fmt="hex", dump=True, desc=(
        "Number of bit used to extract the target index"
    ))
