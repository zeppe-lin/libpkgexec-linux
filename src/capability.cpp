// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#include <libpkgexec-linux/capability.h>

#include <libpkgexec-linux/error.h>

#include "mount_isolation.h"
#include "network_isolation.h"
#include "process_control.h"
#include "support.h"

#include <algorithm>
#include <array>
#include <fstream>
#include <utility>

#include <linux/landlock.h>
#include <sched.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

namespace pkgexec_linux {
namespace {

pkgexec::backend_identity backend_identity(std::string_view name)
{
  return pkgexec::backend_identity::from_sha256(
      detail::sha256_hex("pkgexec-linux/backend/v1", name));
}

capability_state namespace_state(int flag) noexcept
{
  const pid_t child = ::fork();
  if (child < 0) {
    return capability_state::unavailable;
  }
  if (child == 0) {
    _exit(::unshare(flag) == 0 ? 0 : (errno == EPERM ? 2 : 1));
  }
  int status = 0;
  while (::waitpid(child, &status, 0) < 0 && errno == EINTR) {
  }
  if (!WIFEXITED(status)) {
    return capability_state::unavailable;
  }
  if (WEXITSTATUS(status) == 0) {
    return capability_state::available;
  }
  return WEXITSTATUS(status) == 2 ? capability_state::policy_restricted
                                  : capability_state::unavailable;
}

capability_state landlock_state() noexcept
{
#ifdef __NR_landlock_create_ruleset
  const int version = static_cast<int>(::syscall(
      __NR_landlock_create_ruleset, nullptr, 0,
      LANDLOCK_CREATE_RULESET_VERSION));
  if (version >= 1) {
    return capability_state::available;
  }
  return errno == EPERM || errno == EACCES
      ? capability_state::policy_restricted
      : capability_state::unavailable;
#else
  return capability_state::unavailable;
#endif
}

capability_state cgroup_state()
{
  std::ifstream controllers("/sys/fs/cgroup/cgroup.controllers");
  if (!controllers) {
    return capability_state::unavailable;
  }
  std::ofstream probe("/sys/fs/cgroup/cgroup.procs", std::ios::app);
  return probe ? capability_state::available
               : capability_state::policy_restricted;
}

} // namespace

std::string_view to_string(capability_kind value) noexcept
{
  switch (value) {
    case capability_kind::process_supervision: return "process-supervision";
    case capability_kind::closed_environment: return "closed-environment";
    case capability_kind::current_root_view: return "current-root-view";
    case capability_kind::current_credentials: return "current-credentials";
    case capability_kind::writable_resources: return "writable-resources";
    case capability_kind::complete_stream_capture: return "complete-stream-capture";
    case capability_kind::process_group_containment: return "process-group-containment";
    case capability_kind::no_new_privileges: return "no-new-privileges";
    case capability_kind::descriptor_execution: return "descriptor-execution";
    case capability_kind::close_range: return "close-range";
    case capability_kind::pidfd: return "pidfd";
    case capability_kind::mount_namespace: return "mount-namespace";
    case capability_kind::private_mount_propagation: return "private-mount-propagation";
    case capability_kind::openat2: return "openat2";
    case capability_kind::open_tree: return "open-tree";
    case capability_kind::move_mount: return "move-mount";
    case capability_kind::mount_setattr: return "mount-setattr";
    case capability_kind::chroot: return "chroot";
    case capability_kind::capability_drop: return "capability-drop";
    case capability_kind::user_namespace: return "user-namespace";
    case capability_kind::pid_namespace: return "pid-namespace";
    case capability_kind::network_namespace: return "network-namespace";
    case capability_kind::landlock: return "landlock";
    case capability_kind::cgroup_v2: return "cgroup-v2";
    case capability_kind::loopback_configuration: return "loopback-configuration";
  }
  return "unknown";
}

std::string_view to_string(capability_state value) noexcept
{
  switch (value) {
    case capability_state::available: return "available";
    case capability_state::unavailable: return "unavailable";
    case capability_state::policy_restricted: return "policy-restricted";
  }
  return "unknown";
}

capability_observation::capability_observation(
    capability_kind capability, capability_state state, std::string diagnostic)
    : capability_(capability), state_(state), diagnostic_(std::move(diagnostic))
{
}
capability_kind capability_observation::capability() const noexcept
{ return capability_; }
capability_state capability_observation::state() const noexcept
{ return state_; }
const std::string& capability_observation::diagnostic() const noexcept
{ return diagnostic_; }

capability_report::capability_report(
    pkgexec::backend_capability_profile profile,
    std::vector<capability_observation> observations)
    : profile_(std::move(profile)), observations_(std::move(observations))
{
}

capability_report capability_report::probe()
{
  const bool containment = detail::probe_process_group_containment();
  const bool descriptor_execution = detail::probe_descriptor_execution();
  std::vector<pkgexec::execution_guarantee> guarantees{
      pkgexec::execution_guarantee::closed_environment,
      pkgexec::execution_guarantee::root_view,
      pkgexec::execution_guarantee::writable_resources,
      pkgexec::execution_guarantee::fixed_credentials,
      pkgexec::execution_guarantee::complete_stdout_capture,
      pkgexec::execution_guarantee::complete_stderr_capture,
  };
  if (descriptor_execution) {
    guarantees.push_back(pkgexec::execution_guarantee::exact_interpreter);
  }
  if (containment) {
    guarantees.push_back(pkgexec::execution_guarantee::cleanup_verified);
  }
  std::vector<capability_observation> observations{
      {capability_kind::process_supervision, capability_state::available},
      {capability_kind::closed_environment, capability_state::available},
      {capability_kind::current_root_view, capability_state::available,
       "only the current / root view is admitted"},
      {capability_kind::current_credentials, capability_state::available,
       "only the supervisor's current credentials are admitted"},
      {capability_kind::writable_resources, capability_state::available,
       "resources must already exist at their logical paths"},
      {capability_kind::complete_stream_capture, capability_state::available},
      {capability_kind::process_group_containment,
       containment ? capability_state::available : capability_state::unavailable},
      {capability_kind::no_new_privileges,
       containment ? capability_state::available : capability_state::unavailable},
      {capability_kind::descriptor_execution,
       descriptor_execution ? capability_state::available
                            : capability_state::unavailable},
      {capability_kind::close_range,
       detail::probe_close_range() ? capability_state::available
                                   : capability_state::unavailable},
      {capability_kind::pidfd,
       detail::probe_pidfd() ? capability_state::available
                             : capability_state::unavailable},
      {capability_kind::mount_namespace, namespace_state(CLONE_NEWNS)},
      {capability_kind::private_mount_propagation, capability_state::unavailable,
       "host supervisor creates no mount namespace"},
      {capability_kind::openat2,
       detail::probe_openat2() ? capability_state::available
                               : capability_state::unavailable},
      {capability_kind::open_tree, capability_state::unavailable,
       "host supervisor creates no detached mount trees"},
      {capability_kind::move_mount, capability_state::unavailable,
       "host supervisor attaches no mount trees"},
      {capability_kind::mount_setattr, capability_state::unavailable,
       "host supervisor applies no mount attributes"},
      {capability_kind::chroot, capability_state::unavailable,
       "host supervisor enters no alternate root"},
      {capability_kind::capability_drop,
       detail::probe_capability_drop() ? capability_state::available
                                       : capability_state::unavailable},
      {capability_kind::user_namespace, namespace_state(CLONE_NEWUSER)},
      {capability_kind::pid_namespace, namespace_state(CLONE_NEWPID)},
      {capability_kind::network_namespace, namespace_state(CLONE_NEWNET)},
      {capability_kind::landlock, landlock_state()},
      {capability_kind::cgroup_v2, cgroup_state()},
      {capability_kind::loopback_configuration, capability_state::unavailable,
       "host supervisor creates no private loopback view"},
  };
  return capability_report(
      pkgexec::backend_capability_profile::seal(
          backend_identity("host-supervisor"), std::move(guarantees)),
      std::move(observations));
}

capability_report capability_report::probe_isolated()
{
  const bool containment = detail::probe_process_group_containment();
  const bool descriptor_execution = detail::probe_descriptor_execution();
  const bool capability_drop = detail::probe_capability_drop();
  int mount_error = 0;
  const bool filesystem = detail::probe_isolated_filesystem(mount_error);
  const auto mount_state = filesystem
      ? capability_state::available
      : (mount_error == EPERM || mount_error == EACCES
             ? capability_state::policy_restricted
             : capability_state::unavailable);
  detail::network_setup_failure denied_failure{};
  detail::network_setup_failure loopback_failure{};
  const bool denied_network = detail::probe_network_policy(
      pkgexec::network_policy::denied, denied_failure);
  const bool loopback_network = detail::probe_network_policy(
      pkgexec::network_policy::loopback_only, loopback_failure);
  const auto network_state = [](bool available,
                                const detail::network_setup_failure& failure) {
    return available
        ? capability_state::available
        : (failure.error == EPERM || failure.error == EACCES
               ? capability_state::policy_restricted
               : capability_state::unavailable);
  };

  std::vector<pkgexec::execution_guarantee> guarantees{
      pkgexec::execution_guarantee::closed_environment,
      pkgexec::execution_guarantee::fixed_credentials,
      pkgexec::execution_guarantee::complete_stdout_capture,
      pkgexec::execution_guarantee::complete_stderr_capture,
  };
  if (descriptor_execution) {
    guarantees.push_back(pkgexec::execution_guarantee::exact_interpreter);
  }
  if (filesystem && capability_drop) {
    guarantees.push_back(pkgexec::execution_guarantee::root_view);
    guarantees.push_back(pkgexec::execution_guarantee::read_only_resources);
    guarantees.push_back(pkgexec::execution_guarantee::writable_resources);
  }
  if (containment && filesystem) {
    guarantees.push_back(pkgexec::execution_guarantee::cleanup_verified);
  }
  if (containment && capability_drop && denied_network) {
    guarantees.push_back(pkgexec::execution_guarantee::network_denied);
  }
  if (containment && capability_drop && loopback_network) {
    guarantees.push_back(pkgexec::execution_guarantee::loopback_isolated);
  }

  const std::string mount_diagnostic = filesystem
      ? "private descriptor-oriented root and resource mounts are available"
      : detail::errno_message("isolated mount probe", mount_error);
  const auto network_diagnostic = [](bool available,
                                     const detail::network_setup_failure& failure) {
    return available
        ? std::string("private network policy realization is available")
        : detail::errno_message(detail::network_stage_name(failure.stage),
                                failure.error);
  };
  std::vector<capability_observation> observations{
      {capability_kind::process_supervision, capability_state::available},
      {capability_kind::closed_environment, capability_state::available},
      {capability_kind::current_root_view, filesystem ? capability_state::available
                                                     : mount_state,
       "the exact supplied root view is cloned read-only"},
      {capability_kind::current_credentials, capability_state::available,
       "only the supervisor's current numeric credentials are admitted"},
      {capability_kind::writable_resources, filesystem ? capability_state::available
                                                      : mount_state,
       mount_diagnostic},
      {capability_kind::complete_stream_capture, capability_state::available},
      {capability_kind::process_group_containment,
       containment ? capability_state::available : capability_state::unavailable},
      {capability_kind::no_new_privileges,
       containment ? capability_state::available : capability_state::unavailable},
      {capability_kind::descriptor_execution,
       descriptor_execution ? capability_state::available
                            : capability_state::unavailable},
      {capability_kind::close_range,
       detail::probe_close_range() ? capability_state::available
                                   : capability_state::unavailable},
      {capability_kind::pidfd,
       detail::probe_pidfd() ? capability_state::available
                             : capability_state::unavailable},
      {capability_kind::mount_namespace, mount_state, mount_diagnostic},
      {capability_kind::private_mount_propagation, mount_state, mount_diagnostic},
      {capability_kind::openat2,
       detail::probe_openat2() ? capability_state::available
                               : capability_state::unavailable},
      {capability_kind::open_tree, mount_state, mount_diagnostic},
      {capability_kind::move_mount, mount_state, mount_diagnostic},
      {capability_kind::mount_setattr, mount_state, mount_diagnostic},
      {capability_kind::chroot, mount_state, mount_diagnostic},
      {capability_kind::capability_drop,
       capability_drop ? capability_state::available
                       : capability_state::unavailable},
      {capability_kind::user_namespace, namespace_state(CLONE_NEWUSER),
       "observed only; isolated backend does not create a user namespace"},
      {capability_kind::pid_namespace, namespace_state(CLONE_NEWPID)},
      {capability_kind::network_namespace,
       network_state(denied_network, denied_failure),
       network_diagnostic(denied_network, denied_failure)},
      {capability_kind::landlock, landlock_state()},
      {capability_kind::cgroup_v2, cgroup_state()},
      {capability_kind::loopback_configuration,
       network_state(loopback_network, loopback_failure),
       network_diagnostic(loopback_network, loopback_failure)},
  };
  return capability_report(
      pkgexec::backend_capability_profile::seal(
          backend_identity("isolated-filesystem"), std::move(guarantees)),
      std::move(observations));
}

const pkgexec::backend_capability_profile& capability_report::profile() const noexcept
{ return profile_; }
const std::vector<capability_observation>& capability_report::observations() const noexcept
{ return observations_; }
capability_state capability_report::state(capability_kind capability) const
{
  const auto found = std::find_if(
      observations_.begin(), observations_.end(),
      [capability](const capability_observation& value) {
        return value.capability() == capability;
      });
  if (found == observations_.end()) {
    throw error(error_code::invalid_value, "unknown capability observation");
  }
  return found->state();
}

} // namespace pkgexec_linux
