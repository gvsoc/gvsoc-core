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

#include <vp/vp.hpp>
#if defined(CONFIG_GVSOC_ROUTER_IO_ACC)
#include <vp/itf/io_acc.hpp>
#else
#include <vp/itf/io.hpp>
#endif

class RouterCommon : public vp::Component
{
public:
    RouterCommon(vp::ComponentConf &conf);

    std::string handle_command(gv::GvProxy *proxy, FILE *req_file,
        FILE *reply_file, std::vector<std::string> args, std::string cmd_req) override;

private:
    virtual IO_REQ_STATUS handle_req(IO_REQ *req, int port) = 0;

    IO_REQ proxy_req;
};
