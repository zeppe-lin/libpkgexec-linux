// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#include "process_control.h"

#include <cerrno>
#include <chrono>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include <dirent.h>
#include <fcntl.h>
#include <linux/audit.h>
#include <linux/capability.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <poll.h>
#include <sched.h>
#include <signal.h>
#include <stddef.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef CLONE_NEWTIME
#define CLONE_NEWTIME 0x00000080
#endif


namespace pkgexec_linux::detail {
namespace {

#if !defined(__x86_64__) || !defined(AUDIT_ARCH_X86_64)
constexpr bool supported_architecture = false;
#else
constexpr bool supported_architecture = true;
#endif
constexpr idtype_t pidfd_wait_type = static_cast<idtype_t>(3);

bool numeric_name(const char* value) noexcept
{
  if (!value || *value == '\0') {
    return false;
  }
  for (const char* cursor = value; *cursor != '\0'; ++cursor) {
    if (*cursor < '0' || *cursor > '9') {
      return false;
    }
  }
  return true;
}

bool process_group_of(pid_t process, pid_t& group) noexcept
{
  try {
    std::ifstream stream("/proc/" + std::to_string(process) + "/stat");
    std::string line;
    if (!stream || !std::getline(stream, line)) {
      return false;
    }
    const auto close = line.rfind(')');
    if (close == std::string::npos || close + 2U >= line.size()) {
      return false;
    }
    char state = '\0';
    long parent = 0;
    long parsed_group = 0;
    if (std::sscanf(line.c_str() + close + 2U, "%c %ld %ld",
                    &state, &parent, &parsed_group) != 3 ||
        parsed_group <= 0 || parsed_group > INT_MAX) {
      return false;
    }
    (void)state;
    (void)parent;
    group = static_cast<pid_t>(parsed_group);
    return true;
  } catch (...) {
    return false;
  }
}

bool process_ids(std::vector<pid_t>& result) noexcept
{
  result.clear();
  DIR* directory = ::opendir("/proc");
  if (!directory) {
    return false;
  }
  try {
    errno = 0;
    while (dirent* entry = ::readdir(directory)) {
      if (!numeric_name(entry->d_name)) {
        continue;
      }
      char* end = nullptr;
      errno = 0;
      const long value = std::strtol(entry->d_name, &end, 10);
      if (errno == 0 && end && *end == '\0' &&
          value > 0 && value <= INT_MAX) {
        result.push_back(static_cast<pid_t>(value));
      }
    }
    const int saved = errno;
    const bool okay = ::closedir(directory) == 0 && saved == 0;
    return okay;
  } catch (...) {
    (void)::closedir(directory);
    result.clear();
    return false;
  }
}

bool pidfd_exited(int pidfd, int timeout_milliseconds) noexcept
{
  pollfd descriptor{pidfd, POLLIN, 0};
  for (;;) {
    const int result = ::poll(&descriptor, 1, timeout_milliseconds);
    if (result > 0) {
      return (descriptor.revents & (POLLIN | POLLHUP | POLLERR)) != 0;
    }
    if (result == 0) {
      return false;
    }
    if (errno != EINTR) {
      return false;
    }
  }
}

} // namespace

bool install_process_group_containment() noexcept
{
  return install_process_group_containment(false);
}

bool install_process_group_containment(bool seal_resource_limits) noexcept
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
#ifdef __NR_clone
      BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_clone, 0, 3),
      BPF_STMT(BPF_LD | BPF_W | BPF_ABS,
               static_cast<unsigned int>(offsetof(seccomp_data, args[0]))),
      BPF_JUMP(
          BPF_JMP | BPF_JSET | BPF_K,
          static_cast<unsigned int>(
              CLONE_NEWCGROUP | CLONE_NEWIPC | CLONE_NEWNET | CLONE_NEWNS |
              CLONE_NEWPID | CLONE_NEWTIME | CLONE_NEWUSER | CLONE_NEWUTS),
          0, 1),
      BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),
      BPF_STMT(BPF_LD | BPF_W | BPF_ABS,
               static_cast<unsigned int>(offsetof(seccomp_data, nr))),
