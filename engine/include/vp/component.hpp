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

#include <map>
#include <string>
#include <vp/json.hpp>
#include <vp/block.hpp>
#include <vp/itf/clk.hpp>
#include <vp/itf/implem/wire_class.hpp>

#define likely(x) __builtin_expect(x, 1)
#define unlikely(x) __builtin_expect(x, 0)


namespace gv {
    class GvProxy;
    class GvsocLauncher;
};

namespace vp
{

    class config;
    class ClockEngine;
    class Component;
    class signal;
    class TraceEngine;
    class Top;
    class reg_1;
    class reg_8;
    class reg_16;
    class reg_32;
    class reg_64;

    class ComponentConf
    {
    public:
        ComponentConf(std::string name, vp::Component *parent, js::Config *config, js::Config *gv_config,
            vp::TimeEngine *time_engine, vp::TraceEngine *trace_engine,
            vp::PowerEngine *power_engine, vp::MemCheck *memcheck)
            : name(name), parent(parent), config(config), gv_config(gv_config),
            time_engine(time_engine), trace_engine(trace_engine), power_engine(power_engine),
            memcheck(memcheck) {}
        std::string name;
        vp::Component *parent;
        js::Config *config;
        js::Config *gv_config;
        vp::TimeEngine *time_engine;
        vp::TraceEngine *trace_engine;
        vp::PowerEngine *power_engine;
        vp::MemCheck *memcheck;
    };

    /**
     * @brief Component model
     *
     * The component is the basic building block of the GVSOC component-based model.
     * It is supposed to be a coarse-grain model of a piece of hardware.
     * It inherits from the block model and adds on top of it all what is needed to be
     * able to make it interact with other components through interfaces and bindings
     * defined together with the Python generator.
     */
    class Component : public Block
    {

        friend class vp::BlockClock;
        friend class vp::BlockPower;
        friend class vp::PowerSource;
        friend class vp::MasterPort;
        friend class vp::Top;
        friend class vp::TimeEngine;
        friend class gv::GvsocLauncher;

    public:
        /**
         * @brief Construct a new component
         *
         * @note Components are instantiated by the framework, based on what was generated
         * by the python generators. They should not be instantiated by the models directly.
         *
         * @param config The component configuration.
         */
        Component(vp::ComponentConf &config);

        /**
         * @brief Declare a master port
         *
         * A master port can be created to let the upper component bind it to the slave port of
         * another component.
         * Although ports are used to connect components together, it is also possible to connect
         * them to a sub-block of a component by specifying the block instance.
         *
         * @note A port must have a unique port name within the component scope. A master port can
         * not have the same name as a slave port.
         *
         * @param name Name of the port in the component scope
         * @param port Pointer to the master port
         * @param comp Instance owning the port. The handler will be called with this instance as
         *     first argument. If it is NULL, the instance will be the component where the port
         *     is created.
         */
        void new_master_port(std::string name, vp::MasterPort *port, vp::Block *comp=NULL);

        /**
         * @brief Declare a slave port
         *
         * A slave port can be created to let the upper component bind the master port of
         * another component to it.
         * Although ports are used to connect components together, it is also possible to connect
         * them to a sub-block of a component by specifying the block instance.
         *
         * @note A port must have a unique port name within the component scope. A master port can
         * not have the same name as a slave port.
         *
         * @param name Name of the port in the component scope
         * @param port Pointer to the slave port
         * @param comp Instance owning the port. The handler will be called with this instance as
         *     first argument. If it is NULL, the instance will be the component where the port
         *     is created.
         */
        void new_slave_port(std::string name, SlavePort *port, void *comp=NULL);

        /**
         * @brief Get the component JSON configuration
         *
         * Each component is associated a JSON configuration which was generated from the
         * properties specified by this component Python generator.
         * This configuration can be used to get the properties values associated to this instance
         * to take them into account in the model.
         *
         * @return The JSON configuration of the component
         */
        inline js::Config *get_js_config() { return js_config; }

