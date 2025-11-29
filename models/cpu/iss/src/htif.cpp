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
#include <cpu/iss/include/htif.hpp>
#include <cpu/iss/include/iss.hpp>

static std::vector<int> fds;
static bool fds_init = false;

Htif::Htif(IssWrapper &top, Iss &iss)
    : iss(iss), htif_event(&top, (vp::Block *)&iss, &Htif::htif_handler)
{
#ifdef CONFIG_GVSOC_ISS_HTIF
    table.resize(2048);
    table[17] = &Htif::sys_getcwd;
    table[25] = &Htif::sys_fcntl;
    table[34] = &Htif::sys_mkdirat;
    table[35] = &Htif::sys_unlinkat;
    table[37] = &Htif::sys_linkat;
    table[38] = &Htif::sys_renameat;
    table[46] = &Htif::sys_ftruncate;
    table[48] = &Htif::sys_faccessat;
    table[49] = &Htif::sys_chdir;
    table[56] = &Htif::sys_openat;
    table[57] = &Htif::sys_close;
    table[62] = &Htif::sys_lseek;
    table[63] = &Htif::sys_read;
    table[64] = &Htif::sys_write;
    table[67] = &Htif::sys_pread;
    table[68] = &Htif::sys_pwrite;
    table[79] = &Htif::sys_fstatat;
    table[80] = &Htif::sys_fstat;
    table[93] = &Htif::sys_exit;
    table[291] = &Htif::sys_statx;
    table[1039] = &Htif::sys_lstat;
    table[2011] = &Htif::sys_getmainvars;

    if (!fds_init)
    {
        int stdin_fd = dup(0), stdout_fd0 = dup(1), stdout_fd1 = dup(1);
        if (stdin_fd < 0 || stdout_fd0 < 0 || stdout_fd1 < 0)
            throw std::runtime_error("could not dup stdin/stdout");

        fds.alloc(stdin_fd); // stdin -> stdin
        fds.alloc(stdout_fd0); // stdout -> stdout
        fds.alloc(stdout_fd1); // stderr -> stdout

        fds_init = true;
    }
#endif
}


void Htif::build()
{
#ifdef CONFIG_GVSOC_ISS_HTIF
    this->tohost_addr = this->iss.top.get_js_config()->get_uint("htif_tohost");
    this->fromhost_addr = this->iss.top.get_js_config()->get_uint("htif_fromhost");
#endif
}

void Htif::data_response(vp::Block *__this, vp::IoReq *req)
{

}


void Htif::handle_syscall(uint64_t cmd)
{
    if (cmd & 1) // test pass/fail
    {
        this->iss.top.time.get_engine()->quit(cmd >> 1);
    }
    else
    {
        this->dispatch(cmd);
        iss_reg_t value = 1;
        this->target_access(this->fromhost_addr, sizeof(iss_reg_t), true, (uint8_t *)&value);
    }
}

void Htif::exec_syscall()
{
    iss_reg_t n = this->syscall_args[0];
    if (n >= table.size() || !table[n])
        throw std::runtime_error("bad syscall #" + std::to_string(n));

    this->syscall_args[0] = (this->*table[n])(this->syscall_args[1],
        this->syscall_args[2], this->syscall_args[3], this->syscall_args[4], this->syscall_args[5],
        this->syscall_args[6], this->syscall_args[7]);


}

void Htif::dispatch(uint64_t cmd)
{
    this->target_access(cmd, sizeof(this->syscall_args), false, (uint8_t *)this->syscall_args);

    this->exec_syscall();

    this->target_access(cmd, sizeof(this->syscall_args), true, (uint8_t *)this->syscall_args);
}

void Htif::target_access(iss_reg_t addr, int size, bool is_write, uint8_t *data)
{
    this->iss.syscalls.user_access(addr, data, size, is_write);
}

uint8_t Htif::read_uint8(iss_reg_t addr)
{
    uint8_t value;
    this->target_access(addr, 1, false, &value);
    return value;
}

std::string Htif::do_chroot(const char* fn)
{
  if (!chroot.empty() && *fn == '/')
    return chroot + fn;
  return fn;
}

