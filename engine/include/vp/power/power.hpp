/*
 * Copyright (C) 2020 GreenWaves Technologies, SAS, ETH Zurich and
 *                    University of Bologna
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

/*
 * Authors: Germain Haugou, GreenWaves Technologies (germain.haugou@greenwaves-technologies.com)
 */

#pragma once

#include "vp/json.hpp"
#include "vp/vp_data.hpp"

/**
 * @brief Power framework
 * 
 * Power is modeled through 2 classes, one for declaring sources of power consumptions
 * and another one for collecting consumed power from sources.
 * A note on method visibility: all the public functions are there for the HW models
 * to model HW power consumption while protected methods are there for the classes
 * belonging to the framework, to manage the overall power framework.
 * 
 */



namespace vp
{

    class CompPowerReport : public gv::PowerReport
    {
    public:
        CompPowerReport(vp::Block &top);
        std::vector<gv::PowerReport *> get_childs();

    private:
        void compute();
        vp::Block &top;
        std::vector<CompPowerReport *> childs;
    };

#define VP_POWER_DEFAULT_TEMP 25
#define VP_POWER_DEFAULT_VOLT 1.2
#define VP_POWER_DEFAULT_FREQ 50000000

    class PowerLinearTable;
    class PowerEngine;
    class PowerSource;
    class PowerTrace;
    class BlockPower;

    enum PowerSupplyState
    {
        OFF=0,
        ON_CLOCK_GATED=2,
        ON=1
    };

    /**
     * @brief Used to model a power source
     *
     * A power source is a piece of hardware which is consuming energy.
     * This class can be instantiated any time as needed to model all the ways
     * an IP is consuming energy.
     * It can be used for dynamic or leakage power, or for both at the same time, since
     * they are accounted separatly.
     * For dynamic power it can be used either as a quantum-based power source or
     * as background power source.
     * A quantum-based power source will consume energy only when an event is triggered.
     * A background power source will constantly consume power as soon as it is on.
     * A leakage power source is similar to a background power source, but is just accounted
     * differently in the power report.
     */
    class PowerSource
    {
        friend class vp::BlockPower;

    public:
        /**
         * @brief Account the the current energy quantum
         * 
         * This should be used in quantum-based power source to trigger the consumption
         * of a quantum of energy.
         * The accounted quantum is the current one, estimated from the current temperature
         * and voltage.
         * This will just add the quantum of energy to the current consumed energy.
         */
        inline void account_energy_quantum();

        /**
         * @brief Start accounting background power
         * 
         * This should be used for a background dynamic power to start accounting the associated power.
         * The power is accounted until the background power is stopped.
         * The accounted power is the current one, estimated from the current temperature
         * and voltage.
         * This is actually converted to energy and accounted on the total of energy
         * anytime current power is changed or power is stopped.
         */
        inline void dynamic_power_start();

        /**
         * @brief Stop accounting background power
         * 
         * This will trigger the accounting of energy for the current windows
         * and stop accounting power until it is started again.
         */
        inline void dynamic_power_stop();

        /**
         * @brief Start accounting leakage
         * 
         * This should be used for leakage to start accounting the associated power.
         * The power is accounted until the leakage is stopped.
         * The accounted leakage is the current one, estimated from the current temperature
         * and voltage.
         * This is actually converted to energy and accounted on the total of energy
         * anytime current power is changed or leakage is stopped.
         */
        inline void leakage_power_start();

        /**
         * @brief Stop accounting leakage
         * 
         * This will trigger the accounting of energy for the current windows
         * and stop accounting leakage until it is started again.
         */
        inline void leakage_power_stop();

    protected:
        /**
         * @brief Initialize a power source
         *
         * @param top Component containing the power source.
         * @param name Name of the power source, used in traces.
         * @param config Configuration of the power source, giving power numbers.
         * @param trace Power trace where this source should account power consumed.
         */
        int init(Block *top, std::string name, js::Config *config, PowerTrace *trace);
        
