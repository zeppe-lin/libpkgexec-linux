// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#include "runtime_fixture.h"
#include "test.h"

#include <libpkgexec-linux/libpkgexec-linux.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <arpa/inet.h>
#include <grp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

std::filesystem::path network_probe;

class parent_listener final {
public:
  parent_listener()
  {
    socket_ = ::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (socket_ < 0) {
      throw std::runtime_error("cannot create parent network listener");
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    if (::bind(socket_, reinterpret_cast<sockaddr*>(&address),
               sizeof(address)) != 0 ||
        ::listen(socket_, 4) != 0) {
      const int saved = errno;
      ::close(socket_);
      socket_ = -1;
      throw std::runtime_error("cannot bind parent network listener: " +
                               std::to_string(saved));
    }
    socklen_t size = sizeof(address);
    if (::getsockname(socket_, reinterpret_cast<sockaddr*>(&address),
                      &size) != 0) {
      ::close(socket_);
      socket_ = -1;
      throw std::runtime_error("cannot inspect parent network listener");
    }
    port_ = ntohs(address.sin_port);
  }
  ~parent_listener()
  {
    if (socket_ >= 0) {
      ::close(socket_);
    }
  }
  [[nodiscard]] std::uint16_t port() const noexcept { return port_; }
  [[nodiscard]] bool received(int timeout_milliseconds)
  {
    pollfd descriptor{socket_, POLLIN, 0};
    const int ready = ::poll(&descriptor, 1, timeout_milliseconds);
    if (ready <= 0 || (descriptor.revents & POLLIN) == 0) {
      return false;
    }
    const int accepted = ::accept4(socket_, nullptr, nullptr, SOCK_CLOEXEC);
    if (accepted < 0) {
      return false;
    }
    char value = 0;
    const bool okay = ::recv(accepted, &value, sizeof(value), MSG_WAITALL) ==
                          sizeof(value) &&
                      value == 'p';
    ::close(accepted);
    return okay;
  }
private:
  int socket_ = -1;
  std::uint16_t port_ = 0;
};

class temporary_tree final {
public:
  temporary_tree(const pkgexec_linux::interpreter_binding& interpreter,
                 const std::filesystem::path& probe)
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
    runtime_fixture::copy_runtime(root_, interpreter.executable());
    runtime_fixture::copy_runtime(root_, probe, "/bin/network-probe");
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
    const pkgexec_linux::interpreter_binding& shell,
    pkgexec::network_policy network = pkgexec::network_policy::allowed,
    std::optional<std::string> program = std::nullopt)
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
      network, stdin_policy::null_device,
      stream_policy::capture_complete, stream_policy::capture_complete);
  const std::string default_program =
      "test -f /source/input && "
      "! ( : > /source/forbidden ) 2>/dev/null && "
      "! ( : > /root-only/forbidden ) 2>/dev/null && "
      "test ! -e /etc/passwd && "
      "printf isolated > /workspace/output && "
      "printf '%s\\n' \"$PWD\"";
  return execution_request::seal(
      pkgsource::program(pkgsource::program_language::posix_shell,
                         program ? std::move(*program) : default_program),
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

std::string error_output(const pkgexec::execution_result& result)
{
  return result.standard_error() && result.standard_error()->material()
      ? *result.standard_error()->material()
      : std::string{};
}

void require_success(const pkgexec::execution_result& result,
                     std::string_view operation)
{
  if (result.status() != pkgexec::execution_status::succeeded) {
    throw std::runtime_error(std::string(operation) + " failed: " +
                             result.diagnostic() + ": " +
                             error_output(result));
  }
}

bool has_guarantee(const pkgexec::execution_result& result,
                   pkgexec::execution_guarantee guarantee)
{
  return std::find(result.established_guarantees().begin(),
                   result.established_guarantees().end(), guarantee) !=
         result.established_guarantees().end();
}

int test()
{
  const auto shell = pkgexec_linux::interpreter_binding::inspect("/bin/sh");
  temporary_tree tree(shell, network_probe);
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
  require_success(result, "isolated filesystem execution");
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

  parent_listener allowed_listener;
  auto allowed_request = request(
      shell, pkgexec::network_policy::allowed,
      "/bin/network-probe allowed " +
          std::to_string(allowed_listener.port()));
  auto allowed = backend.execute(allowed_request, resources(allowed_request, tree));
  require_success(allowed, "allowed network execution");
  CHECK(allowed_listener.received(1000));

  parent_listener denied_listener;
  auto denied_request = request(
      shell, pkgexec::network_policy::denied,
      "/bin/network-probe denied " +
          std::to_string(denied_listener.port()));
  if (!backend.capabilities().supports(denied_request)) {
    auto unsupported = backend.execute(denied_request,
                                       resources(denied_request, tree));
    CHECK(unsupported.start_state() == pkgexec::execution_start_state::not_started);
    CHECK(unsupported.failure() ==
          pkgexec::execution_failure_kind::backend_unsupported);
    return 77;
  }
  auto denied = backend.execute(denied_request, resources(denied_request, tree));
  require_success(denied, "denied network execution");
  CHECK(has_guarantee(denied,
                      pkgexec::execution_guarantee::network_denied));
  CHECK(!denied_listener.received(100));

  parent_listener loopback_listener;
  auto loopback_request = request(
      shell, pkgexec::network_policy::loopback_only,
      "/bin/network-probe loopback " +
          std::to_string(loopback_listener.port()));
  if (!backend.capabilities().supports(loopback_request)) {
    auto unsupported = backend.execute(loopback_request,
                                       resources(loopback_request, tree));
    CHECK(unsupported.start_state() == pkgexec::execution_start_state::not_started);
    CHECK(unsupported.failure() ==
          pkgexec::execution_failure_kind::backend_unsupported);
    return 77;
  }
  auto loopback = backend.execute(loopback_request,
                                  resources(loopback_request, tree));
  require_success(loopback, "loopback-only network execution");
  CHECK(has_guarantee(loopback,
                      pkgexec::execution_guarantee::loopback_isolated));
  CHECK(!loopback_listener.received(100));
  return 0;
}

} // namespace

int main(int argc, char** argv)
{
  if (argc != 2) {
    return 2;
  }
  network_probe = argv[1];
  return run_test(test);
}
