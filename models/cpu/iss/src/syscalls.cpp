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
#include "iss.hpp"
#include <algorithm>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#ifndef O_BINARY
# define O_BINARY 0
#endif


void Iss::handle_ebreak()
{
  int id = this->cpu.regfile.regs[10];

  switch (id)
  {
  // TODO deprecated, should be removed
#if 0
    case GV_SEMIHOSTING_TRACE_OPEN: {
      int result = -1;
      std::string path = this->read_user_string(this->cpu.regfile.regs[11]);
      if (path == "")
      {
        this->warning.force_warning("Invalid user string while opening trace (addr: 0x%x)\n", this->cpu.regfile.regs[11]);
      }
      else
      {
        vp::trace *trace = this->traces.get_trace_manager()->get_trace_from_path(path);
        if (trace == NULL)
        {
          this->warning.force_warning("Invalid trace (path: %s)\n", path.c_str());
        }
        else
        {
          this->trace.msg("Opened trace (path: %s, id: %d)\n", path.c_str(), trace->id);
          result = trace->id;
        }
      }

      this->cpu.regfile.regs[10] = result;

      break;
    }
    
    case GV_SEMIHOSTING_TRACE_ENABLE: {
      int id = this->cpu.regfile.regs[11];
      vp::trace *trace = this->traces.get_trace_manager()->get_trace_from_id(id);
      if (trace == NULL)
      {
        this->warning.force_warning("Unknown trace ID while dumping trace (id: %d)\n", id);
      }
      else
      {
        trace->set_active(this->cpu.regfile.regs[12]);
      }

      break; 
    }
  #endif

    default:
      this->warning.force_warning("Unknown ebreak call (id: %d)\n", id);
      break;
  }
}

bool Iss::user_access(iss_addr_t addr, uint8_t *buffer, iss_addr_t size, bool is_write)
{
  vp::io_req *req = &io_req;
  std::string str = "";
  while(size != 0)
  {
    req->init();
    req->set_debug(true);
    req->set_addr(addr);
    req->set_size(1);
    req->set_is_write(is_write);
    req->set_data(buffer);
    int err = data.req(req);
    if (err != vp::IO_REQ_OK) 
    {
      if (err == vp::IO_REQ_INVALID)
        this->warning.fatal("Invalid IO response during debug request\n");
      else
        this->warning.fatal("Pending IO response during debug request\n");

      return true;
    }

    addr++;
    size--;
    buffer++;
  }
    
  return false;
}