        /**
         * @brief Set temperature, voltage and frequency
         * 
         * The power source will adapt its power number according to the given characteristics.
         *
         * @param temp Temperature
         * @param volt Voltage
         * @param freq Frequency
         */
        void setup(double temp, double volt, double freq);

        void set_frequency(double freq);

        void set_voltage(double voltage);

        /**
         * @brief Turn on a power source
         *
         * This power source should be turned on when its power domain is turned on, in order to start consuming power
         */
        void turn_on();

        /**
         * @brief Turn off a power source
         *
         * This power source should be turned off when its power domain is turned off, in order to stop consuming power
         */
        void turn_off();

    private:
        void check();

        PowerLinearTable *dyn_table = NULL;  // Table of power values for all supported temperatures and voltages
                                    // imported from the json configuration given when trace was initialized.
        PowerLinearTable *leakage_table = NULL;  // Table of power values for all supported temperatures and voltages
                                    // imported from the json configuration given when trace was initialized.
        double quantum;          // Current quantumm of energy, for quantum-based power consumption.
                                    // The current value is estimated depending on voltage and temperature according
                                    // to the provided json configuration.
        double background_power; // Current background power, for background-based power consumption.
                                    // The current value is estimated depending on voltage and temperature according
                                    // to the provided json configuration.
        double leakage;          // Current leakage power, for leakage-based power consumption.
                                    // The current value is estimated depending on voltage and temperature according
                                    // to the provided json configuration.
        Block *top;          // Top component containing the power source
        PowerTrace *trace;      // Power trace where the power consumption should be reported.
        bool is_dynamic_power_started = false;      // True is the source consuming dynamic backgroun power
        bool is_leakage_power_started = false;      // True is the source should start consuming leakage power
        bool is_on = true;      // True is the power domain containing the power source is on and backgroun-power and leakage should be reported
        bool dynamic_power_is_on_sync = false;
        bool leakage_power_is_on_sync = false;
        double current_temp;
        double current_volt;
        double current_freq;
    };


    /**
     * @brief Used for tracing power consumption
     * 
     * This class can be used to gather the power consumption of several power sources and
     * trace it through VCD traces and power reports.
     * Each power source must be associated to a power trace and a power trace can be associated
     * to one ore more power traces.
     * A power trace is automatically created with each component and is the default trace associated
     * to any power source created in this component. Additional traces can also be created
     * to give finer granularity to the power consumption in the reports.
     */
    class PowerTrace
    {
        friend class vp::PowerSource;
        friend class vp::PowerEngine;
        friend class vp::BlockPower;

    public:
        /**
         * @brief Init the trace
         *
         * @param top    Component containing the power trace.
         * @param name   Name of the power trace. It will be used in traces and in the power report
         * @param parent Optional. Trace parent of this trace. Can be NULL to take the default
         *               one of the parent component. Power consumption of this trace will also
         *               be accounted to the parent to have a hierarchical view of the power
         *               consumption.
         */
        int init(Block *top, std::string name, PowerTrace *parent=NULL);

        /**
         * @brief Return if the trace is enabled
         * 
         * @return true if the trace is active and should account power
         * @return false if the trace is inactive and any activity should be ignored
         */
        inline bool get_active() { return trace.get_event_active(); }

        /**
         * @brief Dump the trace
         *
         * This allowss dumping the power consumption of this trace to a file.
         * This also dumps the consumption of the traces of the child components
         * to get the hierarchical distribution.
         *
         * @param file File descriptor where the trace should.
         */
        void dump(FILE *file);

    protected:

        /**
         * @brief Start monitoring power for report generation.
         *
         * This will clear all pending power values so that a new report starts
         * to be generating within a window which start when this method is
         * called.
         * It can be called several time to generate several reports, each one
         * on a different time window.
         *
         * @param file File descriptor where the trace should.
         */
        void report_start();

