// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#include "../fixtures/isolated.h"
#include "../support/isolated_skip.h"
#include "../support/result.h"
#include "../support/test.h"

#include <libpkgexec-linux/libpkgexec-linux.h>

#include <chrono>
#include <filesystem>
#include <optional>
#include <thread>

namespace {

std::filesystem::path cancellation_probe;
std::filesystem::path resource_probe;

int test()
{
  using namespace pkgexec;
  using namespace pkgexec_linux;
  using namespace isolated_fixture;

  const auto shell = interpreter_binding::inspect("/bin/sh");
  tree material(shell,
                {{cancellation_probe, "/bin/cancellation-probe"},
                 {resource_probe, "/bin/resource-limit-probe"}});
  auto backend = isolated_backend::make({shell});
  const auto marker = material.workspace() / "compose.ready";
  auto request_value = request(
      shell, network_policy::denied,
      "/bin/resource-limit-probe show; "
      "exec /bin/cancellation-probe graceful /workspace/compose.ready",
      cancellation_policy::graceful_then_forced(500),
      resource_limits::make(std::nullopt, 256U * 1024U * 1024U, 4096U, 64U));
  auto cancellation = cancellation_source::for_request(request_value);
  const auto token = cancellation.token();
  if (!backend.capabilities().supports(request_value)) {
    auto unsupported = backend.execute(
        request_value, resources(request_value, material), token);
    CHECK(unsupported.failure() == execution_failure_kind::backend_unsupported);
    return test_support::isolated_skip("composition", backend,
                                       request_value, unsupported);
  }

  std::optional<execution_result> result;
  std::thread worker([&] {
    result.emplace(backend.execute(
        request_value, resources(request_value, material), token));
  });
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  while (!std::filesystem::exists(marker) &&
         std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  const bool ready = std::filesystem::exists(marker);
  const bool requested = cancellation.request_cancellation();
  worker.join();
  CHECK(ready);
  CHECK(requested);
  CHECK(result.has_value());
  CHECK(result->failure() == execution_failure_kind::cancelled);
  CHECK(result->start_state() == execution_start_state::started);
  CHECK(result->cleanup() == cleanup_outcome::verified);
  CHECK(result->established_guarantees() == request_value.required_guarantees());
  CHECK(test_support::output(*result).find(
            "as=268435456/268435456\n"
            "fsize=4096/4096\n"
            "nofile=64/64\n") == 0U);
  CHECK(test_support::has_guarantee(*result, execution_guarantee::network_denied));
  CHECK(test_support::has_guarantee(*result, execution_guarantee::cancellation));
  CHECK(test_support::has_guarantee(*result, execution_guarantee::resource_limits));
  return 0;
}

} // namespace

int main(int argc, char** argv)
{
  if (argc != 3) {
    return 2;
  }
  cancellation_probe = argv[1];
  resource_probe = argv[2];
  return run_test(test);
}
