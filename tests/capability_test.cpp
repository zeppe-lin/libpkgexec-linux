// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#include "test.h"

#include <libpkgexec-linux/capability.h>

#include <algorithm>

namespace {

int test()
{
  using namespace pkgexec_linux;
  const auto first = capability_report::probe();
  const auto second = capability_report::probe();
  CHECK(first.profile().identity() == second.profile().identity());
  CHECK(first.state(capability_kind::process_supervision) ==
        capability_state::available);
  CHECK(first.state(capability_kind::closed_environment) ==
        capability_state::available);
  CHECK(first.state(capability_kind::current_root_view) ==
        capability_state::available);
  CHECK(first.state(capability_kind::current_credentials) ==
        capability_state::available);
  CHECK(first.state(capability_kind::writable_resources) ==
        capability_state::available);
  CHECK(first.state(capability_kind::complete_stream_capture) ==
        capability_state::available);
  CHECK(to_string(capability_kind::descriptor_execution) ==
        "descriptor-execution");
  CHECK(to_string(capability_kind::pidfd_cancellation) ==
        "pidfd-cancellation");
  CHECK(to_string(capability_kind::address_space_limit) ==
        "address-space-limit");
  CHECK(to_string(capability_kind::file_size_limit) ==
        "file-size-limit");
  CHECK(to_string(capability_kind::open_files_limit) ==
        "open-files-limit");
  CHECK(first.state(capability_kind::loopback_configuration) ==
        capability_state::unavailable);
  CHECK(!first.observations().empty());
  CHECK(std::is_sorted(first.profile().guarantees().begin(),
                       first.profile().guarantees().end()));
  CHECK(std::find(first.profile().guarantees().begin(),
                  first.profile().guarantees().end(),
                  pkgexec::execution_guarantee::network_denied) ==
        first.profile().guarantees().end());
  CHECK(std::find(first.profile().guarantees().begin(),
                  first.profile().guarantees().end(),
                  pkgexec::execution_guarantee::read_only_resources) ==
        first.profile().guarantees().end());

  const bool host_cancellation_advertised =
      std::find(first.profile().guarantees().begin(),
                first.profile().guarantees().end(),
                pkgexec::execution_guarantee::cancellation) !=
      first.profile().guarantees().end();
  CHECK(host_cancellation_advertised ==
        (first.state(capability_kind::pidfd_cancellation) ==
         capability_state::available));
  const auto host_limit = [&](capability_kind observation,
                              pkgexec::execution_guarantee guarantee) {
    const bool advertised =
        std::find(first.profile().guarantees().begin(),
                  first.profile().guarantees().end(), guarantee) !=
        first.profile().guarantees().end();
    CHECK(advertised ==
          (first.state(observation) == capability_state::available));
  };
  host_limit(capability_kind::address_space_limit,
             pkgexec::execution_guarantee::address_space_limit);
  host_limit(capability_kind::file_size_limit,
             pkgexec::execution_guarantee::file_size_limit);
  host_limit(capability_kind::open_files_limit,
             pkgexec::execution_guarantee::open_files_limit);
  const bool aggregate_limits =
      std::find(first.profile().guarantees().begin(),
                first.profile().guarantees().end(),
                pkgexec::execution_guarantee::resource_limits) !=
      first.profile().guarantees().end();
  CHECK(aggregate_limits ==
        (first.state(capability_kind::address_space_limit) ==
             capability_state::available ||
         first.state(capability_kind::file_size_limit) ==
             capability_state::available ||
         first.state(capability_kind::open_files_limit) ==
             capability_state::available));

  const auto isolated_first = capability_report::probe_isolated();
  const auto isolated_second = capability_report::probe_isolated();
  CHECK(isolated_first.profile().identity() ==
        isolated_second.profile().identity());
  CHECK(isolated_first.state(capability_kind::openat2) ==
        capability_state::available);
  CHECK(to_string(capability_kind::open_tree) == "open-tree");
  CHECK(to_string(capability_kind::move_mount) == "move-mount");
  const bool network_prerequisites =
      isolated_first.state(capability_kind::process_group_containment) ==
          capability_state::available &&
      isolated_first.state(capability_kind::capability_drop) ==
          capability_state::available;
  const bool denied_advertised =
      std::find(isolated_first.profile().guarantees().begin(),
                isolated_first.profile().guarantees().end(),
                pkgexec::execution_guarantee::network_denied) !=
      isolated_first.profile().guarantees().end();
  const bool loopback_advertised =
      std::find(isolated_first.profile().guarantees().begin(),
                isolated_first.profile().guarantees().end(),
                pkgexec::execution_guarantee::loopback_isolated) !=
      isolated_first.profile().guarantees().end();
  CHECK(denied_advertised ==
        (network_prerequisites &&
         isolated_first.state(capability_kind::network_namespace) ==
             capability_state::available));
  CHECK(loopback_advertised ==
        (network_prerequisites &&
         isolated_first.state(capability_kind::loopback_configuration) ==
             capability_state::available));
  if (isolated_first.state(capability_kind::mount_namespace) ==
      capability_state::available) {
    CHECK(std::find(isolated_first.profile().guarantees().begin(),
                    isolated_first.profile().guarantees().end(),
                    pkgexec::execution_guarantee::read_only_resources) !=
          isolated_first.profile().guarantees().end());
  }

  const bool isolated_cancellation_advertised =
      std::find(isolated_first.profile().guarantees().begin(),
                isolated_first.profile().guarantees().end(),
                pkgexec::execution_guarantee::cancellation) !=
      isolated_first.profile().guarantees().end();
  CHECK(isolated_cancellation_advertised ==
        (isolated_first.state(capability_kind::pidfd_cancellation) ==
         capability_state::available));
  for (const auto& pair : {
           std::pair{capability_kind::address_space_limit,
                     pkgexec::execution_guarantee::address_space_limit},
           std::pair{capability_kind::file_size_limit,
                     pkgexec::execution_guarantee::file_size_limit},
           std::pair{capability_kind::open_files_limit,
                     pkgexec::execution_guarantee::open_files_limit},
       }) {
    const bool advertised =
        std::find(isolated_first.profile().guarantees().begin(),
                  isolated_first.profile().guarantees().end(), pair.second) !=
        isolated_first.profile().guarantees().end();
    CHECK(advertised ==
          (isolated_first.state(pair.first) == capability_state::available));
  }
  CHECK(to_string(capability_kind::loopback_configuration) ==
        "loopback-configuration");
  CHECK(to_string(capability_kind::landlock) == "landlock");
  CHECK(to_string(capability_state::policy_restricted) == "policy-restricted");
  return 0;
}

} // namespace

int main() { return run_test(test); }
