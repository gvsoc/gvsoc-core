# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

"""Watchpoint address-alias declaration component.

A master may reach the same physical memory through two address forms: a global
address and a local *alias* (e.g. on gap9 the FC sees L2 both at 0x1C00_0000 and
at 0x0). Console watchpoints are matched on the master side with the address as
the program issued it, so without help a watchpoint set on one form would miss
accesses done through the other.

This component carries a list of alias windows in its typed config and, at
construction, registers them with the trace engine (``register_alias``). The
engine then folds both the accessed address and the watched address to a
canonical (global) form before matching, so a watchpoint matches either form.

The component has no ports — it is a pure declaration carrier. Instantiate it once
per group of aliases and pass the windows via :class:`WatchpointAliasConfig`.
"""

from __future__ import annotations

from typing import ClassVar
from config_tree import Config, cfg_field
from gvsoc.systree import Component


class WatchpointAliasRegion(Config):
    """One master address-alias window.

    An access by a master whose path contains ``master`` to
    ``[local_base, local_base + size)`` refers to the same memory as the global
    address ``global_base + (addr - local_base)``.
    """

    _defer_parent_init: ClassVar[bool] = True

    master: str = cfg_field(default="", dump=True, desc=(
        "Substring matched against the master path this alias applies to"
    ))
    local_base: int = cfg_field(default=0, dump=True, fmt="hex", desc=(
        "Base of the master-local alias window (inclusive)"
    ))
    global_base: int = cfg_field(default=0, dump=True, fmt="hex", desc=(
        "Canonical (global) base the window folds to"
    ))
    size: int = cfg_field(default=0, dump=True, fmt="hex", desc=(
        "Size of the alias window, in bytes"
    ))


class WatchpointAliasConfig(Config):
    """Configuration for :class:`WatchpointAlias`: the list of alias windows."""

    aliases: list[WatchpointAliasRegion] = cfg_field(default_factory=list, init=False, desc=(
        "Master address-alias windows to register with the trace engine"
    ))

    def add_aliases(self, *aliases: WatchpointAliasRegion):
        """Append alias windows (adopting each into this config)."""
        for alias in aliases:
            alias.adopt(self)
            self.aliases.append(alias)
        return self


class WatchpointAlias(Component):
    """Registers a set of watchpoint address aliases with the trace engine."""

    def __init__(self, parent: Component, name: str, config: WatchpointAliasConfig):
        super().__init__(parent, name, config=config)
        self.add_sources(['utils/watchpoint_alias.cpp'])
