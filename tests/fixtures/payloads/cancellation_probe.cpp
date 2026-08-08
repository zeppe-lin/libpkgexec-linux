// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#include <cerrno>
#include <csignal>
#include <cstring>
#include <string>

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

volatile sig_atomic_t terminate_requested = 0;

void request_termination(int) noexcept
{
  terminate_requested = 1;
}

bool write_ready(const char* path)
{
  const int fd = ::open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
  if (fd < 0) {
    return false;
  }
  const char material[] = "ready\n";
  const ssize_t count = ::write(fd, material, sizeof(material) - 1U);
  const int saved = errno;
  ::close(fd);
  errno = saved;
  return count == static_cast<ssize_t>(sizeof(material) - 1U);
}

[[noreturn]] void pause_forever()
{
  for (;;) {
    ::pause();
  }
}

} // namespace

int main(int argc, char** argv)
{
  if (argc != 3) {
    return 2;
  }
  const std::string mode = argv[1];
  if (mode == "graceful") {
    struct sigaction action {};
    action.sa_handler = request_termination;
    ::sigemptyset(&action.sa_mask);
    if (::sigaction(SIGTERM, &action, nullptr) != 0 || !write_ready(argv[2])) {
      return 3;
    }
    while (!terminate_requested) {
      ::pause();
    }
    return 0;
  }
  if (mode == "forced") {
    if (::signal(SIGTERM, SIG_IGN) == SIG_ERR) {
      return 4;
    }
    const pid_t descendant = ::fork();
    if (descendant < 0) {
      return 5;
    }
    if (descendant == 0) {
      pause_forever();
    }
    if (!write_ready(argv[2])) {
      return 6;
    }
    int status = 0;
    while (::waitpid(descendant, &status, 0) < 0 && errno == EINTR) {
    }
    pause_forever();
  }
  return 7;
}
