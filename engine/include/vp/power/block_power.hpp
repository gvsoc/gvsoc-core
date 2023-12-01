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

#include <vector>
#include <vp/power/power.hpp>

namespace vp
{
    /**
     * @brief Class gathering all power aspects of a component
     *
     * Each component should include an object of this class to include power modeling fetures.
     * A model can use this object to model power consumption.
     */
    class BlockPower
    {
        friend class PowerTrace;
        friend class CompPowerReport;
        friend class vp::Component;
        friend class vp::Block;

    public:
        /**
         * @brief Construct a new component power object
         *
         * @param top Component containing this object.
         */
        BlockPower(vp::Block *parent, Block &top, vp::PowerEngine *engine);

        /**
         * @brief Get the power engine
         *
         *
         * @return PowerEngine* Return the central power engine
         */
        inline PowerEngine *get_engine();

        /**
         * @brief Do all required initializations
         *
         */
        void build();

        /**
         * @brief Declare a new power source
         *
         * Power source can be used to report consumption.
         * One power source can used to account both dynamic and leakage power,
         * but any number of power sources can also be used to better organize
         * the modeling of the power consumption.
         *
         * @param name   Name of the power source.
         * @param source Power source to be declared.
         * @param config JSON configuration of the power source giving the power numbers
         * @param trace  Power trace where the power consumption of this source should be reported.
         * @return int   0 if it was successfully declared, -1 otherwise.
         */
        int new_power_source(std::string name, PowerSource *source, js::Config *config, PowerTrace *trace=NULL);

        /**
         * @brief Declare a new power trace
         *
         * Power traces are used to account and report power consumption.
         * A default trace is associated to each component but more traces can be
         * declared to better organize power consumption modeling/
         *
         * @param name   Name of the power trace
         * @param trace  Power trace to be declared
         * @param parent Optional. Parent trace. Can be NULL to take default parent trace.
         *               Can be use to organize differently the hierarchy of traces for VCD dumping.
         * @return int
         */
        int new_power_trace(std::string name, PowerTrace *trace, vp::PowerTrace *parent=NULL);

        /**
         * @brief Get the default power trace
         *
         * Get the default power trace associated to this component.
         * All power sources have by default this power trace as parent.
         *
         * @return vp::PowerTrace* The default power trace
         */
        vp::PowerTrace *get_power_trace() { return &this->power_trace; }

        /**
         * @brief Set power supply state
         *
         * This sets the power supply for this component and all his childs.
         *
         * @param state Supply state
         */
        void power_supply_set_all(vp::PowerSupplyState state);

        void voltage_set_all(int voltage);

        void set_frequency(int64_t frequency);

        double get_average_power(double &dynamic_power, double &static_power);

        double get_instant_power(double &dynamic_power, double &static_power);

        gv::PowerReport *get_report() { return &this->report; }

        virtual void dump_traces(FILE *file);

        void dump_traces_recursive(FILE *file);

    protected:
        /**
         * @brief Get the report energy from childs object
         *
         * Get the total amount of energy since the begining of the current report window
         * (since report_start was called).
         *
         * @param dynamic Report dynamic energy here
         * @param leakage Report leakage energy here
         */
        void get_report_energy_from_childs(double *dynamic, double *leakage);

        /**
         * @brief Get the instant power from childs
         *
         * This returns the instant power of the whole power hierarchy below this component.
         *
         * @return double Instant power
         */
        double get_power_from_childs();

        /**
         * @brief Dump component power traces
         *
         * All the power traces of this component are dumped to the specified file.
         *
         * @param file  File where to dump the trace
         * @param total Total power used to compute trace contribution
         */
        void dump(FILE *file, double total);

        /**
         * @brief Dump power traces of child components
         *
         * Each component child is asked to dump its power traces to the specified file.
         *
         * @param file  File where to dump the trace
         * @param total Total power used to compute trace contribution
         */
        void dump_child_traces(FILE *file, double total);

    private:
        // Get energy since begining of report window for this component and his childs
        void get_report_energy_from_self_and_childs(double *dynamic, double *leakage);

        // Get instant power for this component and the whole hierarchy below him.
        double get_power_from_self_and_childs();

        Block &top;                                // Component containing the power component object
        vp::PowerTrace power_trace;            // Default power trace of this component
        std::vector<vp::PowerTrace *> traces;  // Vector of power traces of this component
        std::vector<vp::PowerSource *> sources;  // Vector of power sources of this component
        CompPowerReport report;
        PowerEngine *engine = NULL;
    };
};


inline vp::PowerEngine *vp::BlockPower::get_engine() { return this->engine; }