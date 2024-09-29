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
#include <cpu/iss/include/iss.hpp>
#include <algorithm>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <vp/itf/io.hpp>

#ifndef O_BINARY
#define O_BINARY 0
#endif

Syscalls::Syscalls(IssWrapper &top, Iss &iss)
    : iss(iss), htif(top, iss)
{
}

class IssWrapper;

void Syscalls::build()
{
    iss.top.traces.new_trace("syscalls", &this->trace, vp::DEBUG);

    for (int i = 0; i < 32; i++)
    {
        this->pcer_info[i].name = "";
    }

    this->htif.build();
}

void Syscalls::reset(bool active)
{
  this->htif.reset(active);
}


void Syscalls::handle_ebreak()
{
    int id = this->iss.regfile.regs[10];

    switch (id)
    {
        // TODO deprecated, should be removed
#if 0
    case GV_SEMIHOSTING_TRACE_OPEN: {
      int result = -1;
      std::string path = this->iss.read_user_string(this->iss.regfile.regs[11]);
      if (path == "")
      {
        this->trace.force_warning("Invalid user string while opening trace (addr: 0x%x)\n", this->iss.regfile.regs[11]);
      }
      else
      {
        vp::Trace *trace = this->iss.top.traces.get_trace_engine()->get_trace_from_path(path);
        if (trace == NULL)
        {
          this->trace.force_warning("Invalid trace (path: %s)\n", path.c_str());
        }
        else
        {
          this->iss.trace.msg("Opened trace (path: %s, id: %d)\n", path.c_str(), trace->id);
          result = trace->id;
        }
      }

      this->iss.regfile.regs[10] = result;

      break;
    }
    
    case GV_SEMIHOSTING_TRACE_ENABLE: {
      int id = this->iss.regfile.regs[11];
      vp::Trace *trace = this->iss.top.traces.get_trace_engine()->get_trace_from_id(id);
      if (trace == NULL)
      {
        this->trace.force_warning("Unknown trace ID while dumping trace (id: %d)\n", id);
      }
      else
      {
        trace->set_active(this->iss.regfile.regs[12]);
      }

      break; 
    }
#endif

    default:
        this->trace.force_warning("Unknown ebreak call (id: %d)\n", id);
        break;
    }
}

bool Syscalls::user_access(iss_addr_t addr, uint8_t *buffer, iss_addr_t size, bool is_write)
{
    vp::IoReq *req = &this->iss.lsu.io_req;
    std::string str = "";
    while (size != 0)
    {
        req->init();
        req->set_debug(true);
        req->set_addr(addr);
        req->set_size(1);
        req->set_is_write(is_write);
        req->set_data(buffer);
        int err = this->iss.lsu.data.req(req);
        if (err != vp::IO_REQ_OK)
        {
            if (err == vp::IO_REQ_INVALID)
                this->trace.fatal("Invalid IO response during debug request\n");
            else
                this->trace.fatal("Pending IO response during debug request\n");

            return true;
        }

        int64_t latency = req->get_full_latency();
        if (latency > this->latency)
        {
            this->latency = latency;
        }

        addr++;
        size--;
        buffer++;
    }

    return false;
}

std::string Syscalls::read_user_string(iss_addr_t addr, int size)
{
    vp::IoReq *req = &this->iss.lsu.io_req;
    std::string str = "";
    while (size != 0)
    {
        uint8_t buffer;
        req->init();
        req->set_debug(true);
        req->set_addr(addr);
        req->set_size(1);
        req->set_is_write(false);
        req->set_data(&buffer);
        int err = this->iss.lsu.data.req(req);
        if (err != vp::IO_REQ_OK)
        {
            if (err == vp::IO_REQ_INVALID)
                return "";
            else
                this->trace.fatal("Pending IO response during debug request\n");
        }

        if (buffer == 0)
            return str;

        str += buffer;
        addr++;

        if (size > 0)
            size--;
    }

    return str;
}

static const int open_modeflags[12] = {
    O_RDONLY,
    O_RDONLY | O_BINARY,
    O_RDWR,
    O_RDWR | O_BINARY,
    O_WRONLY | O_CREAT | O_TRUNC,
    O_WRONLY | O_CREAT | O_TRUNC | O_BINARY,
    O_RDWR | O_CREAT | O_TRUNC,
    O_RDWR | O_CREAT | O_TRUNC | O_BINARY,
    O_WRONLY | O_CREAT | O_APPEND,
    O_WRONLY | O_CREAT | O_APPEND | O_BINARY,
    O_RDWR | O_CREAT | O_APPEND,
    O_RDWR | O_CREAT | O_APPEND | O_BINARY};

