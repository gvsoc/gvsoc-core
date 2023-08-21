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

#ifndef __VP_COMPONENT_HPP__
#define __VP_COMPONENT_HPP__

#include "vp/vp_data.hpp"
#include <map>
#include <list>
#include <string>
#include <vector>
#include <functional>
#include "vp/ports.hpp"
#include "vp/config.hpp"
#include "vp/clock/clock_event.hpp"
#include "vp/itf/clk.hpp"
#include "vp/clock/component_clock.hpp"
#include "vp/trace/component_trace.hpp"
#include "gv/power.hpp"
#include "json.hpp"
#include <functional>
#include <vp/block.hpp>
#include <vp/register.hpp>
#include <gv/gvsoc.hpp>


#define   likely(x) __builtin_expect(x, 1)
#define unlikely(x) __builtin_expect(x, 0)

using namespace std;

#define VP_ERROR_SIZE (1<<16)
extern char vp_error[];

class Gv_proxy;

class Gvsoc_launcher;

namespace vp {

  class config;
  class clock_engine;
  class component;
  class signal;
  class trace_domain;


  class Notifier {
  public:
      virtual void notify_stop(int64_t time) {}
      virtual void notify_run(int64_t time) {}
  };


  class component : public component_clock, public block
  {

    friend class component_clock;
    friend class vp::power::component_power;

  public:
    component(js::config *config);

    static vp::component *load_component(js::config *config, js::config *gv_config);

    virtual void pre_pre_build() { }
    virtual int build() { return 0; }
    virtual void pre_start() {}
    virtual void start() {}
    virtual void stop() {}
    virtual void flush() {}
    virtual void pre_reset() {}
    virtual void reset(bool active) {}
    virtual void power_supply_set(int state) {}
    virtual void register_exec_notifier(Notifier *notifier) {}

    virtual void dump_traces(FILE *file) {}

    std::string get_component_path(std::string);

    void dump_traces_recursive(FILE *file);

    component *get_parent() { return this->parent; }
    inline js::config *get_js_config() { return comp_js_config; }

    js::config *get_vp_config();
    void set_vp_config(js::config *config);

    inline config *get_config(std::string name);


    inline int get_config_int(std::string name, int index);

    inline int get_config_int(std::string name);

    inline bool get_config_bool(std::string name);

    inline std::string get_config_str(std::string name);

    inline int64_t get_time();

    virtual vp::time_engine *get_time_engine() ;

    string get_path() { return path; }
    string get_name() { return name; }


    void conf(string name, string path, vp::component *parent);

    void add_child(std::string name, vp::component *child);

    config *import_config(const char *config_string);

    void reg_step_pre_start(std::function<void()> callback);
    void register_build_callback(std::function<void()> callback);

    void post_post_build();

    void pre_build() {
      component_clock::pre_build(this);
    }

    int build_new();

    void flush_all();

    void post_post_build_all();

    void pre_start_all();

    void start_all();

    void stop_all();

    void final_bind();

    virtual void *external_bind(std::string comp_name, std::string itf_name, void *handle);
    virtual void time_engine_update(int64_t timestamp) {}

    void reset_all(bool active, bool from_itf=false);

    void new_master_port(std::string name, master_port *port);

    void new_master_port(void *comp, std::string name, master_port *port);

    void new_slave_port(std::string name, slave_port *port);

    void new_slave_port(void *comp, std::string name, slave_port *port);

    void new_service(std::string name, void *service);

    void add_service(std::string name, void *service);

    vp::component *new_component(std::string name, js::config *config, std::string module="");
    void build_instance(std::string name, vp::component *parent);

    int get_ports(bool master, int size, const char *names[], void *ports[]);

    void *get_service(string name);

    void get_trace(std::vector<vp::trace *> &traces, std::string path);

    void new_reg(std::string name, vp::reg_1 *reg, uint8_t reset_val, bool reset=true);
    void new_reg(std::string name, vp::reg_8 *reg, uint8_t reset_val, bool reset=true);
    void new_reg(std::string name, vp::reg_16 *reg, uint16_t reset_val, bool reset=true);
    void new_reg(std::string name, vp::reg_32 *reg, uint32_t reset_val, bool reset=true);
    void new_reg(std::string name, vp::reg_64 *reg, uint64_t reset_val, bool reset=true);

    inline trace *get_trace() { return &this->root_trace; }

    std::vector<vp::component *> get_childs() { return childs; }
    std::map<std::string, vp::component *> get_childs_dict() { return childs_dict; }
    vp::component *get_component(std::vector<std::string> path_list);

    virtual vp::port *get_slave_port(std::string name) { return this->slave_ports[name]; }
    virtual vp::port *get_master_port(std::string name) { return this->master_ports[name]; }

    virtual void add_slave_port(std::string name, vp::slave_port *port);
    virtual void add_master_port(std::string name, vp::master_port *port);

    void throw_error(std::string error);

    virtual std::string handle_command(Gv_proxy *proxy, FILE *req_file, FILE *reply_file, std::vector<std::string> args, std::string req) { return ""; }

    component_trace traces;
    vp::power::component_power power;

    trace warning;

    void set_launcher(Gvsoc_launcher *launcher) { this->launcher = launcher; }

  protected:
    Gvsoc_launcher *get_launcher();
    void create_comps();
    void create_ports();
    void create_bindings();
    void bind_comps();

    std::map<std::string, void *> all_services;
 
    std::vector<component *> childs;
    std::map<std::string, component *> childs_dict;

  private:
    js::config *comp_js_config;
    js::config *vp_config = NULL;
    trace root_trace;

    std::map<std::string, master_port *> master_ports;
    std::map<std::string, slave_port *> slave_ports;

    string path;
    string name;

    component *parent = NULL;

    vector<std::function<void()>> pre_start_callbacks;
    vector<std::function<void()>> build_callbacks;
    vector<vp::reg *> regs;

    bool reset_done_from_itf;

    time_engine *time_engine_ptr = NULL;
    Gvsoc_launcher *launcher;
  };

  std::string __gv_get_component_path(js::config *gv_config, std::string relpath);
};

#endif
