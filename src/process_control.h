// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace pkgexec_linux::detail {

[[nodiscard]] bool install_process_group_containment() noexcept;
[[nodiscard]] bool probe_process_group_containment() noexcept;
[[nodiscard]] bool probe_descriptor_execution() noexcept;
[[nodiscard]] bool probe_close_range() noexcept;
[[nodiscard]] bool probe_pidfd() noexcept;
void close_fds_except(int first, int second) noexcept;

} // namespace pkgexec_linux::detail
