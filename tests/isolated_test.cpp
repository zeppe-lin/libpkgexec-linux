// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#include "test.h"

#include <libpkgexec-linux/libpkgexec-linux.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <grp.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {

class temporary_tree final {
public:
  explicit temporary_tree(
      const pkgexec_linux::interpreter_binding& interpreter)
  {
    std::array<char, 64> pattern{};
    const char* value = "/tmp/pkgexec-isolated.XXXXXX";
    std::copy(value, value + std::char_traits<char>::length(value) + 1U,
              pattern.begin());
    char* created = ::mkdtemp(pattern.data());
    if (!created) {
      throw std::runtime_error("mkdtemp failed");
    }
    path_ = created;
    root_ = path_ / "root";
    source_ = path_ / "source";
    workspace_ = path_ / "workspace";
    for (const auto& directory :
         {root_, source_, workspace_, root_ / "source", root_ / "workspace",
          root_ / "root-only"}) {
      std::filesystem::create_directories(directory);
    }
    copy_runtime(interpreter.executable());
    std::ofstream(source_ / "input") << "source\n";
  }
  ~temporary_tree()
  {
    std::error_code ignored;
    std::filesystem::permissions(
        path_, std::filesystem::perms::owner_all,
        std::filesystem::perm_options::add, ignored);
    std::filesystem::remove_all(path_, ignored);
  }
  [[nodiscard]] const std::filesystem::path& root() const noexcept
  { return root_; }
  [[nodiscard]] const std::filesystem::path& source() const noexcept
  { return source_; }
  [[nodiscard]] const std::filesystem::path& workspace() const noexcept
  { return workspace_; }
private:
  void copy_one(const std::filesystem::path& source)
  {
    const auto destination = root_ / source.relative_path();
    std::filesystem::create_directories(destination.parent_path());
    std::filesystem::copy_file(
        source, destination, std::filesystem::copy_options::overwrite_existing);
    std::filesystem::permissions(
        destination, std::filesystem::status(source).permissions());
  }

  void copy_runtime(const std::filesystem::path& executable)
  {
    copy_one(executable);
    const std::string command = "ldd '" + executable.string() + "'";
    FILE* stream = ::popen(command.c_str(), "r");
    if (!stream) {
      throw std::runtime_error("cannot inspect interpreter runtime closure");
    }
    std::array<char, 4096> line{};
    std::vector<std::filesystem::path> dependencies;
    while (::fgets(line.data(), static_cast<int>(line.size()), stream)) {
      std::string value(line.data());
      std::size_t start = value.find("=> /");
      if (start != std::string::npos) {
        start += 3U;
      } else {
        start = value.find('/');
      }
      if (start == std::string::npos) {
        continue;
      }
      const auto end = value.find_first_of(" \t\n", start);
      const auto path = value.substr(start, end - start);
      if (!path.empty() && path.front() == '/') {
        dependencies.emplace_back(path);
      }
    }
    const int status = ::pclose(stream);
    if (status != 0) {
      throw std::runtime_error("ldd failed for the interpreter fixture");
    }
    std::sort(dependencies.begin(), dependencies.end());
    dependencies.erase(std::unique(dependencies.begin(), dependencies.end()),
                       dependencies.end());
    for (const auto& dependency : dependencies) {
      copy_one(dependency);
    }
  }

  std::filesystem::path path_;
  std::filesystem::path root_;
  std::filesystem::path source_;
  std::filesystem::path workspace_;
};

std::string digest(std::string_view value)
{
  return pkgexec::sha256_digest::of_bytes(value).hex();
}

pkgexec::resource_identity resource(std::string_view value)
{
  return pkgexec::resource_identity::from_sha256(digest(value));
}

pkgexec::root_view_identity root_identity()
{
  return pkgexec::root_view_identity::from_sha256(digest("isolated-root"));
}

std::vector<std::uint64_t> groups()
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