std::string Iss::read_user_string(iss_addr_t addr, int size)
{
  vp::io_req *req = &io_req;
  std::string str = "";
  while(size != 0)
  {
    uint8_t buffer;
    req->init();
    req->set_debug(true);
    req->set_addr(addr);
    req->set_size(1);
    req->set_is_write(false);
    req->set_data(&buffer);
    int err = data.req(req);
    if (err != vp::IO_REQ_OK) 
    {
      if (err == vp::IO_REQ_INVALID)
        return "";
      else
        this->warning.fatal("Pending IO response during debug request\n");
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
        O_RDWR | O_CREAT | O_APPEND | O_BINARY
};


void Iss::handle_riscv_ebreak()
{
  int id = this->cpu.regfile.regs[10];

  switch (id)
  {
    case 0x4:
    {
      std::string path = this->read_user_string(this->cpu.regfile.regs[11]);
      printf("%s", path.c_str());
      break;
    }

    case 0x1:
    {
      iss_reg_t args[3];
      if (this->user_access(this->cpu.regfile.regs[11], (uint8_t *)args, sizeof(args), false))
      {
        this->cpu.regfile.regs[10] = -1;
        return;
      }
      std::string path = this->read_user_string(args[0], args[2]);


      unsigned int mode = args[1];

      this->cpu.regfile.regs[10] = open(path.c_str(), open_modeflags[mode], 0644);

      if (this->cpu.regfile.regs[10] == -1)
        this->warning.force_warning("Caught error during semi-hosted call (name: open, path: %s, mode: 0x%x, error: %s)\n", path.c_str(), mode, strerror(errno));

      break;
    }

    case 0x2:
    {
      this->cpu.regfile.regs[10] = close(this->cpu.regfile.regs[11]);
      break;
    }

    case 0x5:
    {
      iss_reg_t args[3];
      if (this->user_access(this->cpu.regfile.regs[11], (uint8_t *)args, sizeof(args), false))
      {
        this->cpu.regfile.regs[10] = -1;
        return;
      }

      uint8_t buffer[1024];
      int size = args[2];
      iss_reg_t addr = args[1];
      while(size)
      {
        int iter_size = 1024;
        if (size < 1024)
          iter_size = size;

        if (this->user_access(addr, buffer, iter_size, false))
        {
          this->cpu.regfile.regs[10] = -1;
          return;
        }

        if (write(args[0], (void *)(long)buffer, iter_size) != iter_size)
          break;

        fsync(args[0]);

        size -= iter_size;
        addr += iter_size;
      }

      this->cpu.regfile.regs[10] = size;
      break;
    }

    case 0x6:
    {
      iss_reg_t args[3];
      if (this->user_access(this->cpu.regfile.regs[11], (uint8_t *)args, sizeof(args), false))
      {
        this->cpu.regfile.regs[10] = -1;
        return;
      }

      uint8_t buffer[1024];
      int size = args[2];
      iss_reg_t addr = args[1];
      while(size)
      {
        int iter_size = 1024;
        if (size < 1024)
          iter_size = size;

        int read_size = read(args[0], (void *)(long)buffer, iter_size);

        if (read_size <= 0)
        {
          if (read_size < 0)
          {
            this->cpu.regfile.regs[10] = -1;
            return;
          }
          else
          {
            break;
          }
        }

        if (this->user_access(addr, buffer, read_size, true))
        {
          this->cpu.regfile.regs[10] = -1;
          return;
        }

        size -= read_size;
        addr += read_size;
      }

      this->cpu.regfile.regs[10] = size;

      break;
    }

    case 0xA:
    {
      iss_reg_t args[2];
      if (this->user_access(this->cpu.regfile.regs[11], (uint8_t *)args, sizeof(args), false))
      {
        this->cpu.regfile.regs[10] = -1;
        return;
      }

      int pos = lseek(args[0], args[1], SEEK_SET);
      this->cpu.regfile.regs[10] = pos != args[1];
      break;
    }

    case 0x18:
    {
      int status = this->cpu.regfile.regs[11] == 0x20026 ? 0 : 1;

      this->clock->stop_retain(-1);
      this->clock->stop_engine(status & 0x7fffffff);

      break;
    }

    case 0x0C:
    {
      struct stat buf;
      fstat(this->cpu.regfile.regs[11], &buf);
      this->cpu.regfile.regs[10] = buf.st_size;
      break;
    }

    case 0x100:
    {
      iss_reg_t args[2];
      if (this->user_access(this->cpu.regfile.regs[11], (uint8_t *)args, sizeof(args), false))
      {
        this->cpu.regfile.regs[10] = -1;
        return;
      }
      std::string path = this->read_user_string(args[0]);
      if (path == "")
      {
        this->warning.force_warning("Invalid user string while opening trace (addr: 0x%x)\n", args[0]);
      }
      else
      {
        if (args[1])
        {
          this->traces.get_trace_manager()->add_trace_path(0, path);
        }
        else
        {
          this->traces.get_trace_manager()->add_exclude_trace_path(0, path);
        }
        this->traces.get_trace_manager()->check_traces();
      }
      break;
    }

    case 0x101:
    {
      iss_reg_t args[1];
      if (this->user_access(this->cpu.regfile.regs[11], (uint8_t *)args, sizeof(args), false))
      {
        this->cpu.regfile.regs[10] = -1;
        return;
      }

      iss_csr_write(this, CSR_PCER, args[0]);

      break;
    }

    case 0x102:
    {
      iss_csr_write(this, CSR_PCCR(31), 0);
      this->cycle_count = 0;
      iss_reg_t value;
      iss_csr_read(this, CSR_PCMR, &value);
      if ((value & 1) == 1)
      {
        this->cycle_count_start = this->get_cycles();
      }
      break;
    }

    case 0x103:
    {
      iss_reg_t value;
      iss_csr_read(this, CSR_PCMR, &value);
      if ((value & 1) == 0)
      {
        this->cycle_count_start = this->get_cycles();
      }

      iss_csr_write(this, CSR_PCMR, 1);
      break;
    }

    case 0x104:
    {

      iss_reg_t value;
      iss_csr_read(this, CSR_PCMR, &value);
      if ((value & 1) == 1)
      {
        this->cycle_count += this->get_cycles() - this->cycle_count_start;
      }

      iss_csr_write(this, CSR_PCMR, 0);
      break;
    }

    case 0x105:
    {
      iss_reg_t args[1];
      if (this->user_access(this->cpu.regfile.regs[11], (uint8_t *)args, sizeof(args), false))
      {
        return;
      }
      iss_csr_read(this, CSR_PCCR(args[0]), &this->cpu.regfile.regs[10]);
      break;
    }

    case 0x106:
    {
      iss_reg_t args[2];
      if (this->user_access(this->cpu.regfile.regs[11], (uint8_t *)args, sizeof(args), false))
      {
        this->cpu.regfile.regs[10] = -1;
        return;
      }
      std::string path = this->read_user_string(args[0]);
      std::string mode = this->read_user_string(args[1]);
      if (path == "")
      {
        this->warning.force_warning("Invalid user string while opening trace (addr: 0x%x)\n", args[0]);
      }
      else
      {
        if (mode == "")
        {
          this->warning.force_warning("Invalid user string while opening trace (addr: 0x%x)\n", args[1]);
        }
        else
        {
          this->cpu.regfile.regs[10] = 0;
          FILE *file = fopen(path.c_str(), mode.c_str());
          if (file == NULL)
          {
            this->cpu.regfile.regs[10] = -1;
            return;
          }

          iss_reg_t value;
          iss_csr_read(this, CSR_PCMR, &value);
          if ((value & 1) == 1)
          {
            this->cycle_count += this->get_cycles() - this->cycle_count_start;
            this->cycle_count_start = this->get_cycles();
          }

          fprintf(file, "PCER values at timestamp %ld ps, duration %ld cycles\n", this->get_time(), this->cycle_count);
          fprintf(file, "Index; Name; Description; Value\n");
          for (int i=0; i<31; i++)
          {
            if (this->pcer_info[i].name != "")
            {
              iss_reg_t value;
              iss_csr_read(this, CSR_PCCR(i), &value);
              fprintf(file, "%d; %s; %s; %" PRIxFULLREG "\n", i, this->pcer_info[i].name.c_str(), this->pcer_info[i].help.c_str(), value);
            }
          }


          fclose(file);
        }
      }
        break;
    }

    case 0x107: {
      iss_reg_t args[1];
      if (this->user_access(this->cpu.regfile.regs[11], (uint8_t *)args, sizeof(args), false))
      {
        this->cpu.regfile.regs[10] = -1;
        return;
      }
      this->traces.get_trace_manager()->set_global_enable(args[0]);
      break;
    }

    case 0x108: {
      iss_reg_t args[1];
      if (this->user_access(this->cpu.regfile.regs[11], (uint8_t *)args, sizeof(args), false))
      {
        this->cpu.regfile.regs[10] = -1;
        return;
      }

      int result = -1;
      std::string path = this->read_user_string(args[0]);
      if (path == "")
      {
        this->warning.force_warning("Invalid user string while opening VCD trace (addr: 0x%x)\n", args[0]);
      }
      else
      {
        vp::trace *trace = this->traces.get_trace_manager()->get_trace_from_path(path);
        if (trace == NULL)
        {
          this->warning.force_warning("Invalid VCD trace (path: %s)\n", path.c_str());
        }
        else
        {
          this->trace.msg("Opened VCD trace (path: %s, id: %d)\n", path.c_str(), trace->id);
          result = trace->id;
        }
      }

      this->cpu.regfile.regs[10] = result;

      break;
    }
    
    case 0x109:
    break;
    
    case 0x10A: {
      iss_reg_t args[2];
      if (this->user_access(this->cpu.regfile.regs[11], (uint8_t *)args, sizeof(args), false))
      {
        this->cpu.regfile.regs[10] = -1;
        return;
      }

      int id = args[0];
      vp::trace *trace = this->traces.get_trace_manager()->get_trace_from_id(id);
      if (trace == NULL)
      {
        this->warning.force_warning("Unknown trace ID while dumping VCD trace (id: %d)\n", id);
      }
      else
      {
        if (trace->width > 32)
        {
          this->warning.force_warning("Trying to write to VCD trace whose width is bigger than 32 (id: %d)\n", id);
        }
        else
        {
          if (0)
            trace->event(NULL);
          else
            trace->event((uint8_t *)&args[1]);
        }
      }

      break; 
    }
    
    case 0x10C: {
      iss_reg_t args[1];
      if (this->user_access(this->cpu.regfile.regs[11], (uint8_t *)args, sizeof(args), false))
      {
        this->cpu.regfile.regs[10] = -1;
        return;
      }

      int id = args[0];
      vp::trace *trace = this->traces.get_trace_manager()->get_trace_from_id(id);
      if (trace == NULL)
      {
        this->warning.force_warning("Unknown trace ID while dumping VCD trace (id: %d)\n", id);
      }
      else
      {
        if (trace->width > 32)
        {
          this->warning.force_warning("Trying to write to VCD trace whose width is bigger than 32 (id: %d)\n", id);
        }
        else
        {
          trace->event(NULL);
        }
      }

      break; 
    }
    
    case 0x10B: {
      iss_reg_t args[2];
      if (this->user_access(this->cpu.regfile.regs[11], (uint8_t *)args, sizeof(args), false))
      {
        this->cpu.regfile.regs[10] = -1;
        return;
      }

      int id = args[0];
      vp::trace *trace = this->traces.get_trace_manager()->get_trace_from_id(id);
      if (trace == NULL)
      {
        this->warning.force_warning("Unknown trace ID while dumping VCD trace (id: %d)\n", id);
      }
      else
      {
        if (!trace->is_string)
        {
          this->warning.force_warning("Trying to write string to VCD trace which is not a string (id: %d)\n", id);
        }
        else
        {
          std::string path = this->read_user_string(args[1]);

          trace->event_string(path);
        }
      }
      break;
    }

    case 0x10D: {
      this->get_engine()->stop_exec();

      break; 
    }
    
    default:
      this->warning.force_warning("Unknown ebreak call (id: %d)\n", id);
      break;
  }
}


