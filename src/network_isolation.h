// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <string_view>

#include <libpkgexec/model.h>

namespace pkgexec_linux::detail {

enum class network_setup_stage : std::uint32_t {
  network_namespace,
  link_inspection,
  link_configuration,
  policy_validation,
  loopback_roundtrip,
};

struct network_setup_failure final {
  network_setup_stage stage;
  int error;
};

[[nodiscard]] bool setup_network_policy(
    pkgexec::network_policy policy,
    network_setup_failure& failure) noexcept;
[[nodiscard]] bool probe_network_policy(
    pkgexec::network_policy policy,
    network_setup_failure& failure) noexcept;
[[nodiscard]] std::string_view network_stage_name(
    network_setup_stage stage) noexcept;

} // namespace pkgexec_linux::detail
