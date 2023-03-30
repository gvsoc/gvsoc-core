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

#include <vp/vp.hpp>
#include <vp/itf/io.hpp>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


#if defined(WORD_SIZE)
#if WORD_SIZE == 64
#define reg_t uint64_t
#define sreg_t int64_t
#elif WORD_SIZE == 32
#define reg_t uint32_t
#define sreg_t int32_t
#endif
#endif

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

static reg_t sysret_errno(sreg_t ret)
{
  return ret == -1 ? -errno : ret;
}

class bus_watchpoint;

typedef reg_t (bus_watchpoint::*syscall_func_t)(reg_t, reg_t, reg_t, reg_t, reg_t, reg_t, reg_t);

class fds_t
{
 public:
  reg_t alloc(int fd);
  void dealloc(reg_t fd);
  int lookup(reg_t fd);
 private:
  std::vector<int> fds;
};

class bus_watchpoint : public vp::component
{

    friend class MapEntry;

public:
    bus_watchpoint(js::config *config);

    int build();
    void reset(bool active);

private:
    static vp::io_req_status_e req(void *__this, vp::io_req *req);
    void handle_syscall(uint64_t cmd);
    void dispatch(uint64_t cmd);
    void target_access(reg_t addr, int size, bool is_write, uint8_t *data);
    static void data_response(void *__this, vp::io_req *req);
    void exec_syscall();
    uint8_t read_uint8(reg_t addr);

    reg_t sys_exit(reg_t, reg_t, reg_t, reg_t, reg_t, reg_t, reg_t);
    reg_t sys_openat(reg_t, reg_t, reg_t, reg_t, reg_t, reg_t, reg_t);
    reg_t sys_read(reg_t, reg_t, reg_t, reg_t, reg_t, reg_t, reg_t);
    reg_t sys_pread(reg_t, reg_t, reg_t, reg_t, reg_t, reg_t, reg_t);
    reg_t sys_write(reg_t, reg_t, reg_t, reg_t, reg_t, reg_t, reg_t);
    reg_t sys_pwrite(reg_t, reg_t, reg_t, reg_t, reg_t, reg_t, reg_t);
    reg_t sys_close(reg_t, reg_t, reg_t, reg_t, reg_t, reg_t, reg_t);
    reg_t sys_lseek(reg_t, reg_t, reg_t, reg_t, reg_t, reg_t, reg_t);
    reg_t sys_fstat(reg_t, reg_t, reg_t, reg_t, reg_t, reg_t, reg_t);
    reg_t sys_lstat(reg_t, reg_t, reg_t, reg_t, reg_t, reg_t, reg_t);
    reg_t sys_statx(reg_t, reg_t, reg_t, reg_t, reg_t, reg_t, reg_t);
    reg_t sys_fstatat(reg_t, reg_t, reg_t, reg_t, reg_t, reg_t, reg_t);
    reg_t sys_faccessat(reg_t, reg_t, reg_t, reg_t, reg_t, reg_t, reg_t);
    reg_t sys_fcntl(reg_t, reg_t, reg_t, reg_t, reg_t, reg_t, reg_t);
    reg_t sys_ftruncate(reg_t, reg_t, reg_t, reg_t, reg_t, reg_t, reg_t);
    reg_t sys_renameat(reg_t, reg_t, reg_t, reg_t, reg_t, reg_t, reg_t);
    reg_t sys_linkat(reg_t, reg_t, reg_t, reg_t, reg_t, reg_t, reg_t);
    reg_t sys_unlinkat(reg_t, reg_t, reg_t, reg_t, reg_t, reg_t, reg_t);
    reg_t sys_mkdirat(reg_t, reg_t, reg_t, reg_t, reg_t, reg_t, reg_t);
    reg_t sys_getcwd(reg_t, reg_t, reg_t, reg_t, reg_t, reg_t, reg_t);
    reg_t sys_getmainvars(reg_t, reg_t, reg_t, reg_t, reg_t, reg_t, reg_t);
    reg_t sys_chdir(reg_t, reg_t, reg_t, reg_t, reg_t, reg_t, reg_t);