#endif
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
#ifdef __NR_clone3
      BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_clone3, 0, 1),
      BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | ENOSYS),
#endif
#ifdef __NR_mount
      BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_mount, 0, 1),
      BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),
#endif
#ifdef __NR_umount2
      BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_umount2, 0, 1),
      BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),
#endif
#ifdef __NR_pivot_root
      BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_pivot_root, 0, 1),
      BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),
#endif
#ifdef __NR_chroot
      BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_chroot, 0, 1),
      BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),
#endif
#ifdef __NR_open_tree
      BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_open_tree, 0, 1),
      BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),
#endif
#ifdef __NR_move_mount
      BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_move_mount, 0, 1),
      BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),
#endif
#ifdef __NR_mount_setattr
      BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_mount_setattr, 0, 1),
      BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EPERM),
#endif
#ifdef __NR_setrlimit
      BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_setrlimit, 0, 1),
      BPF_STMT(BPF_RET | BPF_K,
               seal_resource_limits ? SECCOMP_RET_ERRNO | EPERM
                                    : SECCOMP_RET_ALLOW),
#endif
#ifdef __NR_prlimit64
      BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_prlimit64, 0, 5),
      BPF_STMT(BPF_LD | BPF_W | BPF_ABS,
               static_cast<unsigned int>(offsetof(seccomp_data, args[2]))),
      BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, 0U, 0, 2),
      BPF_STMT(BPF_LD | BPF_W | BPF_ABS,
               static_cast<unsigned int>(offsetof(seccomp_data, args[2]) + 4U)),
      BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, 0U, 1, 0),
      BPF_STMT(BPF_RET | BPF_K,
               seal_resource_limits ? SECCOMP_RET_ERRNO | EPERM
                                    : SECCOMP_RET_ALLOW),
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
    _exit(install_process_group_containment(false) ? 0 : 1);
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

int open_pidfd(pid_t process) noexcept
{
#ifdef __NR_pidfd_open
  return static_cast<int>(::syscall(__NR_pidfd_open, process, 0U));
#else
  (void)process;
  errno = ENOSYS;
  return -1;
#endif
}

bool send_pidfd_signal(int pidfd, int signal) noexcept
{
#ifdef __NR_pidfd_send_signal
  return ::syscall(__NR_pidfd_send_signal, pidfd, signal, nullptr, 0U) == 0;
#else
  (void)pidfd;
  (void)signal;
  errno = ENOSYS;
  return false;
#endif
}

process_group_signal_result signal_process_group_members(
    pid_t group, pid_t leader, int leader_pidfd, int signal) noexcept
{
  bool complete = true;
  std::vector<pid_t> processes;
  if (!process_ids(processes)) {
    complete = false;
  } else {
    for (const pid_t process : processes) {
      if (process == leader) {
        continue;
      }
      pid_t observed_group = 0;
      if (!process_group_of(process, observed_group) ||
          observed_group != group) {
        continue;
      }
      const int pidfd = open_pidfd(process);
      if (pidfd < 0) {
        if (errno != ESRCH) {
          complete = false;
        }
        continue;
      }
      pid_t confirmed_group = 0;
      const bool member = process_group_of(process, confirmed_group) &&
                          confirmed_group == group;
      if (member && !send_pidfd_signal(pidfd, signal) && errno != ESRCH) {
        complete = false;
      }
      ::close(pidfd);
    }
  }

  bool leader_signaled = send_pidfd_signal(leader_pidfd, signal);
  if (!leader_signaled && errno != ESRCH) {
    complete = false;
  }
  return {complete, leader_signaled};
}

