// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#include "../fixtures/host.h"
#include "../support/test.h"
#include "../support/temporary_directory.h"

#include <chrono>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>

#include <unistd.h>

namespace {


bool wait_for_path(const std::filesystem::path& path)
{
  const auto deadline = std::chrono::steady_clock::now() +
                        std::chrono::seconds(3);
  while (std::chrono::steady_clock::now() < deadline) {
    if (std::filesystem::exists(path)) {
      return true;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  return false;
}

pkgexec::execution_result cancel_running(
    pkgexec_linux::host_supervisor_backend& backend,
    const pkgexec_linux::interpreter_binding& shell,
    const std::filesystem::path& workspace,
    const std::filesystem::path& probe,
    std::string mode,
    std::uint64_t grace)
{
  const auto marker = workspace / (mode + ".ready");
  auto request = fixture::request(
      shell, workspace,
      "exec '" + probe.string() + "' " + mode + " '" + marker.string() + "'",
      pkgexec::network_policy::allowed,
      pkgexec::resource_access::writable, true,
      pkgexec::stream_policy::capture_complete,
      pkgexec::stream_policy::capture_complete,
      static_cast<std::uint64_t>(::getuid()),
      pkgexec::cancellation_policy::graceful_then_forced(grace));
  auto resources = fixture::resources(request, workspace);
  auto source = pkgexec::cancellation_source::for_request(request);
  const auto token = source.token();
  std::optional<pkgexec::execution_result> result;
  std::thread worker([&] {
    result.emplace(backend.execute(request, resources, token));
  });
  const bool ready = wait_for_path(marker);
  const bool requested = source.request_cancellation();
  worker.join();
  CHECK(ready);
  CHECK(requested);
  CHECK(result.has_value());
  return std::move(*result);
}

int test(const std::filesystem::path& probe)
{
  using namespace pkgexec;
  using namespace pkgexec_linux;

  test_support::temporary_directory temporary;
  const auto shell = interpreter_binding::inspect("/bin/sh");
  auto backend = host_supervisor_backend::make({shell});
  if (backend.report().state(capability_kind::pidfd_cancellation) !=
      capability_state::available) {
    return 77;
  }

  auto natural_request = fixture::request(
      shell, temporary.path(), "exit 0", network_policy::allowed,
      resource_access::writable, true,
      stream_policy::capture_complete, stream_policy::capture_complete,
      static_cast<std::uint64_t>(::getuid()),
      cancellation_policy::graceful_then_forced(200));
  auto natural_source = cancellation_source::for_request(natural_request);
  auto natural = backend.execute(
      natural_request, fixture::resources(natural_request, temporary.path()),
      natural_source.token());
  CHECK(natural.status() == execution_status::succeeded);
  CHECK(natural.start_state() == execution_start_state::started);
  CHECK(!natural.failure());

  auto prestart_request = fixture::request(
      shell, temporary.path(), "exit 99", network_policy::allowed,
      resource_access::writable, true,
      stream_policy::capture_complete, stream_policy::capture_complete,
      static_cast<std::uint64_t>(::getuid()),
      cancellation_policy::graceful_then_forced(200));
  auto prestart_source = cancellation_source::for_request(prestart_request);
  CHECK(prestart_source.request_cancellation());
  auto prestart = backend.execute(
      prestart_request, fixture::resources(prestart_request, temporary.path()),
      prestart_source.token());
  CHECK(prestart.status() == execution_status::failed);
  CHECK(prestart.start_state() == execution_start_state::not_started);
  CHECK(prestart.failure() == execution_failure_kind::cancelled);
  CHECK(!prestart.termination());

  auto graceful = cancel_running(
      backend, shell, temporary.path(), probe, "graceful", 500);
  CHECK(graceful.status() == execution_status::failed);
  CHECK(graceful.start_state() == execution_start_state::started);
  CHECK(graceful.failure() == execution_failure_kind::cancelled);
  CHECK(graceful.termination()->kind() == process_termination_kind::cancelled);
  CHECK(graceful.cleanup() == cleanup_outcome::verified);
  CHECK(graceful.diagnostic().find("graceful") != std::string::npos);

  const auto start = std::chrono::steady_clock::now();
  auto forced = cancel_running(
      backend, shell, temporary.path(), probe, "forced", 100);
  const auto elapsed = std::chrono::steady_clock::now() - start;
  CHECK(forced.status() == execution_status::failed);
  CHECK(forced.start_state() == execution_start_state::started);
  CHECK(forced.failure() == execution_failure_kind::cancelled);
  CHECK(forced.termination()->kind() == process_termination_kind::cancelled);
  CHECK(forced.cleanup() == cleanup_outcome::verified);
  CHECK(forced.diagnostic().find("forced") != std::string::npos);
  CHECK(elapsed < std::chrono::seconds(5));
  return 0;
}

} // namespace

int main(int argc, char** argv)
{
  if (argc != 2) {
    return 2;
  }
  try {
    return test(argv[1]);
  } catch (const std::exception& value) {
    std::cerr << value.what() << '\n';
    return EXIT_FAILURE;
  }
}