void Syscalls::handle_riscv_ebreak()
{
    int id = this->iss.regfile.regs[10];
    this->latency = 0;

    switch (id)
    {
    case 0x4:
    {
        std::string path = this->read_user_string(this->iss.regfile.regs[11]);
        printf("%s", path.c_str());
        break;
    }

    case 0x1:
    {
        iss_reg_t args[3];
        if (this->user_access(this->iss.regfile.regs[11], (uint8_t *)args, sizeof(args), false))
        {
            this->iss.regfile.regs[10] = -1;
            return;
        }
        std::string path = this->read_user_string(args[0], args[2]);

        unsigned int mode = args[1];

        this->iss.regfile.regs[10] = open(path.c_str(), open_modeflags[mode], 0644);

        if (this->iss.regfile.regs[10] == -1)
            this->trace.force_warning("Caught error during semi-hosted call (name: open, path: %s, mode: 0x%x, error: %s)\n", path.c_str(), mode, strerror(errno));

        break;
    }

    case 0x2:
    {
        this->iss.regfile.regs[10] = close(this->iss.regfile.regs[11]);
        break;
    }


    case 0x5:
    {
        iss_reg_t args[3];
        if (this->user_access(this->iss.regfile.regs[11], (uint8_t *)args, sizeof(args), false))
        {
            this->iss.regfile.regs[10] = -1;
            return;
        }

        uint8_t buffer[1024];
        int size = args[2];
        iss_reg_t addr = args[1];
        while (size)
        {
            int iter_size = 1024;
            if (size < 1024)
                iter_size = size;

            if (this->user_access(addr, buffer, iter_size, false))
            {
                this->iss.regfile.regs[10] = -1;
                return;
            }

            if (write(args[0], (void *)(long)buffer, iter_size) != iter_size)
                break;

            fsync(args[0]);

            size -= iter_size;
            addr += iter_size;
        }

        this->iss.regfile.regs[10] = size;
        break;
    }

    case 0x6:
    {
        iss_reg_t args[3];
        if (this->user_access(this->iss.regfile.regs[11], (uint8_t *)args, sizeof(args), false))
        {
            this->iss.regfile.regs[10] = -1;
            return;
        }

        uint8_t buffer[1024];
        int size = args[2];
        iss_reg_t addr = args[1];
        while (size)
        {
            int iter_size = 1024;
            if (size < 1024)
                iter_size = size;

            int read_size = read(args[0], (void *)(long)buffer, iter_size);

            if (read_size <= 0)
            {
                if (read_size < 0)
                {
                    this->iss.regfile.regs[10] = -1;
                    return;
                }
                else
                {
                    break;
                }
            }

            if (this->user_access(addr, buffer, read_size, true))
            {
                this->iss.regfile.regs[10] = -1;
                return;
            }

            size -= read_size;
            addr += read_size;
        }

        this->iss.regfile.regs[10] = size;

        break;
    }

    case 0x3:
    {
        iss_reg_t args[1];
        if (this->user_access(this->iss.regfile.regs[11], (uint8_t *)args, sizeof(args), false))
        {
            this->iss.regfile.regs[10] = -1;
            return;
        }
        putchar(args[0]);
        break;
    }

    case 0x7:
    {
        this->iss.regfile.regs[10] = getchar();
        break;
    }

    case 0xA:
    {
        iss_reg_t args[2];
        if (this->user_access(this->iss.regfile.regs[11], (uint8_t *)args, sizeof(args), false))
        {
            this->iss.regfile.regs[10] = -1;
            return;
        }

        int pos = lseek(args[0], args[1], SEEK_SET);
        this->iss.regfile.regs[10] = pos != args[1];
        break;
    }

    case 0x18:
    {
        int status = this->iss.regfile.regs[11] == 0x20026 ? 0 : 1;

        this->iss.top.time.get_engine()->quit(status & 0x7fffffff);

        break;
    }

    case 0x0C:
    {
        struct stat buf;
        fstat(this->iss.regfile.regs[11], &buf);
        this->iss.regfile.regs[10] = buf.st_size;
        break;
    }

    case 0x100:
    {
        iss_reg_t args[2];
        if (this->user_access(this->iss.regfile.regs[11], (uint8_t *)args, sizeof(args), false))
        {
            this->iss.regfile.regs[10] = -1;
            return;
        }
        std::string path = this->read_user_string(args[0]);
        if (path == "")
        {
            this->trace.force_warning("Invalid user string while opening trace (addr: 0x%x)\n", args[0]);
        }
        else
        {
            if (args[1])
            {
                this->iss.top.traces.get_trace_engine()->add_trace_path(0, path);
            }
            else
            {
                this->iss.top.traces.get_trace_engine()->add_exclude_trace_path(0, path);
            }
            this->iss.top.traces.get_trace_engine()->check_traces();
        }
        break;
    }

    case 0x101:
    {
        iss_reg_t args[1];
        if (this->user_access(this->iss.regfile.regs[11], (uint8_t *)args, sizeof(args), false))
        {
            this->iss.regfile.regs[10] = -1;
            return;
        }

        iss_csr_write(&this->iss, CSR_PCER, args[0]);

        break;
    }

    case 0x102:
    {
        iss_csr_write(&this->iss, CSR_PCCR(31), 0);
        this->cycle_count = 0;
        iss_reg_t value;
        iss_csr_read(&this->iss, CSR_PCMR, &value);
        if ((value & 1) == 1)
        {
            this->cycle_count_start = this->iss.top.clock.get_cycles();
        }
        break;
    }

    case 0x103:
    {
        iss_reg_t value;
        iss_csr_read(&this->iss, CSR_PCMR, &value);
        if ((value & 1) == 0)
        {
            this->cycle_count_start = this->iss.top.clock.get_cycles();
        }

        iss_csr_write(&this->iss, CSR_PCMR, 1);
        break;
    }

    case 0x104:
    {

        iss_reg_t value;
        iss_csr_read(&this->iss, CSR_PCMR, &value);
        if ((value & 1) == 1)
        {
            this->cycle_count += this->iss.top.clock.get_cycles() - this->cycle_count_start;
        }

        iss_csr_write(&this->iss, CSR_PCMR, 0);
        break;
    }

    case 0x105:
    {
        iss_reg_t args[1];
        if (this->user_access(this->iss.regfile.regs[11], (uint8_t *)args, sizeof(args), false))
        {
            return;
        }
        iss_csr_read(&this->iss, CSR_PCCR(args[0]), &this->iss.regfile.regs[10]);
        break;
    }

    case 0x106:
    {
        iss_reg_t args[2];
        if (this->user_access(this->iss.regfile.regs[11], (uint8_t *)args, sizeof(args), false))
        {
            this->iss.regfile.regs[10] = -1;
            return;
        }
        std::string path = this->read_user_string(args[0]);
        std::string mode = this->read_user_string(args[1]);
        if (path == "")
        {
            this->trace.force_warning("Invalid user string while opening trace (addr: 0x%x)\n", args[0]);
        }
        else
        {
            if (mode == "")
            {
                this->trace.force_warning("Invalid user string while opening trace (addr: 0x%x)\n", args[1]);
            }
            else
            {
                this->iss.regfile.regs[10] = 0;
                FILE *file = fopen(path.c_str(), mode.c_str());
                if (file == NULL)
                {
                    this->iss.regfile.regs[10] = -1;
                    return;
                }

                iss_reg_t value;
                iss_csr_read(&this->iss, CSR_PCMR, &value);
                if ((value & 1) == 1)
                {
                    this->cycle_count += this->iss.top.clock.get_cycles() - this->cycle_count_start;
                    this->cycle_count_start = this->iss.top.clock.get_cycles();
                }

                fprintf(file, "PCER values at timestamp %ld ps, duration %ld cycles\n", this->iss.top.time.get_time(), this->cycle_count);
                fprintf(file, "Index; Name; Description; Value\n");
                for (int i = 0; i < 31; i++)
                {
                    if (this->pcer_info[i].name != "")
                    {
                        iss_reg_t value;
                        iss_csr_read(&this->iss, CSR_PCCR(i), &value);
                        fprintf(file, "%d; %s; %s; %" PRIdREG "\n", i, this->pcer_info[i].name.c_str(), this->pcer_info[i].help.c_str(), value);
                    }
                }

                fclose(file);
            }
        }
        break;
    }

    case 0x107:
    {
        iss_reg_t args[1];
        if (this->user_access(this->iss.regfile.regs[11], (uint8_t *)args, sizeof(args), false))
        {
            this->iss.regfile.regs[10] = -1;
            return;
        }
        this->iss.top.traces.get_trace_engine()->set_global_enable(args[0]);
        break;
    }

    case 0x108:
    {
        iss_reg_t args[1];
        if (this->user_access(this->iss.regfile.regs[11], (uint8_t *)args, sizeof(args), false))
        {
            this->iss.regfile.regs[10] = -1;
            return;
        }

        int result = -1;
        std::string path = this->read_user_string(args[0]);
        if (path == "")
        {
            this->trace.force_warning("Invalid user string while opening VCD trace (addr: 0x%x)\n", args[0]);
        }
        else
        {
            vp::Trace *trace = this->iss.top.traces.get_trace_engine()->get_trace_from_path(path);
            if (trace == NULL)
            {
                this->trace.force_warning("Invalid VCD trace (path: %s)\n", path.c_str());
            }
            else
            {
                this->trace.msg("Opened VCD trace (path: %s, id: %d)\n", path.c_str(), trace->id);
                result = trace->id;
            }
        }

        this->iss.regfile.regs[10] = result;

        break;
    }

    case 0x109:
        break;

    case 0x10A:
    {
        iss_reg_t args[2];
        if (this->user_access(this->iss.regfile.regs[11], (uint8_t *)args, sizeof(args), false))
        {
            this->iss.regfile.regs[10] = -1;
            return;
        }

        int id = args[0];
        vp::Trace *trace = this->iss.top.traces.get_trace_engine()->get_trace_from_id(id);
        if (trace == NULL)
        {
            this->trace.force_warning("Unknown trace ID while dumping VCD trace (id: %d)\n", id);
        }
        else
        {
            if (trace->width > 32)
            {
                this->trace.force_warning("Trying to write to VCD trace whose width is bigger than 32 (id: %d)\n", id);
            }
            else
            {
                if (0)
                    trace->event_highz();
                else
                    trace->event((uint8_t *)&args[1]);
            }
        }

        break;
    }

    case 0x10C:
    {
        iss_reg_t args[1];
        if (this->user_access(this->iss.regfile.regs[11], (uint8_t *)args, sizeof(args), false))
        {
            this->iss.regfile.regs[10] = -1;
            return;
        }

        int id = args[0];
        vp::Trace *trace = this->iss.top.traces.get_trace_engine()->get_trace_from_id(id);
        if (trace == NULL)
        {
            this->trace.force_warning("Unknown trace ID while dumping VCD trace (id: %d)\n", id);
        }
        else
        {
            trace->event_highz();
        }

        break;
    }

    case 0x10B:
    {
        iss_reg_t args[2];
        if (this->user_access(this->iss.regfile.regs[11], (uint8_t *)args, sizeof(args), false))
        {
            this->iss.regfile.regs[10] = -1;
            return;
        }

        int id = args[0];
        vp::Trace *trace = this->iss.top.traces.get_trace_engine()->get_trace_from_id(id);
        if (trace == NULL)
        {
            this->trace.force_warning("Unknown trace ID while dumping VCD trace (id: %d)\n", id);
        }
        else
        {
            if (!trace->is_string)
            {
                this->trace.force_warning("Trying to write string to VCD trace which is not a string (id: %d)\n", id);
            }
            else
            {
                std::string path = this->read_user_string(args[1]);

                trace->event_string(path.c_str(), true);
            }
        }
        break;
    }

    case 0x10D:
    {
        this->iss.top.time.get_engine()->pause();

        break;
    }

    case 0x10E:
    {
        this->iss.trace.has_reg_dump = true;
        this->iss.trace.reg_dump = this->iss.regfile.regs[11];
        break;
    }

    case 0x10F:
    {
        this->iss.trace.has_reg_dump = false;
        break;
    }

    case 0x110:
    {
        this->iss.trace.has_str_dump = true;
        this->iss.trace.str_dump = this->read_user_string(this->iss.regfile.regs[11]);
        break;
    }

    case 0x111:
    {
        this->iss.trace.has_str_dump = false;
        break;
    }

    case 0x114:
    {
        this->iss.regfile.regs[10] = this->iss.memcheck.mem_alloc(
          this->iss.regfile.regs[11], this->iss.regfile.regs[12], this->iss.regfile.regs[13]);

        break;
    }

    case 0x115:
    {
        this->iss.regfile.regs[10] = this->iss.memcheck.mem_free(this->iss.regfile.regs[11],
          this->iss.regfile.regs[12],
          this->iss.regfile.regs[13]);

        break;
    }

    case 0x116:
    {
      const char *func, *inline_func, *file;
      int line;
      if (!iss_trace_pc_info(this->iss.exec.current_insn, &func, &inline_func, &file, &line))
      {
          this->iss.timing.user_func_trace_event.event_string(func, false);
          this->iss.timing.user_inline_trace_event.event_string(inline_func, false);
          this->iss.timing.user_file_trace_event.event_string(file, false);
          this->iss.timing.user_line_trace_event.event((uint8_t *)&line);
      }
      break;
    }

    default:
        this->trace.force_warning("Unknown ebreak call (id: %d)\n", id);
        break;
    }

    if (this->latency > 0)
    {
        this->iss.timing.stall_load_account(this->latency);
    }
}