    void set_chroot(const char* where);
    std::string do_chroot(const char* fn);
    std::string undo_chroot(const char* fn);
    vp::trace trace;

    vp::io_master out;
    vp::io_slave in;

    unsigned int riscv_fesvr_tohost_addr;
    unsigned int riscv_fesvr_fromhost_addr;

    vp::io_req syscall_req;
    uint64_t syscall_args[8];

    std::vector<syscall_func_t> table;

    fds_t fds;
    std::string chroot;
};

bus_watchpoint::bus_watchpoint(js::config *config)
    : vp::component(config)
{
}

void bus_watchpoint::data_response(void *__this, vp::io_req *req)
{

}


void bus_watchpoint::handle_syscall(uint64_t cmd)
{
    if (cmd & 1) // test pass/fail
    {
        this->clock->stop_engine(cmd >> 1);
    }
    else
    {
        this->dispatch(cmd);
        reg_t value = 1;
        this->target_access(this->riscv_fesvr_fromhost_addr, sizeof(reg_t), true, (uint8_t *)&value);
    }
}

void bus_watchpoint::exec_syscall()
{
    reg_t n = this->syscall_args[0];
    if (n >= table.size() || !table[n])
        throw std::runtime_error("bad syscall #" + std::to_string(n));

    this->syscall_args[0] = (this->*table[n])(this->syscall_args[1],
        this->syscall_args[2], this->syscall_args[3], this->syscall_args[4], this->syscall_args[5],
        this->syscall_args[6], this->syscall_args[7]);


}

void bus_watchpoint::dispatch(uint64_t cmd)
{
    vp::io_req *req = &this->syscall_req;
    req->init();
    req->set_addr(cmd);
    req->set_size(sizeof(this->syscall_args));
    req->set_is_write(false);
    req->set_data((uint8_t *)this->syscall_args);
    int err = this->out.req(req);
    if (err != vp::IO_REQ_OK)
    {
    }

    this->exec_syscall();

    this->target_access(cmd, sizeof(this->syscall_args), true, (uint8_t *)this->syscall_args);
}

void bus_watchpoint::target_access(reg_t addr, int size, bool is_write, uint8_t *data)
{
    vp::io_req *req = &this->syscall_req;
    req->init();
    req->set_addr(addr);
    req->set_size(size);
    req->set_is_write(is_write);
    req->set_data(data);
    int err = this->out.req(req);
    if (err != vp::IO_REQ_OK)
    {
        return;
    }
}

uint8_t bus_watchpoint::read_uint8(reg_t addr)
{
    uint8_t value;
    this->target_access(addr, 1, false, &value);
    return value;
}

vp::io_req_status_e bus_watchpoint::req(void *__this, vp::io_req *req)
{
    bus_watchpoint *_this = (bus_watchpoint *)__this;
    uint64_t offset = req->get_addr();
    bool is_write = req->get_is_write();
    uint64_t size = req->get_size();
    uint8_t *data = req->get_data();

    _this->trace.msg("Received IO req (req: %p, offset: 0x%llx, size: 0x%llx, is_write: %d)\n", req, offset, size, is_write);

    vp::io_req_status_e err = _this->out.req_forward(req);

    if (err != vp::IO_REQ_OK)
    {
        return err;
    }

    if ((offset == _this->riscv_fesvr_tohost_addr || offset == _this->riscv_fesvr_fromhost_addr) && (size == 4 || size == 8))
    {
        if (offset == _this->riscv_fesvr_tohost_addr && is_write)
        {
            reg_t cmd;
            _this->target_access(_this->riscv_fesvr_tohost_addr, size, false, (uint8_t *)&cmd);
            reg_t value = 0;
            _this->target_access(_this->riscv_fesvr_tohost_addr, size, true, (uint8_t *)&value);
            _this->handle_syscall(cmd);
        }
    }

    return vp::IO_REQ_OK;
}

