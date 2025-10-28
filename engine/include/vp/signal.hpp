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
#include <vp/block.hpp>
#include <vp/trace/trace.hpp>

namespace vp {

    class Block;

    class SignalCommon
    {
        friend class Block;
    public:
        enum class ResetKind {
            None,
            Value,
            HighZ
        };

        SignalCommon(Block &parent, std::string name, int width, bool do_reset);
        SignalCommon(Block &parent, std::string name, int width, ResetKind reset_kind=ResetKind::Value);

        inline void release()
        {
            this->trace.msg("Release register\n");
            this->event.dump_highz();
        }

    protected:
        virtual void reset(bool active) {};

        Block &parent;
        std::string name = "";
        vp::Event event;
        vp::Trace trace;
        int width;
        ResetKind reset_kind;
        int nb_bytes;
        uint8_t *value_bytes;
        uint8_t *reset_value_bytes;
    };

    template<class T>
    class Signal : public SignalCommon
    {
    public:
        Signal(Block &parent, std::string name, int width, bool do_reset=true, T reset=0);
        Signal(Block &parent, std::string name, int width, ResetKind reset_kind, T reset_value=0);
        inline void set(T value, int64_t cycle_delay=0, int64_t time_delay=0);
        inline void set_and_release(T value, int64_t cycle_delay=0, int64_t time_delay=0);
        inline T get() const;

        Signal& operator=(T v)
        {
            set(v);
            return *this;
        }

        operator T() const
        {
            return get();
        }

        Signal& operator-=(T v)
        {
            this->set(this->get() - v);
            return *this;
        }

        Signal& operator+=(T v)
        {
            this->set(this->get() + v);
            return *this;
        }

        Signal& operator|=(T v)
        {
            this->set(this->get() | v);
            return *this;
        }

        Signal& operator&=(T v)
        {
            this->set(this->get() & v);
            return *this;
        }

        inline void inc(T value);
        inline void dec(T value);
        inline void release(int64_t cycle_delay=0, int64_t time_delay=0);
        inline void release_next();
    protected:
        void reset(bool active) override;
    private:
        T value;
        T reset_value;
    };
};


template<class T>
inline void vp::Signal<T>::set_and_release(T value, int64_t cycle_delay, int64_t time_delay)
{
    this->set(value, 0, time_delay);
    this->release_next();
}

template<class T>
inline void vp::Signal<T>::set(T value, int64_t cycle_delay, int64_t time_delay)
{
#ifdef VP_TRACE_ACTIVE
    if (this->trace.get_active())
    {
        this->trace.msg(vp::Trace::LEVEL_TRACE, "Setting signal (value: 0x%.*x)\n", this->nb_bytes*2, value);
    }
#endif

    this->value = value;
    this->event.dump_value((uint8_t *)&this->value, time_delay);
}

template<class T>
inline T vp::Signal<T>::get() const
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
vp::Signal<T>::Signal(Block &parent, std::string name, int width, ResetKind reset_kind,
    T reset_value)
: SignalCommon(parent, name, width, reset_kind)
{
    this->reset_value = reset_value;
    this->value_bytes = (uint8_t *)&this->value;
    this->reset_value_bytes = (uint8_t *)&this->reset_value;
}

template<class T>
inline void vp::Signal<T>::release(int64_t cycle_delay, int64_t time_delay)
{
    this->trace.msg("Release register\n");
    this->event.dump_highz(time_delay);
}

template<class T>
inline void vp::Signal<T>::release_next()
{
    this->event.dump_highz_next();
}

template<class T>
void vp::Signal<T>::reset(bool active)
{
    if (active)
    {
        std::ostringstream value;
        // value << std::hex << this->value;
        // this->trace.msg(vp::Trace::LEVEL_TRACE, "Resetting signal (value: %s)\n", value.str().c_str());
        switch (this->reset_kind)
        {
            case ResetKind::Value:
                this->value = this->reset_value;
                break;

            case ResetKind::HighZ:
                this->release();
                break;
        }
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