#if 0
// TODO this is coming from stand-alone ISS. CHeck what should be reintegrated

#include "sa_iss.hpp"

#include <stdexcept>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <utime.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdarg.h>

#define IO_SIZE_MAX 32767
// #define IO_SIZE_MAX 500000
#define MAX_FNAME_LENGTH 1024

// Riscv SYScalls from libgloss/riscv/machine/syscall.h

#define RV_SYS_exit 93
#define RV_SYS_exit_group 94
#define RV_SYS_getpid 172
#define RV_SYS_kill 129
#define RV_SYS_read 63
#define RV_SYS_write 64
#define RV_SYS_open 1024
#define RV_SYS_openat 56
#define RV_SYS_close 57
#define RV_SYS_lseek 62
#define RV_SYS_brk 214
#define RV_SYS_link 1025
#define RV_SYS_unlink 1026
#define RV_SYS_mkdir 1030
#define RV_SYS_chdir 49
#define RV_SYS_getcwd 17
#define RV_SYS_stat 1038
#define RV_SYS_fstat 80
#define RV_SYS_lstat 1039
#define RV_SYS_fstatat 79
#define RV_SYS_access 1033
#define RV_SYS_faccessat 48
#define RV_SYS_pread 67
#define RV_SYS_pwrite 68
#define RV_SYS_uname 160
#define RV_SYS_getuid 174
#define RV_SYS_geteuid 175
#define RV_SYS_getgid 176
#define RV_SYS_getegid 177
#define RV_SYS_mmap 222
#define RV_SYS_munmap 215
#define RV_SYS_mremap 216
#define RV_SYS_time 1062
#define RV_SYS_getmainvars 2011
#define RV_SYS_rt_sigaction 134
#define RV_SYS_writev 66
#define RV_SYS_gettimeofday 169
#define RV_SYS_times 153
#define RV_SYS_fcntl 25
#define RV_SYS_getdents 61
#define RV_SYS_dup 23

