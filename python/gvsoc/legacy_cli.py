#
# Copyright (C) 2026 Germain Haugou
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

"""The ``gvsoc`` command, kept for the existing command lines.

It used to start the gapy launcher. It now translates the gapy options into
gvrun ones and runs gvrun in this process:

- ``--binary=FILE`` sets every target parameter named ``binary`` which got no
  value otherwise (``--parameter`` still wins). Targets declare it at different
  paths (``chip/soc/binary``, ``system/binary``, ...), so it is resolved while
  the target is built. A target without one is an error.
- The gapy flash and OTA options have no gvrun equivalent and are refused.
- Everything else (``--target``, ``--target-dir``, ``--work-dir``,
  ``--config-opt``, the run options, the commands) is the same in gvrun.
"""

import argparse
import os
import sys

# The gapy options gvrun does not have
_UNSUPPORTED = [
    '--debug-binary',
    '--flash-layout-level',
    '--flash-no-auto',
    '--flash-property-override',
    '--multi-flash-content',
    '--ota-sign-dgst',
    '--ota-sign-key',
]

# The commands which need the binary
_BINARY_COMMANDS = {'image', 'flash', 'run', 'build', 'all'}


def _error(msg: str) -> int:
    print(f'gvsoc: {msg}', file=sys.stderr)
    return 1


def main(argv: list[str] | None = None) -> int:
    if argv is None:
        argv = sys.argv[1:]

    parser = argparse.ArgumentParser(add_help=False, allow_abbrev=False)
    parser.add_argument('--binary', default=None)
    for opt in _UNSUPPORTED:
        parser.add_argument(opt, dest=opt[2:].replace('-', '_'), action='append',
            nargs='?', const=True, default=None)
    args, gvrun_argv = parser.parse_known_args(argv)

    for opt in _UNSUPPORTED:
        if getattr(args, opt[2:].replace('-', '_')) is not None:
            return _error(f'{opt} is not supported anymore (gvsoc now runs gvrun)')

    import gvrun.__main__
    import gvrun.commands
    import gvrun.parameter
    import gvrun.systree

    binary = args.binary
    binary_params: list[str] = []

    if binary is not None:
        binary = os.path.abspath(binary)
        get_value = gvrun.parameter.get_parameter_arg_value

        def get_value_with_binary(name: str):
            value = get_value(name)
            if value is None and name.rsplit('/', 1)[-1] == 'binary':
                binary_params.append(name)
                return binary
            return value

        # systree imported it by name, patch both
        gvrun.parameter.get_parameter_arg_value = get_value_with_binary
        gvrun.systree.get_parameter_arg_value = get_value_with_binary

        handle_commands = gvrun.commands.handle_commands

        def handle_commands_with_binary(target, cmd_args):
            if len(binary_params) == 0 and \
                    len(_BINARY_COMMANDS & set(cmd_args.command)) > 0:
                raise RuntimeError(
                    f'--binary: target {cmd_args.target} has no binary parameter, '
                    'give it with --parameter <path>/<name>=<file>')
            handle_commands(target, cmd_args)

        gvrun.commands.handle_commands = handle_commands_with_binary

    # Some target and runner code parses sys.argv itself
    sys.argv = ['gvrun'] + gvrun_argv

    return gvrun.__main__.main(gvrun_argv)


if __name__ == '__main__':
    raise SystemExit(main())
