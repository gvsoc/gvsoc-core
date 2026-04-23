// SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
//
// SPDX-License-Identifier: Apache-2.0
//
// Authors: Germain Haugou (germain.haugou@gmail.com)
//
// ELF binary loader on the io_v2 protocol.
//
// Direct port of loader.cpp to io_v2. The model mmap's one or more ELF
// binaries from the host filesystem, walks their PT_LOAD segments, and
// streams the section data into simulated memory through its ``out``
// master port. Each chunk is up to 64 KiB; one chunk is in flight at
// any time. After the last chunk lands, the loader optionally writes
// the entry address to a configured location and/or a fetch-enable
// value to another location, then pulses ``entry`` and ``start`` wires
// so downstream cores can start executing.
//
// What changed vs v1:
//   - ``out`` port is an io_v2 master. Replies travel back through its
//     ``resp()`` callback, and a DENY is resolved via the output's
//     ``retry()`` callback.
//   - Status codes are ``IO_REQ_DONE`` / ``IO_REQ_GRANTED`` /
//     ``IO_REQ_DENIED``. The response status
//     (``IO_RESP_OK`` / ``IO_RESP_INVALID``) is consumed when the
//     downstream replies.
//   - Section pointer advances only on success: on a DENY the chunk
//     stays pending, and is re-sent from the output's retry callback
//     with the same (addr, data, size) — guaranteeing no byte is lost
//     or duplicated.
//   - Error responses (``IO_RESP_INVALID``) no longer hang the loader:
//     the chunk is considered consumed, a warning is emitted, and the
//     next chunk is scheduled normally (v1 would stall forever on any
//     INVALID).
//   - The ``bss`` zero-fill buffer is a persistent member of the
//     component rather than a stack array, so the downstream may
//     consume it safely even if it defers the copy until after
//     ``req()`` returns.
//
// Timing model: big-packet. The loader does not annotate latency
// itself; it simply paces itself by the downstream's ``req->latency``
// annotation (sync path) or the wall-clock time at which ``resp()``
// fires (async path). Between one chunk's completion and the next
// one's issue, a single idle cycle is added so the simulator has a
// chance to advance other components.

#include <cstring>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <list>
#include <memory>
#include <vector>
#include <vp/vp.hpp>
#include <vp/itf/io_v2.hpp>

#include "elf.h"


class Section
{
public:
    Section(uint64_t paddr, uint8_t *data, size_t size)
        : paddr(paddr), data(data), size(size) {}

    uint64_t  paddr;
    uint8_t  *data;     // nullptr for bss/zero-fill
    size_t    size;
};


class Loader : public vp::Component
{
public:
    Loader(vp::ComponentConf &conf);
    void reset(bool active) override;

private:
    static void event_handler(vp::Block *__this, vp::ClockEvent *event);
    static void output_resp(vp::Block *__this, vp::IoReq *req);
    static void output_retry(vp::Block *__this);

    bool load_elf(const char *file, uint64_t *entry);
    bool load_elf32(unsigned char *file, uint64_t *entry);
    bool load_elf64(unsigned char *file, uint64_t *entry);
    void section_copy(uint64_t paddr, uint8_t *data, size_t size);
    void section_clear(uint64_t paddr, size_t size);

    // Emit the current chunk on the output. Called from event_handler
    // (initial attempt) and from output_retry (after a DENY).
    void send_chunk();
    // Called after any successful completion (DONE or deferred resp) of
    // the in-flight chunk. Advances the section pointer and schedules
    // the next event.
    void chunk_completed();

    vp::Trace trace;
    vp::IoMaster out_itf{&Loader::output_retry, &Loader::output_resp};
    vp::WireMaster<bool>     start_itf;
    vp::WireMaster<uint64_t> entry_itf;

    vp::IoReq         req;            // Reused across chunks.
    vp::ClockEvent   *event = nullptr;

    std::list<std::unique_ptr<Section>> sections;
    Section *current_section = nullptr;

    // In-flight chunk snapshot. Populated when a chunk is issued; used to
    // replay the same transfer on a downstream DENY and to drive the
    // section advance on completion.
    uint64_t  chunk_paddr = 0;
    uint8_t  *chunk_data  = nullptr;  // nullptr if the chunk is a clear
    size_t    chunk_size  = 0;
    bool      chunk_in_flight = false;

    // Persistent all-zero buffer used for ``bss`` sections. Sized once on
    // construction; safe to point downstream at even if it defers the
    // copy until after the req() call returns.
    std::vector<uint8_t> zero_buffer;

    uint64_t  entry = 0;
    bool      is_32 = true;
    uint64_t  fetchen_value = 0;

    static constexpr size_t MAX_CHUNK = 1 << 16;   // 64 KiB
};


