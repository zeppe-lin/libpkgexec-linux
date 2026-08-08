// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "test.h"

#include <libpkgexec/result.h>

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace test_support {

inline std::string material(
    const std::optional<pkgexec::stream_capture>& capture)
{
  CHECK(capture.has_value());
  CHECK(capture->material().has_value());
  return *capture->material();
}

inline std::string output(const pkgexec::execution_result& result)
{
  return result.standard_output() && result.standard_output()->material()
      ? *result.standard_output()->material()
      : std::string{};
}

inline std::string error_output(const pkgexec::execution_result& result)
{
  return result.standard_error() && result.standard_error()->material()
      ? *result.standard_error()->material()
      : std::string{};
}

inline bool has_guarantee(const pkgexec::execution_result& result,
                          pkgexec::execution_guarantee guarantee)
{
  return std::binary_search(result.established_guarantees().begin(),
                            result.established_guarantees().end(), guarantee);
}

inline void require_success(const pkgexec::execution_result& result,
                            std::string_view operation)
{
  if (result.status() != pkgexec::execution_status::succeeded) {
    throw std::runtime_error(std::string(operation) + " failed: " +
                             result.diagnostic() + ": " +
                             error_output(result));
  }
  CHECK(result.start_state() == pkgexec::execution_start_state::started);
  CHECK(result.cleanup() == pkgexec::cleanup_outcome::verified);
  CHECK(result.established_guarantees() ==
        result.request().required_guarantees());
}

} // namespace test_support
