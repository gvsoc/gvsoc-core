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

namespace vp
{
    class Component;
    class Block;
    class AudioSlave;

    // Audio master interface which can be used to push samples to another audio component
    // It can also propagate the sampling rate to other components or receives it from other
    // components
    class AudioMaster : public MasterPort
    {
        friend class AudioSlave;

    public:
        AudioMaster();

        // Push a sample
        inline void push_sample(double value);

        // Notify the rate being used, to align everyone one same rate
        inline void set_rate(int rate);

        // Register master callback for set_rate called by slave
        void set_set_rate_meth(void (*)(vp::Block *_this, int rate));
        // Register master callback for set_rate called by slave when muxed
        void set_set_rate_meth_muxed(void (*)(vp::Block *_this, int rate, int), int id);
        // Tells if master interface is bound to at least one slave interface
        inline bool is_bound();

    private:
        // Bind this master interface to a slave interface
        void bind_to(vp::Port *port, js::Config *config);
        // Finalize the interface. Used to check if frequency domains are different
        void finalize();

        // Default master set_rate method which is called when it is not set 
        static inline void set_rate_default(vp::Block *, int value);

        // When slave push_sample method is multiplexed, this stub is called to add
        // the multiplexer ID to the call
        static inline void remote_push_sample_muxed_stub(AudioMaster *_this, double value);
        // When slave set_rate method is multiplexed, this stub is called to add
        // the multiplexer ID to the call
        static inline void remote_set_rate_muxed_stub(AudioMaster *_this, int rate);

        // Master set_rate method set by user on master interface
        void (*set_rate_meth)(vp::Block *comp, int value);
        // Master set_rate multiplexed method set by user on master interface
        // NULL means this method is not multiplexed
        void (*set_rate_meth_mux)(vp::Block *comp, int value, int id);

        // Slave push_sample method set by user on slave interface. It is copied from slave
        // interface to here when they are bound
        // If the interface is multiplexed, this actually contains a stub
        void (*remote_push_sample_meth)(vp::Block *, double value);
        // If the push_sample slave interface is multiplexed, this contains the real method
        void (*remote_push_sample_meth_mux)(vp::Block *, double value, int id);

        // Slave set_rate method set by user on slave interface. It is copied from slave
        // interface to here when they are bound
        // If the interface is multiplexed, this actually contains a stub
        void (*remote_set_rate_meth)(vp::Block *comp, int value);
        // If the set_rate slave interface is multiplexed, this contains the real method
        void (*remote_set_rate_meth_mux)(vp::Block *comp, int value, int id);

        // If any slave method is multiplexed, since the remote context is actually filled
        // with the master context to have the stub properly working, this contains the actual
        // slave context
        vp::Component *remote_context_for_mux;
        // If the push_sample slave method is multiplexed, this contains multiplexer ID which must
        // be given when calling the slave push_sample method
        int remote_push_sample_mux_id;
        // If the set_rate slave method is multiplexed, this contains multiplexer ID which must
        // be given when calling the slave set_rate method
        int remote_set_rate_mux_id;

        // Remote slave port
        AudioSlave *slave_port = NULL;
        // Used for chaining several slave interfaces together when this master interface is bound
        // to several slave interfaces
        AudioMaster *next = NULL;

        // Multiplexer ID of the master method set by the user. This will be passed to the method
        // when the slave calls it.
        int set_rate_mux_id;
    };

    // Audio slave interface used to receive samples from other components
    // It can also propagate the sampling rate to other components or receives it from other
    // components
    class AudioSlave : public SlavePort
    {

        friend class AudioMaster;

    public:
        inline AudioSlave();

        // Notify the rate being used, to align everyone one same rate
        inline void set_rate(int rate);

        // Register slave callback for set_rate method called by master
        void set_set_rate_meth(void (*)(vp::Block *_this, int rate));
        // Register slave multiplxed callback for set_rate method called by master
        // When set_rate is called, the mux ID will be passed back
        void set_set_rate_meth_muxed(void (*)(vp::Block *_this, int rate, int mux_id), int mux_id);

        // Register slave callback for push_sample method called by master
        void set_push_sample_meth(void (*)(vp::Block *_this, double value));
        // Register slave multiplxed callback for push_sample method called by master
        // When push_sample is called, the mux ID will be passed back
        void set_push_sample_meth_muxed(void (*)(vp::Block *_this, double value, int mux_id), int mux_id);
        // Tells if slave interface is bound to a master interface
        inline bool is_bound();

    private:
        // Bind this master interface to a slave interface
        inline void bind_to(vp::Port *_port, js::Config *config);
        // When master set_rate method is multiplexed, this stub is called to add
        // the multiplexer ID to the call
        static inline void set_rate_muxed_stub(AudioSlave *_this, int rate);

        // Slave set_rate method set by user on slave interface
        void (*set_rate_meth)(vp::Block *comp, int rate);
        // Slave set_rate multiplexed method set by user on slave interface
        // NULL means this method is not multiplexed
        void (*set_rate_meth_mux)(vp::Block *comp, int rate, int id);

        // Slave push_sample method set by user on slave interface
        void (*push_sample_meth)(vp::Block *comp, double value);
        // Slave push_sample multiplexed method set by user on slave interface
        // NULL means this method is not multiplexed
        void (*push_sample_meth_mux)(vp::Block *comp, double value, int id);

        // Master set_rate method set by user on master interface. It is copied from master
        // interface to here when they are bound
        // If the interface is multiplexed, this actually contains a stub
        void (*remote_set_rate_meth)(vp::Block *comp, int value);
        // If the set_rate slave interface is multiplexed, this contains the real method
        void (*remote_set_rate_meth_mux)(vp::Block *comp, int value, int id);

        // If any master method is multiplexed, since the remote context is actually filled
        // with the slave context to have the stub properly working, this contains the actual
        // master context
        vp::Component *remote_context_for_mux;

        // If the set_rate master method is multiplexed, this contains multiplexer ID which must
        // be given when calling the master set_rate method
        int remote_set_rate_mux_id;

        // Used to track how many times it is connected to a master interface, to properly chain
        // them
        int bind_count = 0;

        // Remote master port
        AudioMaster *master_port = NULL;

        // Used for chaining several master interfaces together when this slave interface is bound
        // to several master interfaces
        AudioSlave *next = NULL;

        // Multiplexer ID of the set_rate slave method set by the user. This will be passed to the method
        // when the master calls it.
        int set_rate_mux_id;
        // Multiplexer ID of the push_sample slave method set by the user. This will be passed to the method
        // when the master calls it.
        int push_sample_mux_id;
    };

};


#include <vp/itf/implem/audio_implem.hpp>