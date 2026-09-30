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
     * `description` is an optional free-form metadata string (encoded as
     *     "<dir>|<type>" by convention — VCD doesn't carry direction, so
     *     the plugin-side helper just emits "|<vcd-type>" e.g. "|wire" or
     *     "|parm". May be NULL or empty when no metadata is available.
     *     The pointer must stay valid for the lifetime of the simulation;
     *     the host is responsible for owning a copy if it needs to.
     * Returns an opaque handle, or NULL if the host refuses (e.g. width too
     * wide). Safe to call multiple times before the simulation starts.
     */
    VlSignal (*reg_logical)(void *ctx, const char *path, int width,
                            const char *description);

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
     * Arguments starting with "--" are options of the RTL simulator, from
     * the host component's properties: --design=DIR (compiled design, for
     * simulators which load one), --stats, --slice=T (time per step without
     * step_until), --gui-scope=S[:N] (signals shown in the GUI, repeatable).
     * A plugin ignores the ones it does not know.
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

/*
 * Version 2 (proposed by gvsoc_svsim; hosts that don't know it keep using
 * gv_verilator_plugin_get and are unaffected).
 *
 * Signals: a v2 plugin registers every signal of the design with
 * reg_logical but only needs to produce the values of the signals somebody
 * looks at. A v2 host therefore registers them without enabling them: they
 * are declared to the GUI (visible in its signal browser) and enabled when
 * the user adds them to the timeline. The host reports every enable change
 * with signal_enabled (initially every signal is disabled); the plugin then
 * pushes the current value of a newly enabled signal and its changes from
 * then on.
 *
 * Time: instead of step(), the host calls step_until with the time of its
 * own next event (the end of a run-for-time or cycle step, another model's
 * event; -1 when there is none). The plugin simulates up to that time at
 * most, and stops earlier, between two of its time steps, when should_stop
 * returns true (the host's stop request, e.g. the GUI's stop button; the
 * plugin polls it periodically). It returns the time it reached, which the
 * host adopts as its current time, and the time of the design's next event,
 * where the host calls it again. The design never runs ahead of the host.
 */
typedef struct {
    int exit_code;       /* -1: continue; >= 0: the design finished with this code */
    int64_t reached_ps;  /* the design has been simulated up to this time */
    int64_t next_ps;     /* time of its next event (-1: none) */
} VlStepUntilResult;

/*
 * Version 3 (4-state values): host callbacks with push_logical_flags, which
 * reports a value with its per-bit flags, as vp::Signal::set(value, flags):
 * (flag 0, value b) is b, (flag 1, value 0) X, (flag 1, value 1) Z. A v3 host
 * (version >= 3 and set_host_callbacks_v3 set) calls set_host_callbacks_v3
 * instead of set_host_callbacks; older hosts ignore the field.
 */
typedef struct {
    VlHostCb base;
    void (*push_logical_flags)(void *ctx, VlSignal sig, uint64_t value,
                               uint64_t flags, int64_t time_ps);
} VlHostCbV3;

typedef struct {
    VlPluginVtable base;
    uint32_t version;  /* 2, or 3 with set_host_callbacks_v3 */
    void (*signal_enabled)(VlPlugin *, VlSignal sig, int enabled);
    VlStepUntilResult (*step_until)(VlPlugin *, int64_t limit_ps,
                                    int (*should_stop)(void *ctx), void *ctx);
    void (*set_host_callbacks_v3)(VlPlugin *, const VlHostCbV3 *cb);
} VlPluginVtableV2;

const VlPluginVtableV2 *gv_verilator_plugin_get_v2(void);

#ifdef __cplusplus
}
#endif

#endif
