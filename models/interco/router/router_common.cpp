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

#include "router_common.hpp"
#include <vp/proxy.hpp>

RouterCommon::RouterCommon(vp::ComponentConf &config)
: vp::Component(config)
{
    this->proxy_req.set_data(new uint8_t[4]);
}


std::string RouterCommon::handle_command(gv::GvProxy *proxy, FILE *req_file,
    FILE *reply_file, std::vector<std::string> args, std::string cmd_req)
{
    if (args[0] == "mem_write" || args[0] == "mem_read")
    {
        int error = 0;
        bool is_write = args[0] == "mem_write";
        long long int addr = strtoll(args[1].c_str(), NULL, 0);
        long long int size = strtoll(args[2].c_str(), NULL, 0);

        uint8_t *buffer = new uint8_t[size];

        if (is_write)
        {
            int read_size = fread(buffer, 1, size, req_file);
            if (read_size != size)
            {
                error = 1;
            }
        }

        IO_REQ *req = &this->proxy_req;
#if !defined(CONFIG_GVSOC_ROUTER_IO_ACC)
        req->init();
#endif
        req->set_data((uint8_t *)buffer);
        req->set_is_write(is_write);
        req->set_size(size);
        req->set_addr(addr);
#if !defined(CONFIG_GVSOC_ROUTER_IO_ACC)
        req->set_debug(true);
#endif

        IO_REQ_STATUS result = this->handle_req(req, 0);
        // TODO should be ported to IO acc
#if !defined(CONFIG_GVSOC_ROUTER_IO_ACC)
        error |= result != vp::IO_REQ_OK;
#endif

        if (!is_write)
        {
            error = proxy->send_payload(reply_file, cmd_req, buffer, size);
        }

        delete[] buffer;

        return "err=" + std::to_string(error);
    }
    return "err=1";
}
