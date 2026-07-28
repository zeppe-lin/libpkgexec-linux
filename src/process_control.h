// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <chrono>

#include <sys/types.h>

namespace pkgexec_linux::detail {

[[nodiscard]] bool install_process_group_containment() noexcept;
[[nodiscard]] bool install_process_group_containment(
    bool seal_resource_limits) noexcept;
[[nodiscard]] bool probe_process_group_containment() noexcept;
[[nodiscard]] bool probe_descriptor_execution() noexcept;
[[nodiscard]] bool probe_close_range() noexcept;
[[nodiscard]] int open_pidfd(pid_t process) noexcept;
[[nodiscard]] bool send_pidfd_signal(int pidfd, int signal) noexcept;
struct process_group_signal_result final {
  bool complete;
  bool leader_signaled;
};

[[nodiscard]] process_group_signal_result signal_process_group_members(
    pid_t group, pid_t leader, int leader_pidfd, int signal) noexcept;
[[nodiscard]] bool wait_process_group_members_gone(
    pid_t group, pid_t leader, std::chrono::milliseconds timeout) noexcept;
[[nodiscard]] bool probe_pidfd() noexcept;
[[nodiscard]] bool probe_pidfd_cancellation() noexcept;
[[nodiscard]] bool drop_process_capabilities() noexcept;
[[nodiscard]] bool probe_capability_drop() noexcept;
void close_fds_except(int first, int second) noexcept;

} // namespace pkgexec_linux::detail
