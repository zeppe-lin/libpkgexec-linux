// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#include <libpkgexec-linux/capability.h>

#include <libpkgexec-linux/error.h>

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

pkgexec::backend_identity host_backend_identity()
{
  return pkgexec::backend_identity::from_sha256(
      detail::sha256_hex("pkgexec-linux/backend/v1", "host-supervisor"));
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
    case capability_kind::user_namespace: return "user-namespace";
    case capability_kind::pid_namespace: return "pid-namespace";
    case capability_kind::network_namespace: return "network-namespace";
    case capability_kind::landlock: return "landlock";
    case capability_kind::cgroup_v2: return "cgroup-v2";
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
      {capability_kind::user_namespace, namespace_state(CLONE_NEWUSER)},
      {capability_kind::pid_namespace, namespace_state(CLONE_NEWPID)},
      {capability_kind::network_namespace, namespace_state(CLONE_NEWNET)},
      {capability_kind::landlock, landlock_state()},
      {capability_kind::cgroup_v2, cgroup_state()},
  };
  return capability_report(
      pkgexec::backend_capability_profile::seal(host_backend_identity(),
                                                std::move(guarantees)),
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
