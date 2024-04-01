// Copyright (c) 2010-2017, The Regents of the University of California
// (Regents).  All Rights Reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
// 3. Neither the name of the Regents nor the
//    names of its contributors may be used to endorse or promote products
//    derived from this software without specific prior written permission.
//
// IN NO EVENT SHALL REGENTS BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,
// SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST PROFITS, ARISING
// OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF REGENTS HAS
// BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// REGENTS SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE. THE SOFTWARE AND ACCOMPANYING DOCUMENTATION, IF ANY, PROVIDED
// HEREUNDER IS PROVIDED "AS IS". REGENTS HAS NO OBLIGATION TO PROVIDE
// MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.

#pragma once

#include <vp/vp.hpp>
#include <cpu/iss/include/types.hpp>
#include <vp/itf/io.hpp>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


#define RISCV_AT_FDCWD -100

struct riscv_stat
{
  uint64_t dev;
  uint64_t ino;
  uint32_t mode;
  uint32_t nlink;
  uint32_t uid;
  uint32_t gid;
  uint64_t rdev;
  uint64_t __pad1;
  uint64_t size;
  uint32_t blksize;
  uint32_t __pad2;
  uint64_t blocks;
  uint64_t atime;
  uint64_t __pad3;
  uint64_t mtime;
  uint64_t __pad4;
  uint64_t ctime;
  uint64_t __pad5;
  uint32_t __unused4;
  uint32_t __unused5;

  riscv_stat(const struct stat& s)
    : dev(s.st_dev),
      ino(s.st_ino),
      mode(s.st_mode),
      nlink(s.st_nlink),
      uid(s.st_uid),
      gid(s.st_gid),
      rdev(s.st_rdev), __pad1(),
      size(s.st_size),
      blksize(s.st_blksize), __pad2(),
      blocks(s.st_blocks),
      atime(s.st_atime), __pad3(),
      mtime(s.st_mtime), __pad4(),
      ctime(s.st_ctime), __pad5(),
      __unused4(), __unused5() {}
};

static iss_reg_t sysret_errno(iss_sim_t ret)
{
  return ret == -1 ? -errno : ret;
}

class IssWrapper;

class Htif;

typedef iss_reg_t (Htif::*syscall_func_t)(iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t);

class fds_t
{
 public:
  iss_reg_t alloc(int fd);
  void dealloc(iss_reg_t fd);
  int lookup(iss_reg_t fd);
 private:
  std::vector<int> fds;
};

class Htif
{

public:
    Htif(IssWrapper &top, Iss &iss);

    void build();
    void reset(bool active);

private:
    void handle_syscall(uint64_t cmd);
    void dispatch(uint64_t cmd);
    void target_access(iss_reg_t addr, int size, bool is_write, uint8_t *data);
    bool target_access_async(iss_reg_t addr, int size, bool is_write, uint8_t *data);
    static void data_response(vp::Block *__this, vp::IoReq *req);
    void exec_syscall();
    uint8_t read_uint8(iss_reg_t addr);
    static void htif_handler(vp::Block *__this, vp::ClockEvent *event);

    iss_reg_t sys_exit(iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t);
    iss_reg_t sys_openat(iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t);
    iss_reg_t sys_read(iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t);
    iss_reg_t sys_pread(iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t);
    iss_reg_t sys_write(iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t);
    iss_reg_t sys_pwrite(iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t);
    iss_reg_t sys_close(iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t);
    iss_reg_t sys_lseek(iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t);
    iss_reg_t sys_fstat(iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t);
    iss_reg_t sys_lstat(iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t);
    iss_reg_t sys_statx(iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t);
    iss_reg_t sys_fstatat(iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t);
    iss_reg_t sys_faccessat(iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t);
    iss_reg_t sys_fcntl(iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t);
    iss_reg_t sys_ftruncate(iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t);
    iss_reg_t sys_renameat(iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t);
    iss_reg_t sys_linkat(iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t);
    iss_reg_t sys_unlinkat(iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t);
    iss_reg_t sys_mkdirat(iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t);
    iss_reg_t sys_getcwd(iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t);
    iss_reg_t sys_getmainvars(iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t);
    iss_reg_t sys_chdir(iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t, iss_reg_t);

    void set_chroot(const char* where);
    std::string do_chroot(const char* fn);
    std::string undo_chroot(const char* fn);

    Iss &iss;

    uint64_t syscall_args[8];

    std::vector<syscall_func_t> table;

    fds_t fds;
    std::string chroot;

    iss_reg_t tohost_addr;
    iss_reg_t fromhost_addr;
    vp::ClockEvent htif_event;
};
