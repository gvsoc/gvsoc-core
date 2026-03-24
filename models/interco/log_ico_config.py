# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

from config_tree import Config, cfg_field

class LogIcoConfig(Config):

    nb_masters: int = cfg_field(default=0, dump=True, desc=(
        "Number of masters"
    ))
    nb_slaves: int = cfg_field(default=0, dump=True, desc=(
        "Number of slaves"
    ))
    interleaving_width: int = cfg_field(default=0, dump=True, desc=(
        "Number of bits for the interleaving"
    ))
    remove_offset: int = cfg_field(default=0, fmt="hex", dump=True, desc=(
        "Offset to be removed to the incoming requests"
    ))
