// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <libpkgexec-linux/libpkgexec-linux.h>

#include <algorithm>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <grp.h>
#include <unistd.h>

namespace fixture {

inline std::string digest(std::string_view value)
{
  return pkgexec::sha256_digest::of_bytes(value).hex();
}
inline pkgexec::resource_identity resource(std::string_view value)
{
  return pkgexec::resource_identity::from_sha256(digest(value));
}
inline pkgexec::root_view_identity root(std::string_view value = "host-root")
{
  return pkgexec::root_view_identity::from_sha256(digest(value));
}
inline std::vector<std::uint64_t> groups()
{
  const int count = ::getgroups(0, nullptr);
  std::vector<gid_t> native(static_cast<std::size_t>(std::max(count, 0)));
  if (count > 0) {
    (void)::getgroups(count, native.data());
  }
  std::vector<std::uint64_t> result;
  for (const auto value : native) {
    result.push_back(static_cast<std::uint64_t>(value));
  }
  std::sort(result.begin(), result.end());
  result.erase(std::unique(result.begin(), result.end()), result.end());
  result.erase(std::remove(result.begin(), result.end(),
                           static_cast<std::uint64_t>(::getgid())),
               result.end());
  return result;
}

inline pkgexec::execution_request request(
    const pkgexec_linux::interpreter_binding& interpreter,
    const std::filesystem::path& workspace,
    std::string program,
    pkgexec::network_policy network = pkgexec::network_policy::allowed,
    pkgexec::resource_access access = pkgexec::resource_access::writable,
    bool no_new_privileges = true,
    pkgexec::stream_policy standard_output = pkgexec::stream_policy::capture_complete,
    pkgexec::stream_policy standard_error = pkgexec::stream_policy::capture_complete,
    std::uint64_t user = static_cast<std::uint64_t>(::getuid()),
    pkgexec::cancellation_policy cancellation =
        pkgexec::cancellation_policy::disabled(),
    pkgexec::resource_limits limits = pkgexec::resource_limits::make())
{
  using namespace pkgexec;
  const auto slot = resource_slot::singleton(resource_role::build_workspace);
  std::vector<resource_binding> bindings;
  bindings.emplace_back(slot, resource("workspace"), resource_access::writable,
                        logical_path::parse(workspace.string()));
  if (access == resource_access::read_only) {
    bindings.emplace_back(
        resource_slot::named(resource_role::source_tree, "main"),
        resource("source"), resource_access::read_only,
        logical_path::parse((workspace / "source").string()));
  }
  auto layout = resource_layout::seal(std::move(bindings), slot);
  auto environment = environment_policy::hermetic(
      {logical_path::parse("/usr/bin"), logical_path::parse("/bin")},
      logical_path::parse(workspace.string()),
      logical_path::parse(workspace.string()), 3, 0027, 42, network,
      stdin_policy::null_device, standard_output, standard_error,
      {environment_variable("VISIBLE", "yes")});
  return execution_request::seal(
      pkgsource::program(pkgsource::program_language::posix_shell,
                         std::move(program)),
      execution_purpose::build(), interpreter.identity(), root(),
      std::move(layout), std::move(environment),
      credential_policy::fixed(user, static_cast<std::uint64_t>(::getgid()),
                               groups(), no_new_privileges),
      std::move(limits), std::move(cancellation));
}

inline pkgexec::execution_resources resources(
    const pkgexec::execution_request& request,
    const std::filesystem::path& workspace)
{
  std::vector<pkgexec::resource_materialization> values;
  values.emplace_back(resource("workspace"), workspace);
  for (const auto& binding : request.resources().bindings()) {
    if (binding.resource() == resource("source")) {
      std::filesystem::create_directories(workspace / "source");
      values.emplace_back(resource("source"), workspace / "source");
    }
  }
  return pkgexec::execution_resources::admit(
      request, root(), "/", std::move(values));
}

} // namespace fixture