static char IO_Buffer[IO_SIZE_MAX];

static void sim_io_error(iss_insn_t *At_PC, const char *Message, ...)

{
  va_list Args;

  va_start(Args, Message);
  fprintf(stderr, "Error At PC=%X:", At_PC->addr);
  vfprintf(stderr, Message, Args);
  fprintf(stderr, ". Aborting simulation\n");
  exit(0);
}

static void sim_io_eprintf(const char *Message, ...)

{
  va_list Args;

  va_start(Args, Message);
  vfprintf(stderr, Message, Args);
}

static int copy_name(Iss *cpu, iss_insn_t *pc, unsigned int rv32_buff, char *Host_buff)

{
  Iss *iss = (Iss *)cpu;
  char c;
  int i = 0;

  while (1) {
    loadByte(iss, (rv32_buff+i), (uint8_t *)&c);
    if (c == 0) break;
    Host_buff[i++] = c;
    if (i == (MAX_FNAME_LENGTH-1)) {
          sim_io_error (pc, "Max file/path name length exceed");
      i--;
      break;
    }
  }
  Host_buff[i] = 0;
  return 0;
}


static int transpose_code(int code)

{
  int alt = 0;

  if ((code & 0x0) == 0x0) alt |= O_RDONLY;
  if ((code & 0x1) == 0x1) alt |= O_WRONLY;
  if ((code & 0x2) == 0x2) alt |= O_RDWR;
  if ((code & 0x8) == 0x8) alt |= O_APPEND;
  if ((code & 0x200) == 0x200) alt |= O_CREAT;
  if ((code & 0x400) == 0x400) alt |= O_TRUNC;
  if ((code & 0x800) == 0x800) alt |= O_EXCL;
  if ((code & 0x2000) == 0x2000) alt |= O_SYNC;
  if ((code & 0x4000) == 0x4000) alt |= O_NONBLOCK;
  if ((code & 0x8000) == 0x8000) alt |= O_NOCTTY;

  return alt;
}

