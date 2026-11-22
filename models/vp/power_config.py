#
# Copyright (C) 2026 GreenWaves Technologies
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

"""
Shared power table configuration classes and YAML loader.

Models supporting power modeling declare one :class:`PowerSourceConfig` field
per power source, grouped in a plain-data nested config (a Config with
``_defer_parent_init = True``), plus a ``power_model`` string field carrying
the path of a YAML power model file::

    class MyModelPowerConfig(Config):
        _defer_parent_init: ClassVar[bool] = True
        background: PowerSourceConfig = cfg_field(default_factory=PowerSourceConfig)
        read_32: PowerSourceConfig = cfg_field(default_factory=PowerSourceConfig)

    class MyModelConfig(Config):
        power_model: str = cfg_field(default='')
        power: MyModelPowerConfig = cfg_field(default=None)

The ``power`` tables are only created when a power model is given (see
:func:`power_tables`): without one, the field stays None, which is compiled
as an all-empty struct, so that the models without power numbers do not pay
for them.

The component consumes the file in its ``configure()`` hook with
:func:`consume_power_model`, and the C++ model initializes its power sources
straight from the compiled nested struct with
``vp::new_power_source_from_config`` (see ``vp/power/power_table_convert.hpp``)::

    def configure(self):
        vp.power_config.consume_power_model(self.get_config())

An untouched field keeps its empty tables and the corresponding power source
is inert. Positional, variable-count tables (e.g. per-instruction-group
energies) keep the ``list[PowerSourceConfig]`` form and are loaded with
:func:`load_power_yaml_list`.

The YAML schema::

    <source-name>:
        dynamic: { type: linear, unit: pJ|W, values: { <temp>: { <volt>: { <freq>|any: <value> } } } }
        leakage: { type: linear, unit: W,    values: { <temp>: { <volt>: { <freq>|any: <value> } } } }

Temperatures are in celsius, voltages in V, frequencies in Hz. The special
frequency key ``any`` declares a frequency-independent value. Unit ``pJ``
declares a per-event energy quantum, ``W`` a background power.
"""

from __future__ import annotations

import os
import sys
from dataclasses import fields
from typing import ClassVar, get_type_hints

import yaml

from config_tree import Config, cfg_field


class PowerValue(Config):
    """One (temperature, voltage, frequency) -> value point of a linear power table."""

    _defer_parent_init: ClassVar[bool] = True

    temp: float = cfg_field(default=25.0, desc=(
        "Temperature, in celsius"
    ))
    volt: float = cfg_field(default=0.0, desc=(
        "Voltage, in V"
    ))
    freq: float = cfg_field(default=-1.0, desc=(
        "Frequency, in Hz. A negative value means 'any', i.e. the value does not "
        "depend on frequency."
    ))
    value: float = cfg_field(default=0.0, desc=(
        "Power characteristic value, in pJ for energy quanta or in W for "
        "background and leakage power"
    ))


class PowerSourceConfig(Config):
    """One named power source: a dynamic table (pJ quantum or W background) and a
    leakage table (W). The Config ``name`` field carries the source name
    (e.g. read_8, background, ext2loc)."""

    _defer_parent_init: ClassVar[bool] = True

    dynamic_unit: str = cfg_field(default='', desc=(
        "'pJ' for a per-event energy quantum, 'W' for background power, '' when "
        "the source has no dynamic part"
    ))
    dynamic: list[PowerValue] = cfg_field(default_factory=list, desc=(
        "Dynamic table points"
    ))
    leakage: list[PowerValue] = cfg_field(default_factory=list, desc=(
        "Leakage table points, always in W"
    ))


# Cache of parsed YAML files, keyed by absolute path
_yaml_cache = {}


def _resolve_path(path: str) -> str:
    # Like systree.Component.get_file_path, resolve the file from PYTHONPATH
    if os.path.isabs(path):
        return path

    for dirpath in sys.path:
        full_path = os.path.join(dirpath, path)
        if os.path.exists(full_path):
            return full_path

    raise FileNotFoundError(f'Could not find power model file from PYTHONPATH: {path}')


def _load_yaml(path: str) -> dict:
    full_path = _resolve_path(path)
    if full_path not in _yaml_cache:
        with open(full_path, 'r', encoding='utf-8') as file_desc:
            _yaml_cache[full_path] = yaml.safe_load(file_desc)
    return _yaml_cache[full_path]