int bus_watchpoint::build()
{
    traces.new_trace("trace", &trace, vp::DEBUG);

    in.set_req_meth(&bus_watchpoint::req);
    new_slave_port("input", &in);

    out.set_resp_meth(&bus_watchpoint::data_response);
    new_master_port("output", &out);

    this->riscv_fesvr_tohost_addr = this->get_config_int("riscv_fesvr_tohost_addr");
    this->riscv_fesvr_fromhost_addr = this->get_config_int("riscv_fesvr_fromhost_addr");

    table.resize(2048);
    table[17] = &bus_watchpoint::sys_getcwd;
    table[25] = &bus_watchpoint::sys_fcntl;
    table[34] = &bus_watchpoint::sys_mkdirat;
    table[35] = &bus_watchpoint::sys_unlinkat;
    table[37] = &bus_watchpoint::sys_linkat;
    table[38] = &bus_watchpoint::sys_renameat;
    table[46] = &bus_watchpoint::sys_ftruncate;
    table[48] = &bus_watchpoint::sys_faccessat;
    table[49] = &bus_watchpoint::sys_chdir;
    table[56] = &bus_watchpoint::sys_openat;
    table[57] = &bus_watchpoint::sys_close;
    table[62] = &bus_watchpoint::sys_lseek;
    table[63] = &bus_watchpoint::sys_read;
    table[64] = &bus_watchpoint::sys_write;
    table[67] = &bus_watchpoint::sys_pread;
    table[68] = &bus_watchpoint::sys_pwrite;
    table[79] = &bus_watchpoint::sys_fstatat;
    table[80] = &bus_watchpoint::sys_fstat;
    table[93] = &bus_watchpoint::sys_exit;
    table[291] = &bus_watchpoint::sys_statx;
    table[1039] = &bus_watchpoint::sys_lstat;
    table[2011] = &bus_watchpoint::sys_getmainvars;

    int stdin_fd = dup(0), stdout_fd0 = dup(1), stdout_fd1 = dup(1);
    if (stdin_fd < 0 || stdout_fd0 < 0 || stdout_fd1 < 0)
        throw std::runtime_error("could not dup stdin/stdout");

    fds.alloc(stdin_fd); // stdin -> stdin
    fds.alloc(stdout_fd0); // stdout -> stdout
    fds.alloc(stdout_fd1); // stderr -> stdout

    return 0;
}

std::string bus_watchpoint::do_chroot(const char* fn)
{
  if (!chroot.empty() && *fn == '/')
    return chroot + fn;
  return fn;
}

std::string bus_watchpoint::undo_chroot(const char* fn)
{
  if (chroot.empty())
    return fn;
  if (strncmp(fn, chroot.c_str(), chroot.size()) == 0
      && (chroot.back() == '/' || fn[chroot.size()] == '/'))
    return fn + chroot.size() - (chroot.back() == '/');
  return "/";
}

#define AT_SYSCALL(syscall, fd, name, ...) \
  (syscall(fds.lookup(fd), int(fd) == RISCV_AT_FDCWD ? do_chroot(name).c_str() : (name), __VA_ARGS__))

reg_t bus_watchpoint::sys_getcwd(reg_t pbuf, reg_t size, reg_t a2, reg_t a3, reg_t a4, reg_t a5, reg_t a6)
{
    std::vector<char> buf(size);
    char* ret = getcwd(buf.data(), size);
    if (ret == NULL)
        return sysret_errno(-1);
    std::string tmp = undo_chroot(buf.data());
    if (size <= tmp.size())
        return -ENOMEM;
    this->target_access(pbuf, tmp.size() + 1, true, (uint8_t *)tmp.data());
    return tmp.size() + 1;
}

