// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <string_view>

#include <libpkgexec/model.h>

namespace pkgexec_linux::detail {

enum class resource_limit_setup_stage : std::uint32_t {
  address_space,
  file_size,
  open_files,
};

struct resource_limit_setup_failure final {
  resource_limit_setup_stage stage = resource_limit_setup_stage::address_space;
  int error = 0;
};

[[nodiscard]] bool setup_resource_limits(
    const pkgexec::resource_limits& limits,
    resource_limit_setup_failure& failure) noexcept;
[[nodiscard]] std::string_view resource_limit_stage_name(
    resource_limit_setup_stage stage) noexcept;

[[nodiscard]] bool probe_address_space_limit() noexcept;
[[nodiscard]] bool probe_file_size_limit() noexcept;
[[nodiscard]] bool probe_open_files_limit() noexcept;

} // namespace pkgexec_linux::detail
