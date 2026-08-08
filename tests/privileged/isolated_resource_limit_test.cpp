// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#include "../fixtures/isolated.h"
#include "../support/isolated_skip.h"
#include "../support/result.h"
#include "../support/test.h"

#include <libpkgexec-linux/libpkgexec-linux.h>

#include <filesystem>
#include <optional>

namespace {

std::filesystem::path probe_path;

int test()
{
  using namespace pkgexec;
  using namespace pkgexec_linux;
  using namespace isolated_fixture;

  const auto shell = interpreter_binding::inspect("/bin/sh");
  tree material(shell, {{probe_path, "/bin/resource-limit-probe"}});
  auto backend = isolated_backend::make({shell});

  auto runtime_request = request(
      shell, network_policy::allowed, "/bin/resource-limit-probe show");
  if (!backend.capabilities().supports(runtime_request)) {
    auto unsupported = backend.execute(
        runtime_request, resources(runtime_request, material));
    CHECK(unsupported.failure() == execution_failure_kind::backend_unsupported);
    return test_support::isolated_skip("resource-limit-runtime", backend,
                                       runtime_request, unsupported);
  }
  auto runtime = backend.execute(runtime_request,
                                 resources(runtime_request, material));
  test_support::require_success(runtime, "isolated runtime fixture execution");

  auto request_value = request(
      shell, network_policy::allowed, "/bin/resource-limit-probe show",
      cancellation_policy::disabled(),
      resource_limits::make(std::nullopt, 256U * 1024U * 1024U, 4096U, 64U));
  if (!backend.capabilities().supports(request_value)) {
    auto unsupported = backend.execute(request_value,
                                       resources(request_value, material));
    CHECK(unsupported.failure() == execution_failure_kind::backend_unsupported);
    return test_support::isolated_skip("resource-limits", backend,
                                       request_value, unsupported);
  }

  auto result = backend.execute(request_value, resources(request_value, material));
  test_support::require_success(result, "isolated resource-limit execution");
  CHECK(test_support::output(result) ==
        "as=268435456/268435456\n"
        "fsize=4096/4096\n"
        "nofile=64/64\n");
  CHECK(test_support::has_guarantee(result, execution_guarantee::resource_limits));
  CHECK(test_support::has_guarantee(result, execution_guarantee::address_space_limit));
  CHECK(test_support::has_guarantee(result, execution_guarantee::file_size_limit));
  CHECK(test_support::has_guarantee(result, execution_guarantee::open_files_limit));
  return 0;
}

} // namespace

int main(int argc, char** argv)
{
  if (argc != 2) {
    return 2;
  }
  probe_path = argv[1];
  return run_test(test);
}
