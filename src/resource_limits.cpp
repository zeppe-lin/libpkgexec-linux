// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#include "resource_limits.h"

#include "process_control.h"

#include <cerrno>
#include <cstdint>
#include <limits>
#include <optional>

#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

namespace pkgexec_linux::detail {
namespace {

bool exact_value(std::uint64_t value, rlim_t& result) noexcept
{
  if constexpr (sizeof(rlim_t) < sizeof(std::uint64_t)) {
    if (value > static_cast<std::uint64_t>(
                    std::numeric_limits<rlim_t>::max())) {
      errno = EOVERFLOW;
      return false;
    }
  }
  result = static_cast<rlim_t>(value);
  if (result == RLIM_INFINITY) {
    errno = EOVERFLOW;
    return false;
  }
  return true;
}

bool setup_one(int resource, std::uint64_t value,
               resource_limit_setup_stage stage,
               resource_limit_setup_failure& failure) noexcept
{
  rlim_t exact = 0;
  if (!exact_value(value, exact)) {
    failure = {stage, errno};
    return false;
  }

  rlimit inherited{};
  if (::getrlimit(resource, &inherited) != 0) {
    failure = {stage, errno};
    return false;
  }
  if (inherited.rlim_max != RLIM_INFINITY && exact > inherited.rlim_max) {
    failure = {stage, EPERM};
    return false;
  }

  const rlimit requested{exact, exact};
  if (::setrlimit(resource, &requested) != 0) {
    failure = {stage, errno};
    return false;
  }

  rlimit observed{};
  if (::getrlimit(resource, &observed) != 0) {
    failure = {stage, errno};
    return false;
  }
  if (observed.rlim_cur != exact || observed.rlim_max != exact) {
    failure = {stage, EIO};
    return false;
  }
  return true;
}

std::optional<std::uint64_t> probe_value(int resource,
                                         std::uint64_t preferred) noexcept
{
  rlimit inherited{};
  if (::getrlimit(resource, &inherited) != 0) {
    return std::nullopt;
  }
  rlim_t candidate = static_cast<rlim_t>(preferred);
  if (candidate == RLIM_INFINITY) {
    return std::nullopt;
  }
  if (inherited.rlim_max != RLIM_INFINITY && candidate >= inherited.rlim_max) {
    if (inherited.rlim_max <= 2U) {
      return std::nullopt;
    }
    candidate = inherited.rlim_max - 1U;
  }
  if (candidate == 0U || candidate == RLIM_INFINITY) {
    return std::nullopt;
  }
  return static_cast<std::uint64_t>(candidate);
}

bool probe_one(pkgexec::resource_limit_kind kind, int resource,
               std::uint64_t preferred) noexcept
{
  const pid_t child = ::fork();
  if (child < 0) {
    return false;
  }
  if (child == 0) {
    const auto value = probe_value(resource, preferred);
    if (!value) {
      _exit(1);
    }
    pkgexec::resource_limits limits = pkgexec::resource_limits::make(
        std::nullopt,
        kind == pkgexec::resource_limit_kind::address_space
            ? value : std::nullopt,
        kind == pkgexec::resource_limit_kind::file_size
            ? value : std::nullopt,
        kind == pkgexec::resource_limit_kind::open_files
            ? value : std::nullopt,
        std::nullopt);
    resource_limit_setup_failure failure{};
    if (!setup_resource_limits(limits, failure)) {
      _exit(1);
    }
    if (!install_process_group_containment(true)) {
      _exit(1);
    }

    rlimit attempted{static_cast<rlim_t>(*value + 1U),
                     static_cast<rlim_t>(*value + 1U)};
    errno = 0;
    if (::setrlimit(resource, &attempted) == 0 || errno != EPERM) {
      _exit(1);
    }
    rlimit observed{};
    if (::getrlimit(resource, &observed) != 0 ||
        observed.rlim_cur != static_cast<rlim_t>(*value) ||
        observed.rlim_max != static_cast<rlim_t>(*value)) {
      _exit(1);
    }
    _exit(0);
  }

  int status = 0;
  while (::waitpid(child, &status, 0) < 0 && errno == EINTR) {
  }
  return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

} // namespace

bool setup_resource_limits(const pkgexec::resource_limits& limits,
                           resource_limit_setup_failure& failure) noexcept
{
  if (limits.address_space_bytes() &&
      !setup_one(RLIMIT_AS, *limits.address_space_bytes(),
                 resource_limit_setup_stage::address_space, failure)) {
    return false;
  }
  if (limits.file_size_bytes() &&
      !setup_one(RLIMIT_FSIZE, *limits.file_size_bytes(),
                 resource_limit_setup_stage::file_size, failure)) {
    return false;
  }
  if (limits.open_files() &&
      !setup_one(RLIMIT_NOFILE, *limits.open_files(),
                 resource_limit_setup_stage::open_files, failure)) {
    return false;
  }
  return true;
}

std::string_view resource_limit_stage_name(
    resource_limit_setup_stage stage) noexcept
{
  switch (stage) {
    case resource_limit_setup_stage::address_space:
      return "address-space resource limit";
    case resource_limit_setup_stage::file_size:
      return "file-size resource limit";
    case resource_limit_setup_stage::open_files:
      return "open-files resource limit";
  }
  return "resource limit";
}

bool probe_address_space_limit() noexcept
{
  return probe_one(pkgexec::resource_limit_kind::address_space,
                   RLIMIT_AS, 64U * 1024U * 1024U);
}

bool probe_file_size_limit() noexcept
{
  return probe_one(pkgexec::resource_limit_kind::file_size,
                   RLIMIT_FSIZE, 1024U * 1024U);
}

bool probe_open_files_limit() noexcept
{
  return probe_one(pkgexec::resource_limit_kind::open_files,
                   RLIMIT_NOFILE, 64U);
}

} // namespace pkgexec_linux::detail
