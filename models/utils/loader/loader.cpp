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

#include <vp/vp.hpp>
#include <vp/itf/io.hpp>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "elf.h"


#define MAX_CHUNK_SIZE 0x8000


class Section
{
public:
    Section(uint64_t paddr, void *data, size_t size) :
        paddr(paddr), data(data), size(size) {}

    uint64_t paddr;
    void *data;
    size_t size;    
};


class loader : public vp::component
{

public:
    loader(js::config *config);

    int build();
    void start();
    void reset(bool active);

    static void grant(void *_this, vp::io_req *req);
    static void response(void *_this, vp::io_req *req);

private:
    static void event_handler(void *__this, vp::clock_event *event);
    bool load_elf(const char* file, uint64_t *entry);
    bool load_elf32(unsigned char* file, uint64_t *entry);
    bool load_elf64(unsigned char* file, uint64_t *entry);
    void section_copy(uint64_t paddr, void *data, size_t size);
    void section_clear(uint64_t paddr, size_t size);

    vp::trace trace;
    std::list<Section *> sections;
    vp::clock_event *event;
    vp::io_master out_itf;
    vp::wire_master<bool> start_itf;
    vp::wire_master<uint64_t> entry_itf;
    vp::io_req req;
    uint64_t entry;

    int64_t req_size;
    int64_t chunk_size;
    int iterations;
};



loader::loader(js::config *config)
    : vp::component(config)
{
}



void loader::grant(void *_this, vp::io_req *req)
{
}



void loader::response(void *__this, vp::io_req *req)
{
}



int loader::build()
{
    traces.new_trace("trace", &trace, vp::DEBUG);

    this->out_itf.set_resp_meth(&loader::response);
    this->out_itf.set_grant_meth(&loader::grant);
    new_master_port("out", &this->out_itf);

    new_master_port("start", &this->start_itf);

    new_master_port("entry", &this->entry_itf);

    this->event = this->event_new(loader::event_handler);

    this->req_size   = 0;
    this->chunk_size = 0;
    this->iterations = 0;

    return 0;
}



void loader::start()
{
}

void loader::reset(bool active)
{
    if (!active)
    {
        for (auto x:this->get_js_config()->get("binary")->get_elems())
        {
            this->load_elf(x->get_str().c_str(), &this->entry);
        }

        if (this->sections.size() > 0)
        {
            this->event_enqueue(this->event, 1);
        }
    }
}


void loader::event_handler(void *__this, vp::clock_event *event)
{
    loader *_this = (loader *)__this;
    if (_this->sections.size() > 0)
    {
        Section *section = _this->sections.front();

        if(_this->req_size == 0)
        {
            _this->req_size = section->size;
            _this->chunk_size = MAX_CHUNK_SIZE;
            _this->iterations = 0;
        }

        if(_this->req_size)
        {
            if(_this->req_size > MAX_CHUNK_SIZE)
            {
                _this->req_size -= _this->chunk_size;
            }
            else
            {
                _this->chunk_size = _this->req_size;
                _this->req_size = 0;
            }

            uint8_t data[_this->chunk_size];

            _this->trace.msg(vp::trace::LEVEL_DEBUG, "Starting section (addr: 0x%x, data: %p, size: 0x%x)\n",
                section->paddr + _this->iterations*MAX_CHUNK_SIZE, (uint8_t *)section->data + _this->iterations*MAX_CHUNK_SIZE, _this->chunk_size);

            _this->req.init();
            _this->req.set_addr(section->paddr + _this->iterations*MAX_CHUNK_SIZE);
            _this->req.set_size(_this->chunk_size);
            _this->req.set_is_write(true);

            if ((uint8_t *)section->data + _this->iterations*MAX_CHUNK_SIZE)
            {
                _this->req.set_data((uint8_t *)section->data + _this->iterations*MAX_CHUNK_SIZE);
            }
            else
            {
                memset(data, 0, _this->chunk_size);
                _this->req.set_data(data);
            }

            vp::io_req_status_e err = _this->out_itf.req(&_this->req);
            if (err == vp::IO_REQ_OK)
            {
                _this->trace.msg(vp::trace::LEVEL_DEBUG, "Section done (latency: %d)\n",
                    _this->req.get_full_latency());

                _this->event_enqueue(_this->event, std::max((int)_this->req.get_full_latency(), 1));
            }
            else
            {
                if (err == vp::IO_REQ_INVALID)
                {
                    _this->trace.force_warning("Received error during copy (addr: 0x%x, data: %p, size: 0x%x)\n",
                        section->paddr + _this->iterations*MAX_CHUNK_SIZE, (uint8_t *)section->data + _this->iterations*MAX_CHUNK_SIZE, _this->chunk_size);
                }
                else
                {
                    _this->trace.force_warning("Unimplemented synchronous requests in loader\n");
                }
            }

            _this->iterations++;
        }

        if(_this->req_size == 0)
        {
            _this->sections.pop_front();
        }
    }
    else
    {
        _this->entry_itf.sync(_this->entry);
        _this->start_itf.sync(true);
    }
}



