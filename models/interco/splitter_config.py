# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

from config_tree import Config, cfg_field

class SplitterConfig(Config):

    input_width: int = cfg_field(default=0, fmt="hex", dump=True, desc=(
        "Width in bytes of the input"
    ))
    output_width: int = cfg_field(default=0, fmt="hex", dump=True, desc=(
        "Width in bytes of each output"
    ))