std::string Htif::undo_chroot(const char* fn)
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

iss_reg_t Htif::sys_getcwd(iss_reg_t pbuf, iss_reg_t size, iss_reg_t a2, iss_reg_t a3, iss_reg_t a4, iss_reg_t a5, iss_reg_t a6)
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

iss_reg_t Htif::sys_fcntl(iss_reg_t fd, iss_reg_t cmd, iss_reg_t arg, iss_reg_t a3, iss_reg_t a4, iss_reg_t a5, iss_reg_t a6)
{
  return sysret_errno(fcntl(fds.lookup(fd), cmd, arg));
}

iss_reg_t Htif::sys_mkdirat(iss_reg_t dirfd, iss_reg_t pname, iss_reg_t len, iss_reg_t mode, iss_reg_t a4, iss_reg_t a5, iss_reg_t a6)
{
    std::vector<char> name(len);
    this->target_access(pname, len, false, (uint8_t *)name.data());
    return sysret_errno(AT_SYSCALL(mkdirat, dirfd, name.data(), mode));
}

iss_reg_t Htif::sys_unlinkat(iss_reg_t dirfd, iss_reg_t pname, iss_reg_t len, iss_reg_t flags, iss_reg_t a4, iss_reg_t a5, iss_reg_t a6)
{
    std::vector<char> name(len);
    this->target_access(pname, len, false, (uint8_t *)name.data());
    return sysret_errno(AT_SYSCALL(unlinkat, dirfd, name.data(), flags));
}

iss_reg_t Htif::sys_linkat(iss_reg_t odirfd, iss_reg_t poname, iss_reg_t olen, iss_reg_t ndirfd, iss_reg_t pnname, iss_reg_t nlen, iss_reg_t flags)
{
    std::vector<char> oname(olen), nname(nlen);
    this->target_access(poname, olen, false, (uint8_t *)oname.data());
    this->target_access(pnname, nlen, false, (uint8_t *)nname.data());
    return sysret_errno(linkat(fds.lookup(odirfd), int(odirfd) == RISCV_AT_FDCWD ? do_chroot(oname.data()).c_str() : oname.data(),
                                fds.lookup(ndirfd), int(ndirfd) == RISCV_AT_FDCWD ? do_chroot(nname.data()).c_str() : nname.data(),
                                flags));
}

iss_reg_t Htif::sys_renameat(iss_reg_t odirfd, iss_reg_t popath, iss_reg_t olen, iss_reg_t ndirfd, iss_reg_t pnpath, iss_reg_t nlen, iss_reg_t a6)
{
    std::vector<char> opath(olen), npath(nlen);
    this->target_access(popath, olen, false, (uint8_t *)opath.data());
    this->target_access(pnpath, nlen, false, (uint8_t *)npath.data());
    return sysret_errno(renameat(fds.lookup(odirfd), int(odirfd) == RISCV_AT_FDCWD ? do_chroot(opath.data()).c_str() : opath.data(),
                                fds.lookup(ndirfd), int(ndirfd) == RISCV_AT_FDCWD ? do_chroot(npath.data()).c_str() : npath.data()));
}

iss_reg_t Htif::sys_ftruncate(iss_reg_t fd, iss_reg_t len, iss_reg_t a2, iss_reg_t a3, iss_reg_t a4, iss_reg_t a5, iss_reg_t a6)
{
  return sysret_errno(ftruncate(fds.lookup(fd), len));
}

iss_reg_t Htif::sys_faccessat(iss_reg_t dirfd, iss_reg_t pname, iss_reg_t len, iss_reg_t mode, iss_reg_t a4, iss_reg_t a5, iss_reg_t a6)
{
    std::vector<char> name(len);
    this->target_access(pname, len, false, (uint8_t *)name.data());
    return sysret_errno(AT_SYSCALL(faccessat, dirfd, name.data(), mode, 0));
}

iss_reg_t Htif::sys_chdir(iss_reg_t path, iss_reg_t a1, iss_reg_t a2, iss_reg_t a3, iss_reg_t a4, iss_reg_t a5, iss_reg_t a6)
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

