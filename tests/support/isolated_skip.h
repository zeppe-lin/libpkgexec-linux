// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <libpkgexec-linux/backend.h>

#include <iostream>
#include <string_view>

namespace test_support {

inline int isolated_skip(
    std::string_view scenario,
    const pkgexec_linux::isolated_backend& backend,
    const pkgexec::execution_request& request,
    const pkgexec::execution_result& result)
{
  std::cerr << "libpkgexec-linux:isolated:" << scenario << ": "
            << result.diagnostic() << '\n';
  for (const auto& observation : backend.report().observations()) {
    if (observation.state() == pkgexec_linux::capability_state::available) {
      continue;
    }
    std::cerr << "  " << pkgexec_linux::to_string(observation.capability())
              << '=' << pkgexec_linux::to_string(observation.state());
    if (!observation.diagnostic().empty()) {
      std::cerr << ": " << observation.diagnostic();
    }
    std::cerr << '\n';
  }
  std::cerr << "  required:";
  for (const auto guarantee : request.required_guarantees()) {
    std::cerr << ' ' << pkgexec::to_string(guarantee);
  }
  std::cerr << '\n';
  return 77;
}

} // namespace test_support
