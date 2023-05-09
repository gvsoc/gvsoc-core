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


double my_stod (std::string const& s) {
    std::istringstream iss (s);
    iss.imbue (std::locale("C"));
    double d;
    iss >> d;
    // insert error checking.
    return d;
}


vp::power::Linear_table::Linear_table(js::config *config)
{
    // Extract the power table from json file for each temperature
    for (auto &x : config->get_childs())
    {
        temp_tables.push_back(new Linear_temp_table(my_stod(x.first), x.second));
    }

    std::sort(this->temp_tables.begin(), this->temp_tables.end(),
    [](vp::power::Linear_temp_table *a, vp::power::Linear_temp_table *b){ return a->get_temp() <= b->get_temp(); });
}



double vp::power::Linear_table::get(double temp, double volt, double frequency)
{
    int low_temp_index = -1, high_temp_index = -1;

    // We need to estimate the actual power value using interpolation on the existing tables.
    // We first estimate it for the 2 temperatures surrounding the requested temperature,
    // and then do an interpolation from these 2 values.

    // Go through the temperatures to find the one just below and the one
    // just above
    for (unsigned int i = 0; i < this->temp_tables.size(); i++)
    {
        if (this->temp_tables[i]->get_temp() == temp)
        {
            low_temp_index = high_temp_index = i;
            break;
        }

        if (this->temp_tables[i]->get_temp() > temp)
        {
            high_temp_index = i;
            break;
        }

        low_temp_index = i;
    }

    // If we didn't find lower and upper temperature, just take the limit so that we don't do
    // any estimation outside the given ranges
    if (high_temp_index == -1)
        high_temp_index = low_temp_index;

    if (low_temp_index == -1)
        low_temp_index = high_temp_index;


    double value;

    if (high_temp_index == low_temp_index)
    {
        // Case where we were outside the given ranges or we found the exact temperature, just return the value at given 
        // voltage and frequency.
        value = this->temp_tables[low_temp_index]->get(volt, frequency);
    }
    else
    {
        // Otherwise get the power numbers for the 2 temperatures
        double low_temp = this->temp_tables[low_temp_index]->get_temp();
        double high_temp = this->temp_tables[high_temp_index]->get_temp();

        double value_at_low_temp = this->temp_tables[low_temp_index]->get(volt, frequency);
        double value_at_high_temp = this->temp_tables[high_temp_index]->get(volt, frequency);

        // And do the interpolation
        double temp_ratio = (temp - low_temp) / (high_temp - low_temp);
        value = (value_at_high_temp - value_at_low_temp) * temp_ratio + value_at_low_temp;
    }
    return value;
}



vp::power::Linear_temp_table::Linear_temp_table(double temp, js::config *config)
{
    this->temp = temp;

    for (auto &x : config->get_childs())
    {
        volt_tables.push_back(new Linear_volt_table(my_stod(x.first), x.second));
    }

    std::sort(this->volt_tables.begin(), this->volt_tables.end(),
    [](vp::power::Linear_volt_table *a, vp::power::Linear_volt_table *b){ return a->get_volt() <= b->get_volt(); });
}



double vp::power::Linear_temp_table::get(double volt, double frequency)
{
    int low_index = -1, high_index = -1;

    // We need to estimate the actual power value using interpolation on the existing tables.
    // We first estimate it for the 2 voltages surrounding the requested voltage,
    // and then do an interpolation from these 2 values.

    // Go through the temperatures to find the one just below and the one
    // just above
    for (unsigned int i = 0; i < this->volt_tables.size(); i++)
    {
        if (this->volt_tables[i]->get_volt() == volt)
        {
            low_index = high_index = i;
            break;
        }

        if (this->volt_tables[i]->get_volt() > volt)
        {
            high_index = i;
            break;
        }

        low_index = i;
    }

    // If we didn't find lower and upper voltage, just take the limit so that we don't do
    // any estimation outside the given ranges
    if (high_index == -1)
        high_index = low_index;

    if (low_index == -1)
        low_index = high_index;

    double value;
    if (high_index == low_index)
    {
        // Case where we were outside the given ranges or we found the exact voltage, just return the value at given 
        // frequency.
        value = this->volt_tables[low_index]->get(frequency);
    }
    else
    {
        // Otherwise get the power numbers for the 2 voltages
        double low_volt = this->volt_tables[low_index]->get_volt();
        double high_volt = this->volt_tables[high_index]->get_volt();

        double value_at_low_volt = this->volt_tables[low_index]->get(frequency);
        double value_at_high_volt = this->volt_tables[high_index]->get(frequency);

        // And do the interpolation
        double volt_ratio = (volt - low_volt) / (high_volt - low_volt);
        value = (value_at_high_volt - value_at_low_volt) * volt_ratio + value_at_low_volt;
    }
    return value;
}



vp::power::Linear_volt_table::Linear_volt_table(double volt, js::config *config)
{
    this->volt = volt;

    // Depending on frequency is currently not supported, so we'll just return
    // the power value
    for (auto &x : config->get_childs())
    {
        if (x.first == "any")
        {
            this->any = new Linear_freq_table(0, x.second);
        }
        else
        {
            this->freq_tables.push_back(new Linear_freq_table(my_stod(x.first), x.second));
        }
    }

    std::sort(this->freq_tables.begin(), this->freq_tables.end(),
        [](vp::power::Linear_freq_table *a, vp::power::Linear_freq_table *b){ return a->get_freq() <= b->get_freq(); });
}



double vp::power::Linear_volt_table::get(double frequency)
{
    if (this->any != NULL)
    {
        return this->any->get();
    }

    int low_index = -1, high_index = -1;

    // We need to estimate the actual power value using interpolation on the existing tables.
    // We first estimate it for the 2 frequencies surrounding the requested voltage,
    // and then do an interpolation from these 2 values.

    // Go through the frequencies to find the one just below and the one
    // just above
    for (unsigned int i = 0; i < this->freq_tables.size(); i++)
    {
        if (this->freq_tables[i]->get_freq() == frequency)
        {
            low_index = high_index = i;
            break;
        }

        if (this->freq_tables[i]->get_freq() > frequency)
        {
            high_index = i;
            break;
        }

        low_index = i;
    }

    // If we didn't find lower and upper voltage, just take the limit so that we don't do
    // any estimation outside the given ranges
    if (high_index == -1)
        high_index = low_index;

    if (low_index == -1)
        low_index = high_index;

    double value;
    if (high_index == low_index)
    {
        // Case where we were outside the given ranges or we found the exact voltage, just return the value at given 
        // frequency.
        value = this->freq_tables[low_index]->get();
    }
    else
    {
        // Otherwise get the power numbers for the 2 frequencies
        double low_freq = this->freq_tables[low_index]->get_freq();
        double high_freq = this->freq_tables[high_index]->get_freq();

        double value_at_low_freq = this->freq_tables[low_index]->get();
        double value_at_high_freq = this->freq_tables[high_index]->get();

        // And do the interpolation
        double freq_ratio = (frequency - low_freq) / (high_freq - low_freq);
        value = (value_at_high_freq - value_at_low_freq) * freq_ratio + value_at_low_freq;
    }
    return value;
}



vp::power::Linear_freq_table::Linear_freq_table(double freq, js::config *config)
: freq(freq)
{
    this->value = my_stod(config->get_str());
}
