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


class Section
{
public:
    Section(uint64_t paddr, uint8_t *data, size_t size) :
        paddr(paddr), data(data), size(size) {}

    uint64_t paddr;
    uint8_t *data;
    size_t size;
};


class loader : public vp::Component
{

public:
    loader(vp::ComponentConf &conf);

    void reset(bool active);

    static void grant(vp::Block *__this, vp::IoReq *req);
    static void response(vp::Block *__this, vp::IoReq *req);

private:
    static void event_handler(vp::Block *__this, vp::ClockEvent *event);
    bool load_elf(const char* file, uint64_t *entry);
    bool load_elf32(unsigned char* file, uint64_t *entry);
    bool load_elf64(unsigned char* file, uint64_t *entry);
    void section_copy(uint64_t paddr, uint8_t *data, size_t size);
    void section_clear(uint64_t paddr, size_t size);

    vp::Trace trace;
    std::list<Section *> sections;
    vp::ClockEvent *event;
    vp::IoMaster out_itf;
    vp::WireMaster<bool> start_itf;
    vp::WireMaster<uint64_t> entry_itf;
    vp::IoReq req;
    uint64_t entry;
    bool is_32 = true;
    Section *current_section = NULL;
    uint64_t fetchen_value;
};



loader::loader(vp::ComponentConf &config)
    : vp::Component(config)
{
    traces.new_trace("trace", &trace, vp::DEBUG);

    this->out_itf.set_resp_meth(&loader::response);
    this->out_itf.set_grant_meth(&loader::grant);
    new_master_port("out", &this->out_itf);

    new_master_port("start", &this->start_itf);

    new_master_port("entry", &this->entry_itf);

    this->event = this->event_new(loader::event_handler);

}



void loader::grant(vp::Block *__this, vp::IoReq *req)
{
}



void loader::response(vp::Block *__this, vp::IoReq *req)
{
    loader *_this = (loader *)__this;
    _this->event_enqueue(_this->event, _this->req.get_full_latency());
}



void loader::reset(bool active)
{
    if (!active)
    {
        for (auto x:this->get_js_config()->get("binary")->get_elems())
        {
            this->load_elf(x->get_str().c_str(), &this->entry);
        }

        js::Config *entry_addr_conf = this->get_js_config()->get("entry_addr");
        if (entry_addr_conf != NULL)
        {
            uint64_t entry_addr = entry_addr_conf->get_int();
            this->section_copy(entry_addr, (uint8_t *)&this->entry, this->is_32 ? 4 : 8);
        }

        js::Config *fetchen_addr_conf = this->get_js_config()->get("fetchen_addr");
        if (fetchen_addr_conf != NULL)
        {
            uint64_t fetchen_addr = fetchen_addr_conf->get_int();
            this->fetchen_value = this->get_js_config()->get("fetchen_value")->get_int();
            this->section_copy(fetchen_addr, (uint8_t *)&this->fetchen_value, this->is_32 ? 4 : 8);
        }

        if (this->sections.size() > 0)
        {
            this->event_enqueue(this->event, 1);
        }
    }
}


void loader::event_handler(vp::Block *__this, vp::ClockEvent *event)
{
    loader *_this = (loader *)__this;
    if (_this->sections.size() > 0 && _this->current_section == NULL)
    {
        _this->current_section = _this->sections.front();
        _this->sections.pop_front();
        _this->trace.msg(vp::Trace::LEVEL_DEBUG, "Starting section (addr: 0x%x, data: %p, size: 0x%x)\n",
            _this->current_section->paddr, _this->current_section->data, _this->current_section->size);
    }

    if (_this->current_section != NULL)
    {
        int size = _this->current_section->size;
        uint64_t paddr = _this->current_section->paddr;
        uint8_t *data = (uint8_t *)_this->current_section->data;
        int64_t latency = 1;

        int itersize = std::min(size, 1 << 16);

        uint8_t buffer[itersize];

        _this->req.init();
        _this->req.set_addr(paddr);
        _this->req.set_size(itersize);
        _this->req.set_is_write(true);

        _this->trace.msg(vp::Trace::LEVEL_DEBUG, "Handling section chunk (addr: 0x%x, data: %p, size: 0x%x)\n",
            paddr, data, size);

        if (data)
        {
            _this->req.set_data((uint8_t *)data);
        }
        else
        {
            memset(buffer, 0, itersize);
            _this->req.set_data(buffer);
        }

        _this->current_section->paddr += itersize;
        if (data)
        {
            _this->current_section->data += itersize;
        }
        _this->current_section->size -= itersize;

        if (_this->current_section->size == 0)
        {
            _this->current_section = NULL;
        }

        vp::IoReqStatus err = _this->out_itf.req(&_this->req);
        if (err == vp::IO_REQ_OK)
        {
            latency += _this->req.get_full_latency();
            _this->event_enqueue(_this->event, latency);
        }
        else
        {
            if (err == vp::IO_REQ_INVALID)
            {
                _this->trace.force_warning("Received error during copy (addr: 0x%x, data: %p, size: 0x%x)\n",
                    paddr, data, itersize);
            }
        }
    }
    else
    {
        js::Config *entry_conf = _this->get_js_config()->get("entry");
        if (entry_conf != NULL)
        {
            _this->entry = entry_conf->get_int();
        }

        if (_this->entry_itf.is_bound())
        {
            _this->trace.msg(vp::Trace::LEVEL_DEBUG, "Sending entry (addr: 0x%x)\n", _this->entry);
            _this->entry_itf.sync(_this->entry);
        }
        if (_this->start_itf.is_bound())
        {
            _this->trace.msg(vp::Trace::LEVEL_DEBUG, "Sending start\n");
            _this->start_itf.sync(true);
        }
    }
}



bool loader::load_elf(const char* file, uint64_t *entry)
{
    int fd = open(file, O_RDONLY);
    struct stat s;
    if (fstat(fd, &s) < 0)
    {
        this->trace.force_warning("Unable to open binary (path: %s, error: %s)\n", file, strerror(errno));
        return true;
    }
    size_t size = s.st_size;

    unsigned char* buf = (unsigned char*)mmap(NULL, s.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (buf == MAP_FAILED)
    {
        this->trace.force_warning("Unable to open binary (path: %s, error: %s)\n", file, strerror(errno));
        close(fd);
        return true;
    }
    close(fd);

    this->is_32 = buf[EI_CLASS] == ELFCLASS32;

    if (buf[EI_CLASS] == ELFCLASS32)
    {
        return this->load_elf32(buf, entry);
    }
    else
    {
        return this->load_elf64(buf, entry);
    }
}



void loader::section_copy(uint64_t paddr, uint8_t *data, size_t size)
{
    if (size > 0)
    {
        this->sections.push_back(new Section(paddr, data, size));
    }
}



void loader::section_clear(uint64_t paddr, size_t size)
{
    if (size > 0)
    {
        this->sections.push_back(new Section(paddr, NULL, size));
    }
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




extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
  return new loader(config);
}
