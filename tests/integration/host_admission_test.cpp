// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#include "../fixtures/host.h"
#include "../support/test.h"
#include "../support/temporary_directory.h"

#include <libpkgexec-linux/libpkgexec-linux.h>

#include <filesystem>
#include <fstream>

namespace {

int test()
{
  using namespace pkgexec;
  using namespace pkgexec_linux;

  test_support::temporary_directory temporary;
  const auto shell = interpreter_binding::inspect("/bin/sh");
  auto backend = host_supervisor_backend::make({shell});

  auto network_request = fixture::request(
      shell, temporary.path(), "true", network_policy::denied);
  auto network = backend.execute(
      network_request, fixture::resources(network_request, temporary.path()));
  CHECK(network.start_state() == execution_start_state::not_started);
  CHECK(network.failure() == execution_failure_kind::backend_unsupported);
  CHECK(network.diagnostic().find("network-denied") != std::string::npos);

  auto readonly_request = fixture::request(
      shell, temporary.path(), "true", network_policy::allowed,
      resource_access::read_only);
  auto readonly = backend.execute(
      readonly_request, fixture::resources(readonly_request, temporary.path()));
  CHECK(readonly.start_state() == execution_start_state::not_started);
  CHECK(readonly.failure() == execution_failure_kind::backend_unsupported);
  CHECK(readonly.diagnostic().find("read-only-resources") != std::string::npos);

  auto credential_request = fixture::request(
      shell, temporary.path(), "true", network_policy::allowed,
      resource_access::writable, true, stream_policy::capture_complete,
      stream_policy::capture_complete,
      static_cast<std::uint64_t>(::getuid()) + 1U);
  auto credential = backend.execute(
      credential_request,
      fixture::resources(credential_request, temporary.path()));
  CHECK(credential.start_state() == execution_start_state::not_started);
  CHECK(credential.failure() == execution_failure_kind::request_rejected);

  auto privileges_request = fixture::request(
      shell, temporary.path(), "true", network_policy::allowed,
      resource_access::writable, false);
  auto privileges = backend.execute(
      privileges_request,
      fixture::resources(privileges_request, temporary.path()));
  CHECK(privileges.start_state() == execution_start_state::not_started);
  CHECK(privileges.failure() == execution_failure_kind::request_rejected);

  test_support::temporary_directory other;
  auto mismatch_request = fixture::request(shell, temporary.path(), "true");
  auto mismatch_resources = execution_resources::admit(
      mismatch_request, fixture::root(), "/",
      {resource_materialization(fixture::resource("workspace"), other.path())});
  auto mismatch = backend.execute(mismatch_request, mismatch_resources);
  CHECK(mismatch.start_state() == execution_start_state::not_started);
  CHECK(mismatch.failure() == execution_failure_kind::request_rejected);

  auto root_request = fixture::request(shell, temporary.path(), "true");
  auto alternate_root_resources = execution_resources::admit(
      root_request, fixture::root(), temporary.path(),
      {resource_materialization(fixture::resource("workspace"),
                                temporary.path())});
  auto alternate_root = backend.execute(root_request, alternate_root_resources);
  CHECK(alternate_root.start_state() == execution_start_state::not_started);
  CHECK(alternate_root.failure() == execution_failure_kind::request_rejected);
  CHECK(alternate_root.diagnostic().find("current / root view") !=
        std::string::npos);

  const auto symlink = temporary.path() / "workspace-link";
  std::filesystem::create_directory_symlink(temporary.path(), symlink);
  auto symlink_request = fixture::request(shell, symlink, "true");
  auto symlink_resources = execution_resources::admit(
      symlink_request, fixture::root(), "/",
      {resource_materialization(fixture::resource("workspace"), symlink)});
  auto symlink_rejected = backend.execute(symlink_request, symlink_resources);
  CHECK(symlink_rejected.start_state() == execution_start_state::not_started);
  CHECK(symlink_rejected.failure() == execution_failure_kind::request_rejected);
  CHECK(symlink_rejected.diagnostic().find("symlink component") !=
        std::string::npos);

  const auto regular_path = temporary.path() / "workspace-file";
  {
    std::ofstream regular(regular_path);
    regular << "not a directory\n";
  }
  auto regular_request = fixture::request(shell, regular_path, "true");
  auto regular_resources = execution_resources::admit(
      regular_request, fixture::root(), "/",
      {resource_materialization(fixture::resource("workspace"), regular_path)});
  auto regular_rejected = backend.execute(regular_request, regular_resources);
  CHECK(regular_rejected.start_state() == execution_start_state::not_started);
  CHECK(regular_rejected.failure() == execution_failure_kind::request_rejected);
  CHECK(regular_rejected.diagnostic().find("existing directory") !=
        std::string::npos);

  const auto copied_shell_path = temporary.path() / "shell-copy";
  std::filesystem::copy_file(shell.executable(), copied_shell_path);
  std::filesystem::permissions(
      copied_shell_path,
      std::filesystem::perms::owner_read |
          std::filesystem::perms::owner_write |
          std::filesystem::perms::owner_exec);
  const auto copied_shell = interpreter_binding::inspect(copied_shell_path);
  auto changed_backend = host_supervisor_backend::make({copied_shell});
  auto changed_request = fixture::request(copied_shell, temporary.path(), "true");
  {
    std::ofstream changed(copied_shell_path, std::ios::binary | std::ios::trunc);
    changed << "not the admitted interpreter";
  }
  auto changed = changed_backend.execute(
      changed_request, fixture::resources(changed_request, temporary.path()));
  CHECK(changed.start_state() == execution_start_state::not_started);
  CHECK(changed.failure() == execution_failure_kind::interpreter_unavailable);

  const auto alternate_interpreter_path = temporary.path() / "alternate-interpreter";
  {
    std::ofstream alternate(alternate_interpreter_path,
                            std::ios::binary | std::ios::trunc);
    alternate << "#!" << shell.executable().string() << "\nexit 0\n";
  }
  std::filesystem::permissions(
      alternate_interpreter_path,
      std::filesystem::perms::owner_read |
          std::filesystem::perms::owner_write |
          std::filesystem::perms::owner_exec);
  const auto alternate_interpreter =
      interpreter_binding::inspect(alternate_interpreter_path);
  auto missing_backend = host_supervisor_backend::make({alternate_interpreter});
  auto ordinary_request = fixture::request(shell, temporary.path(), "true");
  auto missing = missing_backend.execute(
      ordinary_request, fixture::resources(ordinary_request, temporary.path()));
  CHECK(missing.start_state() == execution_start_state::not_started);
  CHECK(missing.failure() == execution_failure_kind::interpreter_unavailable);

  return 0;
}

} // namespace

int main() { return run_test(test); }