        /**
         * @brief Report the average power consumed on the active window.
         *
         * The time duration of the report window (starting at the time where
         * report_start was called) is computed, the average power estimated
         * on this window and returned (both dynamic and leakage).
         *
         * @param dynamic Dynamic power consumed on the time window is reported here.
         * @param leakage Leakage power consumed on the time window is reported here.
         */
        void get_report_power(double *dynamic, double *leakage);

        /**
         * @brief Report the energy consumed on the active window.
         *
         * The time duration of the report window (starting at the time where
         * report_start was called) is computed, the total amount of energy consumed
         * estimated on this window and returned (both dynamic and leakage).
         *
         * @param dynamic Dynamic power consumed on the time window is reported here.
         * @param leakage Leakage power consumed on the time window is reported here.
         */
        void get_report_energy(double *dynamic, double *leakage);

        /**
         * @brief Report the current instant power.
         *
         * Report the sum of background dynamic power, leakage power, and
         * power corresponding to the dynamic energy consumed in the current cycle.
         * For what concerns the energy, since this is based on events, this can grow
         * several times in the same cycle. So it is important to call this function
         * everytime the energy in the cycle is increased, to get the proper power at the
         * end of the cycle.
         * 
         * @return The current instant power
         */
        inline double get_power();

        /**
         * @brief Increment the current dynamic background power.
         *
         * The dynamic background power is incremented by the specified value
         * to reflect a change in power consumption.
         * Note that this power is consumed constantly until it is modified again.
         * This can be used to model some power consumed in background activity
         * (e.g. a core in idle mode).
         *
         * @param power_inc Power increase. Can be negative to reflect a decrease.
         */
        void inc_dynamic_power(double power_inc);

        /**
         * @brief Increment the current leakage power.
         *
         * The leakage power is incremented by the specified value
         * to reflect a change in power consumption.
         * Note that this power is consumed constantly until it is modified again.
         * This can be used to model the leakage power which is consumed in background
         * as soon as an IP is on.
         *
         * @param power_inc Power increase. Can be negative to reflect a decrease.
         */
        void inc_leakage_power(double power_incr);

        /**
         * @brief Increment the current energy consumed.
         *
         * The amount of energy consumed is increased by the specified amount of
         * energy (in joule) to reflect that an event occured and consumed some
         * energy (like a memory access).
         *
         * @param power_inc Power increase. Can be negative to reflect a decrease.
         */
        void inc_dynamic_energy(double energy);

    private:
        // Regularly, current power consumption is converted into energy and added
        // to the total amount of energy consumed, for example when the current power
        // consumption is modified.
        // Calling this function will do this conversion for the dynamic part of the
        // power consumed.
        void account_dynamic_power();

        // Regularly, current power consumption is converted into energy and added
        // to the total amount of energy consumed, for example when the current power
        // consumption is modified.
        // Calling this function will do this conversion for the leakage part of the
        // power consumed.
        void account_leakage_power();

        // Check if the current amount of power due to quantum of energies
        // is not for the current cycle
        // (by checking the timestamp), and if not, reset it to zero.
        inline void flush_quantum_power_for_cycle();

        // Get the average power of the current cycle due to quantums of energy
        inline double get_quantum_power_for_cycle();

        // Get the energy spent in the current cycle due to quantums of energy
        inline double get_quantum_energy_for_cycle();

        // Return the total amount of dynamic energy spent since the beginning
        // of the report windows (since report_start was called)
        inline double get_report_dynamic_energy();

        // Return the total amount of leakage energy spent since the beginning
        // of the report windows (since report_start was called)
        inline double get_report_leakage_energy();

        // Dump VCD trace reporting the power consumption
        // This should be called everytime the energy consumed in the current cycle
        // or the current background or leakage power is modified.
        void dump_vcd_trace();

        // Event handler called after an energy quantum has been accounted
        // in order to dump the new value of the vcd trace since the quantum
        // has to be removed from the vcd value in the next cycle
        static void trace_handler(vp::Block *__this, vp::ClockEvent *event);


