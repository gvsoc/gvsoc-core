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
        Vcd_event_type_flame_chart
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
         * Called by GVSOC to register a new VCD event.
         *
         * This call is used by GVSOC to identify each VCD event by an integer and to give details
         * about the associated signal.
         * No GVSOC API can be called from this callback.
         *
         * @param path The path of the VCD event in the simulated system.
         * @param type The type of the VCD event.
         * @param width The width of the VCD event.
         * @param clock_path If any, path of the clock trace.
         *
         * @return The trace identifier, to be used to refer to this trace when an event is pushed.
         */
        virtual void *event_register(std::string path, Vcd_event_type type, int width,
            std::string clock_path="") { return NULL; }

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
         * Enable VCD events
         *
         * This will add the specified list of events to the included ones.
         *
         * @param path The path of the VCD events to enable.
         * @param is_regex True if the specified path is a regular expression.
         */
        virtual void event_add(std::string path, bool is_regex=false) {}

        /**
         * Disable VCD events
         *
         * This will add the specified list of events to the excluded ones.
         *
         * @param path The path of the VCD events to disable.
         * @param is_regex True if the specified path is a regular expression.
         */
        virtual void event_exclude(std::string path, bool is_regex=false) {}
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
     * GVSOC interface
     *
     * Gather all the methods which can be called to control GVSOC execution and other features
     */
    class Gvsoc : public Io, public Vcd, public Wire, public Power
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
         * Socket of a GVSOC proxy.
         *
         * This can be used to connect to a GVSOC instance which has been launched separately
         * with the proxy enabled. This instance will just act as a client interacting with the
         * already running GVSOC instance.
         */
        int proxy_socket = -1;

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
