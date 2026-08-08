// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#include "../support/test.h"

#include <libpkgexec-linux/capability.h>
#include <libpkgexec-linux/error.h>

#include <string>

namespace {

int test()
{
  using namespace pkgexec_linux;

  const capability_observation observation(
      capability_kind::mount_namespace,
      capability_state::policy_restricted,
      "operation not permitted");
  CHECK(observation.capability() == capability_kind::mount_namespace);
  CHECK(observation.state() == capability_state::policy_restricted);
  CHECK(observation.diagnostic() == "operation not permitted");

  CHECK(to_string(capability_kind::process_supervision) == "process-supervision");
  CHECK(to_string(capability_kind::closed_environment) == "closed-environment");
  CHECK(to_string(capability_kind::current_root_view) == "current-root-view");
  CHECK(to_string(capability_kind::current_credentials) == "current-credentials");
  CHECK(to_string(capability_kind::writable_resources) == "writable-resources");
  CHECK(to_string(capability_kind::complete_stream_capture) ==
        "complete-stream-capture");
  CHECK(to_string(capability_kind::process_group_containment) ==
        "process-group-containment");
  CHECK(to_string(capability_kind::no_new_privileges) == "no-new-privileges");
  CHECK(to_string(capability_kind::descriptor_execution) == "descriptor-execution");
  CHECK(to_string(capability_kind::close_range) == "close-range");
  CHECK(to_string(capability_kind::pidfd) == "pidfd");
  CHECK(to_string(capability_kind::pidfd_cancellation) == "pidfd-cancellation");
  CHECK(to_string(capability_kind::mount_namespace) == "mount-namespace");
  CHECK(to_string(capability_kind::private_mount_propagation) ==
        "private-mount-propagation");
  CHECK(to_string(capability_kind::openat2) == "openat2");
  CHECK(to_string(capability_kind::open_tree) == "open-tree");
  CHECK(to_string(capability_kind::move_mount) == "move-mount");
  CHECK(to_string(capability_kind::mount_setattr) == "mount-setattr");
  CHECK(to_string(capability_kind::chroot) == "chroot");
  CHECK(to_string(capability_kind::capability_drop) == "capability-drop");
  CHECK(to_string(capability_kind::user_namespace) == "user-namespace");
  CHECK(to_string(capability_kind::pid_namespace) == "pid-namespace");
  CHECK(to_string(capability_kind::network_namespace) == "network-namespace");
  CHECK(to_string(capability_kind::landlock) == "landlock");
  CHECK(to_string(capability_kind::cgroup_v2) == "cgroup-v2");
  CHECK(to_string(capability_kind::loopback_configuration) ==
        "loopback-configuration");
  CHECK(to_string(capability_kind::address_space_limit) == "address-space-limit");
  CHECK(to_string(capability_kind::file_size_limit) == "file-size-limit");
  CHECK(to_string(capability_kind::open_files_limit) == "open-files-limit");
  CHECK(to_string(capability_state::available) == "available");
  CHECK(to_string(capability_state::unavailable) == "unavailable");
  CHECK(to_string(capability_state::policy_restricted) == "policy-restricted");

  const error value(error_code::duplicate_interpreter, "duplicate");
  CHECK(value.code() == error_code::duplicate_interpreter);
  CHECK(std::string(value.what()) == "duplicate");
  return 0;
}

} // namespace

int main() { return run_test(test); }
