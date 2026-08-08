// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#include "../fixtures/isolated.h"
#include "../support/isolated_skip.h"
#include "../support/network_listener.h"
#include "../support/result.h"
#include "../support/test.h"

#include <libpkgexec-linux/libpkgexec-linux.h>

#include <filesystem>
#include <string>

namespace {

std::filesystem::path probe_path;

int test()
{
  using namespace pkgexec;
  using namespace pkgexec_linux;
  using namespace isolated_fixture;

  const auto shell = interpreter_binding::inspect("/bin/sh");
  tree material(shell, {{probe_path, "/bin/network-probe"}});
  auto backend = isolated_backend::make({shell});

  auto baseline = request(shell);
  if (!backend.capabilities().supports(baseline)) {
    auto unsupported = backend.execute(baseline, resources(baseline, material));
    return test_support::isolated_skip("network-baseline", backend, baseline,
                                       unsupported);
  }

  test_support::parent_listener allowed_listener;
  auto allowed_request = request(
      shell, network_policy::allowed,
      "/bin/network-probe allowed " + std::to_string(allowed_listener.port()));
  auto allowed = backend.execute(allowed_request, resources(allowed_request, material));
  test_support::require_success(allowed, "allowed network execution");
  CHECK(allowed_listener.received(1000));

  test_support::parent_listener denied_listener;
  auto denied_request = request(
      shell, network_policy::denied,
      "/bin/network-probe denied " + std::to_string(denied_listener.port()));
  if (!backend.capabilities().supports(denied_request)) {
    auto unsupported = backend.execute(denied_request,
                                       resources(denied_request, material));
    CHECK(unsupported.failure() == execution_failure_kind::backend_unsupported);
    return test_support::isolated_skip("network-denied", backend,
                                       denied_request, unsupported);
  }
  auto denied = backend.execute(denied_request, resources(denied_request, material));
  test_support::require_success(denied, "denied network execution");
  CHECK(test_support::has_guarantee(denied, execution_guarantee::network_denied));
  CHECK(!denied_listener.received(100));

  test_support::parent_listener loopback_listener;
  auto loopback_request = request(
      shell, network_policy::loopback_only,
      "/bin/network-probe loopback " + std::to_string(loopback_listener.port()));
  if (!backend.capabilities().supports(loopback_request)) {
    auto unsupported = backend.execute(loopback_request,
                                       resources(loopback_request, material));
    CHECK(unsupported.failure() == execution_failure_kind::backend_unsupported);
    return test_support::isolated_skip("network-loopback", backend,
                                       loopback_request, unsupported);
  }
  auto loopback = backend.execute(loopback_request,
                                  resources(loopback_request, material));
  test_support::require_success(loopback, "loopback-only network execution");
  CHECK(test_support::has_guarantee(loopback,
                                    execution_guarantee::loopback_isolated));
  CHECK(!loopback_listener.received(100));
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