        Block *top;                   // Component containing this power trace
        PowerTrace *parent;              // Parent trace where power consumption should be
                                            // also be accounted to build the hierarchical view.
        vp::ClockEvent *trace_event;     // Clock event used to adjust VCD trace value after
                                            // energy quantum has been accounted.
        vp::Trace trace;                  // Trace used for reporting power in VCD traces
        vp::Trace dyn_trace;              // Trace used for reporting dynamic power in VCD traces
        vp::Trace static_trace;           // Trace used for reporting static power in VCD traces

        int64_t curent_cycle_timestamp;   // Timestamp of the current cycle, used to compute energy spent in the
                                            // current cycle. As soon as current time is different, the timestamp
                                            // is set to current time and the current energy is set to 0.
        double quantum_power_for_cycle;  // Power spent by quentum of energy in the current cycle.
                                            // It is increased everytime a quantum of energy is
                                            // spent and reset to zero when the current cycle is
                                            // over. It is mostly used to compute the instant power
                                            // displayed in VCD traces.

        int64_t report_start_timestamp;   // Time where the current report window was started.
                                            // It is used to compute the average power when the report is dumped
        double report_dynamic_energy;     // Total amount of dynamic energy spent since the report was started
        double report_leakage_energy;     // Total amount of leakage energy spent since the report was started

        int64_t current_dynamic_power_timestamp; // Indicate the timestamp of the last time the background energy
                                                    // was accounted. This is used everytime background power is
                                                    // updated or dumped to compute the energy spent over the period.
        double current_dynamic_power;            // Current dynamic background power. This is used to account the
                                                    // energy over the period being measured. Everytime it is updated,
                                                    // the energy should be computed and the timestamp updated.

        int64_t current_leakage_power_timestamp; // Indicate the timestamp of the last time the leakage energy
                                                    // was accounted. This is used everytime leakage power is
                                                    // updated or dumped to compute the energy spent over the period.
        double current_leakage_power;            // Current leakage power. This is used to account the
                                                    // energy over the period being measured. Everytime it is updated,
                                                    // the energy should be computed and the timestamp updated.

        double current_power;    // Instant power of the current cycle. This is updated everytime
                                    // background or leakage power is updated and also when a quantum of energy is 
                                    // accounted, in order to proerly update the VCD trace

        double instant_dynamic_power;// Instant dynamic power of the current cycle. This is updated everytime
                                    // background or leakage power is updated and also when a quantum of energy is 
                                    // accounted, in order to proerly update the VCD trace

        double instant_static_power;// Instant static power of the current cycle. This is updated everytime
                                    // background or leakage power is updated and also when a quantum of energy is 
                                    // accounted, in order to proerly update the VCD trace
    };




    /**
     * @brief Class for power engine
     * 
     * The engine is a central object used for managing power modeling.
     */
    class PowerEngine
    {
        friend class vp::BlockPower;

    public:
        /**
         * @brief Construct a new engine object
         * 
         * @param top Top component of teh simulated system.
         */
        PowerEngine();

        ~PowerEngine();

        void init(vp::Block *top);

        /**
         * @brief Start power report generation
         * 
         * This will start accounting power consumption for dumping the power report.
         */
        void start_capture();

        /**
         * @brief Dump power report
         * 
         * This dumps a report of the power consumed since start_capture was called.
         */
        void stop_capture();

        double get_average_power(double &dynamic_power, double &static_power);

    protected:
        /**
         * @brief Register a new trace
         * 
         * Any power trace has to be registered in the engine by calling this method.
         * 
         * @param trace Trace to be registered.
         */
        void reg_trace(vp::PowerTrace *trace);

    private:
        std::vector<vp::PowerTrace *> traces; // Vector of all traces.

        vp::Block *top;  // Top component of the simulated architecture

        FILE *file; // File where the power reports are dumped
    };

};