def _get_table_values(path: str, source_name: str, table: dict) -> tuple[str, list[PowerValue]]:
    if table.get('type') != 'linear':
        raise ValueError(
            f'{path}: power source "{source_name}" has unsupported table type '
            f'"{table.get("type")}" (only "linear" is supported)')

    unit = str(table.get('unit', ''))
    values = []
    for temp, volt_table in table.get('values', {}).items():
        for volt, freq_table in volt_table.items():
            for freq, value in freq_table.items():
                # YAML keys may come out as int, float or str; normalize through str
                freq_value = -1.0 if str(freq) == 'any' else float(str(freq))
                values.append(PowerValue(temp=float(str(temp)), volt=float(str(volt)),
                    freq=freq_value, value=float(str(value))))

    return unit, values


def _get_source_config(path: str, source_name: str, source: dict) -> PowerSourceConfig:
    config = PowerSourceConfig(name=source_name)

    for table_name, table in source.items():
        unit, values = _get_table_values(path, source_name, table)
        if table_name == 'dynamic':
            config.dynamic_unit = unit
            config.dynamic = values
        elif table_name == 'leakage':
            if unit != 'W':
                raise ValueError(
                    f'{path}: leakage table of power source "{source_name}" must '
                    f'have unit "W" (got "{unit}")')
            config.leakage = values
        else:
            raise ValueError(
                f'{path}: power source "{source_name}" has unknown table '
                f'"{table_name}" (must be "dynamic" or "leakage")')

    return config


def apply_power_yaml(config: Config, path: str, names: list[str] | None = None):
    """Load a YAML power model file and apply it onto a power config.

    Every top-level source name of the file is assigned to the
    :class:`PowerSourceConfig` field of ``config`` with the same name; a
    name with no matching field is an error. Fields with no entry in the
    file are left untouched, so their power sources stay inert.

    The file is resolved from PYTHONPATH, like other model property files.

    Parameters
    ----------
    config: Config
        Config whose PowerSourceConfig fields should be filled, typically
        the nested power config of a model.
    path: str
        Path of the YAML file, relative to PYTHONPATH.
    names: list[str] | None
        Names of the sources to apply. All sources of the file if None.
        A requested source missing from the file is an error. Use this
        when the file also carries entries not meant for ``config``
        (e.g. a positional source list).
    """
    content = _load_yaml(path)

    if names is None:
        names = list(content.keys())

    for name in names:
        if name not in content:
            raise KeyError(f'{path}: power source "{name}" not found')
        if not isinstance(getattr(config, name, None), PowerSourceConfig):
            valid = [f.name for f in fields(config)
                     if isinstance(getattr(config, f.name, None), PowerSourceConfig)]
            raise KeyError(
                f'{path}: power source "{name}" has no matching field in '
                f'{type(config).__name__} (valid sources: {valid})')
        setattr(config, name, _get_source_config(path, name, content[name]))


def power_tables(config: Config) -> Config:
    """Return the ``power`` tables of a config, creating them if needed.

    The ``power`` field of a model config is None until a power model is
    given: this creates it, empty, with the class the field declares, so that
    the power model can be applied onto it.

    Parameters
    ----------
    config: Config
        The component config, carrying the ``power`` field.

    Returns
    -------
    Config
        The power tables of the config.
    """
    power = config.power
    if power is None:
        power = get_type_hints(type(config))['power']()
        config.power = power
    return power


def consume_power_model(config: Config):
    """Apply the power model file named by ``config.power_model``, if any.

    Helper for the ``configure()`` hook of components following the
    ``power_model``/``power`` convention (see the module docstring): when
    the ``power_model`` field carries a file path, the file is applied
    onto the ``power`` nested config with :func:`apply_power_yaml`.

    Parameters
    ----------
    config: Config
        The component config, carrying ``power_model`` and ``power`` fields.
    """
    if getattr(config, 'power_model', ''):
        apply_power_yaml(power_tables(config), config.power_model)


def load_power_yaml_list(path: str, key: str) -> list[PowerSourceConfig]:
    """Load an ordered list of power sources from a YAML power model file.

    The file entry must be a YAML list of tables (like the riscy 'insn_groups').
    The returned sources are named '<key>_<index>' and their order is preserved,
    so they can be indexed positionally (e.g. by instruction power group).

    Parameters
    ----------
    path: str
        Path of the YAML file, relative to PYTHONPATH.
    key: str
        Name of the file entry containing the list.

    Returns
    -------
    list[PowerSourceConfig]
        One config per list element, in file order.
    """
    content = _load_yaml(path)

    if key not in content:
        raise KeyError(f'{path}: power source list "{key}" not found')

    entries = content[key]
    if not isinstance(entries, list):
        raise ValueError(f'{path}: power source list "{key}" must be a YAML list')

    return [_get_source_config(path, f'{key}_{index}', entry)
        for index, entry in enumerate(entries)]
