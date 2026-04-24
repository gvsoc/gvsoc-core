/*
 * Shim that lets the vendored libfst sources resolve their `#include <config.h>`
 * to fst_config.h (Verilator's pre-portabilized configuration). Kept tiny so the
 * vendored fstapi.c / fastlz.c / lz4.c files stay byte-identical to upstream and
 * can be re-vendored by overwriting them.
 */
#include "fst_config.h"
