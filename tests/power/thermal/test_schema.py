# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)

"""Schema validation tests for the thermal YAML parser.

Pure-Python checks of ``load_thermal_yaml`` / ``apply_thermal_yaml``
covering the geometry and temp_min/temp_max extensions: valid files must
round-trip their values onto the configs, malformed ones must raise a
ValueError naming the offending sync point. Runs standalone (needs the
generators on PYTHONPATH, i.e. a sourced environment):

    python3 test_schema.py
"""

import os
import shutil
import sys
import tempfile

try:
    from thermal.thermal_model import (ThermalModelConfig, apply_thermal_yaml,
                                       load_thermal_yaml)
except ModuleNotFoundError:
    # Standalone run: the generators are importable under gvrun (which puts
    # the model dirs on sys.path) but not from the plain sourced env; locate
    # the installed generators next to the gvrun binary.
    gvrun = shutil.which('gvrun')
    if gvrun is None:
        raise
    sys.path.append(os.path.join(os.path.dirname(os.path.dirname(gvrun)),
                                 'generators'))
    from thermal.thermal_model import (ThermalModelConfig, apply_thermal_yaml,
                                       load_thermal_yaml)


GOOD_YAML = """
period: 1000000
temp_ambient: 25.0
temp_min: 25.0
temp_max: 60.0
sync_points:
  mem0:
    path: mem0
    rth: 100.0
    tau: 2.0e-6
    geometry:
      x: 0.0
      y: 1.0
      w: 2.0
      h: 3.0
  mem1:
    path: mem1
"""

failures = []


def check(label, condition, details=''):
    if condition:
        print(f'PASS {label}')
    else:
        failures.append(label)
        print(f'FAIL {label} {details}')


def parse(content):
    """Write the YAML to a temp file and parse it."""
    with tempfile.NamedTemporaryFile('w', suffix='.yaml', delete=False) as file_desc:
        file_desc.write(content)
        file_desc.flush()
        return load_thermal_yaml(file_desc.name)


def check_raises(label, content, message_part):
    try:
        parse(content)
    except ValueError as exc:
        check(label, message_part in str(exc),
              f'(error does not mention "{message_part}": {exc})')
    else:
        check(label, False, '(no ValueError raised)')


# Valid file: geometry and color-scale bounds round-trip
points = parse(GOOD_YAML)
check('good file parses', len(points) == 2)
check('geometry round-trips',
      (points[0].geo_x, points[0].geo_y, points[0].geo_width,
       points[0].geo_height) == (0.0, 1.0, 2.0, 3.0))
check('geometry-less point defaults to no geometry',
      points[1].geo_width == 0.0 and points[1].geo_height == 0.0)

config = ThermalModelConfig()
with tempfile.NamedTemporaryFile('w', suffix='.yaml', delete=False) as file_desc:
    file_desc.write(GOOD_YAML)
    file_desc.flush()
    apply_thermal_yaml(config, file_desc.name)
check('apply sets color-scale bounds',
      config.temp_min == 25.0 and config.temp_max == 60.0)
check('apply keeps sync points', len(config.sync_points) == 2)

# Geometry validation errors, all naming the sync point
BASE = """
sync_points:
  mem0:
    path: mem0
    geometry:
{geometry}
"""

check_raises('unknown geometry key',
             BASE.format(geometry='      x: 0.0\n      y: 0.0\n      w: 1.0\n'
                                   '      h: 1.0\n      depth: 1.0'), 'mem0')
check_raises('missing geometry key',
             BASE.format(geometry='      x: 0.0\n      y: 0.0\n      w: 1.0'),
             'mem0')
check_raises('non-positive geometry width',
             BASE.format(geometry='      x: 0.0\n      y: 0.0\n      w: 0.0\n'
                                   '      h: 1.0'), 'mem0')
check_raises('geometry not a mapping',
             'sync_points:\n  mem0:\n    path: mem0\n    geometry: [0, 0, 1, 1]\n',
             'mem0')

# Color-scale bound validation
check_raises('temp_min without temp_max',
             'temp_min: 25.0\nsync_points:\n  mem0:\n    path: mem0\n',
             'temp_min and temp_max must be given together')
check_raises('temp_max not above temp_min',
             'temp_min: 60.0\ntemp_max: 25.0\n'
             'sync_points:\n  mem0:\n    path: mem0\n',
             'temp_max must be > temp_min')

# Pre-existing strictness must survive the extension
check_raises('unknown sync point key still raises',
             'sync_points:\n  mem0:\n    path: mem0\n    rht: 1.0\n', 'mem0')

if failures:
    print(f'{len(failures)} schema test(s) failed: {failures}')
    sys.exit(1)

print('All schema tests passed')