Loader::Loader(vp::ComponentConf &config)
    : vp::Component(config)
{
    this->traces.new_trace("trace", &this->trace, vp::DEBUG);

    this->new_master_port("out",   &this->out_itf);
    this->new_master_port("start", &this->start_itf);
    this->new_master_port("entry", &this->entry_itf);

    this->event = this->event_new(&Loader::event_handler);

    this->zero_buffer.assign(MAX_CHUNK, 0);
}


void Loader::reset(bool active)
{
    if (!active)
    {
        // Fresh start: load every configured binary, then enqueue any
        // extra entries (entry_addr / fetchen_addr) before kicking off
        // the streaming event.
        for (auto x : this->get_js_config()->get("binary")->get_elems())
        {
            this->load_elf(x->get_str().c_str(), &this->entry);
        }

        js::Config *entry_addr_conf = this->get_js_config()->get("entry_addr");
        if (entry_addr_conf != NULL)
        {
            uint64_t entry_addr = (uint64_t)entry_addr_conf->get_int();
            this->section_copy(entry_addr, (uint8_t *)&this->entry,
                               this->is_32 ? 4 : 8);
        }

        js::Config *fetchen_addr_conf = this->get_js_config()->get("fetchen_addr");
        if (fetchen_addr_conf != NULL)
        {
            uint64_t fetchen_addr = (uint64_t)fetchen_addr_conf->get_int();
            this->fetchen_value = (uint64_t)
                this->get_js_config()->get("fetchen_value")->get_int();
            this->section_copy(fetchen_addr, (uint8_t *)&this->fetchen_value,
                               this->is_32 ? 4 : 8);
        }

        if (!this->sections.empty())
        {
            this->event_enqueue(this->event, 1);
        }
    }
}


void Loader::section_copy(uint64_t paddr, uint8_t *data, size_t size)
{
    if (size > 0)
    {
        this->sections.push_back(std::make_unique<Section>(paddr, data, size));
    }
}

void Loader::section_clear(uint64_t paddr, size_t size)
{
    if (size > 0)
    {
        this->sections.push_back(std::make_unique<Section>(paddr, nullptr, size));
    }
}


void Loader::send_chunk()
{
    this->req.prepare();
    this->req.set_addr(this->chunk_paddr);
    this->req.set_size((uint64_t)this->chunk_size);
    this->req.set_opcode(vp::WRITE);
    this->req.set_resp_status(vp::IO_RESP_OK);
    this->req.set_data(this->chunk_data != nullptr
                           ? this->chunk_data
                           : this->zero_buffer.data());

    this->chunk_in_flight = true;

    vp::IoReqStatus st = this->out_itf.req(&this->req);

    if (st == vp::IO_REQ_DONE)
    {
        this->chunk_in_flight = false;
        if (this->req.get_resp_status() == vp::IO_RESP_INVALID)
        {
            // A downstream error is logged but not fatal — the loader
            // intentionally moves on to the next chunk so a single bad
            // memory region does not deadlock boot.
            this->trace.force_warning_no_error(
                "Received error during copy (addr: 0x%llx, data: %p, size: 0x%llx)\n",
                (unsigned long long)this->chunk_paddr,
                this->chunk_data,
                (unsigned long long)this->chunk_size);
        }
        this->chunk_completed();
    }
    else if (st == vp::IO_REQ_DENIED)
    {
        // Hold the chunk; output_retry will re-enter send_chunk.
        // chunk_in_flight stays true so output_retry knows there is work
        // to redo; the section pointer is left untouched.
    }
    // IO_REQ_GRANTED: wait for output_resp to drive chunk_completed.
}


void Loader::chunk_completed()
{
    // Advance the section pointer past the chunk that just landed. If
    // the section is drained, release it.
    if (this->current_section != nullptr)
    {
        this->current_section->paddr += this->chunk_size;
        if (this->current_section->data != nullptr)
        {
            this->current_section->data += this->chunk_size;
        }
        this->current_section->size -= this->chunk_size;

        if (this->current_section->size == 0)
        {
            this->current_section = nullptr;
        }
    }

    // Schedule the next iteration. One cycle plus whatever latency the
    // downstream annotated on the request object.
    int64_t latency = 1 + this->req.get_latency();
    if (latency < 1) latency = 1;
    this->event_enqueue(this->event, latency);
}


