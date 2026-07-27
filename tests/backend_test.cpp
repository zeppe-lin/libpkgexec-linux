// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#include "fixture.h"
#include "test.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

namespace {

class temporary_directory final {
public:
  temporary_directory()
  {
    std::string pattern = "/tmp/pkgexec-linux-test.XXXXXX";
    char* value = ::mkdtemp(pattern.data());
    if (!value) {
      throw std::runtime_error("mkdtemp failed");
    }
    path_ = value;
  }
  ~temporary_directory()
  {
    std::error_code ignored;
    std::filesystem::remove_all(path_, ignored);
  }
  const std::filesystem::path& path() const noexcept { return path_; }
private:
  std::filesystem::path path_;
};

std::string material(const std::optional<pkgexec::stream_capture>& capture)
{
  CHECK(capture.has_value());
  CHECK(capture->material().has_value());
  return *capture->material();
}

int test()
{
  using namespace pkgexec;
  using namespace pkgexec_linux;

  temporary_directory temporary;
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
  CHECK(success.status() == execution_status::succeeded);
  CHECK(success.start_state() == execution_start_state::started);
  CHECK(success.cleanup() == cleanup_outcome::verified);
  CHECK(material(success.standard_output()).find(temporary.path().string()) == 0U);
  CHECK(material(success.standard_output()).find("|3|42|yes|unset") !=
        std::string::npos);
  CHECK(material(success.standard_error()) == "stderr\n");
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
  CHECK(descriptor.status() == execution_status::succeeded);

  auto volume_request = fixture::request(
      shell, temporary.path(),
      "i=0; while [ $i -lt 5000 ]; do "
      "printf '0123456789abcdef0123456789abcdef\n'; "
      "printf 'fedcba9876543210fedcba9876543210\n' >&2; "
      "i=$((i + 1)); done");
  auto volume = backend.execute(
      volume_request, fixture::resources(volume_request, temporary.path()));
  CHECK(volume.status() == execution_status::succeeded);
  CHECK(volume.standard_output()->byte_count() == 165000U);
  CHECK(volume.standard_error()->byte_count() == 165000U);

  auto nonzero_request = fixture::request(shell, temporary.path(), "exit 19");
  auto nonzero = backend.execute(
      nonzero_request, fixture::resources(nonzero_request, temporary.path()));
  CHECK(nonzero.status() == execution_status::failed);
  CHECK(nonzero.failure() == execution_failure_kind::program_exited_nonzero);
  CHECK(nonzero.termination()->kind() == process_termination_kind::exited);
  CHECK(nonzero.termination()->value() == 19U);

  auto signal_request = fixture::request(shell, temporary.path(), "kill -TERM $$");
  auto signaled = backend.execute(
      signal_request, fixture::resources(signal_request, temporary.path()));
  CHECK(signaled.failure() == execution_failure_kind::program_terminated_by_signal);
  CHECK(signaled.termination()->kind() == process_termination_kind::signaled);
  CHECK(signaled.termination()->value() == static_cast<std::uint32_t>(SIGTERM));

  const auto start = std::chrono::steady_clock::now();
  auto descendant_request = fixture::request(
      shell, temporary.path(), "sleep 30 & printf done");
  auto descendant = backend.execute(
      descendant_request,
      fixture::resources(descendant_request, temporary.path()));
  const auto elapsed = std::chrono::steady_clock::now() - start;
  CHECK(descendant.status() == execution_status::succeeded);
  CHECK(material(descendant.standard_output()) == "done");
  CHECK(elapsed < std::chrono::seconds(5));

  auto escape_request = fixture::request(
      shell, temporary.path(), "setsid true >/dev/null 2>&1; test $? -ne 0");
  auto escape = backend.execute(
      escape_request, fixture::resources(escape_request, temporary.path()));
  CHECK(escape.status() == execution_status::succeeded);

  auto network_request = fixture::request(
      shell, temporary.path(), "true", network_policy::denied);
  auto network = backend.execute(
      network_request, fixture::resources(network_request, temporary.path()));
  CHECK(network.start_state() == execution_start_state::not_started);
  CHECK(network.failure() == execution_failure_kind::backend_unsupported);

  auto readonly_request = fixture::request(
      shell, temporary.path(), "true", network_policy::allowed,
      resource_access::read_only);
  auto readonly = backend.execute(
      readonly_request, fixture::resources(readonly_request, temporary.path()));
  CHECK(readonly.failure() == execution_failure_kind::backend_unsupported);

  auto credential_request = fixture::request(
      shell, temporary.path(), "true", network_policy::allowed,
      resource_access::writable, true,
      stream_policy::capture_complete, stream_policy::capture_complete,
      static_cast<std::uint64_t>(::getuid()) + 1U);
  auto credential = backend.execute(
      credential_request,
      fixture::resources(credential_request, temporary.path()));
  CHECK(credential.failure() == execution_failure_kind::request_rejected);

  auto privileges_request = fixture::request(
      shell, temporary.path(), "true", network_policy::allowed,
      resource_access::writable, false);
  auto privileges = backend.execute(
      privileges_request,
      fixture::resources(privileges_request, temporary.path()));
  CHECK(privileges.failure() == execution_failure_kind::request_rejected);

  temporary_directory other;
  auto mismatch_request = fixture::request(shell, temporary.path(), "true");
  auto mismatch_resources = pkgexec::execution_resources::admit(
      mismatch_request, fixture::root(), "/",
      {pkgexec::resource_materialization(fixture::resource("workspace"),
                                         other.path())});
  auto mismatch = backend.execute(mismatch_request, mismatch_resources);
  CHECK(mismatch.failure() == execution_failure_kind::request_rejected);

  auto discard_request = fixture::request(
      shell, temporary.path(), "printf lost; printf error >&2",
      network_policy::allowed, resource_access::writable, true,
      stream_policy::discard, stream_policy::discard);
  auto discarded = backend.execute(
      discard_request, fixture::resources(discard_request, temporary.path()));
  CHECK(discarded.status() == execution_status::succeeded);
  CHECK(!discarded.standard_output());
  CHECK(!discarded.standard_error());

  const auto copied_shell_path = temporary.path() / "shell-copy";
  std::filesystem::copy_file(shell.executable(), copied_shell_path);
  std::filesystem::permissions(
      copied_shell_path,
      std::filesystem::perms::owner_read |
          std::filesystem::perms::owner_write |
          std::filesystem::perms::owner_exec);
  const auto copied_shell = interpreter_binding::inspect(copied_shell_path);
  auto changed_backend = host_supervisor_backend::make({copied_shell});
  auto changed_request = fixture::request(
      copied_shell, temporary.path(), "true");
  {
    std::ofstream changed(copied_shell_path, std::ios::binary | std::ios::trunc);
    changed << "not the admitted interpreter";
  }
  auto changed = changed_backend.execute(
      changed_request, fixture::resources(changed_request, temporary.path()));
  CHECK(changed.failure() == execution_failure_kind::interpreter_unavailable);

  const auto false_program = interpreter_binding::inspect("/usr/bin/false");
  auto missing_backend = host_supervisor_backend::make({false_program});
  auto missing = missing_backend.execute(
      success_request, fixture::resources(success_request, temporary.path()));
  CHECK(missing.failure() == execution_failure_kind::interpreter_unavailable);
  return 0;
}

} // namespace

int main() { return run_test(test); }
