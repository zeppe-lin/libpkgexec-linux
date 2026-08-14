// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#include "../fixtures/isolated.h"
#include "../support/isolated_skip.h"
#include "../support/result.h"
#include "../support/test.h"

#include <libpkgexec-linux/libpkgexec-linux.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

int test()
{
  using namespace pkgexec;
  using namespace pkgexec_linux;
  using namespace isolated_fixture;

  const auto shell = interpreter_binding::inspect("/bin/sh");
  tree material(shell);
  auto backend = isolated_backend::make({shell});
  auto baseline = request(shell);
  auto baseline_resources = resources(baseline, material);
  if (!backend.capabilities().supports(baseline)) {
    auto unsupported = backend.execute(baseline, baseline_resources);
    CHECK(unsupported.failure() == execution_failure_kind::backend_unsupported);
    return test_support::isolated_skip(
        "filesystem", backend, baseline, unsupported);
  }

  auto result = backend.execute(baseline, baseline_resources);
  test_support::require_success(result, "isolated filesystem execution");

  auto privileges_request = request(
      shell, network_policy::allowed, "true", cancellation_policy::disabled(),
      resource_limits::make(), false);
  auto privileges = backend.execute(
      privileges_request, resources(privileges_request, material));
  CHECK(privileges.start_state() == execution_start_state::not_started);
  CHECK(privileges.failure() == execution_failure_kind::request_rejected);

  auto credential_request = request(
      shell, network_policy::allowed, "true", cancellation_policy::disabled(),
      resource_limits::make(), true,
      static_cast<std::uint64_t>(::getuid()) + 1U);
  auto credentials = backend.execute(
      credential_request, resources(credential_request, material));
  CHECK(credentials.start_state() == execution_start_state::not_started);
  CHECK(credentials.failure() == execution_failure_kind::request_rejected);
  CHECK(test_support::output(result) == "/workspace\n");
  CHECK(std::filesystem::exists(material.workspace() / "output"));
  CHECK(!std::filesystem::exists(material.source() / "forbidden"));
  CHECK(!std::filesystem::exists(material.root() / "root-only" / "forbidden"));

  auto device_request = request(
      shell, network_policy::allowed,
      "test -c /dev/null && printf discarded > /dev/null");
  auto device = backend.execute(device_request, resources(device_request, material));
  test_support::require_success(device, "isolated private null-device execution");
  std::ifstream root_null(material.root() / "dev" / "null");
  std::string root_null_line;
  CHECK(static_cast<bool>(std::getline(root_null, root_null_line)));
  CHECK(root_null_line == "root device sentinel");

  const auto input_parent = material.root() / "inputs";
  const auto first_input = material.workspace().parent_path() / "first-input";
  const auto second_input = material.workspace().parent_path() / "second-input";
  std::filesystem::create_directories(input_parent);
  std::filesystem::create_directories(first_input);
  std::filesystem::create_directories(second_input);
  std::ofstream(first_input / "value") << "first\n";
  std::ofstream(second_input / "value") << "second\n";

  const auto source_slot = resource_slot::named(resource_role::source_tree, "main");
  const auto first_slot =
      resource_slot::named(resource_role::build_input_tree, "first");
  const auto second_slot =
      resource_slot::named(resource_role::build_input_tree, "second");
  const auto workspace_slot =
      resource_slot::singleton(resource_role::build_workspace);
  auto input_layout = resource_layout::seal(
      {
          resource_binding(source_slot, resource("source"),
                           resource_access::read_only,
                           logical_path::parse("/source")),
          resource_binding(first_slot, resource("first-input"),
                           resource_access::read_only,
                           logical_path::parse("/inputs/first")),
          resource_binding(second_slot, resource("second-input"),
                           resource_access::read_only,
                           logical_path::parse("/inputs/second")),
          resource_binding(workspace_slot, resource("workspace"),
                           resource_access::writable,
                           logical_path::parse("/workspace")),
      },
      workspace_slot);
  auto input_environment = environment_policy::hermetic(
      {logical_path::parse("/bin"), logical_path::parse("/usr/bin")},
      logical_path::parse("/workspace"), logical_path::parse("/workspace"),
      1U, 0022, std::nullopt, network_policy::allowed,
      stdin_policy::null_device, stream_policy::capture_complete,
      stream_policy::capture_complete);
  auto input_request = execution_request::seal(
      pkgsource::program(
          pkgsource::program_language::posix_shell,
          "IFS= read -r first < /inputs/first/value && "
          "IFS= read -r second < /inputs/second/value && "
          "case \"$first:$second\" in first:second) ;; *) exit 1 ;; esac && "
          "! ( : > /inputs/first/forbidden ) 2>/workspace/input-error && "
          "! ( : > /inputs/third ) 2>/workspace/namespace-error"),
      execution_purpose::build(), shell.identity(), root_identity(),
      std::move(input_layout), std::move(input_environment),
      credential_policy::fixed(
          static_cast<std::uint64_t>(::getuid()),
          static_cast<std::uint64_t>(::getgid()), groups(), true),
      resource_limits::make(), cancellation_policy::disabled());
  auto input_resources = execution_resources::admit(
      input_request, root_identity(), material.root(),
      {
          resource_materialization(resource("source"), material.source()),
          resource_materialization(resource("first-input"), first_input),
          resource_materialization(resource("second-input"), second_input),
          resource_materialization(resource("workspace"), material.workspace()),
      });
  auto input_result = backend.execute(input_request, input_resources);
  test_support::require_success(input_result,
                                "isolated private input-namespace execution");
  CHECK(std::filesystem::is_empty(input_parent));
  CHECK(!std::filesystem::exists(first_input / "forbidden"));

  std::ofstream(input_parent / "stale") << "root input state\n";
  auto occupied_input_result = backend.execute(input_request, input_resources);
  CHECK(occupied_input_result.start_state() == execution_start_state::not_started);
  CHECK(occupied_input_result.failure() ==
        execution_failure_kind::resource_admission_failed);

  const auto symlink = material.workspace().parent_path() / "source-link";
  std::filesystem::create_directory_symlink(material.source(), symlink);
  auto bad_resources = execution_resources::admit(
      baseline, root_identity(), material.root(),
      {
          resource_materialization(resource("source"), symlink),
          resource_materialization(resource("workspace"), material.workspace()),
      });
  auto symlink_rejected = backend.execute(baseline, bad_resources);
  CHECK(symlink_rejected.start_state() == execution_start_state::not_started);
  CHECK(symlink_rejected.failure() == execution_failure_kind::resource_admission_failed);

  auto live_root = execution_resources::admit(
      baseline, root_identity(), "/",
      {
          resource_materialization(resource("source"), material.source()),
          resource_materialization(resource("workspace"), material.workspace()),
      });
  auto live_root_rejected = backend.execute(baseline, live_root);
  CHECK(live_root_rejected.start_state() == execution_start_state::not_started);
  CHECK(live_root_rejected.failure() == execution_failure_kind::resource_admission_failed);

  auto root_overlap = execution_resources::admit(
      baseline, root_identity(), material.root(),
      {
          resource_materialization(resource("source"), material.root() / "source"),
          resource_materialization(resource("workspace"), material.workspace()),
      });
  auto root_overlap_rejected = backend.execute(baseline, root_overlap);
  CHECK(root_overlap_rejected.start_state() == execution_start_state::not_started);
  CHECK(root_overlap_rejected.failure() == execution_failure_kind::resource_admission_failed);

  std::filesystem::create_directories(material.source() / "nested-workspace");
  auto resource_overlap = execution_resources::admit(
      baseline, root_identity(), material.root(),
      {
          resource_materialization(resource("source"), material.source()),
          resource_materialization(resource("workspace"),
                                   material.source() / "nested-workspace"),
      });
  auto resource_overlap_rejected = backend.execute(baseline, resource_overlap);
  CHECK(resource_overlap_rejected.start_state() == execution_start_state::not_started);
  CHECK(resource_overlap_rejected.failure() == execution_failure_kind::resource_admission_failed);
  return 0;
}

} // namespace

int main() { return run_test(test); }
