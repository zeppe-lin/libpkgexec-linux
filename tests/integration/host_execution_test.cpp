// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#include "../fixtures/host.h"
#include "../support/result.h"
#include "../support/temporary_directory.h"

#include <libpkgexec-linux/libpkgexec-linux.h>

#include <chrono>
#include <cstdlib>
#include <string>

#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

namespace {

int test()
{
  using namespace pkgexec;
  using namespace pkgexec_linux;

  test_support::temporary_directory temporary;
  const auto shell = interpreter_binding::inspect("/bin/sh");
  auto backend = host_supervisor_backend::make({shell});
  if (backend.report().state(capability_kind::process_group_containment) !=
          capability_state::available ||
      backend.report().state(capability_kind::descriptor_execution) !=
          capability_state::available) {
    return 77;
  }

  ::setenv("INHERITED", "must-not-leak", 1);
  auto success_request = fixture::request(
      shell, temporary.path(),
      "printf '%s\\n' \"$PWD\"; "
      "printf '%s\\n' \"$PATH|$HOME|$LANG|$LC_ALL|$TZ|$TMPDIR|"
      "$PKGEXEC_JOBS|$SOURCE_DATE_EPOCH|$VISIBLE|${INHERITED-unset}\"; "
      "printf '%s\\n' stderr >&2");
  auto success = backend.execute(
      success_request, fixture::resources(success_request, temporary.path()));
  test_support::require_success(success, "host success");
  CHECK(test_support::material(success.standard_output()).find(
            temporary.path().string()) == 0U);
  CHECK(test_support::material(success.standard_output()).find(
            "|3|42|yes|unset") != std::string::npos);
  CHECK(test_support::material(success.standard_error()) == "stderr\n");
  CHECK(success.observed_interpreter() == shell.identity());

  const int inherited = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
  CHECK(inherited >= 0);
  CHECK(::dup2(inherited, 200) == 200);
  ::close(inherited);
  auto descriptor_request = fixture::request(
      shell, temporary.path(), "test ! -e /proc/self/fd/200");
  auto descriptor = backend.execute(
      descriptor_request,
      fixture::resources(descriptor_request, temporary.path()));
  ::close(200);
  test_support::require_success(descriptor, "descriptor closure");

  auto volume_request = fixture::request(
      shell, temporary.path(),
      "i=0; while [ $i -lt 5000 ]; do "
      "printf '0123456789abcdef0123456789abcdef\\n'; "
      "printf 'fedcba9876543210fedcba9876543210\\n' >&2; "
      "i=$((i + 1)); done");
  auto volume = backend.execute(
      volume_request, fixture::resources(volume_request, temporary.path()));
  test_support::require_success(volume, "complete stream capture");
  CHECK(volume.standard_output()->byte_count() == 165000U);
  CHECK(volume.standard_error()->byte_count() == 165000U);

  auto nonzero_request = fixture::request(shell, temporary.path(), "exit 19");
  auto nonzero = backend.execute(
      nonzero_request, fixture::resources(nonzero_request, temporary.path()));
  CHECK(nonzero.status() == execution_status::failed);
  CHECK(nonzero.start_state() == execution_start_state::started);
  CHECK(nonzero.failure() == execution_failure_kind::program_exited_nonzero);
  CHECK(nonzero.termination()->kind() == process_termination_kind::exited);
  CHECK(nonzero.termination()->value() == 19U);
  CHECK(nonzero.cleanup() == cleanup_outcome::verified);
  CHECK(nonzero.established_guarantees() == nonzero_request.required_guarantees());

  auto signal_request = fixture::request(
      shell, temporary.path(), "kill -TERM $$");
  auto signaled = backend.execute(
      signal_request, fixture::resources(signal_request, temporary.path()));
  CHECK(signaled.failure() == execution_failure_kind::program_terminated_by_signal);
  CHECK(signaled.termination()->kind() == process_termination_kind::signaled);
  CHECK(signaled.termination()->value() == static_cast<std::uint32_t>(SIGTERM));
  CHECK(signaled.cleanup() == cleanup_outcome::verified);
  CHECK(signaled.established_guarantees() == signal_request.required_guarantees());

  const auto start = std::chrono::steady_clock::now();
  auto descendant_request = fixture::request(
      shell, temporary.path(), "sleep 30 & printf done");
  auto descendant = backend.execute(
      descendant_request,
      fixture::resources(descendant_request, temporary.path()));
  const auto elapsed = std::chrono::steady_clock::now() - start;
  test_support::require_success(descendant, "descendant cleanup");
  CHECK(test_support::material(descendant.standard_output()) == "done");
  CHECK(elapsed < std::chrono::seconds(5));

  auto escape_request = fixture::request(
      shell, temporary.path(), "setsid true >/dev/null 2>&1; test $? -ne 0");
  auto escape = backend.execute(
      escape_request, fixture::resources(escape_request, temporary.path()));
  test_support::require_success(escape, "process-group containment");

  auto discard_request = fixture::request(
      shell, temporary.path(), "printf lost; printf error >&2",
      network_policy::allowed, resource_access::writable, true,
      stream_policy::discard, stream_policy::discard);
  auto discarded = backend.execute(
      discard_request, fixture::resources(discard_request, temporary.path()));
  test_support::require_success(discarded, "discarded streams");
  CHECK(!discarded.standard_output());
  CHECK(!discarded.standard_error());
  return 0;
}

} // namespace

int main() { return run_test(test); }
