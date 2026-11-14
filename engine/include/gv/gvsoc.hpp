/*
 * Copyright (C) 2021 GreenWaves Technologies, SAS, ETH Zurich and University of Bologna
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once


#include <string>
#include <stdint.h>
#include <vector>

namespace gv {

    /**
     * Compatibility version of the gv:: public API (Gvsoc / Vcd / Vcd_user / Io / Wire).
     *
     * BUMP THIS on every breaking change to those interfaces: changing or removing a
     * virtual signature, reordering virtuals (vtable layout), or changing protocol
     * semantics a consumer depends on. Purely additive changes that keep old consumers
     * working need not bump it (bumping is always safe — it just forces a rebuild).
     *
     * v1 == trace declare/subscribe protocol (event_declare / event_subscribe).
     * v2 == ComponentTreeNode carries TreeMapping address maps (layout change).
     * v3 == Gvsoc::restart() added (a consumer calling it needs an engine providing it).
     * v4 == TreeBinding carries master/slave signature labels (layout change).
     * v5 == GvsocConf gains proxy_enabled (layout change).
     * v6 == Gvsoc gains the Stdout base for the console-output channel (vtable layout change).
     * v7 == Gvsoc gains get_memcheck_fault() (appended vtable slot).
     * v8 == Vcd_user gains event_update_varlen() (appended vtable slot) and the
     *       Vcd_event_type_varlen event type (appended enumerator).
     * v9 == Vcd gains trace_subscribe()/trace_unsubscribe() (appended vtable slots):
     *       text-trace recording control, streamed as varlen events.
     * v10 == Vcd gains trace_level_set() (appended vtable slot): runtime global
     *       trace-level control (the --trace-level option).
     * v11 == vp::ComponentTreeNode gains ports/num_ports (appended fields): the
     *       port list of each component comes from the compiled tree.
     */
    #define GV_API_VERSION 11

    /**
     * Return the GV_API_VERSION the engine was built with.
     *
     * Strong C symbol baked into the engine shared library so a consumer (e.g. the GUI)
     * can verify at runtime that the engine it actually loaded matches the API it was
     * compiled against. The symbol is intentionally non-weak: a consumer linked against
     * an engine predating this accessor fails to resolve it and refuses to start.
     */
    extern "C" int gv_api_version();

    /**
     * IO request status.
     */
    enum Api_mode {
        // Asynchronous mode where some commands run in background
        Api_mode_async,
        // Synchronous mode where all commands are executed from the function call
        Api_mode_sync
    };

    /**
     * IO request status.
     */
    enum Io_request_status {
        // Request was successfull
        Io_request_ok,
        // Request failed
        Io_request_ko
    };

    /**
     * IO request type.
     */
    enum Io_request_type {
        // Request was successfull
        Io_request_read = 0,
        // Request failed
        Io_request_write = 1
    };


    /**
     * Class used to represent a memory-mapped request.
     *
     * Used in all methods related to IO requests.
     */
    class Io_request
    {
    public:
        Io_request() : data(NULL), size(0), type(gv::Io_request_read), addr(0) {}

        // Pointer to the area containing the data
        uint8_t *data;
        // Size in bytes of the access
        size_t size;
        // Request type, possible values defined by Io_request_type.
        int type;
        // Address of the access
        uint64_t addr;
        // Status of the access
        Io_request_status retval;

        // Reserved
        void *handle;
        bool sent = false;
        bool granted = false;
        bool replied = false;
    };


    /**
     * Class used to represent IO bindings.
     *
     * This class defines all the methods that the external C++ can call to interact with
     * GVSOC for what concerns IO requests.
     */
    class Io_binding
    {
    public:
        /**
         * Inject a memory-mapped access.
         *
         * This operation is asynchronous. The request can be granted and replied by callbacks
         * executed afterwards.
         *
         * @param req The IO request describing the memory-mapped access.
         */
        virtual void access(Io_request *req) {}

        /**
         * Grant a memory-mapped access.
         *
         * This can be used to grant an incoming request. This can be used to notify the
         * initiator of the request that the request is accepted and that another one can be sent.
         * The initiator will usually use this information to pipeline several requests.
         * Not granting a request will prevent the initiator from sending another request.
         * Note that this step is optional, the request can be directly replied.
         *
         * @param req The IO request describing the memory-mapped access.
         */
        virtual void grant(Io_request *req) {}

        /**
         * Reply to a memory-mapped access.
         *
         * This can be used to reply to an incoming request. This can be used to notify the
         * initiator of the request that the request is over. The request contains the status
         * of the request and in case of a read command, the request contains the read data.
         *
         * @param req The IO request describing the memory-mapped access.
         */
        virtual void reply(Io_request *req) {}
    };


    /**
     * Class required for IO binding.
     *
     * When the external C++ code creates an IO binding, it must implement all the methods of this class
     * to properly interact with GVSOC for what concerns IO requests.
     */
    class Io_user
    {
    public:
        /**
         * Called by GVSOC to inject a memory-mapped access.
         *
         * This operation is asynchronous. This method must return immediately and the request must be granted and/or
         * replied afterwards.
         * No GVSOC API can be called from this callback.
         *
         * @param req The IO request describing the memory-mapped access.
         */
        virtual void access(Io_request *req) {}

        /**
         * Called by GVSOC tp grant a memory-mapped access.
         *
         * This can be used to grant an incoming request. This can be used to notify the
         * initiator of the request that the request is accepted and that another one can be sent.
         * The initiator will usually use this information to pipeline several requests.
         * Not granting a request will prevent the initiator from sending another request.
         * Note that this step is optional, the request can be directly replied.
         * No GVSOC API can be called from this callback.
         *
         * @param req The IO request describing the memory-mapped access.
         */
        virtual void grant(Io_request *req) {}

        /**
         * Called by GVSOC to reply to a memory-mapped access.
         *
         * This can be used to reply to an incoming request. This can be used to notify the
         * initiator of the request that the request is over. The request contains the status
         * of the request and in case of a read command, the request contains the read data.
         * No GVSOC API can be called from this callback.
         *
         * @param req The IO request describing the memory-mapped access.
         */
        virtual void reply(Io_request *req) {}
    };

    /**
     * Class used to represent wire bindings.
     *
     * Wire bindings can be used to interact with a simple wire holding a value.
     * This class defines all the methods that the external C++ can call to interact with
     * GVSOC for what concerns wire requests.
     */
    class Wire_binding
    {
    public:
        /**
         * Update the value of the wire.
         *
         * This can be called to notify GVSOC that the value of the wire has changed and to give
         * the new value.
         *
         * @param value The value of the wire.
         */
        virtual void update(int value) {}
    };

    /**
     * Class required for wire binding.
     *
     * Wire bindings can be used to interact with a simple wire holding a value.
     * When the external C++ code creates a wire binding, it must implement all the methods of this class
     * to properly interact with GVSOC for what concerns updates of the wire.
     */
    class Wire_user
    {
    public:
        /**
         * Called by GVSOC to update the value of the wire.
         *
         * This is called everytime the value of the wire is changed to give the new value.
         *
         * @param value The value of the wire.
         */
        virtual void update(int value) {}
    };


    /**
     * VCD event type
     */
    enum Vcd_event_type {
        // Single bit signal
        Vcd_event_type_logical,
        // Bitfield
        Vcd_event_type_bitfield,
        // Real signals
        Vcd_event_type_real,
        // Strings
        Vcd_event_type_string,
        // Flame chart
        Vcd_event_type_flame_chart,
        // Variable-length data. Each event carries an arbitrary byte payload which is
        // stored length-prefixed by the consumer, so mostly-unique payloads (e.g. system
        // trace lines) are stored by value instead of being interned like strings.
        Vcd_event_type_varlen
    };


    /**
     * Class required for receiving VCD events.
     *
     * When the external C++ connects to the VCD support, it must implement all the methods of this class
     * to properly interact with GVSOC for what concerns VCD events.
     */
    class Vcd_user
    {
    public:
        /**
         * Called by GVSOC at simulation startup to declare every VCD event in
         * the platform.
         *
         * Invoked once per declared signal, for every Vcd_user bound at
         * vcd_bind() time, before simulation runs. The default implementation
         * is a no-op so legacy Vcd_user subclasses (which only override
         * event_register) keep working unchanged — they still receive only
         * events the engine's regex / include_raw filter matched.
         *
         * Override this to capture the full declared signal set independent
         * of whether the engine matched a regex — e.g. populate the GUI's
         * signal browser. The subclass can then call
         * gv::Gvsoc::event_subscribe() to ask the engine to stream events
         * for any subset of those signals it cares about.
         *
         * No GVSOC API may be called from inside this callback.
         */
        virtual void *event_declare(std::string path, Vcd_event_type type, int width,
            std::string description, std::string clock_path="") { return NULL; }

        /**
         * Called by GVSOC on every enable/disable transition of a streamed event (regex match,
         * explicit include_raw, or event_subscribe()).
         *
         *  enabled=true : the event starts being dumped. Allocate (or look up) and return the handle
         *      threaded back into event_update_*() calls. When timestamp > 0 the event had no data
         *      before now, so the disabled period ending here is closed: [0, timestamp) on the
         *      first-ever enable (the no-data lead-in), or [last-disable, timestamp) on a re-enable.
         *  enabled=false: the event stops being dumped. Opens a disabled period at timestamp (closed
         *      by the next enable, or left open to the end of the trace).
         *
         * The default implementation is a no-op returning NULL, so consumers that don't care about
         * streaming (e.g. the VCD file dumper) are unaffected. event_declare() is separate and still
         * used to discover the full signal set.
         */
        virtual void *event_enable(std::string path, Vcd_event_type type, int width,
            std::string description, std::string clock_path, bool enabled, int64_t timestamp)
            { return NULL; }

        /**
         * Called by GVSOC to update the value of a logical VCD event.
         *
         * This call is used by GVSOC to notify the external C code that the VCD event has a new value.
         * No GVSOC API can be called from this callback.
         *
         * @param timestamp Timestamp of the new value in picoseconds.
         * @param id ID of the VCD event.
         * @param value The new value.
         */
        virtual bool event_update_logical(int64_t timestamp, int64_t cycles, void *id, uint64_t value,
            int flags) { return false; }

        /**
         * Called by GVSOC to update the value of a bitfield VCD event.
         *
         * This call is used by GVSOC to notify the external C code that the VCD event has a new value.
         * No GVSOC API can be called from this callback.
         *
         * @param timestamp Timestamp of the new value in picoseconds.
         * @param id ID of the VCD event.
         * @param value The new value.
         */
        virtual bool event_update_bitfield(int64_t timestamp, int64_t cycles, void *id,
            uint8_t *value, uint8_t *flags) { return false; }

        /**
         * Called by GVSOC to update the value of a real VCD event.
         *
         * This call is used by GVSOC to notify the external C code that the VCD event has a new value.
         * No GVSOC API can be called from this callback.
         *
         * @param timestamp Timestamp of the new value in picoseconds.
         * @param id ID of the VCD event.
         * @param value The new value.
         */
        virtual bool event_update_real(int64_t timestamp, int64_t cycles, void *id, double value)
            { return false; }

        /**
         * Called by GVSOC to update the value of a string VCD event.
         *
         * This call is used by GVSOC to notify the external C code that the VCD event has a new value.
         * No GVSOC API can be called from this callback.
         *
         * @param timestamp Timestamp of the new value in picoseconds.
         * @param id ID of the VCD event.
         * @param value The new value.
         * @param flags Signal flags. 0 for normal signal, 1 for high impedance (Z) and 2 for
         *      conflict (X)
         * @param realloc True if the string must be reallocated by the callee or false if it the
         *      string from the caller can be kept.
         */
        virtual bool event_update_string(int64_t timestamp, int64_t cycles, void *id,
            const char *value, int flags, bool realloc) { return false; }

        /**
         * Called by GVSOC to lock the external controller.
         *
         * As vcd traces are updated in chunks, GVSOC will call this function before the chunk
         * starts, in case something needs to be protected, and to avoid protecting it in each
         * call to the event update methods.
         */
        virtual void lock() {}

        /**
         * Called by GVSOC to unlock the external controller.
         *
         * As vcd traces are updated in chunks, GVSOC will call this function after the chunk
         * has ended.
         */
        virtual void unlock() {}

        /**
         * Called by GVSOC to push a new value of a variable-length VCD event.
         *
         * Unlike string events, the payload is always copied by the callee, so the caller
         * keeps ownership of the buffer and may reuse it right after the call.
         * No GVSOC API can be called from this callback.
         *
         * NOTE: declared last on purpose so the vtable slots of the existing entries are
         * unchanged; only this new slot is added. GV_API_VERSION is bumped accordingly.
         *
         * @param timestamp Timestamp of the new value in picoseconds.
         * @param cycles Cycle count of the new value.
         * @param id ID of the VCD event.
         * @param data The payload bytes (not null-terminated).
         * @param size Number of bytes in data.
         * @param flags Event flags, free for the producer (e.g. a trace level).
         */
        virtual bool event_update_varlen(int64_t timestamp, int64_t cycles, void *id,
            const uint8_t *data, int size, int flags) { return false; }
    };


    /**
     * Class required for receiving simulated-software console output.
     *
     * The external C++ (e.g. the GUI) implements this to receive the bytes
     * the simulated software prints (semi-hosting writes, MMIO putchar, ...).
     * Unlike the VCD/trace channel, this is always active regardless of build
     * optimization level, so console output is never compiled out.
     */
    class Stdout_user
    {
    public:
        /**
         * Called by GVSOC whenever a model emits console output.
         *
         * @param timestamp Simulation time of the output in picoseconds.
         * @param path Hierarchical path of the emitting component.
         * @param data The emitted bytes (not null-terminated).
         * @param size Number of bytes in data.
         */
        virtual void stdout_dump(int64_t timestamp, std::string path,
            const char *data, int size) {}
    };


    /**
     * GVSOC interface for VCD events
     *
     * Gather all the methods which can be called to bind the simulated system with external C++ code
     * so that VCD features can be configured.
     */
    class Vcd
    {
    public:
        /**
         * Bind external C++ code VCD features
         *
         * @param user A pointer to the caller class instance which will be called for all VCD callbacks. This caller
         *             must implement all the methods defined in class Vcd_user
         */
        virtual void vcd_bind(Vcd_user *user) {}

        /**
         * Enable VCD tracing
         *
         * This allows VCD traces to be dumped. This is by default enabled.
         */
        virtual void vcd_enable() {}

        /**
         * Disable VCD tracing
         *
         * After this call, no more VCD trace is dumped. They have to be reenabled again to
         * continue dumping.
         */
        virtual void vcd_disable() {}

        /**
         * Match kind for event_subscribe()/event_unsubscribe().
         */
        enum class MatchKind { Exact = 0, Prefix, Regex };

        /**
         * Subscribe to a set of declared events.
         *
         * Walks the engine's declared-event list at call time, matches every
         * entry against `pattern` according to `kind`, and bumps the per-event
         * subscriber count. On the 0->1 edge the dump pipeline is activated
         * and the bound Vcd_user starts receiving event_update_*() callbacks
         * for the matched events.
         *
         * There is no standing state — late-declared events are not
         * retroactively matched. In practice all events declare during the
         * one-shot TraceEngine::start() loop, so any subscribe issued after
         * vcd_bind() + start() sees them all.
         *
         * @param pattern The path / prefix / regex to match.
         * @param kind  Match policy. Default Exact = full-path match.
         * @return Number of events newly subscribed (refcount transitions counted).
         */
        virtual int event_subscribe(std::string pattern,
            MatchKind kind = MatchKind::Exact) { return 0; }

        /**
         * Symmetric teardown — unsubscribe events matching pattern under the
         * given kind. Returns the number of subscriptions removed.
         */
        virtual int event_unsubscribe(std::string pattern,
            MatchKind kind = MatchKind::Exact) { return 0; }

        /**
         * Subscribe to a set of system (text) traces for recording.
         *
         * Matches the platform's text traces (the ones --trace prints) against
         * `pattern` and bumps their per-trace subscriber count. On the 0->1
         * edge the trace's formatted lines start streaming to the bound
         * Vcd_user as varlen events (event_update_varlen), with the trace
         * level in the flags. This controls recording only: it is independent
         * from the --trace console/file output, which keeps working unchanged.
         * Lines respect the global trace level like console output.
         *
         * NOTE: declared last on purpose so the vtable slots of the existing
         * entries are unchanged; only these new slots are added.
         * GV_API_VERSION is bumped accordingly.
         *
         * @param pattern The path / prefix / regex to match.
         * @param kind  Match policy. Default Regex, like --trace.
         * @return Number of traces newly subscribed.
         */
        virtual int trace_subscribe(std::string pattern,
            MatchKind kind = MatchKind::Regex) { return 0; }

        /**
         * Symmetric teardown — drop the recording refcount of the matching
         * text traces; the 1->0 edge genuinely stops their streaming (unlike
         * the proxy "trace remove" command, which only stacks an exclude
         * regex). Returns the number of subscriptions removed.
         */
        virtual int trace_unsubscribe(std::string pattern,
            MatchKind kind = MatchKind::Regex) { return 0; }

        /**
         * Set the global trace level, like the --trace-level command-line option.
         *
         * Both console (--trace) and recording (trace_subscribe) output is gated by
         * this level: a trace only fires when its own level is at or below the
         * global one. Accepts "error", "warning", "info", "debug" or "trace"
         * (increasing verbosity); an unknown value is ignored.
         *
         * NOTE: declared last on purpose so the vtable slots of the existing entries
         * are unchanged; only this new slot is added. GV_API_VERSION is bumped.
         */
        virtual void trace_level_set(std::string level) {}
    };


    /**
     * GVSOC interface for simulated-software console output
     *
     * Bind external C++ code so it receives the bytes the simulated software
     * prints. Always-on (independent of the trace/VCD system).
     */
    class Stdout
    {
    public:
        /**
         * Bind external C++ code to receive console output.
         *
         * @param user A pointer to the caller class instance which will be called for every chunk
         *             of console output. This caller must implement class Stdout_user.
         */
        virtual void stdout_bind(Stdout_user *user) {}
    };


    /**
     * GVSOC interface for IO requests
     *
     * Gather all the methods which can be called to bind the simulated system with external C++ code
     * so that requests can be exchanged in both directions.
     */
    class Io
    {
    public:
        /**
         * Create a binding to the simulated system
         *
         * This creates a binding so that the external C++ code can exchange IO requests in both direction
         * with the simulated system. This is based on the same set of methods which must be implemented on
         * both sides.
         *
         * @param user A pointer to the caller class instance which will be called for all IO callbacks. This caller
         *             must implement all the methods defined in class Io_user
         * @param comp_name The name of the component where to connect.
         * @param itf_name The name of the component interface where to connect.
         *
         * @return A class instance which can be used to inject IO requests, or grant and reply to incoming requests.
         */
        virtual Io_binding *io_bind(Io_user *user, std::string comp_name, std::string itf_name)
            { return NULL; }

    };



    /**
     * GVSOC interface for wire updates.
     *
     * Gather all the methods which can be called to bind the simulated system with external C++ code
     * so that wire updates can be exchanged in both directions.
     * Wire updates can be used to synchronize raw signals like pads, interrupts or registers.
     */
    class Wire
    {
    public:
        /**
         * Create a binding to the simulated system.
         *
         * This creates a binding so that the external C++ code can exchange wire updates in both direction
         * with the simulated system. This is based on the same set of methods which must be implemented on
         * both sides.
         *
         * @param user A pointer to the caller class instance which will be called for all wire callbacks. This caller
         *             must implement all the methods defined in class Wire_user
         * @param comp_name The name of the component where to connect.
         * @param itf_name The name of the component interface where to connect.
         *
         * @return A class instance which can be used to inject wire updates.
         */
        virtual Wire_binding *wire_bind(Wire_user *user, std::string comp_name, std::string itf_name)
            { return NULL; }
    };



    /**
     * Class used to describe the power consumption.
     *
     * An instance of this class is reported when a power report is requested.
     * Each instance of this class describes a the power consumption of one block, including all
     * the blocks it contains.
     * To get a description of the hierarchy of blocks, the childs of the block can recursively
     * retrieved.
     */
    class PowerReport
    {
    public:
        /** The name of the clock in the hardware hierarchy. */
        std::string name;
        /** The overal power consumption. */
        double power;
        /** The dynamic power consumption, due to switching activity. */
        double dynamic_power;
        /** The static power consumption, due to leakage. */
        double static_power;

        /**
         * Return the childs of this block.
         *
         * This function allows exploring the hardware hierarchy and getting the power consumption
         * at each level.
         *
         * @return A vector of the power reports of the child blocks
         */
        virtual std::vector<PowerReport *> get_childs() { return std::vector<PowerReport *>(); }
    };



    /**
     * GVSOC interface for power aspects.
     *
     * Gather all the methods which can be called to query about the power consumption of the
     * simulated system.
     */
    class Power
    {
    public:
        /**
         * Get the overall instant power consumption.
         *
         * This gives the power consumption of the whole simulated system. The power is the instant
         * one at the time where the simulation stopped.
         * This is a fast method and can be called at every step if needed.
         *
         * @param dynamic_power The dynamic power, i.e. the one due to switching activity, is reported here.
         * @param static_power The static power, i.e. the one due to leakage, is reported here.
         *
         * @return The total instant power consumption.
         */
        virtual double get_instant_power(double &dynamic_power, double &static_power) { return 0; }

        /**
         * Get the overall average power consumption.
         *
         * This gives the power consumtion of the whole simulated system. The power is the average
         * one on the interval described by the call to report_start() and report_stop().
         *
         * @param dynamic_power The dynamic power, i.e. the one due to switching activity, is reported here.
         * @param static_power The static power, i.e. the one due to leakage, is reported here.
         *
         * @return The total average power consumption.
         */
        virtual double get_average_power(double &dynamic_power, double &static_power) { return 0; }

        /**
         * Start point of the period where average power is computed.
         *
         * This allows setting the beginning of the period where the power is measured to get the average.
         */
        virtual void report_start() {}

        /**
         * Stop point of the period where average power is computed.
         *
         * This allows setting the end of the period where the power is measured to get the average.
         */
        virtual void report_stop() {}

        /**
         * Get a detailed report of the average power consumption.
         *
         * The returned report allows getting the overall average power consumption over the period described by the
         * calls to report_start() and report_stop() of the whole simulated system.
         * Childs can then be recursively retrieved in order to get a detailed report of the whole hierarchy of the
         * simulated system.
         *
         * @return A pointer to the power report. The report is reused everytime this method is called and thus must
         * not be kept or freed.
         */
        virtual PowerReport *report_get() { return NULL; }

    };



    /**
     * Class required for receiving GVSOC events.
     *
     * When the external C++ connects to GVSOC, it must implement all the methods of this class
     * to properly get notified about GVSOC events.
     */
    class Gvsoc_user
    {
    public:
        /**
         * Called by GVSOC to notify the simulation has ended.
         *
         * This means the simulated software is over and probably exited and GVSOC cannnot further
         * simulate it.
         *
         * @param status The simulation termination status.
         */
        virtual void has_ended(int status) {}

        /**
         * Called by GVSOC to notify the simulation has stopped.
         *
         * This means the an event occurs which stopped the simulation. Simulation can be still be
         * resumed.
         */
        virtual void has_stopped() {}

        /**
         * Called by GVSOC to notify the simulation engine was updated.
         *
         * This means a new event was posted to the engine and modified the timestamp of the next
         * event to be executed.
         */
        virtual void was_updated() {}

        /**
         * Called by GVSOC to notify and end of step.
         *
         * This notifies the termination of an asynchronous step enqueued with step_request.
         * The data provided during the request is given back to identify which step has finished.
         *
         * @param status The request data.
         */
        virtual void handle_step_end(void *data) {}

        /**
         * Called by GVSOC to notify a stop syscall.
         *
         * This is triggered when the simulation executes a semi-hosting stop call.
         * This cam be used to implement synchronization between the simulated software and the
         * client.
         */
        virtual void handle_syscall_stop() {}
    };

    /**
     * Description of the last memory-checker fault, for front-ends (e.g. the GUI
     * fault panel). Filled by get_memcheck_fault() from the engine's registry.
     */
    class MemcheckFault
    {
    public:
        bool valid = false;
        // Engine time of the faulting access
        int64_t time = 0;
        // Component path of the faulting core, faulting instruction
        std::string core;
        uint64_t pc = 0;
        // Access description
        uint64_t addr = 0;
        uint8_t size = 0;
        bool is_write = false;
        // overflow / underflow / use-after-free / cross-region / uninit-branch /
        // uninit-address
        std::string kind;
        // Full formatted report line
        std::string message;
        // Faulty buffer, 0 for uninitialized-value faults; the fields below are
        // only meaningful when buffer_id != 0
        uint32_t buffer_id = 0;
        std::string region;
        uint64_t buffer_base = 0;
        uint64_t buffer_size = 0;
        uint64_t alloc_pc = 0;
        uint64_t alloc_ra = 0;
        bool freed = false;
        uint64_t free_pc = 0;
        uint64_t free_ra = 0;
    };

    /**
     * GVSOC interface
     *
     * Gather all the methods which can be called to control GVSOC execution and other features
     */
    class Gvsoc : public Io, public Vcd, public Wire, public Power, public Stdout
    {
    public:

        virtual ~Gvsoc() {}

        /**
         * Open a GVSOC configuration
         *
         * This instantiates the system described in the specified configuration file.
         * Once this operation is done, some bindings can be created to interact with this system
         * and GVSOC execution can be started.
         * Can only be called by the main controller.
         *
         * @param conf The configuration describing how to open GVSOC.
         */
        virtual void open() {}

        /**
         * Bind external C++ code to GVSOC
         *
         * @param user A pointer to the caller class instance which will be called for all GVSOC events. This caller
         *             must implement all the methods defined in class Gvsoc_user
         */
        virtual void bind(Gvsoc_user *user) {}

        /**
         * Close a GVSOC configuration
         *
         * This closes the currently opened configuration so that another one can be
         * opened.
         * Can only be called by the main controller.
         *
         */
        virtual void close() {}

        /**
         * Start the simulated system
         *
         * This executes all the required steps so that the simulated system is ready to execute.
         * Can only be called by the main controller.
         *
         */
        virtual void start() {}

        /**
         * Run execution
         *
         * Set the client as runnable.
         * Simulation will be run only when other clients are runnable and no one locked
         * the engine.
         * When simulation is run, it runs until the end of simulation is reached or any client
         * is not runnable anymore or locks the engine.
         * In asynchronous mode, this call immediately returns, even if the simulation can not
         * be run due to other clients, because the simulation execution is managed by a dedicated
         * thread. The engine will just take into account that the client is runnable.
         * In synchronous mode, this waits until the simulation can be run and returns only when the
         * engine is stopped.
         */
        virtual void run() {}

        /**
         * Stop execution
         *
         * Sets the client as stopped.
         * If the simulation was running, this stops it when there is no more event to execute in
         * the current timestamp.
         * No event is then executed until all clients become runnable and no one locked the engine.
         *
         * @returns The timestamp where the execution stopped.
         */
        virtual int64_t stop() { return 0; }

        /**
         * Force simulation termination
         *
         * This can be called to force simulation termination in case it does not terminate by
         * itself to avoid blocking on join call.
         * By default, if not called, the join call will block until simulation terminates
         * by itself.
         */
        virtual void terminate() {}

        /**
         * Quit
         *
         * Make this client quit.
         * The simulation can still continue if other clients are connected.
         * After this call, this client can no longer prevent the simulation from running.
         * The specified status will be taken into account to generate the final status gathered
         * from simulation status and other client status.
         *
         * @param timestamp The current time to be set in the engine.
         */
        virtual void quit(int status) {}

        /**
         * Update the engine current time
         *
         * This can be called to update the current time of the engine in order to synchronize
         * it with the external engine.
         * All events with a lower timestamp must have been executed first.
         *
         * @param timestamp The current time to be set in the engine.
         *
         */
        virtual void update(int64_t timestamp) {}

        /**
         * Step execution for specified duration
         *
         * Make the client runnable and make it stopped when the specified duration has been
         * simulated. This is equivalent to calling run and stop after the specified duration.
         * Once the duration has been simulated, simulation stops as this client is not runnable
         * anymore.
         * A stop request will not stop this call.
         * The step can be shorter if the end of simulation is reached, which aborts the step.
         * This calls immediately returns.
         * In asynchronous mode, this call is asynchronous and will generate a call to
         * handle_step_end once the step is done. A data can be given to get it back when
         * handle_step_end is called.
         * In synchronous mode, this call is blocking and returns once the step is done.
         *
         * @param duration The amount of time in picoseconds during which GVSOC should execute.
         * @param wait True if the call must be blocking. Only relevant in asynchronous mode.
         * @param data The data to be given back when handle_step_end is called. Only relevant in
         *             asynchronous mode.
         */
        virtual void step(int64_t duration, bool wait=false, void *data=NULL) {}

        /**
         * Step execution for specified timestamp
         *
         * Make the client runnable and make it stopped when the specified timestamp has been
         * reached. This is equivalent to calling run and stop after the specified timestamp.
         * Once the timestamp has been reached, simulation stops as this client is not runnable
         * anymore.
         * A stop request will not stop this call.
         * The step can be shorter if the end of simulation is reached, which aborts the step.
         * This calls immediately returns.
         * In asynchronous mode, this call is asynchronous and will generate a call to
         * handle_step_end once the step is done. A data can be given to get it back when
         * handle_step_end is called.
         * In synchronous mode, this call is blocking and returns once the step is done.
         *
         * @param timestamp The timestamp in picoseconds until which GVSOC should execute.
         */
        virtual void step_until(int64_t timestamp, bool wait=false, void *data=NULL) {}

        /**
         * Wait end of execution and end of clients.
         *
         * This blocks the caller until the simulation has finished and all connected client has
         * quit.
         * It returns 0 if the simulation and all clients has returned 0, otherwise it returns 1.
         *
         * @returns The return status of the simulation and the clients.
         */
        virtual int join() { return 0; }

        /**
         * Flush internal data.
         *
         * This can be useful when the simulation reaches an interaction point to flush
         * all internal data, so that they can be visible to the user.
         * This can be used for example when the profiler is pausing the simulation
         * so that it gets all pending events.
         */
        virtual void flush() {}

        /**
         * Get a component.
         *
         * This goes trough the simulated architecture to find the component
         * with the specified path and returns the descriptor of the component.
         *
         * @param path The path of the component.
         *
         * @returns The descriptor of the component if it was found or NULL otherwise.
         */
        virtual void *get_component(std::string path) { return NULL; }

        /**
         * Lock the engine
         *
         * This stops the engine and make sure no client is accessing the engine or the models.
         * This can be used when directly interacting with the models to avoid any race condition.
         * Since the cost is quite high, this should be avoided as much as possible.
         */
        virtual void lock() {}

        /**
         * Unlock the engine
         */
        virtual void unlock() {}

        /**
         * Wait until simulation can be run
         *
         * This blocks the caller until the simulation can be run. This means that no client
         * has stopped or locked the engine.
         * This must only be used in synchronous mode from the main controller.
         */
        virtual void wait_runnable() {}

        /**
         * Get current time
         *
         * This returns the time where the last event was executed.
         *
         * @returns The current time.
         */
        virtual int64_t get_time() { return -1; }

        /**
         * Get the next event timestamp
         *
         * This returns the timestamp of the first event which will get executed the next time
         * the engine is run.
         * This can be useful when synchronizing GVSOC engine with an external engine.
         *
         * @returns The timestamp of the next event to be executed or -1 if there is no event.
         */
        virtual int64_t get_next_event_time() { return -1; }

        /**
         * Restart the simulated system
         *
         * This tears down the whole simulated system and instantiates it again from the same
         * configuration, as if GVSOC had just been opened and started: all components are
         * destroyed and re-created (so their state is back to power-on), time is back to 0 and
         * the simulation is left stopped, waiting for run() to be called.
         * The caller must have stopped the simulation first (see stop()).
         * Bindings registered with bind() and vcd_bind() are re-applied to the new system, and
         * trace events are declared/registered again to the VCD user with the same paths.
         * Dynamic event subscriptions (event_subscribe) are lost and must be re-issued by the
         * caller. Any component handle obtained with get_component(), io_bind() or wire_bind()
         * is invalidated.
         * Restart is refused (with an error message) on SystemC-enabled platforms (SystemC time
         * cannot be rewound in-process) and when the proxy is enabled.
         * Can only be called by the main controller.
         *
         * NOTE: declared last on purpose so that the vtable slots of the existing entries are
         * unchanged; only this new slot is added. GV_API_VERSION is bumped accordingly so a
         * consumer built against this header refuses to run against an engine that predates
         * restart() (and would have no slot for it) rather than calling into a missing entry.
         */
        virtual void restart() {}

        /**
         * Get the last memory-checker fault, if any.
         *
         * Returns true and fills `out` when the memory checker has recorded a
         * fault (e.g. a buffer overflow that paused the simulation); false
         * otherwise. Used by front-ends to display the fault and navigate to it.
         *
         * NOTE: declared last on purpose so the vtable slots of the existing
         * entries are unchanged; only this new slot is added. GV_API_VERSION is
         * bumped accordingly.
         */
        virtual bool get_memcheck_fault(MemcheckFault &out) { return false; }
    };


    /**
     * GVSOC configuration
     *
     * This is used to describe the way GVSOC should be configured when it is instantiated.
     * This class is instantiated with default values which can be overriden to
     * specify the desired configuration.
     */
    class GvsocConf
    {
    public:
        /**
         * Path to the GVSOC configuration
         *
         * When specified, GVSOC is instantiated and will simulated the specified
         * configuration.
         */
        std::string config_path = "";

        /**
         * Path to the per-run runtime config values
         *
         * Key/value file regenerated by gvrun at every invocation, carrying the
         * values of the config fields marked runtime so that they can change
         * without rebuilding the platform tree. Empty means no runtime values.
         */
        std::string runtime_config_path = "";

        /**
         * Socket of a GVSOC proxy.
         *
         * This can be used to connect to a GVSOC instance which has been launched separately
         * with the proxy enabled. This instance will just act as a client interacting with the
         * already running GVSOC instance.
         */
        int proxy_socket = -1;

        /**
         * Open an in-process proxy for an in-process host (e.g. the GUI).
         *
         * When set, the engine opens a proxy endpoint so external clients (e.g. gvconsole, or the
         * GUI's command console) can drive the simulation. The host (this in-process embedder)
         * keeps running the engine continuously through the gvsoc API; each proxy session owns a
         * control client that AND-gates the engine against the host, so a session run/stop starts
         * and parks it. An ephemeral port and no startup wait are implied for an in-process host.
         * The opened port is returned in proxy_socket. This is an alternative to enabling the proxy
         * through the platform configuration ("proxy/enabled").
         */
        bool proxy_enabled = false;

        /**
         * API mode.
         *
         * This can be used to control how the commands are handled by the engine.
         * In asynchronous mode, the engine runs in a separated thread and most of the commands are
         * just posted and executed in background, so that the caller can get back control
         * immediately.
         * In synchronous mode, the engine runs in the thread of the caller, and all commands
         * are executed within the function call of the caller.
         */
        Api_mode api_mode = Api_mode_async;
    };

    /**
     * Instantiate GVSOC client
     *
     * This has to be called to instantiate an object representing GVSOC which can
     * then be used to control GVSOC execution and other features like bindings.
     * Note that it instantiates a client which connects to GVSOC and interact with it.
     * Several clients can be instantiated to control GVSOC from different actors.
     * The first instantiated client is called the main controller while others are secondary
     * controllers.
     * Only the main controller can open/close GVSOC, the others can only interact with the
     * simulation execution.
     *
     * @returns A pointer to the allocated GVSOC objet.
     */
    Gvsoc *gvsoc_new(GvsocConf *conf, std::string name="main");

}
