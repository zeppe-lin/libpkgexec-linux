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
  CHECK(to_string(capability_kind::landlock) == "landlock");
  CHECK(to_string(capability_state::policy_restricted) == "policy-restricted");
  return 0;
}

} // namespace

int main() { return run_test(test); }