reg_t bus_watchpoint::sys_fcntl(reg_t fd, reg_t cmd, reg_t arg, reg_t a3, reg_t a4, reg_t a5, reg_t a6)
{
  return sysret_errno(fcntl(fds.lookup(fd), cmd, arg));
}

reg_t bus_watchpoint::sys_mkdirat(reg_t dirfd, reg_t pname, reg_t len, reg_t mode, reg_t a4, reg_t a5, reg_t a6)
{
    std::vector<char> name(len);
    this->target_access(pname, len, false, (uint8_t *)name.data());
    return sysret_errno(AT_SYSCALL(mkdirat, dirfd, name.data(), mode));
}

reg_t bus_watchpoint::sys_unlinkat(reg_t dirfd, reg_t pname, reg_t len, reg_t flags, reg_t a4, reg_t a5, reg_t a6)
{
    std::vector<char> name(len);
    this->target_access(pname, len, false, (uint8_t *)name.data());
    return sysret_errno(AT_SYSCALL(unlinkat, dirfd, name.data(), flags));
}

reg_t bus_watchpoint::sys_linkat(reg_t odirfd, reg_t poname, reg_t olen, reg_t ndirfd, reg_t pnname, reg_t nlen, reg_t flags)
{
    std::vector<char> oname(olen), nname(nlen);
    this->target_access(poname, olen, false, (uint8_t *)oname.data());
    this->target_access(pnname, nlen, false, (uint8_t *)nname.data());
    return sysret_errno(linkat(fds.lookup(odirfd), int(odirfd) == RISCV_AT_FDCWD ? do_chroot(oname.data()).c_str() : oname.data(),
                                fds.lookup(ndirfd), int(ndirfd) == RISCV_AT_FDCWD ? do_chroot(nname.data()).c_str() : nname.data(),
                                flags));
}

reg_t bus_watchpoint::sys_renameat(reg_t odirfd, reg_t popath, reg_t olen, reg_t ndirfd, reg_t pnpath, reg_t nlen, reg_t a6)
{
    std::vector<char> opath(olen), npath(nlen);
    this->target_access(popath, olen, false, (uint8_t *)opath.data());
    this->target_access(pnpath, nlen, false, (uint8_t *)npath.data());
    return sysret_errno(renameat(fds.lookup(odirfd), int(odirfd) == RISCV_AT_FDCWD ? do_chroot(opath.data()).c_str() : opath.data(),
                                fds.lookup(ndirfd), int(ndirfd) == RISCV_AT_FDCWD ? do_chroot(npath.data()).c_str() : npath.data()));
}

reg_t bus_watchpoint::sys_ftruncate(reg_t fd, reg_t len, reg_t a2, reg_t a3, reg_t a4, reg_t a5, reg_t a6)
{
  return sysret_errno(ftruncate(fds.lookup(fd), len));
}

reg_t bus_watchpoint::sys_faccessat(reg_t dirfd, reg_t pname, reg_t len, reg_t mode, reg_t a4, reg_t a5, reg_t a6)
{
    std::vector<char> name(len);
    this->target_access(pname, len, false, (uint8_t *)name.data());
    return sysret_errno(AT_SYSCALL(faccessat, dirfd, name.data(), mode, 0));
}

reg_t bus_watchpoint::sys_chdir(reg_t path, reg_t a1, reg_t a2, reg_t a3, reg_t a4, reg_t a5, reg_t a6)
{
    size_t size = 0;
    while (this->read_uint8(path + size++))
        ;
    std::vector<char> buf(size);
    for (size_t offset = 0;; offset++)
    {
        buf[offset] = this->read_uint8(path + offset);
        if (!buf[offset])
        break;
    }
    return sysret_errno(chdir(buf.data()));
}