void Loader::event_handler(vp::Block *__this, vp::ClockEvent *event)
{
    Loader *_this = (Loader *)__this;

    // Pick up the next section if there is none in progress.
    if (_this->current_section == nullptr && !_this->sections.empty())
    {
        auto next = std::move(_this->sections.front());
        _this->sections.pop_front();
        _this->current_section = next.release();  // freed on completion
        _this->trace.msg(vp::Trace::LEVEL_DEBUG,
            "Starting section (addr: 0x%llx, data: %p, size: 0x%llx)\n",
            (unsigned long long)_this->current_section->paddr,
            _this->current_section->data,
            (unsigned long long)_this->current_section->size);
    }

    if (_this->current_section != nullptr)
    {
        size_t iter_size = std::min(_this->current_section->size, MAX_CHUNK);
        _this->chunk_paddr = _this->current_section->paddr;
        _this->chunk_data  = _this->current_section->data;   // may be nullptr
        _this->chunk_size  = iter_size;

        _this->trace.msg(vp::Trace::LEVEL_DEBUG,
            "Handling section chunk (addr: 0x%llx, data: %p, size: 0x%llx)\n",
            (unsigned long long)_this->chunk_paddr,
            _this->chunk_data,
            (unsigned long long)_this->chunk_size);

        _this->send_chunk();
        return;
    }

    // All sections delivered — delete the last completed section object
    // (owned raw pointer) and fire the finalisation wires.
    // Note: current_section is already null here; nothing to free.

    js::Config *entry_conf = _this->get_js_config()->get("entry");
    if (entry_conf != nullptr)
    {
        _this->entry = (uint64_t)entry_conf->get_int();
    }

    if (_this->entry_itf.is_bound())
    {
        _this->trace.msg(vp::Trace::LEVEL_DEBUG,
            "Sending entry (addr: 0x%llx)\n", (unsigned long long)_this->entry);
        _this->entry_itf.sync(_this->entry);
    }
    if (_this->start_itf.is_bound())
    {
        _this->trace.msg(vp::Trace::LEVEL_DEBUG, "Sending start\n");
        _this->start_itf.sync(true);
    }
}


void Loader::output_resp(vp::Block *__this, vp::IoReq *req)
{
    Loader *_this = (Loader *)__this;
    _this->chunk_in_flight = false;

    if (req->get_resp_status() == vp::IO_RESP_INVALID)
    {
        _this->trace.force_warning_no_error(
            "Received error during copy (addr: 0x%llx, size: 0x%llx)\n",
            (unsigned long long)_this->chunk_paddr,
            (unsigned long long)_this->chunk_size);
    }
    _this->chunk_completed();
}


void Loader::output_retry(vp::Block *__this)
{
    Loader *_this = (Loader *)__this;
    if (_this->chunk_in_flight)
    {
        // Replay the same chunk — send_chunk re-uses chunk_paddr /
        // chunk_data / chunk_size, which were never advanced on DENY.
        _this->send_chunk();
    }
}


// ---------------------------------------------------------------------------
// ELF loading
// ---------------------------------------------------------------------------

bool Loader::load_elf(const char *file, uint64_t *entry)
{
    int fd = open(file, O_RDONLY);
    struct stat s;
    if (fstat(fd, &s) < 0)
    {
        this->trace.force_warning_no_error(
            "Unable to open binary (path: %s, error: %s)\n", file, strerror(errno));
        return true;
    }
    size_t size = s.st_size;

    unsigned char *buf = (unsigned char *)mmap(NULL, size, PROT_READ,
                                                MAP_PRIVATE, fd, 0);
    if (buf == MAP_FAILED)
    {
        this->trace.force_warning_no_error(
            "Unable to open binary (path: %s, error: %s)\n", file, strerror(errno));
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


bool Loader::load_elf32(unsigned char *file, uint64_t *entry)
{
    Elf32_Ehdr *header = (Elf32_Ehdr *)file;
    Elf32_Phdr *pg_headers = (Elf32_Phdr *)&file[header->e_phoff];

    for (int i = 0; i < header->e_phnum; i++)
    {
        Elf32_Phdr *segment = &pg_headers[i];
        if (segment->p_type == PT_LOAD && segment->p_memsz)
        {
            this->section_copy(segment->p_paddr,
                               &file[segment->p_offset],
                               segment->p_filesz);
            if (segment->p_filesz < segment->p_memsz)
            {
                this->section_clear(segment->p_paddr + segment->p_filesz,
                                    segment->p_memsz - segment->p_filesz);
            }
        }
    }

    *entry = header->e_entry;
    return false;
}


bool Loader::load_elf64(unsigned char *file, uint64_t *entry)
{
    Elf64_Ehdr *header = (Elf64_Ehdr *)file;
    Elf64_Phdr *pg_headers = (Elf64_Phdr *)&file[header->e_phoff];

    for (int i = 0; i < header->e_phnum; i++)
    {
        Elf64_Phdr *segment = &pg_headers[i];
        if (segment->p_type == PT_LOAD && segment->p_filesz)
        {
            this->section_copy(segment->p_paddr,
                               &file[segment->p_offset],
                               segment->p_filesz);
            if (segment->p_filesz < segment->p_memsz)
            {
                this->section_clear(segment->p_paddr + segment->p_filesz,
                                    segment->p_memsz - segment->p_filesz);
            }
        }
    }

    *entry = header->e_entry;
    return false;
}


extern "C" vp::Component *gv_new(vp::ComponentConf &config)
{
    return new Loader(config);
}
