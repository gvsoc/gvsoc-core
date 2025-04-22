/*
 * Copyright (C) 2020  GreenWaves Technologies, SAS, SAS, ETH Zurich and University of Bologna
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

#include <stdint.h>
#include <sstream>

namespace vp {

    class Block;

    class SignalCommon
    {
        friend class Block;
    public:
        SignalCommon(Block &parent, std::string name, int width, bool do_reset);

        inline void release()
        {
            this->trace.msg("Release register\n");
            if (this->reg_event.get_event_active())
                this->reg_event.event_highz();
        }

    protected:
        virtual void reset(bool active) {};

        std::string name = "";
        vp::Trace trace;
        vp::Trace reg_event;
        int width;
        bool do_reset;
        int nb_bytes;
        uint8_t *value_bytes;
        uint8_t *reset_value_bytes;
    };

    template<class T>
    class Signal : public SignalCommon
    {
    public:
        Signal(Block &parent, std::string name, int width, bool do_reset=false, T reset=0);
        inline void set(T value);
        inline T get();
        inline void inc(T value);
        inline void dec(T value);
        inline void release();
    protected:
        void reset(bool active) override;
    private:
        T value;
        T reset_value;
    };
};


template<class T>
inline void vp::Signal<T>::set(T value)
{
#ifdef VP_TRACE_ACTIVE
    if (this->trace.get_active())
    {
        this->trace.msg(vp::Trace::LEVEL_TRACE, "Setting signal (value: 0x%.*x)\n", this->nb_bytes*2, value);
    }
#endif

    this->value = value;
    this->reg_event.event((uint8_t *)&this->value);
}

template<class T>
inline T vp::Signal<T>::get()
{
    return this->value;
}

template<class T>
vp::Signal<T>::Signal(vp::Block &parent, std::string name, int width, bool do_reset, T reset)
    : SignalCommon(parent, name, width, do_reset)
{
    this->reset_value = reset;
    this->value_bytes = (uint8_t *)&this->value;
    this->reset_value_bytes = (uint8_t *)&this->reset_value;
}

template<class T>
inline void vp::Signal<T>::release()
{
    this->trace.msg("Release register\n");
    if (this->reg_event.get_event_active())
        this->reg_event.event_highz();
}

template<class T>
void vp::Signal<T>::reset(bool active)
{
    if (active)
    {
        std::ostringstream value;
        // value << std::hex << this->value;
        // this->trace.msg(vp::Trace::LEVEL_TRACE, "Resetting signal (value: %s)\n", value.str().c_str());
        this->value = this->reset_value;
    }
}

template<class T>
inline void vp::Signal<T>::inc(T value)
{
    this->set(this->get() + value);
}

template<class T>
inline void vp::Signal<T>::dec(T value)
{
    this->set(this->get() - value);
}