reg_t bus_watchpoint::sys_openat(reg_t dirfd, reg_t pname, reg_t len, reg_t flags, reg_t mode, reg_t a5, reg_t a6)
{
    std::vector<char> name(len);
    this->target_access(pname, len, false, (uint8_t *)name.data());
    int fd = sysret_errno(AT_SYSCALL(openat, dirfd, name.data(), flags, mode));
    if (fd < 0)
        return sysret_errno(-1);
    return fds.alloc(fd);
}

reg_t bus_watchpoint::sys_close(reg_t fd, reg_t a1, reg_t a2, reg_t a3, reg_t a4, reg_t a5, reg_t a6)
{
    if (close(fds.lookup(fd)) < 0)
        return sysret_errno(-1);
    fds.dealloc(fd);
    return 0;
}

reg_t bus_watchpoint::sys_lseek(reg_t fd, reg_t ptr, reg_t dir, reg_t a3, reg_t a4, reg_t a5, reg_t a6)
{
    return sysret_errno(lseek(fds.lookup(fd), ptr, dir));
}

reg_t bus_watchpoint::sys_read(reg_t fd, reg_t pbuf, reg_t len, reg_t a3, reg_t a4, reg_t a5, reg_t a6)
{
    std::vector<char> buf(len);
    ssize_t ret = read(fds.lookup(fd), buf.data(), len);
    reg_t ret_errno = sysret_errno(ret);
    if (ret > 0)
        this->target_access(pbuf, ret, true, (uint8_t *)buf.data());
    return ret_errno;
}

reg_t bus_watchpoint::sys_pread(reg_t fd, reg_t pbuf, reg_t len, reg_t off, reg_t a4, reg_t a5, reg_t a6)
{
    std::vector<char> buf(len);
    ssize_t ret = pread(fds.lookup(fd), buf.data(), len, off);
    reg_t ret_errno = sysret_errno(ret);
    if (ret > 0)
        this->target_access(pbuf, ret, true, (uint8_t *)buf.data());
    return ret_errno;
}

reg_t bus_watchpoint::sys_pwrite(reg_t fd, reg_t pbuf, reg_t len, reg_t off, reg_t a4, reg_t a5, reg_t a6)
{
    std::vector<char> buf(len);
    this->target_access(pbuf, len, false, (uint8_t *)buf.data());
    reg_t ret = sysret_errno(pwrite(fds.lookup(fd), buf.data(), len, off));
    return ret;
}

reg_t bus_watchpoint::sys_fstatat(reg_t dirfd, reg_t pname, reg_t len, reg_t pbuf, reg_t flags, reg_t a5, reg_t a6)
{
    std::vector<char> name(len);
    this->target_access(pname, len, false, (uint8_t *)name.data());

    struct stat buf;
    reg_t ret = sysret_errno(AT_SYSCALL(fstatat, dirfd, name.data(), &buf, flags));
    if (ret != (reg_t)-1)
    {
        riscv_stat rbuf(buf);
        this->target_access(pbuf, sizeof(rbuf), true, (uint8_t *)&rbuf);
    }
    return ret;
}

reg_t bus_watchpoint::sys_write(reg_t fd, reg_t pbuf, reg_t len, reg_t a3, reg_t a4, reg_t a5, reg_t a6)
{
    std::vector<char> buf(len);
    this->target_access(pbuf, len, false, (uint8_t *)buf.data());

    reg_t ret = sysret_errno(write(fds.lookup(fd), buf.data(), len));
    return ret;
}

reg_t bus_watchpoint::sys_fstat(reg_t fd, reg_t pbuf, reg_t a2, reg_t a3, reg_t a4, reg_t a5, reg_t a6)
{
  struct stat buf;
  reg_t ret = sysret_errno(fstat(fds.lookup(fd), &buf));
  if (ret != (reg_t)-1)
  {
    riscv_stat rbuf(buf);
    this->target_access(pbuf, sizeof(rbuf), true, (uint8_t *)&rbuf);
  }
  return ret;
}

