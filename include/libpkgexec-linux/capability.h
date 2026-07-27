// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later

/*! \file capability.h
 *  \brief Linux host-supervisor capability observations.
 */
#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <libpkgexec/result.h>

namespace pkgexec_linux {

enum class capability_kind {
  process_supervision,
  closed_environment,
  current_root_view,
  current_credentials,
  writable_resources,
  complete_stream_capture,
  process_group_containment,
  no_new_privileges,
  descriptor_execution,
  close_range,
  pidfd,
  mount_namespace,
  private_mount_propagation,
  openat2,
  open_tree,
  move_mount,
  mount_setattr,
  chroot,
  capability_drop,
  user_namespace,
  pid_namespace,
  network_namespace,
  landlock,
  cgroup_v2,
};

enum class capability_state {
  available,
  unavailable,
  policy_restricted,
};

[[nodiscard]] std::string_view to_string(capability_kind value) noexcept;
[[nodiscard]] std::string_view to_string(capability_state value) noexcept;

class capability_observation final {
public:
  capability_observation(capability_kind capability,
                         capability_state state,
                         std::string diagnostic = {});
  [[nodiscard]] capability_kind capability() const noexcept;
  [[nodiscard]] capability_state state() const noexcept;
  [[nodiscard]] const std::string& diagnostic() const noexcept;
private:
  capability_kind capability_;
  capability_state state_;
  std::string diagnostic_;
};

/*! \brief Current backend guarantees plus diagnostic Linux feature probes. */
class capability_report final {
public:
  [[nodiscard]] static capability_report probe();
  [[nodiscard]] static capability_report probe_isolated();
  [[nodiscard]] const pkgexec::backend_capability_profile& profile() const noexcept;
  [[nodiscard]] const std::vector<capability_observation>& observations() const noexcept;
  [[nodiscard]] capability_state state(capability_kind capability) const;
private:
  capability_report(pkgexec::backend_capability_profile profile,
                    std::vector<capability_observation> observations);
  pkgexec::backend_capability_profile profile_;
  std::vector<capability_observation> observations_;
};

} // namespace pkgexec_linux