bool loader::load_elf(const char* file, uint64_t *entry)
{
    int fd = open(file, O_RDONLY);
    struct stat s;
    if (fstat(fd, &s) < 0)
    {
        this->trace.force_warning("Unable to open binary (path: %s, error: %s)\n", strerror(errno));
        return true;
    }
    size_t size = s.st_size;

    unsigned char* buf = (unsigned char*)mmap(NULL, s.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (buf == MAP_FAILED)
    {
        this->trace.force_warning("Unable to open binary (path: %s, error: %s)\n", strerror(errno));
        close(fd);
        return true;
    }
    close(fd);

    if (buf[EI_CLASS] == ELFCLASS32)
    {
        return this->load_elf32(buf, entry);
    }
    else
    {
        return this->load_elf64(buf, entry);
    }
}



void loader::section_copy(uint64_t paddr, void *data, size_t size)
{
    this->sections.push_back(new Section(paddr, data, size));
}



void loader::section_clear(uint64_t paddr, size_t size)
{
    this->sections.push_back(new Section(paddr, NULL, size));
}



bool loader::load_elf32(unsigned char* file, uint64_t *entry)
{
    Elf32_Ehdr *header = (Elf32_Ehdr *) file;
    Elf32_Phdr *pg_headers = (Elf32_Phdr *) &file[header->e_phoff];

    for (int i=0; i<header->e_phnum; i++)
    {
        Elf32_Phdr *segment = &pg_headers[i];
        if (segment->p_type == PT_LOAD && segment->p_memsz)
        {
            this->section_copy(segment->p_paddr, &file[segment->p_offset], segment->p_filesz);
            if (segment->p_filesz < segment->p_memsz)
            {
                this->section_clear(segment->p_paddr + segment->p_filesz, segment->p_memsz - segment->p_filesz);
            }
        }
    }

    *entry = header->e_entry;

    return false;
}



bool loader::load_elf64(unsigned char* file, uint64_t *entry)
{
    Elf64_Ehdr *header = (Elf64_Ehdr *) file;
    Elf64_Phdr *pg_headers = (Elf64_Phdr *) &file[header->e_phoff];

    for (int i=0; i<header->e_phnum; i++)
    {
        Elf64_Phdr *segment = &pg_headers[i];
        if (segment->p_type == PT_LOAD && segment->p_filesz)
        {
            this->section_copy(segment->p_paddr, &file[segment->p_offset], segment->p_filesz);
            if (segment->p_filesz < segment->p_memsz)
            {
                this->section_clear(segment->p_paddr + segment->p_filesz, segment->p_memsz - segment->p_filesz);
            }
        }
    }

    *entry = header->e_entry;

    return 0;
}




extern "C" vp::component *vp_constructor(js::config *config)
{
  return new loader(config);
}
