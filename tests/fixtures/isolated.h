// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "runtime_root.h"
#include "../support/test.h"
#include "../support/temporary_directory.h"

#include <libpkgexec-linux/libpkgexec-linux.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <grp.h>
#include <sys/stat.h>
#include <unistd.h>

namespace isolated_fixture {

inline std::string digest(std::string_view value)
{
  return pkgexec::sha256_digest::of_bytes(value).hex();
}

inline pkgexec::resource_identity resource(std::string_view value)
{
  return pkgexec::resource_identity::from_sha256(digest(value));
}

inline pkgexec::root_view_identity root_identity()
{
  return pkgexec::root_view_identity::from_sha256(digest("isolated-root"));
}

inline std::vector<std::uint64_t> groups()
{
  const int count = ::getgroups(0, nullptr);
  std::vector<gid_t> native(static_cast<std::size_t>(std::max(count, 0)));
  if (count > 0) {
    CHECK(::getgroups(count, native.data()) == count);
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

class tree final {
public:
  tree(const pkgexec_linux::interpreter_binding& interpreter,
       std::vector<std::pair<std::filesystem::path, std::filesystem::path>>
           payloads = {})
      : temporary_("/tmp/pkgexec-isolated.XXXXXX")
  {
    root_ = temporary_.path() / "root";
    source_ = temporary_.path() / "source";
    workspace_ = temporary_.path() / "workspace";
    for (const auto& directory :
         {root_, source_, workspace_, root_ / "source", root_ / "workspace",
          root_ / "root-only", root_ / "dev"}) {
      std::filesystem::create_directories(directory);
    }
    runtime_fixture::copy_runtime(root_, interpreter.executable());
    for (const auto& [host, logical] : payloads) {
      runtime_fixture::copy_runtime(root_, host, logical);
    }
    std::ofstream(root_ / "dev" / "null") << "root device sentinel\n";
    std::ofstream(source_ / "input") << "source\n";
  }

  [[nodiscard]] const std::filesystem::path& root() const noexcept
  {
    return root_;
  }
  [[nodiscard]] const std::filesystem::path& source() const noexcept
  {
    return source_;
  }
  [[nodiscard]] const std::filesystem::path& workspace() const noexcept
  {
    return workspace_;
  }
private:
  test_support::temporary_directory temporary_;
  std::filesystem::path root_;
  std::filesystem::path source_;
  std::filesystem::path workspace_;
};

inline pkgexec::execution_request request(
    const pkgexec_linux::interpreter_binding& shell,
    pkgexec::network_policy network = pkgexec::network_policy::allowed,
    std::optional<std::string> program = std::nullopt,
    pkgexec::cancellation_policy cancellation =
        pkgexec::cancellation_policy::disabled(),
    pkgexec::resource_limits limits = pkgexec::resource_limits::make(),
    bool no_new_privileges = true,
    std::uint64_t user_id = static_cast<std::uint64_t>(::getuid()))
{
  using namespace pkgexec;
  const auto source_slot = resource_slot::named(resource_role::source_tree, "main");
  const auto workspace_slot = resource_slot::singleton(resource_role::build_workspace);
  auto layout = resource_layout::seal(
      {
          resource_binding(source_slot, resource("source"),
                           resource_access::read_only,
                           logical_path::parse("/source")),
          resource_binding(workspace_slot, resource("workspace"),
                           resource_access::writable,
                           logical_path::parse("/workspace")),
      },
      workspace_slot);
  auto environment = environment_policy::hermetic(
      {logical_path::parse("/bin"), logical_path::parse("/usr/bin")},
      logical_path::parse("/workspace"), logical_path::parse("/workspace"),
      1U, 0022, std::nullopt, network, stdin_policy::null_device,
      stream_policy::capture_complete, stream_policy::capture_complete);
  const std::string default_program =
      "test -f /source/input && "
      "! ( : > /source/forbidden ) 2>/workspace/source-error && "
      "! ( : > /root-only/forbidden ) 2>/workspace/root-error && "
      "test ! -e /etc/passwd && "
      "printf isolated > /workspace/output && "
      "printf '%s\\n' \"$PWD\"";
  return execution_request::seal(
      pkgsource::program(pkgsource::program_language::posix_shell,
                         program ? std::move(*program) : default_program),
      execution_purpose::build(), shell.identity(), root_identity(),
      std::move(layout), std::move(environment),
      credential_policy::fixed(user_id, static_cast<std::uint64_t>(::getgid()),
                               groups(), no_new_privileges),
      std::move(limits), std::move(cancellation));
}

inline pkgexec::execution_resources resources(
    const pkgexec::execution_request& value, const tree& material)
{
  return pkgexec::execution_resources::admit(
      value, root_identity(), material.root(),
      {
          pkgexec::resource_materialization(resource("source"), material.source()),
          pkgexec::resource_materialization(resource("workspace"),
                                            material.workspace()),
      });
}

} // namespace isolated_fixture