bool wait_process_group_members_gone(
    pid_t group, pid_t leader, std::chrono::milliseconds timeout) noexcept
{
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  for (;;) {
    std::vector<pid_t> processes;
    if (!process_ids(processes)) {
      return false;
    }
    bool found = false;
    for (const pid_t process : processes) {
      if (process == leader) {
        continue;
      }
      pid_t member_group = 0;
      if (process_group_of(process, member_group) && member_group == group) {
        found = true;
        break;
      }
    }
    if (!found) {
      return true;
    }
    if (std::chrono::steady_clock::now() >= deadline) {
      return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

bool probe_pidfd() noexcept
{
  const int fd = open_pidfd(::getpid());
  if (fd < 0) {
    return errno != ENOSYS && errno != EINVAL;
  }
  ::close(fd);
  return true;
}

bool probe_pidfd_cancellation() noexcept
{
  int ready[2] = {-1, -1};
  if (::pipe2(ready, O_CLOEXEC) != 0) {
    return false;
  }
  const pid_t child = ::fork();
  if (child < 0) {
    ::close(ready[0]);
    ::close(ready[1]);
    return false;
  }
  if (child == 0) {
    ::close(ready[0]);
    if (::setsid() < 0) {
      _exit(2);
    }
    const pid_t descendant = ::fork();
    if (descendant < 0) {
      _exit(3);
    }
    if (descendant == 0) {
      for (;;) {
        ::pause();
      }
    }
    const unsigned char byte = 1;
    (void)::write(ready[1], &byte, 1);
    for (;;) {
      ::pause();
    }
  }

  ::close(ready[1]);
  unsigned char byte = 0;
  ssize_t count = -1;
  do {
    count = ::read(ready[0], &byte, 1);
  } while (count < 0 && errno == EINTR);
  ::close(ready[0]);
  if (count != 1) {
    (void)::kill(-child, SIGKILL);
    (void)::waitpid(child, nullptr, 0);
    return false;
  }

  const int pidfd = open_pidfd(child);
  if (pidfd < 0) {
    (void)::kill(-child, SIGKILL);
    (void)::waitpid(child, nullptr, 0);
    return false;
  }
  const auto signal_result = signal_process_group_members(
      child, child, pidfd, SIGTERM);
  const bool exited = pidfd_exited(pidfd, 1000);
  const bool members_gone = wait_process_group_members_gone(
      child, child, std::chrono::milliseconds(1000));
  siginfo_t information{};
  const bool observed = ::waitid(pidfd_wait_type, static_cast<id_t>(pidfd),
                                 &information, WEXITED | WNOWAIT) == 0;
  int status = 0;
  while (::waitpid(child, &status, 0) < 0 && errno == EINTR) {
  }
  ::close(pidfd);
  return signal_result.complete && signal_result.leader_signaled &&
         exited && members_gone && observed &&
         information.si_code == CLD_KILLED &&
         information.si_status == SIGTERM;
}

bool drop_process_capabilities() noexcept
{
#ifdef __NR_capset
  __user_cap_header_struct header{};
  header.version = _LINUX_CAPABILITY_VERSION_3;
  header.pid = 0;
  __user_cap_data_struct data[2]{};
  if (::syscall(__NR_capset, &header, data) != 0) {
    return false;
  }
#ifdef PR_CAP_AMBIENT
  if (::prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_CLEAR_ALL, 0L, 0L, 0L) != 0 &&
      errno != EINVAL) {
    return false;
  }
#endif
  return true;
#else
  errno = ENOSYS;
  return false;
#endif
}

bool probe_capability_drop() noexcept
{
  const pid_t child = ::fork();
  if (child < 0) {
    return false;
  }
  if (child == 0) {
    _exit(drop_process_capabilities() ? 0 : 1);
  }
  int status = 0;
  while (::waitpid(child, &status, 0) < 0 && errno == EINTR) {
  }
  return WIFEXITED(status) && WEXITSTATUS(status) == 0;
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
