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

#include <gv/gvsoc.hpp>
#include <algorithm>
#include <dlfcn.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>




int main(int argc, char *argv[])
{
    char *config_path = NULL;
    bool open_proxy = false;

    for (int i=1; i<argc; i++)
    {
        if (strncmp(argv[i], "--config=", 9) == 0)
        {
            config_path = &argv[i][9];
        }
        else if (strcmp(argv[i], "--proxy") == 0)
        {
            open_proxy = true;
        }
    }

    if (config_path == NULL)
    {
        fprintf(stderr, "No configuration specified, please specify through option --config=<config path>.\n");
        return -1;
    }

    gv::GvsocConf conf = { .config_path=config_path };
    gv::Gvsoc *gvsoc = gv::gvsoc_new(&conf);
    gvsoc->open();
    gvsoc->start();

    if (conf.proxy_socket != -1)
    {
        printf("Opened proxy on socket %d\n", conf.proxy_socket);
    }

    gvsoc->run();
    int retval = gvsoc->join();

    gvsoc->stop();
    gvsoc->close();

    return retval;
}