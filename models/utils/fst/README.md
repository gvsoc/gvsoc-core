# Vendored libfst (FST waveform reader)

These sources are imported verbatim from Verilator's pre-portabilized copy of
GTKWave's libfst. They are consumed by `utils.fst_dumper` to inject FST
waveform files into the GVSoC GUI's trace database.

## Upstream

- Project: https://github.com/verilator/verilator
- Path: `include/gtkwave/`
- Pinned commit: `985f45759ca122d78f902d9bbcc9185ffedce14c` (2026-03-11, "Update libfst from upstream")
- Verilator tracks GTKWave upstream (https://github.com/gtkwave/gtkwave,
  `gtkwave3-gtk3/src/helpers/fst/`); we use Verilator's tree because it
  includes a `fst_config.h` that replaces the autoconf-generated `config.h`
  expected by `fstapi.c`, plus a few portability fixes that GTKWave has not
  yet upstreamed.

## Files

| File              | Purpose                                          | License             |
|-------------------|--------------------------------------------------|---------------------|
| `fstapi.h`        | FST reader/writer public API                     | MIT                 |
| `fstapi.c`        | FST reader/writer implementation                 | MIT                 |
| `fastlz.h`        | FastLZ compression header                        | MIT                 |
| `fastlz.c`        | FastLZ compression implementation                | MIT                 |
| `lz4.h`           | LZ4 compression header                           | BSD-2-Clause        |
| `lz4.c`           | LZ4 compression implementation                   | BSD-2-Clause        |
| `fst_config.h`    | Verilator-supplied autoconf shim                 | CC0-1.0             |
| `fst_win_unistd.h`| Windows POSIX shim (unused on Linux)             | MIT                 |
| `config.h`        | Shim → `#include "fst_config.h"` (one-liner, ours) | n/a               |

All licenses are compatible with the project's Apache-2.0 licence.

## How to update

1. Find Verilator's latest commit touching `include/gtkwave/`:
   ```bash
   curl -s 'https://api.github.com/repos/verilator/verilator/commits?path=include/gtkwave&per_page=1' | grep -E 'sha|date'
   ```
2. Overwrite all files (except `config.h`, `README.md`) with the new revision.
3. Update the pinned commit hash above.
4. Rebuild and run the FST dumper smoke test.

## Notes

- Only the **reader** path of libfst is exercised by `utils.fst_dumper`. The
  writer path is compiled in but unused; it has no runtime side-effects.
- `fstapi.c` `#include <config.h>` resolves to our `config.h` shim, which
  pulls in `fst_config.h` (Verilator's). Keeping the shim local means the
  vendored sources stay byte-identical to upstream.
- Link-time deps: `-lz` (system zlib) and `-lpthread` (already in the
  GVSoC link line).