reg_t bus_watchpoint::sys_statx(reg_t fd, reg_t pname, reg_t len, reg_t flags, reg_t mask, reg_t pbuf, reg_t a6)
{
#ifndef HAVE_STATX
  return -ENOSYS;
#else
  std::vector<char> name(len);
  memif->read(pname, len, name.data());

  struct statx buf;
  reg_t ret = sysret_errno(statx(fds.lookup(fd), do_chroot(name.data()).c_str(), flags, mask, &buf));
  if (ret != (reg_t)-1)
  {
    riscv_statx rbuf(buf, htif);
    memif->write(pbuf, sizeof(rbuf), &rbuf);
  }
  return ret;
#endif
}

reg_t bus_watchpoint::sys_lstat(reg_t pname, reg_t len, reg_t pbuf, reg_t a3, reg_t a4, reg_t a5, reg_t a6)
{
    std::vector<char> name(len);
    this->target_access(pname, len, false, (uint8_t *)name.data());

    struct stat buf;
    reg_t ret = sysret_errno(lstat(do_chroot(name.data()).c_str(), &buf));
    if (ret != (reg_t)-1)
    {
        riscv_stat rbuf(buf);
        this->target_access(pbuf, sizeof(rbuf), true, (uint8_t *)&rbuf);
    }
    return ret;
}

reg_t bus_watchpoint::sys_exit(reg_t code, reg_t a1, reg_t a2, reg_t a3, reg_t a4, reg_t a5, reg_t a6)
{
    this->clock->stop_engine(code);
    return 0;
}

reg_t bus_watchpoint::sys_getmainvars(reg_t pbuf, reg_t limit, reg_t a2, reg_t a3, reg_t a4, reg_t a5, reg_t a6)
{
    std::vector<std::string> args = { "app" };

    for (auto x: this->get_js_config()->get("args")->get_elems())
    {
        args.push_back(x->get_str());
    }

    std::vector<uint64_t> words(args.size() + 3);
    words[0] = args.size();
    words[args.size()+1] = 0; // argv[argc] = NULL
    words[args.size()+2] = 0; // envp[0] = NULL

    size_t sz = (args.size() + 3) * sizeof(words[0]);
    for (size_t i = 0; i < args.size(); i++)
    {
        words[i+1] = sz + pbuf;
        sz += args[i].length() + 1;
    }

    std::vector<char> bytes(sz);
    memcpy(bytes.data(), words.data(), sizeof(words[0]) * words.size());
    for (size_t i = 0; i < args.size(); i++)
        strcpy(&bytes[words[i+1] - pbuf], args[i].c_str());

    if (bytes.size() > limit)
        return -ENOMEM;

    this->target_access(pbuf, bytes.size(), true, (uint8_t *)bytes.data());
    return 0;
}

void bus_watchpoint::set_chroot(const char* where)
{
  char buf1[PATH_MAX], buf2[PATH_MAX];

  if (getcwd(buf1, sizeof(buf1)) == NULL
      || chdir(where) != 0
      || getcwd(buf2, sizeof(buf2)) == NULL
      || chdir(buf1) != 0)
  {
    fprintf(stderr, "could not chroot to %s\n", where);
    exit(-1);
  }

  chroot = buf2;
}

reg_t fds_t::alloc(int fd)
{
  reg_t i;
  for (i = 0; i < fds.size(); i++)
    if (fds[i] == -1)
      break;

  if (i == fds.size())
    fds.resize(i+1);

  fds[i] = fd;
  return i;
}

void fds_t::dealloc(reg_t fd)
{
  fds[fd] = -1;
}

int fds_t::lookup(reg_t fd)
{
  if (int(fd) == RISCV_AT_FDCWD)
    return AT_FDCWD;
  return fd >= fds.size() ? -1 : fds[fd];
}

void bus_watchpoint::reset(bool active)
{
    if (active)
    {
    }
}

extern "C" vp::component *vp_constructor(js::config *config)
{
    return new bus_watchpoint(config);
}