void handle_syscall(Iss *iss, iss_insn_t *pc)
{
  static int Trace = 0;

  int sys_fun = iss->regfile.get_reg(17);    // SYScall opcode in a7

  if (Trace) printf("SYSCall with sys_fun = %d\n", sys_fun);

  switch (sys_fun) {
    case RV_SYS_brk:
      {
        unsigned int head_ptr = iss->regfile.get_reg(10);
        unsigned int incr = iss->regfile.get_reg(11);
        unsigned int stack = iss->regfile.get_reg(12);
        unsigned int sp = iss->regfile.get_reg(2);
        unsigned int gp = iss->regfile.get_reg(3);
        printf("Mem request: Head: %8X, Incr: %8X, Stack: %8X, Sp: %8X, Gp: %8X, New Head: %8X, Gap Frame/Stack: %d\n",
          head_ptr, incr, stack, sp, gp, (head_ptr+incr), (int) (sp - (head_ptr+incr)));

      }
      break;
    case RV_SYS_open:
      {
        unsigned int filename = iss->regfile.get_reg(10);  // filename in a0
        int flags = transpose_code(iss->regfile.get_reg(11));  // flags in a1
        int mode = iss->regfile.get_reg(12);     // mode in a2
        int Res;
        char fname[MAX_FNAME_LENGTH];

        copy_name(iss, pc, filename, fname);
        Res = open(fname, flags, mode);
        iss->regfile.set_reg(10, Res);       // Ret in a0
      }
      break;
    case RV_SYS_exit:
      iss_exit(iss, iss->regfile.get_reg(10));
      break;
    case RV_SYS_close:
      {
        int fd = iss->regfile.get_reg(10);     // fd in a0
        int Status=0;
      
        if (!(fd == STDIN_FILENO || fd == STDOUT_FILENO || fd == STDERR_FILENO))
          Status = close(fd); // To avoid closing stdin/out/err when shutting down the simulated program ....
        iss->regfile.set_reg(10, Status);      // Ret in a0
      }
      break;
    case RV_SYS_write:
      {
        int fd = iss->regfile.get_reg(10);     // fd in a0
        unsigned int buffer = iss->regfile.get_reg(11);  // buffer in a1
        size_t Len = iss->regfile.get_reg(12);     // length in a2
        ssize_t Write_Len=0;
        unsigned int Off=0;
        unsigned int i;

/*
        if (Len > IO_SIZE_MAX)
          sim_io_error (pc, "Max IO buffer size exceeded (write), Length: %d, MaxLength= %d", (int) Len,  IO_SIZE_MAX);
        for (i = 0; i<Len; i++) IO_Buffer[i] = loadByte(iss, pc, (buffer+i));
        Write_Len = write(fd, IO_Buffer, Len);
*/
        while (Len != 0) {
          unsigned int L = (Len > IO_SIZE_MAX)?IO_SIZE_MAX:Len;
          for (i = 0; i<L; i++) loadByte(iss, (buffer+i+Off), (uint8_t *)&IO_Buffer[i]);
          Off += L;
          Write_Len += write(fd, IO_Buffer, L);
          Len -= L;
        }
        iss->regfile.set_reg(10, Write_Len);   // Ret in a0
      }
      break;
    case RV_SYS_read:
      {
        int fd = iss->regfile.get_reg(10);     // fd in a0
        unsigned int buffer = iss->regfile.get_reg(11);  // buffer in a1
        size_t Len = iss->regfile.get_reg(12);   // length in a2
        ssize_t Read_Len=0;
        unsigned int Off=0;
        int i;
/*
        if (Len > IO_SIZE_MAX)
          sim_io_error (pc, "Max IO buffer size exceeded (read), Length: %d, MaxLength= %d", (int) Len,  IO_SIZE_MAX);
        Read_Len = read(fd, IO_Buffer, Len);
        for (i = 0; i<Read_Len; i++) storeByte(iss, pc, (buffer+i), IO_Buffer[i]);
*/

        while (Len != 0) {
          unsigned int L = (Len > IO_SIZE_MAX)?IO_SIZE_MAX:Len;
          ssize_t L1 = read(fd, IO_Buffer, L);
          Read_Len += L1;
          for (i = 0; i<L1; i++) storeByte(iss, (buffer+i+Off), IO_Buffer[i]);
          Len -= L;
          Off += L;
        }
        iss->regfile.set_reg(10, Read_Len);    // Ret in a0

      }
      break;
    case RV_SYS_lseek:
      {
        int fd = iss->regfile.get_reg(10);
        off_t off = iss->regfile.get_reg(11);
        int whence = iss->regfile.get_reg(12);
        off_t Res = lseek(fd, off, whence);

        iss->regfile.set_reg(10, Res);
      }
      break;
    case RV_SYS_link:
      {
        unsigned int Old_Path = iss->regfile.get_reg(10);
        unsigned int New_Path = iss->regfile.get_reg(11);
        char Host_Old_Path[MAX_FNAME_LENGTH];
        char Host_New_Path[MAX_FNAME_LENGTH];
        int Res;

        copy_name(iss, pc, Old_Path, Host_Old_Path);
        copy_name(iss, pc, New_Path, Host_New_Path);
        Res = link(Host_Old_Path, Host_New_Path);
        iss->regfile.set_reg(10, Res);
      }
      break;
    case RV_SYS_unlink:
      {
        unsigned int Path = iss->regfile.get_reg(10);
        char Host_Path[MAX_FNAME_LENGTH];
        int Res;

        copy_name(iss, pc, Path, Host_Path);
        Res = unlink(Host_Path);
        iss->regfile.set_reg(10, Res);
      }
      break;
    case RV_SYS_gettimeofday:
      {
        unsigned int tv_buf = iss->regfile.get_reg(10);
        struct timeval Tv;
        int Res;

        Res = gettimeofday(&Tv, NULL);
        storeWord(iss, tv_buf,     (int) Tv.tv_sec);
        storeWord(iss, (tv_buf+4), (int) (Tv.tv_sec>>32));
        storeWord(iss, (tv_buf+8), (int) Tv.tv_usec);
        iss->regfile.set_reg(10, Res);
      }
      break;
    case RV_SYS_stat:
      {
        unsigned int Path = iss->regfile.get_reg(10);
        unsigned int Stat_Buf = iss->regfile.get_reg(11);
        char Host_Path[MAX_FNAME_LENGTH];
        struct stat B;
        int Res;

        copy_name(iss, pc, Path, Host_Path);
        Res = stat(Host_Path, &B);
        storeWord(iss, (Stat_Buf   ), (int)  B.st_dev);
        storeWord(iss, (Stat_Buf+4 ), (int) (B.st_dev>>32));

        storeWord(iss, (Stat_Buf+8 ), (int)  B.st_ino);
        storeWord(iss, (Stat_Buf+12), (int) (B.st_ino>>32));

        storeWord(iss, (Stat_Buf+16), (int)  B.st_mode);
        storeWord(iss, (Stat_Buf+20), (int)  B.st_nlink);
        storeWord(iss, (Stat_Buf+24), (int)  B.st_uid);
        storeWord(iss, (Stat_Buf+28), (int)  B.st_gid);

        storeWord(iss, (Stat_Buf+32), (int)  B.st_rdev);
        storeWord(iss, (Stat_Buf+36), (int) (B.st_rdev>>32));

        storeWord(iss, (Stat_Buf+40), (int)  B.st_size);
        storeWord(iss, (Stat_Buf+44), (int) (B.st_size>>32));

        storeWord(iss, (Stat_Buf+48), (int)   B.st_atime);
        storeWord(iss, (Stat_Buf+52), (int)  (B.st_atime>>32));
        // storeWord(iss, pc, (Stat_Buf+56), (int)   B.st_spare1);

        storeWord(iss, (Stat_Buf+60), (int)   B.st_mtime);
        storeWord(iss, (Stat_Buf+64), (int)  (B.st_mtime>>32));
        // storeWord(iss, pc, (Stat_Buf+68), (int)   B.st_spare2);

        storeWord(iss, (Stat_Buf+72), (int)   B.st_ctime);
        storeWord(iss, (Stat_Buf+76), (int)  (B.st_ctime>>32));
        // storeWord(iss, pc, (Stat_Buf+80), (int)   B.st_spare3);

        storeWord(iss, (Stat_Buf+84), (int)   B.st_blksize);
        storeWord(iss, (Stat_Buf+88), (int)   B.st_blocks);

        // storeWord(iss, pc, (Stat_Buf+92), (int)   B.st_spare4[0]);
        // storeWord(iss, pc, (Stat_Buf+96), (int)   B.st_spare4[1]);

        iss->regfile.set_reg(10, Res);
      }
      break;
    case RV_SYS_fstat:
      {
        int fd = iss->regfile.get_reg(10);
        unsigned int Stat_Buf = iss->regfile.get_reg(11);
        struct stat B;
        int Res;

        Res = fstat(fd, &B);
        storeWord(iss, (Stat_Buf   ), (int)  B.st_dev);
        storeWord(iss, (Stat_Buf+4 ), (int) (B.st_dev>>32));

        storeWord(iss, (Stat_Buf+8 ), (int)  B.st_ino);
        storeWord(iss, (Stat_Buf+12), (int) (B.st_ino>>32));

        storeWord(iss, (Stat_Buf+16), (int)  B.st_mode);
        storeWord(iss, (Stat_Buf+20), (int)  B.st_nlink);
        storeWord(iss, (Stat_Buf+24), (int)  B.st_uid);
        storeWord(iss, (Stat_Buf+28), (int)  B.st_gid);

        storeWord(iss, (Stat_Buf+32), (int)  B.st_rdev);
        storeWord(iss, (Stat_Buf+36), (int) (B.st_rdev>>32));

        storeWord(iss, (Stat_Buf+40), (int)  B.st_size);
        storeWord(iss, (Stat_Buf+44), (int) (B.st_size>>32));

        storeWord(iss, (Stat_Buf+48), (int)   B.st_atime);
        storeWord(iss, (Stat_Buf+52), (int)  (B.st_atime>>32));
        // storeWord(iss, pc, (Stat_Buf+56), (int)   B.st_spare1);

        storeWord(iss, (Stat_Buf+60), (int)   B.st_mtime);
        storeWord(iss, (Stat_Buf+64), (int)  (B.st_mtime>>32));
        // storeWord(iss, pc, (Stat_Buf+68), (int)   B.st_spare2);

        storeWord(iss, (Stat_Buf+72), (int)   B.st_ctime);
        storeWord(iss, (Stat_Buf+76), (int)  (B.st_ctime>>32));
        // storeWord(iss, pc, (Stat_Buf+80), (int)   B.st_spare3);

        storeWord(iss, (Stat_Buf+84), (int)   B.st_blksize);
        storeWord(iss, (Stat_Buf+88), (int)   B.st_blocks);

        // storeWord(iss, pc, (Stat_Buf+92), (int)   B.st_spare4[0]);
        // storeWord(iss, pc, (Stat_Buf+96), (int)   B.st_spare4[1]);

        iss->regfile.set_reg(10, Res);
      }
      break;
/*
    case RV_SYS_chmod:
      {
        unsigned int Path = iss->regfile.get_reg(10);
        char Host_Path[MAX_FNAME_LENGTH];
        mode_t mode = iss->regfile.get_reg(11);
        int Res;

        copy_name(iss, pc, Path, Host_Path);
        Res = chmod(Host_Path, mode);
        iss->regfile.set_reg(10, Res);
      }
      break;
    case RV_SYS_fchmod:
      {
        int fd = iss->regfile.get_reg(10);
        mode_t mode = iss->regfile.get_reg(11);
        int Res;

        Res = fchmod(fd, mode);
        iss->regfile.set_reg(10, Res);
      }
      break;
    case RV_SYS_chown:
      {
        unsigned int Path = iss->regfile.get_reg(10);
        char Host_Path[MAX_FNAME_LENGTH];
        uid_t uid = iss->regfile.get_reg(11);
        gid_t gid = iss->regfile.get_reg(12);
        int Res;

        copy_name(iss, pc, Path, Host_Path);
        Res = chown(Host_Path, uid, gid);
        iss->regfile.set_reg(10, Res);
      }
      break;
    case RV_SYS_fchown:
      {
        int fd = iss->regfile.get_reg(10);
        uid_t uid = iss->regfile.get_reg(11);
        gid_t gid = iss->regfile.get_reg(12);
        int Res;

        Res = fchown(fd, uid, gid);
        iss->regfile.set_reg(10, Res);
      }
      break;
    case RV_SYS_utime:
      {
        unsigned int Path = iss->regfile.get_reg(10);
        char Host_Path[MAX_FNAME_LENGTH];
        unsigned int tv_buf = iss->regfile.get_reg(11);
        struct utimbuf Tv;
        int Res;
        unsigned long long Tmp;

        Tmp = (unsigned long long) loadWord(iss, pc, (tv_buf)) |
              ((unsigned long long) loadWord(iss, pc, (tv_buf+4))<<32);
        Tv.actime = (time_t) Tmp;
        Tmp = (unsigned long long) loadWord(iss, pc, (tv_buf+8)) |
              ((unsigned long long) loadWord(iss, pc, (tv_buf+12))<<32);
        Tv.modtime = (time_t) Tmp;
        copy_name(iss, pc, Path, Host_Path);
        Res = utime(Host_Path, &Tv);
        iss->regfile.set_reg(10, Res);
      }
      break;
    case RV_SYS_utimes:
      {
        unsigned int Path = iss->regfile.get_reg(10);
        char Host_Path[MAX_FNAME_LENGTH];
        unsigned int tv_buf = iss->regfile.get_reg(11);
        struct timeval Tv[2];
        int Res;
        unsigned long long Tmp;

        Tmp = (unsigned long long) loadWord(iss, pc, (tv_buf)) |
              ((unsigned long long) loadWord(iss, pc, (tv_buf+4))<<32);
        Tv[0].tv_sec  = (time_t) Tmp;
        Tv[0].tv_usec = (suseconds_t) loadWord(iss, pc, (tv_buf+8));
        Tmp = (unsigned long long) loadWord(iss, pc, (tv_buf+12)) |
              ((unsigned long long) loadWord(iss, pc, (tv_buf+16))<<32);
        Tv[1].tv_sec  = (time_t) Tmp;
        Tv[1].tv_usec = (suseconds_t) loadWord(iss, pc, (tv_buf+20));
        copy_name(iss, pc, Path, Host_Path);
        Res = utimes(Host_Path, Tv);
        iss->regfile.set_reg(10, Res);
      }
      break;
*/
    default:
      errno = EBADRQC;
      iss->regfile.set_reg(10, -1);
          sim_io_error (pc, "SYS call %X (%d) not supported", sys_fun, sys_fun);
      break;
  }
}

#endif