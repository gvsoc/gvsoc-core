// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)

/*
 * GVSoC Verilator plugin ABI.
 *
 * A "verilator plugin" is a shared library that wraps a Verilator-built
 * RTL design behind a tiny C ABI. The GVSoC `utils.verilator` model
 * dlopens such a library at runtime and drives it cycle-by-cycle from a
 * permanent ClockEvent.
 *
 * Each plugin .so must export a single symbol:
 *
 *     extern "C" const VlPluginVtable *gv_verilator_plugin_get(void);
 *
 * Everything design-specific (clock toggling, reset sequence, trace dumping,
 * exit-pin sampling, signal injection) lives behind that vtable.
 */

#ifndef GVSOC_VERILATOR_PLUGIN_H
#define GVSOC_VERILATOR_PLUGIN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VlPlugin VlPlugin;

/*
 * Opaque handle for a host-side signal registered via VlHostCb::reg_logical.
 * Plugins must treat it as completely opaque — the host derefs it.
 */
typedef void *VlSignal;

/*
 * Host-side callbacks the plugin uses to push trace events back to GVSoC.
 *
 * Set on the plugin via VlPluginVtable::set_host_callbacks. The plugin uses
 * reg_logical at setup time to declare the signals it will report, then
 * push_logical from inside step() (or from a Verilator VPI cbValueChange
 * handler) to report value changes. Updates land in the GVSoC trace engine
 * and become visible in the gvsoc-gui3 timeline.
 */
typedef struct {
    /* Opaque host context. Pass back as the first arg to every callback. */
    void *ctx;

    /*
     * Register a logical-typed signal of the given bit width (1..64).
     * `path` is a `/`-separated hierarchical name (e.g. "core/regfile/x5").
     * Returns an opaque handle, or NULL if the host refuses (e.g. width too
     * wide). Safe to call multiple times before the simulation starts.
     */
    VlSignal (*reg_logical)(void *ctx, const char *path, int width);

    /*
     * Report a new value for a previously registered signal.
     *
     * `time_ps` is the absolute time of the event in picoseconds. The
     * plugin reports the actual Verilator time of each value change, so
     * batched flushes (engine pause, sim end) preserve waveform shape
     * — without per-cycle flushes during the run, the host still sees
     * each event at its real timestamp instead of every event collapsing
     * to "now".
     */
    void (*push_logical)(void *ctx, VlSignal sig, uint64_t value,
                         int64_t time_ps);
} VlHostCb;

/*
 * Result of a step() call. The plugin owns the simulation time inside
 * its VerilatedContext; it tells the host how far to advance before
 * calling step again. This lets the plugin batch multiple internal
 * cycles per host call (lower engine round-trip overhead) and lets the
 * host schedule a single TimeEvent instead of a per-cycle ClockEvent.
 */
typedef struct {
    /*
     * -1  -> simulation should continue (host re-schedules at +time_to_next)
     * >=0 -> design has finished, value is the exit code (host calls close)
     */
    int exit_code;

    /*
     * Picoseconds the host should wait before calling step() again.
     * Only meaningful when exit_code == -1; ignored otherwise.
     * For pure clocked designs this is typically the clock period
     * (one cycle); the plugin may return a multiple to amortise the
     * host's scheduling cost.
     */
    int64_t time_to_next;
} VlStepResult;

typedef struct {
    /*
     * Build the design and parse Verilator-style plusargs from argv
     * (e.g. "+firmware=foo.hex", "+trace=foo.vcd", "+maxcycles=N").
     * Returns NULL on failure.
     */
    VlPlugin *(*open)(int argc, const char *const *argv);

    /*
     * Advance the design (one or more internal cycles), dump traces, and
     * report when the host should call back. See VlStepResult.
     */
    VlStepResult (*step)(VlPlugin *);

    /*
     * Final + cleanup. Closes any trace file and deletes all internal state.
     * Safe to call exactly once after a successful `open`.
     */
    void (*close)(VlPlugin *);

    /*
     * Optional. Called once after open() returns. The plugin should use
     * `cb` to register the signals it wants to expose to the GUI and arm
     * any underlying notification mechanism (e.g. VPI cbValueChange).
     * Plugins that don't expose signals can leave this pointer NULL.
     */
    void (*set_host_callbacks)(VlPlugin *, const VlHostCb *cb);

    /*
     * Optional. Flush any buffered trace state to the host immediately.
     * The host calls this when the engine pauses (and at simulation end)
     * so signal values reach the GUI without waiting for the plugin's
     * internal buffers to fill. Plugins that don't buffer can leave this
     * pointer NULL.
     */
    void (*flush)(VlPlugin *);
} VlPluginVtable;

const VlPluginVtable *gv_verilator_plugin_get(void);

#ifdef __cplusplus
}
#endif

#endif
