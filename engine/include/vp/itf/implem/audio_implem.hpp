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


namespace vp
{

    AudioMaster::AudioMaster()
    {
        // Init to NULL so that slave can detect if set_rate method is muxed or not
        this->set_rate_meth_mux = NULL;
        // Default set_rate implementation which may be overriden by user method
        this->set_rate_meth = &AudioMaster::set_rate_default;
    }

    inline void AudioMaster::push_sample(double value)
    {
        if (next) next->push_sample(value);
        this->remote_push_sample_meth((vp::Block *)this->get_remote_context(), value);
    }

    inline void AudioMaster::set_rate(int rate)
    {
        if (next) next->set_rate(rate);
        this->remote_set_rate_meth((vp::Block *)this->get_remote_context(), rate);
    }

    inline void AudioMaster::set_set_rate_meth(void (*meth)(vp::Block *, int rate))
    {
        this->set_rate_meth = meth;
    }

    inline void AudioMaster::set_set_rate_meth_muxed(void (*meth)(vp::Block *, int, int), int id)
    {
        this->set_rate_meth_mux = meth;
        this->set_rate_mux_id = id;
    }

    inline void AudioMaster::bind_to(vp::Port *_port, js::Config *config)
    {
        this->remote_port = _port;
        AudioSlave *port = (AudioSlave *)_port;

        if (this->slave_port != NULL)
        {
            AudioMaster *master = new AudioMaster();
            port->master_port = master;
            master->bind_to(_port, config);
            master->next = this->next;
            this->next = master;
        }
        else
        {
            port->master_port = this;
            if (port->set_rate_meth_mux == NULL && port->push_sample_meth_mux == NULL)
            {
                this->remote_push_sample_meth = port->push_sample_meth;
                this->remote_set_rate_meth = port->set_rate_meth;
                this->set_remote_context(port->get_context());
            }
            else
            {
                this->remote_push_sample_meth_mux =
                    port->push_sample_meth_mux == NULL ?
                    (void (*)(vp::Block *, double, int))port->push_sample_meth : port->push_sample_meth_mux;
                this->remote_set_rate_meth_mux = port->set_rate_meth_mux == NULL ?
                    (void (*)(vp::Block *, int, int))port->set_rate_meth : port->set_rate_meth_mux;

                this->remote_push_sample_meth = (void (*)(vp::Block *, double)) &AudioMaster::remote_push_sample_muxed_stub;
                this->remote_set_rate_meth = (void (*)(vp::Block *, int)) &AudioMaster::remote_set_rate_muxed_stub;

                this->set_remote_context(this);
                this->remote_context_for_mux = (vp::Component *)port->get_context();
                this->remote_push_sample_mux_id = port->push_sample_mux_id;
                this->remote_set_rate_mux_id = port->set_rate_mux_id;
            }
        }
    }

    inline bool AudioMaster::is_bound()
    {
        return this->slave_port != NULL;
    }

    inline void AudioMaster::finalize()
    {
    }

    inline void AudioMaster::set_rate_default(vp::Block *, int value)
    {
    }

    inline void AudioMaster::remote_push_sample_muxed_stub(AudioMaster *_this, double value)
    {
        return _this->remote_push_sample_meth_mux(_this->remote_context_for_mux, value, _this->remote_push_sample_mux_id);
    }

    inline void AudioMaster::remote_set_rate_muxed_stub(AudioMaster *_this, int rate)
    {
        return _this->remote_set_rate_meth_mux(_this->remote_context_for_mux, rate, _this->remote_set_rate_mux_id);
    }


    inline AudioSlave::AudioSlave()
    {
        this->set_rate_meth_mux = NULL;
        this->push_sample_meth_mux = NULL;
    }

    inline void AudioSlave::set_rate(int rate)
    {
        if (next) next->set_rate(rate);
        this->remote_set_rate_meth((vp::Block *)this->get_remote_context(), rate);
    }

    inline void AudioSlave::set_set_rate_meth(void (*meth)(vp::Block *, int rate))
    {
        this->set_rate_meth = meth;
    }

    inline void AudioSlave::set_set_rate_meth_muxed(void (*meth)(vp::Block *, int, int), int id)
    {
        this->set_rate_meth_mux = meth;
        this->set_rate_mux_id = id;
    }

    inline void AudioSlave::set_push_sample_meth(void (*meth)(vp::Block *, double value))
    {
        this->push_sample_meth = meth;
    }

    inline void AudioSlave::set_push_sample_meth_muxed(void (*meth)(vp::Block *, double, int), int id)
    {
        this->push_sample_meth_mux = meth;
        this->push_sample_mux_id = id;
    }

    inline void AudioSlave::bind_to(vp::Port *_port, js::Config *config)
    {
        SlavePort::bind_to(_port, config);
        AudioMaster *port = (AudioMaster *)_port;

        this->bind_count++;

        if (this->bind_count > 1)
        {
            AudioSlave *slave = new AudioSlave();
            port->slave_port = slave;
            slave->bind_to(_port, config);
            slave->next = this->next;
            this->next = slave;
        }
        else
        {
            port->slave_port = this;
            if (port->set_rate_meth_mux == NULL)
            {
                this->remote_set_rate_meth = port->set_rate_meth;
                this->set_remote_context(port->get_context());
            }
            else
            {
                this->remote_set_rate_mux_id = port->set_rate_mux_id;
                this->remote_set_rate_meth = (void (*)(vp::Block *, int)) & AudioSlave::set_rate_muxed_stub;

                this->set_remote_context(this);
                this->remote_context_for_mux = (vp::Component *)port->get_context();
            }
        }
    }

    inline void AudioSlave::set_rate_muxed_stub(AudioSlave *_this, int rate)
    {
        _this->set_rate_meth_mux(_this->remote_context_for_mux, rate, _this->remote_set_rate_mux_id);
    }

};