pkgexec::execution_request request(
    const pkgexec_linux::interpreter_binding& shell)
{
  using namespace pkgexec;
  const auto source_slot =
      resource_slot::named(resource_role::source_tree, "main");
  const auto workspace_slot =
      resource_slot::singleton(resource_role::build_workspace);
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
      logical_path::parse("/workspace"),
      logical_path::parse("/workspace"), 1U, 0022, std::nullopt,
      network_policy::allowed, stdin_policy::null_device,
      stream_policy::capture_complete, stream_policy::capture_complete);
  const std::string script =
      "test -f /source/input && "
      "! ( : > /source/forbidden ) 2>/dev/null && "
      "! ( : > /root-only/forbidden ) 2>/dev/null && "
      "test ! -e /etc/passwd && "
      "printf isolated > /workspace/output && "
      "printf '%s\\n' \"$PWD\"";
  return execution_request::seal(
      pkgsource::program(pkgsource::program_language::posix_shell, script),
      execution_purpose::build(), shell.identity(), root_identity(),
      std::move(layout), std::move(environment),
      credential_policy::fixed(static_cast<std::uint64_t>(::getuid()),
                               static_cast<std::uint64_t>(::getgid()),
                               groups(), true),
      resource_limits::make(), cancellation_policy::disabled());
}

pkgexec::execution_resources resources(
    const pkgexec::execution_request& value,
    const temporary_tree& tree)
{
  return pkgexec::execution_resources::admit(
      value, root_identity(), tree.root(),
      {
          pkgexec::resource_materialization(resource("source"), tree.source()),
          pkgexec::resource_materialization(resource("workspace"),
                                            tree.workspace()),
      });
}

std::string output(const pkgexec::execution_result& result)
{
  return result.standard_output() && result.standard_output()->material()
      ? *result.standard_output()->material()
      : std::string{};
}

int test()
{
  const auto shell = pkgexec_linux::interpreter_binding::inspect("/bin/sh");
  temporary_tree tree(shell);
  auto backend = pkgexec_linux::isolated_backend::make({shell});
  auto execution_request = request(shell);
  auto execution_resources = resources(execution_request, tree);

  if (!backend.capabilities().supports(execution_request)) {
    auto unsupported = backend.execute(execution_request, execution_resources);
    CHECK(unsupported.start_state() == pkgexec::execution_start_state::not_started);
    CHECK(unsupported.failure() ==
          pkgexec::execution_failure_kind::backend_unsupported);
    return 77;
  }

  auto result = backend.execute(execution_request, execution_resources);
  CHECK(result.status() == pkgexec::execution_status::succeeded);
  CHECK(result.cleanup() == pkgexec::cleanup_outcome::verified);
  CHECK(output(result) == "/workspace\n");
  CHECK(std::filesystem::exists(tree.workspace() / "output"));
  CHECK(!std::filesystem::exists(tree.source() / "forbidden"));
  CHECK(!std::filesystem::exists(tree.root() / "root-only" / "forbidden"));

  const auto symlink = tree.workspace().parent_path() / "source-link";
  std::filesystem::create_directory_symlink(tree.source(), symlink);
  auto bad_resources = pkgexec::execution_resources::admit(
      execution_request, root_identity(), tree.root(),
      {
          pkgexec::resource_materialization(resource("source"), symlink),
          pkgexec::resource_materialization(resource("workspace"),
                                            tree.workspace()),
      });
  auto rejected = backend.execute(execution_request, bad_resources);
  CHECK(rejected.start_state() == pkgexec::execution_start_state::not_started);
  CHECK(rejected.failure() ==
        pkgexec::execution_failure_kind::resource_admission_failed);

  auto live_root = pkgexec::execution_resources::admit(
      execution_request, root_identity(), "/",
      {
          pkgexec::resource_materialization(resource("source"), tree.source()),
          pkgexec::resource_materialization(resource("workspace"),
                                            tree.workspace()),
      });
  auto live_root_rejected = backend.execute(execution_request, live_root);
  CHECK(live_root_rejected.start_state() ==
        pkgexec::execution_start_state::not_started);
  CHECK(live_root_rejected.failure() ==
        pkgexec::execution_failure_kind::resource_admission_failed);

  auto overlapping_resources = pkgexec::execution_resources::admit(
      execution_request, root_identity(), tree.root(),
      {
          pkgexec::resource_materialization(resource("source"),
                                            tree.root() / "source"),
          pkgexec::resource_materialization(resource("workspace"),
                                            tree.workspace()),
      });
  auto overlap_rejected =
      backend.execute(execution_request, overlapping_resources);
  CHECK(overlap_rejected.start_state() ==
        pkgexec::execution_start_state::not_started);
  CHECK(overlap_rejected.failure() ==
        pkgexec::execution_failure_kind::resource_admission_failed);
  return 0;
}

} // namespace

int main() { return run_test(test); }