iss_reg_t Htif::sys_openat(iss_reg_t dirfd, iss_reg_t pname, iss_reg_t len, iss_reg_t flags, iss_reg_t mode, iss_reg_t a5, iss_reg_t a6)
{
    std::vector<char> name(len);
    this->target_access(pname, len, false, (uint8_t *)name.data());
    int fd = sysret_errno(AT_SYSCALL(openat, dirfd, name.data(), flags, mode));
    if (fd < 0)
        return sysret_errno(-1);
    return fds.alloc(fd);
}

iss_reg_t Htif::sys_close(iss_reg_t fd, iss_reg_t a1, iss_reg_t a2, iss_reg_t a3, iss_reg_t a4, iss_reg_t a5, iss_reg_t a6)
{
    if (close(fds.lookup(fd)) < 0)
        return sysret_errno(-1);
    fds.dealloc(fd);
    return 0;
}

iss_reg_t Htif::sys_lseek(iss_reg_t fd, iss_reg_t ptr, iss_reg_t dir, iss_reg_t a3, iss_reg_t a4, iss_reg_t a5, iss_reg_t a6)
{
    return sysret_errno(lseek(fds.lookup(fd), ptr, dir));
}

iss_reg_t Htif::sys_read(iss_reg_t fd, iss_reg_t pbuf, iss_reg_t len, iss_reg_t a3, iss_reg_t a4, iss_reg_t a5, iss_reg_t a6)
{
    std::vector<char> buf(len);
    ssize_t ret = read(fds.lookup(fd), buf.data(), len);
    iss_reg_t ret_errno = sysret_errno(ret);
    if (ret > 0)
        this->target_access(pbuf, ret, true, (uint8_t *)buf.data());
    return ret_errno;
}

iss_reg_t Htif::sys_pread(iss_reg_t fd, iss_reg_t pbuf, iss_reg_t len, iss_reg_t off, iss_reg_t a4, iss_reg_t a5, iss_reg_t a6)
{
    std::vector<char> buf(len);
    ssize_t ret = pread(fds.lookup(fd), buf.data(), len, off);
    iss_reg_t ret_errno = sysret_errno(ret);
    if (ret > 0)
        this->target_access(pbuf, ret, true, (uint8_t *)buf.data());
    return ret_errno;
}

iss_reg_t Htif::sys_pwrite(iss_reg_t fd, iss_reg_t pbuf, iss_reg_t len, iss_reg_t off, iss_reg_t a4, iss_reg_t a5, iss_reg_t a6)
{
    std::vector<char> buf(len);
    this->target_access(pbuf, len, false, (uint8_t *)buf.data());
    iss_reg_t ret = sysret_errno(pwrite(fds.lookup(fd), buf.data(), len, off));
    return ret;
}

iss_reg_t Htif::sys_fstatat(iss_reg_t dirfd, iss_reg_t pname, iss_reg_t len, iss_reg_t pbuf, iss_reg_t flags, iss_reg_t a5, iss_reg_t a6)
{
    std::vector<char> name(len);
    this->target_access(pname, len, false, (uint8_t *)name.data());

    struct stat buf;
    iss_reg_t ret = sysret_errno(AT_SYSCALL(fstatat, dirfd, name.data(), &buf, flags));
    if (ret != (iss_reg_t)-1)
    {
        riscv_stat rbuf(buf);
        this->target_access(pbuf, sizeof(rbuf), true, (uint8_t *)&rbuf);
    }
    return ret;
}

iss_reg_t Htif::sys_write(iss_reg_t fd, iss_reg_t pbuf, iss_reg_t len, iss_reg_t a3, iss_reg_t a4, iss_reg_t a5, iss_reg_t a6)
{
    std::vector<char> buf(len);
    this->target_access(pbuf, len, false, (uint8_t *)buf.data());

    iss_reg_t ret = sysret_errno(write(fds.lookup(fd), buf.data(), len));
    return ret;
}

