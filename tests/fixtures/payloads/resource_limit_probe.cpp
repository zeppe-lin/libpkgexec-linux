// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
#include <vector>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace {

std::uint64_t number(const char* value)
{
  char* end = nullptr;
  errno = 0;
  const unsigned long long parsed = std::strtoull(value, &end, 10);
  if (errno != 0 || !end || *end != '\0') {
    std::exit(2);
  }
  return static_cast<std::uint64_t>(parsed);
}

void show_one(const char* name, int resource)
{
  rlimit value{};
  if (::getrlimit(resource, &value) != 0) {
    std::exit(3);
  }
  std::printf("%s=%llu/%llu\n", name,
              static_cast<unsigned long long>(value.rlim_cur),
              static_cast<unsigned long long>(value.rlim_max));
}

int show()
{
  show_one("as", RLIMIT_AS);
  show_one("fsize", RLIMIT_FSIZE);
  show_one("nofile", RLIMIT_NOFILE);
  return 0;
}

int raise_limit(int resource, std::uint64_t requested)
{
  const rlimit value{static_cast<rlim_t>(requested),
                     static_cast<rlim_t>(requested)};
  errno = 0;
  const int result = ::setrlimit(resource, &value);
  std::printf("result=%d errno=%d\n", result, errno);
  return result == -1 && errno == EPERM ? 0 : 4;
}

int prlimit_read(int resource)
{
#ifdef __NR_prlimit64
  rlimit observed{};
  errno = 0;
  const long result = ::syscall(__NR_prlimit64, 0, resource, nullptr,
                                &observed);
  std::printf("result=%ld errno=%d value=%llu/%llu\n", result, errno,
              static_cast<unsigned long long>(observed.rlim_cur),
              static_cast<unsigned long long>(observed.rlim_max));
  return result == 0 ? 0 : 4;
#else
  (void)resource;
  return 2;
#endif
}

int prlimit_raise(int resource, std::uint64_t requested)
{
#ifdef __NR_prlimit64
  const rlimit value{static_cast<rlim_t>(requested),
                     static_cast<rlim_t>(requested)};
  errno = 0;
  const long result = ::syscall(__NR_prlimit64, 0, resource, &value,
                                nullptr);
  std::printf("result=%ld errno=%d\n", result, errno);
  return result == -1 && errno == EPERM ? 0 : 4;
#else
  (void)resource;
  (void)requested;
  return 2;
#endif
}

int open_until_failure()
{
  std::vector<int> descriptors;
  for (;;) {
    const int descriptor = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
    if (descriptor < 0) {
      std::printf("open=%zu errno=%d\n", descriptors.size(), errno);
      for (const int value : descriptors) {
        ::close(value);
      }
      return errno == EMFILE ? 0 : 5;
    }
    descriptors.push_back(descriptor);
  }
}

int map_bytes(std::uint64_t count)
{
  if (count > static_cast<std::uint64_t>(
                  std::numeric_limits<std::size_t>::max())) {
    return 2;
  }
  errno = 0;
  void* mapping = ::mmap(nullptr, static_cast<std::size_t>(count),
                         PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (mapping == MAP_FAILED) {
    std::printf("errno=%d\n", errno);
    return errno == ENOMEM ? 0 : 6;
  }
  ::munmap(mapping, static_cast<std::size_t>(count));
  std::printf("mapped\n");
  return 7;
}

int write_bytes(const char* path, std::uint64_t count)
{
  const int descriptor = ::open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC,
                                0600);
  if (descriptor < 0) {
    return 8;
  }
  char buffer[4096]{};
  std::uint64_t written = 0;
  while (written < count) {
    const std::size_t amount = static_cast<std::size_t>(
        std::min<std::uint64_t>(sizeof(buffer), count - written));
    const ssize_t result = ::write(descriptor, buffer, amount);
    if (result < 0) {
      const int saved = errno;
      ::close(descriptor);
      std::printf("written=%llu errno=%d\n",
                  static_cast<unsigned long long>(written), saved);
      return 9;
    }
    written += static_cast<std::uint64_t>(result);
  }
  ::close(descriptor);
  std::printf("written=%llu\n", static_cast<unsigned long long>(written));
  return 0;
}

} // namespace

int main(int argc, char** argv)
{
  if (argc == 2 && std::string(argv[1]) == "show") {
    return show();
  }
  const std::string operation = argc >= 2 ? argv[1] : "";
  if ((argc == 4 && (operation == "raise" ||
                     operation == "prlimit-raise")) ||
      (argc == 3 && operation == "prlimit-read")) {
    const std::string kind(argv[2]);
    const int resource = kind == "as" ? RLIMIT_AS
                       : kind == "fsize" ? RLIMIT_FSIZE
                       : kind == "nofile" ? RLIMIT_NOFILE : -1;
    if (resource < 0) {
      return 2;
    }
    if (operation == "raise") {
      return raise_limit(resource, number(argv[3]));
    }
    if (operation == "prlimit-read") {
      return prlimit_read(resource);
    }
    return prlimit_raise(resource, number(argv[3]));
  }
  if (argc == 2 && std::string(argv[1]) == "open") {
    return open_until_failure();
  }
  if (argc == 3 && std::string(argv[1]) == "mmap") {
    return map_bytes(number(argv[2]));
  }
  if (argc == 4 && std::string(argv[1]) == "write") {
    return write_bytes(argv[2], number(argv[3]));
  }
  return 2;
}
