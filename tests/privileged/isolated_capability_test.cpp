// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#include "../support/test.h"

#include <libpkgexec-linux/libpkgexec-linux.h>

#include <cerrno>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

const pkgexec_linux::capability_observation& observation(
    const pkgexec_linux::capability_report& report,
    pkgexec_linux::capability_kind capability)
{
  for (const auto& value : report.observations()) {
    if (value.capability() == capability) {
      return value;
    }
  }
  throw std::runtime_error("isolated capability observation is absent");
}

bool has_errno_diagnostic(
    const pkgexec_linux::capability_observation& value, int error)
{
  const std::string suffix = std::string(": ") + std::strerror(error);
  const auto& diagnostic = value.diagnostic();
  return diagnostic.size() >= suffix.size() &&
      diagnostic.compare(diagnostic.size() - suffix.size(), suffix.size(),
                         suffix) == 0;
}

int test()
{
  using namespace pkgexec_linux;
  const auto report = capability_report::probe_isolated();
  const auto& mount = observation(report, capability_kind::mount_namespace);
  if (mount.state() == capability_state::available) {
    return 0;
  }

  std::cerr << "libpkgexec-linux:isolated-capability: "
            << to_string(mount.state());
  if (!mount.diagnostic().empty()) {
    std::cerr << ": " << mount.diagnostic();
  }
  std::cerr << '\n';

  // Request-level tests may skip an unsupported isolated profile so that
  // unprivileged development hosts remain useful. The direct privileged gate
  // must not let this known provider-internal cleanup failure masquerade as
  // environmental capability absence.
  CHECK(!has_errno_diagnostic(mount, EBUSY));
  return 77;
}

} // namespace

int main() { return run_test(test); }