iss_reg_t Htif::sys_fstat(iss_reg_t fd, iss_reg_t pbuf, iss_reg_t a2, iss_reg_t a3, iss_reg_t a4, iss_reg_t a5, iss_reg_t a6)
{
  struct stat buf;
  iss_reg_t ret = sysret_errno(fstat(fds.lookup(fd), &buf));
  if (ret != (iss_reg_t)-1)
  {
    riscv_stat rbuf(buf);
    this->target_access(pbuf, sizeof(rbuf), true, (uint8_t *)&rbuf);
  }
  return ret;
}

iss_reg_t Htif::sys_statx(iss_reg_t fd, iss_reg_t pname, iss_reg_t len, iss_reg_t flags, iss_reg_t mask, iss_reg_t pbuf, iss_reg_t a6)
{
#ifndef HAVE_STATX
  return -ENOSYS;
#else
  std::vector<char> name(len);
  memif->read(pname, len, name.data());

  struct statx buf;
  iss_reg_t ret = sysret_errno(statx(fds.lookup(fd), do_chroot(name.data()).c_str(), flags, mask, &buf));
  if (ret != (iss_reg_t)-1)
  {
    riscv_statx rbuf(buf, htif);
    memif->write(pbuf, sizeof(rbuf), &rbuf);
  }
  return ret;
#endif
}

iss_reg_t Htif::sys_lstat(iss_reg_t pname, iss_reg_t len, iss_reg_t pbuf, iss_reg_t a3, iss_reg_t a4, iss_reg_t a5, iss_reg_t a6)
{
    std::vector<char> name(len);
    this->target_access(pname, len, false, (uint8_t *)name.data());

    struct stat buf;
    iss_reg_t ret = sysret_errno(lstat(do_chroot(name.data()).c_str(), &buf));
    if (ret != (iss_reg_t)-1)
    {
        riscv_stat rbuf(buf);
        this->target_access(pbuf, sizeof(rbuf), true, (uint8_t *)&rbuf);
    }
    return ret;
}

iss_reg_t Htif::sys_exit(iss_reg_t code, iss_reg_t a1, iss_reg_t a2, iss_reg_t a3, iss_reg_t a4, iss_reg_t a5, iss_reg_t a6)
{
    this->iss.top.time.get_engine()->quit(code);
    return 0;
}

iss_reg_t Htif::sys_getmainvars(iss_reg_t pbuf, iss_reg_t limit, iss_reg_t a2, iss_reg_t a3, iss_reg_t a4, iss_reg_t a5, iss_reg_t a6)
{
    std::vector<std::string> args = { "app" };

    for (auto x: this->iss.top.get_js_config()->get("args")->get_elems())
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

void Htif::set_chroot(const char* where)
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

iss_reg_t fds_t::alloc(int fd)
{
  iss_reg_t i;
  for (i = 0; i < fds.size(); i++)
    if (fds[i] == -1)
      break;

  if (i == fds.size())
    fds.resize(i+1);

  fds[i] = fd;
  return i;
}

void fds_t::dealloc(iss_reg_t fd)
{
  fds[fd] = -1;
}

int fds_t::lookup(iss_reg_t fd)
{
  if (int(fd) == RISCV_AT_FDCWD)
    return AT_FDCWD;
  return fd >= fds.size() ? -1 : fds[fd];
}

void Htif::reset(bool active)
{
    if (active)
    {
#ifdef CONFIG_GVSOC_ISS_HTIF
        if (this->tohost_addr != 0)
        {
            this->htif_event.enqueue();
        }
#endif
    }
}

void Htif::htif_handler(vp::Block *__this, vp::ClockEvent *event)
{
    Iss *iss = (Iss *)__this;

    if (iss->exec.fetch_enable_reg.get())
    {
        iss_reg_t cmd;
        iss->syscalls.htif.target_access(iss->syscalls.htif.tohost_addr, sizeof(cmd), false, (uint8_t *)&cmd);

        if (cmd != 0)
        {
            iss_reg_t value = 0;
            iss->syscalls.user_access(iss->syscalls.htif.tohost_addr, (uint8_t *)&value, sizeof(value), true);
            iss->syscalls.htif.handle_syscall(cmd);
        }
    }

    iss->syscalls.htif.htif_event.enqueue(1000);
}
