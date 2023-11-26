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

#include "vp/vp.hpp"
#include "vp/trace/trace.hpp"

vp::BlockPower::BlockPower(vp::Block *parent, vp::Block &top, vp::PowerEngine *engine)
    : top(top), report(top), engine(engine)
{
    if (this->engine == NULL && parent != NULL)
    {
        this->engine = parent->power.get_engine();
    }
}

void vp::BlockPower::build()
{
    this->new_power_trace("power_trace", &this->power_trace);

    for (auto trace : this->traces)
    {
        this->get_engine()->reg_trace(trace);
    }

}

int vp::BlockPower::new_power_trace(std::string name, vp::PowerTrace *trace, vp::PowerTrace *parent)
{
    if (trace->init(&top, name, parent))
        return -1;

    this->traces.push_back(trace);

    return 0;
}

int vp::BlockPower::new_power_source(std::string name, PowerSource *source, js::Config *config, vp::PowerTrace *trace)
{
    if (trace == NULL)
    {
        trace = &this->power_trace;
    }

    if (source->init(&top, name, config, trace))
        return -1;

    source->setup(VP_POWER_DEFAULT_TEMP, VP_POWER_DEFAULT_VOLT, VP_POWER_DEFAULT_FREQ);

    this->sources.push_back(source);

    return 0;
}

void vp::BlockPower::set_frequency(int64_t frequency)
{
    for (PowerSource *power_source : this->sources)
    {
        power_source->set_frequency(frequency);
    }

    for (auto child : this->top.get_childs())
    {
        child->power.set_frequency(frequency);
    }
}

double vp::BlockPower::get_average_power(double &dynamic_power, double &static_power)
{
    double result = 0.0;

    for (auto x : this->traces)
    {
        double dynamic, leakage;
        x->get_report_power(&dynamic, &leakage);
        result += dynamic + leakage;
    }

    return result;
}

double vp::BlockPower::get_instant_power(double &dynamic_power, double &static_power)
{
    double result = 0.0;
    dynamic_power = 0.0;
    static_power = 0.0;

    for (auto x : this->traces)
    {
        dynamic_power += x->instant_dynamic_power;
        static_power += x->instant_static_power;
        result += x->current_power;
    }

    return result;
}

// TODO that seems wrong and deprecated
double vp::BlockPower::get_power_from_childs()
{
    double result = 0.0;
    for (auto &x : this->top.get_childs())
    {
        result += x->power.get_power_from_self_and_childs();
    }
    return result;
}

double vp::BlockPower::get_power_from_self_and_childs()
{
    double result = 0.0;

    for (auto &x : this->traces)
    {
        result += x->get_power();
    }

    result += this->get_power_from_childs();

    return result;
}

void vp::BlockPower::get_report_energy_from_childs(double *dynamic, double *leakage)
{
    for (auto &x : this->top.get_childs())
    {
        x->power.get_report_energy_from_self_and_childs(dynamic, leakage);
    }
}

void vp::BlockPower::get_report_energy_from_self_and_childs(double *dynamic, double *leakage)
{
    for (auto &x : this->traces)
    {
        double trace_dynamic, trace_leakage;
        x->get_report_energy(&trace_dynamic, &trace_leakage);
        *dynamic += trace_dynamic;
        *leakage += trace_leakage;
    }

    this->get_report_energy_from_childs(dynamic, leakage);
}

void vp::BlockPower::dump(FILE *file, double total)
{
    for (auto x : this->traces)
    {
        double dynamic, leakage;
        x->get_report_power(&dynamic, &leakage);
        double percentage = 0.0;
        if (total != 0.0)
        {
            percentage = (dynamic + leakage) / total;
        }
        fprintf(file, "%s; %.12f; %.12f; %.12f; %.6f\n", x->trace.get_full_path().c_str(), dynamic, leakage, dynamic + leakage, percentage);
    }
}

void vp::BlockPower::dump_child_traces(FILE *file, double total)
{
    for (auto &x : this->top.get_childs())
    {
        x->power.dump(file, total);
    }
}

void vp::BlockPower::power_supply_set_all(vp::PowerSupplyState state)
{
    this->top.get_trace()->msg(vp::TraceLevel::DEBUG, "Setting power state (state: %d)\n", state);

    for (auto &x : this->top.get_childs())
    {
        x->power.power_supply_set_all(state);
    }

    for (auto &x : this->sources)
    {
        if (state == 1 || state == 2)
        {
            x->turn_on();
        }
        else
        {
            x->turn_off();
        }
    }

    this->top.power_supply_set(state);
}

void vp::BlockPower::voltage_set_all(int voltage)
{
    this->top.get_trace()->msg(vp::TraceLevel::DEBUG, "Setting voltage (voltage: %d)\n", voltage);

    for (PowerSource *power_source : this->sources)
    {
        power_source->set_voltage(voltage);
    }

    for (auto &x : this->top.get_childs())
    {
        x->power.voltage_set_all(voltage);
    }
}

std::vector<gv::PowerReport *> vp::CompPowerReport::get_childs()
{
    std::vector<gv::PowerReport *> result;
    for (vp::Block *x : this->top.get_childs())
    {
        vp::CompPowerReport *report = (vp::CompPowerReport *)x->power.get_report();
        report->compute();
        result.push_back((gv::PowerReport *)report);
    }
    return result;
}

vp::CompPowerReport::CompPowerReport(vp::Block &top)
    : top(top)
{
}

void vp::CompPowerReport::compute()
{
    this->name = this->top.get_name();
    this->power = this->top.power.get_average_power(this->dynamic_power, this->static_power);
}

void vp::BlockPower::dump_traces_recursive(FILE *file)
{
    this->dump_traces(file);

    for (auto& x: this->top.get_childs())
    {
        x->power.dump_traces_recursive(file);
    }
}