        /**
         * @brief Get the launcher
         *
         * The launcher is the top class controlling the GVSOC execution.
         * It can be retrieved in order to control the simulation at high-level point of view,
         * for example to pause it.
         *
         * @return The launcher
         */
        gv::GvsocLauncher *get_launcher();


        /**
         * @brief DEPRECATED
        */
        void new_reg(std::string name, vp::reg_1 *reg, uint8_t reset_val, bool reset = true);

        /**
         * @brief DEPRECATED
        */
        void new_reg(std::string name, vp::reg_8 *reg, uint8_t reset_val, bool reset = true);

        /**
         * @brief DEPRECATED
        */
        void new_reg(std::string name, vp::reg_16 *reg, uint16_t reset_val, bool reset = true);

        /**
         * @brief DEPRECATED
        */
        void new_reg(std::string name, vp::reg_32 *reg, uint32_t reset_val, bool reset = true);

        /**
         * @brief DEPRECATED
        */
        void new_reg(std::string name, vp::reg_64 *reg, uint64_t reset_val, bool reset = true);

    private:
        /*
         * Private members accessed by other classes of the framework
         */
        // Used by the launcher to build the whole system hierarchy (bind, start, etc)
        int build_all();

        // Load a component module from workstation. It is static so that the launcher can call it
        // for top component.
        static vp::Component *load_component(js::Config *config, js::Config *gv_config,
            vp::Component *parent, std::string name,
            vp::TimeEngine *time_engine, vp::TraceEngine *trace_engine,
            vp::PowerEngine *power_engine, vp::MemCheck *memcheck);

        // Used by the launcher to set himself as launcher. Could be moved to ComponentConfig
        void set_launcher(gv::GvsocLauncher *launcher);

        std::map<std::string, Component *> childs_dict;

        bool is_component() { return true; }



        /*
         * Real private members
         */
        // Create all sub-components specified in json config
        void create_comps();

        // Create all ports specified in json config
        void create_ports();

        // Create all bindings specified in json config
        void create_bindings();

        // Do the connectins between master and slave ports
        void bind_comps();

        // Do the final binding which is a step that ports can overload to do extra operations
        void final_bind();

        // Return a port from its name
        vp::Port *get_port(std::string name);

        // Register a new port
        void add_port(std::string name, vp::Port *port);

        // Get child from its name, used for bindings
        std::map<std::string, vp::Component *> get_childs_dict() { return childs_dict; }

        // Search for a module from its name into the module search dirs and return its path
        static std::string get_module_path(js::Config *gv_config, std::string relpath);

        // Add a new component
        void add_child(std::string name, vp::Component *child);

        // Create a new component
        vp::Component *new_component(std::string name, js::Config *config, std::string module = "");

        // Clock interface handler to catch clock registering and propagate to sub components
        static void clk_reg(vp::Component *_this, vp::Component *clock);

        // Clock interface handle to catche frequency setting to propagate to power framework
        static void clk_set_frequency(vp::Component *_this, int64_t frequency);

        // Power interface handle to propagate supply change to power framework
        static void power_supply_sync(vp::Block *_this, int state);

        // Power interface handle to propagate voltage change to power framework
        static void voltage_sync(vp::Block *_this, int voltage);

        // Reset interface handle to propagate reset to all childs
        static void reset_sync(vp::Block *_this, bool active);

        // Name of the component
        std::string name;

        // Parent of the component
        vp::Component *parent;

        // JSON config of the component
        js::Config *js_config;

        // GVSOC global config
        js::Config *gv_config;

        // Ports of the component, used for bindings
        std::unordered_map<std::string, vp::Port *> ports;

        // Top launcher, this can sometimes be needed by components to interact with the launcher.
        gv::GvsocLauncher *launcher;

        // Reset port to synchronize the component reset with a wire instead of the upper component
        vp::WireSlave<bool> reset_port;

        // Power port to update power supply
        vp::WireSlave<int> power_port;

        // Power port to update power voltage
        vp::WireSlave<int> voltage_port;

        // Power port to update frequency from wire instead of uppper component
        vp::ClkSlave            clock_port;
    };

};
