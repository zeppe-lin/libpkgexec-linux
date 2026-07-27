// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#include "process_control.h"

#include <cerrno>
#include <climits>
#include <cstddef>
#include <cstdint>

#include <fcntl.h>
#include <linux/audit.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <signal.h>
#include <stddef.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace pkgexec_linux::detail {
namespace {

#if !defined(__x86_64__) || !defined(AUDIT_ARCH_X86_64)
constexpr bool supported_architecture = false;
#else
constexpr bool supported_architecture = true;
#endif

} // namespace

bool install_process_group_containment() noexcept
{
  if (!supported_architecture) {
    return false;
  }
  if (::prctl(PR_SET_NO_NEW_PRIVS, 1L, 0L, 0L, 0L) != 0) {
    return false;
  }
#if defined(__x86_64__) && defined(AUDIT_ARCH_X86_64)
  const sock_filter filter[] = {
      BPF_STMT(BPF_LD | BPF_W | BPF_ABS,
               static_cast<unsigned int>(offsetof(seccomp_data, arch))),
      BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, AUDIT_ARCH_X86_64, 1, 0),
      BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS),
      BPF_STMT(BPF_LD | BPF_W | BPF_ABS,
               static_cast<unsigned int>(offsetof(seccomp_data, nr))),
#ifdef __NR_setsid
      BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_setsid, 0, 1),
      BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),
#endif
#ifdef __NR_setpgid
      BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_setpgid, 0, 1),
      BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),
#endif
#ifdef __NR_unshare
      BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_unshare, 0, 1),
      BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),
#endif
#ifdef __NR_setns
      BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_setns, 0, 1),
      BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),
#endif
      BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
  };
  const sock_fprog program{
      static_cast<unsigned short>(sizeof(filter) / sizeof(filter[0])),
      const_cast<sock_filter*>(filter),
  };
  return ::prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &program) == 0;
#else
  return false;
#endif
}

bool probe_process_group_containment() noexcept
{
  const pid_t child = ::fork();
  if (child < 0) {
    return false;
  }
  if (child == 0) {
    _exit(install_process_group_containment() ? 0 : 1);
  }
  int status = 0;
  while (::waitpid(child, &status, 0) < 0 && errno == EINTR) {
  }
  return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

bool probe_descriptor_execution() noexcept
{
#ifdef __NR_execveat
  errno = 0;
  (void)::syscall(__NR_execveat, -1, "", nullptr, nullptr, AT_EMPTY_PATH);
  return errno != ENOSYS;
#else
  return false;
#endif
}

bool probe_close_range() noexcept
{
#ifdef __NR_close_range
  errno = 0;
  const long result = ::syscall(__NR_close_range, UINT_MAX, UINT_MAX, 0U);
  return result == 0 || errno != ENOSYS;
#else
  return false;
#endif
}

bool probe_pidfd() noexcept
{
#ifdef __NR_pidfd_open
  const int fd = static_cast<int>(::syscall(__NR_pidfd_open, ::getpid(), 0U));
  if (fd < 0) {
    return errno != ENOSYS && errno != EINVAL;
  }
  ::close(fd);
  return true;
#else
  return false;
#endif
}

void close_fds_except(int first, int second) noexcept
{
  if (second < first) {
    const int temporary = first;
    first = second;
    second = temporary;
  }
#ifdef __NR_close_range
  bool closed = true;
  if (first > 3) {
    closed = ::syscall(__NR_close_range, 3U,
                       static_cast<unsigned int>(first - 1), 0U) == 0;
  }
  if (closed && second > first + 1) {
    closed = ::syscall(__NR_close_range,
                       static_cast<unsigned int>(first + 1),
                       static_cast<unsigned int>(second - 1), 0U) == 0;
  }
  if (closed) {
    closed = ::syscall(__NR_close_range,
                       static_cast<unsigned int>(second + 1), UINT_MAX, 0U) == 0;
  }
  if (closed) {
    return;
  }
#endif
  const long maximum = ::sysconf(_SC_OPEN_MAX);
  const int upper = maximum > 0 && maximum < INT_MAX ? static_cast<int>(maximum)
                                                     : 65536;
  for (int fd = 3; fd < upper; ++fd) {
    if (fd != first && fd != second) {
      ::close(fd);
    }
  }
}

} // namespace pkgexec_linux::detail
