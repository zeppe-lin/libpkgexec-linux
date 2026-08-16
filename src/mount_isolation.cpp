// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#include "mount_isolation.h"

#include <libpkgexec-linux/error.h>

#include "process_control.h"
#include "support.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <climits>
#include <cstring>
#include <optional>
#include <set>
#include <string_view>
#include <utility>

#include <dirent.h>
#include <fcntl.h>
#include <linux/mount.h>
#include <linux/openat2.h>
#include <sched.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace pkgexec_linux::detail {
namespace {

#ifndef OPEN_TREE_CLONE
#define OPEN_TREE_CLONE 1
#endif
#ifndef OPEN_TREE_CLOEXEC
#define OPEN_TREE_CLOEXEC O_CLOEXEC
#endif
#ifndef MOVE_MOUNT_F_EMPTY_PATH
#define MOVE_MOUNT_F_EMPTY_PATH 0x00000004
#endif
#ifndef MOVE_MOUNT_T_EMPTY_PATH
#define MOVE_MOUNT_T_EMPTY_PATH 0x00000040
#endif

int open_exact_path(const std::filesystem::path& path)
{
#ifdef __NR_openat2
  const std::string text = path.string();
  const open_how how{
      static_cast<std::uint64_t>(O_PATH | O_DIRECTORY | O_CLOEXEC),
      0,
      static_cast<std::uint64_t>(RESOLVE_NO_SYMLINKS |
                                 RESOLVE_NO_MAGICLINKS),
  };
  return static_cast<int>(::syscall(__NR_openat2, AT_FDCWD, text.c_str(),
                                    &how, sizeof(how)));
#else
  (void)path;
  errno = ENOSYS;
  return -1;
#endif
}

int open_beneath(int root_fd, std::string_view logical)
{
#ifdef __NR_openat2
  std::string relative(logical);
  while (!relative.empty() && relative.front() == '/') {
    relative.erase(relative.begin());
  }
  if (relative.empty()) {
    relative = ".";
  }
  const open_how how{
      static_cast<std::uint64_t>(O_PATH | O_DIRECTORY | O_CLOEXEC),
      0,
      static_cast<std::uint64_t>(RESOLVE_BENEATH | RESOLVE_NO_SYMLINKS |
                                 RESOLVE_NO_MAGICLINKS),
  };
  return static_cast<int>(::syscall(__NR_openat2, root_fd, relative.c_str(),
                                    &how, sizeof(how)));
#else
  (void)root_fd;
  (void)logical;
  errno = ENOSYS;
  return -1;
#endif
}

inode_identity identity_of(int fd)
{
  struct stat info {};
  if (::fstat(fd, &info) != 0) {
    throw error(error_code::invalid_value,
                errno_message("fstat admitted resource", errno));
  }
  if (!S_ISDIR(info.st_mode)) {
    throw error(error_code::invalid_value,
                "root views and execution resources must be directories");
  }
  return {static_cast<std::uint64_t>(info.st_dev),
          static_cast<std::uint64_t>(info.st_ino)};
}

bool same_inode(int fd, const inode_identity& expected) noexcept
{
  struct stat info {};
  return ::fstat(fd, &info) == 0 && S_ISDIR(info.st_mode) &&
         static_cast<std::uint64_t>(info.st_dev) == expected.device &&
         static_cast<std::uint64_t>(info.st_ino) == expected.inode;
}

bool path_contains(const std::filesystem::path& parent,
                   const std::filesystem::path& child)
{
  auto first = parent.lexically_normal();
  auto second = child.lexically_normal();
  if (first == second) {
    return true;
  }
  auto p = first.begin();
  auto c = second.begin();
  for (; p != first.end(); ++p, ++c) {
    if (c == second.end() || *p != *c) {
      return false;
    }
  }
  return c != second.end();
}

void validate_role_access(const pkgexec::resource_binding& binding)
{
  using pkgexec::resource_access;
  using pkgexec::resource_role;
  const auto role = binding.slot().role();
  const auto access = binding.access();
  if ((role == resource_role::source_tree ||
       role == resource_role::build_input_tree ||
       role == resource_role::check_input_tree ||
       role == resource_role::package_tree) &&
      access != resource_access::read_only) {
    throw error(error_code::invalid_value,
                "source, package, and package-input resources must be read-only");
  }
  if ((role == resource_role::build_workspace ||
       role == resource_role::package_output_root ||
       role == resource_role::private_temporary_root ||
       role == resource_role::managed_target_root) &&
      access != resource_access::writable) {
    throw error(error_code::invalid_value,
                "workspace, output, temporary, and target resources must be writable");
  }
}

bool writable_by_current_credentials(int fd) noexcept
{
#ifdef __NR_faccessat2
  return ::syscall(__NR_faccessat2, fd, "", W_OK | X_OK,
                   AT_EMPTY_PATH | AT_EACCESS) == 0;
#else
  (void)fd;
  errno = ENOSYS;
  return false;
#endif
}

bool directory_is_empty(int fd)
{
  owned_fd readable(::openat(fd, ".",
                             O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW));
  if (!readable) {
    throw error(error_code::invalid_value,
                errno_message("open resource namespace", errno));
  }
  if (!same_inode(readable.get(), identity_of(fd))) {
    throw error(error_code::invalid_value,
                "resource namespace changed before enumeration");
  }
  DIR* directory = ::fdopendir(readable.release());
  if (!directory) {
    throw error(error_code::invalid_value,
                errno_message("open resource namespace stream", errno));
  }
  bool empty = true;
  errno = 0;
  while (dirent* entry = ::readdir(directory)) {
    const std::string_view name(entry->d_name);
    if (name != "." && name != "..") {
      empty = false;
      break;
    }
    errno = 0;
  }
  const int saved = errno;
  const int closed = ::closedir(directory);
  if (saved != 0 || closed != 0) {
    throw error(error_code::invalid_value,
                errno_message("read resource namespace",
                              saved != 0 ? saved : errno));
  }
  return empty;
}

bool is_input_role(pkgexec::resource_role role) noexcept
{
  return role == pkgexec::resource_role::build_input_tree ||
         role == pkgexec::resource_role::check_input_tree;
}

bool seal_namespace_mount(int fd) noexcept
{
#ifdef __NR_mount_setattr
  mount_attr attributes{};
  attributes.attr_set = MOUNT_ATTR_RDONLY | MOUNT_ATTR_NOSUID |
                        MOUNT_ATTR_NODEV | MOUNT_ATTR_NOEXEC;
  return ::syscall(__NR_mount_setattr, fd, "", AT_EMPTY_PATH,
                   &attributes, sizeof(attributes)) == 0;
#else
  (void)fd;
  errno = ENOSYS;
  return false;
#endif
}

std::filesystem::path make_scratch()
{
  std::array<char, 64> pattern{};
  const char* value = "/tmp/libpkgexec-linux.XXXXXX";
  std::memcpy(pattern.data(), value, std::strlen(value) + 1U);
  char* created = ::mkdtemp(pattern.data());
  if (!created) {
    throw error(error_code::invalid_value,
                errno_message("mkdtemp isolation root", errno));
  }
  const std::filesystem::path scratch(created);
  if (::chmod(scratch.c_str(), 0700) != 0 ||
      ::mkdir((scratch / "root").c_str(), 0700) != 0) {
    const int saved = errno;
    (void)::rmdir((scratch / "root").c_str());
    (void)::rmdir(scratch.c_str());
    throw error(error_code::invalid_value,
                errno_message("prepare isolation root", saved));
  }
  return scratch;
}

int clone_tree(int source_fd) noexcept
{
#ifdef __NR_open_tree
  return static_cast<int>(::syscall(
      __NR_open_tree, source_fd, "",
      OPEN_TREE_CLONE | OPEN_TREE_CLOEXEC | AT_EMPTY_PATH));
#else
  (void)source_fd;
  errno = ENOSYS;
  return -1;
#endif
}

bool set_tree_access(int tree_fd, pkgexec::resource_access access) noexcept
{
#ifdef __NR_mount_setattr
  mount_attr attributes{};
  attributes.attr_set = MOUNT_ATTR_NOSUID | MOUNT_ATTR_NODEV;
  attributes.attr_clr = MOUNT_ATTR_NOEXEC;
  if (access == pkgexec::resource_access::read_only) {
    attributes.attr_set |= MOUNT_ATTR_RDONLY;
  } else {
    attributes.attr_clr |= MOUNT_ATTR_RDONLY;
  }
  return ::syscall(__NR_mount_setattr, tree_fd, "", AT_EMPTY_PATH,
                   &attributes, sizeof(attributes)) == 0;
#else
  (void)tree_fd;
  (void)access;
  errno = ENOSYS;
  return false;
#endif
}

bool attach_tree(int tree_fd, int target_fd) noexcept
{
#ifdef __NR_move_mount
  return ::syscall(__NR_move_mount, tree_fd, "", target_fd, "",
                   MOVE_MOUNT_F_EMPTY_PATH | MOVE_MOUNT_T_EMPTY_PATH) == 0;
#else
  (void)tree_fd;
  (void)target_fd;
  errno = ENOSYS;
  return false;
#endif
}

bool create_private_root(const isolated_admission& admission,
                         mount_setup_failure& failure) noexcept
{
  // Realize detached trees while the inherited source descriptors still
  // belong to this process's current mount namespace. Older kernels reject
  // copying a path from another mount namespace, and also reject cloning an
  // already-detached anonymous mount. These child-owned trees cross the
  // namespace boundary only through move_mount(), which is the supported
  // detached-mount handoff.
  owned_fd realized_root_tree(clone_tree(admission.root_source_fd()));
  if (!realized_root_tree ||
      !set_tree_access(realized_root_tree.get(),
                       pkgexec::resource_access::read_only)) {
    failure = {mount_setup_stage::root_tree, errno};
    return false;
  }
  std::vector<owned_fd> realized_bindings;
  realized_bindings.reserve(admission.bindings().size());
  for (const auto& binding : admission.bindings()) {
    owned_fd realized_tree(clone_tree(binding.source.get()));
    if (!realized_tree ||
        !set_tree_access(realized_tree.get(), binding.access)) {
      failure = {mount_setup_stage::resource_tree, errno};
      return false;
    }
    realized_bindings.push_back(std::move(realized_tree));
  }

  if (::unshare(CLONE_NEWNS) != 0) {
    failure = {mount_setup_stage::mount_namespace, errno};
    return false;
  }
  if (::mount(nullptr, "/", nullptr, MS_REC | MS_PRIVATE, nullptr) != 0) {
    failure = {mount_setup_stage::private_propagation, errno};
    return false;
  }
  if (::mount("tmpfs", admission.scratch_path().c_str(), "tmpfs",
              MS_NOSUID | MS_NODEV, "mode=0700,size=4m") != 0) {
    failure = {mount_setup_stage::scratch_mount, errno};
    return false;
  }
  const auto root_path = admission.scratch_path() / "root";
  if (::mkdir(root_path.c_str(), 0700) != 0 && errno != EEXIST) {
    failure = {mount_setup_stage::scratch_mount, errno};
    return false;
  }
  owned_fd target(open_exact_path(root_path));
  if (!target) {
    failure = {mount_setup_stage::root_tree, errno};
    return false;
  }
  if (!attach_tree(realized_root_tree.get(), target.get())) {
    failure = {mount_setup_stage::root_tree, errno};
    return false;
  }

  owned_fd attached_root(open_exact_path(root_path));
  if (!attached_root) {
    failure = {mount_setup_stage::root_tree, errno};
    return false;
  }

  const auto device_path = root_path / "dev";
  struct stat device_directory {};
  if (::lstat(device_path.c_str(), &device_directory) != 0 ||
      !S_ISDIR(device_directory.st_mode)) {
    failure = {mount_setup_stage::device_filesystem,
               errno == 0 ? ENOTDIR : errno};
    return false;
  }
  if (::mount("tmpfs", device_path.c_str(), "tmpfs",
              MS_NOSUID | MS_NOEXEC, "mode=0755,size=64k") != 0) {
    failure = {mount_setup_stage::device_filesystem, errno};
    return false;
  }
  const auto null_path = device_path / "null";
  if (::mknod(null_path.c_str(), S_IFCHR | 0666, makedev(1, 3)) != 0 ||
      ::chmod(null_path.c_str(), 0666) != 0) {
    failure = {mount_setup_stage::device_filesystem, errno};
    return false;
  }

  for (const auto& input_namespace : admission.namespaces()) {
    owned_fd destination(
        open_beneath(attached_root.get(), input_namespace.logical_path));
    if (!destination ||
        !same_inode(destination.get(), input_namespace.target_identity)) {
      failure = {mount_setup_stage::input_namespace,
                 destination ? ESTALE : errno};
      return false;
    }
    const auto namespace_path =
        root_path / std::filesystem::path(input_namespace.logical_path).relative_path();
    if (::mount("tmpfs", namespace_path.c_str(), "tmpfs",
                MS_NOSUID | MS_NODEV | MS_NOEXEC, "mode=0755") != 0) {
      failure = {mount_setup_stage::input_namespace, errno};
      return false;
    }
    for (const auto& child : input_namespace.children) {
      if (::mkdir((namespace_path / child).c_str(), 0555) != 0) {
        failure = {mount_setup_stage::input_namespace, errno};
        return false;
      }
    }
  }

  for (std::size_t index = 0; index < admission.bindings().size(); ++index) {
    const auto& binding = admission.bindings()[index];
    owned_fd destination(open_beneath(attached_root.get(), binding.logical_path));
    if (!destination ||
        (!binding.synthetic_destination &&
         !same_inode(destination.get(), binding.target_identity))) {
      failure = {mount_setup_stage::resource_tree,
                 destination ? ESTALE : errno};
      return false;
    }
    if (!attach_tree(realized_bindings[index].get(), destination.get())) {
      failure = {mount_setup_stage::resource_tree, errno};
      return false;
    }
    owned_fd visible(open_beneath(attached_root.get(), binding.logical_path));
    if (!visible) {
      failure = {mount_setup_stage::resource_tree, errno};
      return false;
    }
    if (!same_inode(visible.get(), binding.source_identity)) {
      failure = {mount_setup_stage::resource_tree, ESTALE};
      return false;
    }
  }
  for (const auto& input_namespace : admission.namespaces()) {
    owned_fd namespace_fd(
        open_beneath(attached_root.get(), input_namespace.logical_path));
    if (!namespace_fd) {
      failure = {mount_setup_stage::input_namespace, errno};
      return false;
    }
    if (!seal_namespace_mount(namespace_fd.get())) {
      failure = {mount_setup_stage::input_namespace, errno};
      return false;
    }
  }
  if (::fchdir(attached_root.get()) != 0 || ::chroot(".") != 0 ||
      ::chdir("/") != 0) {
    failure = {mount_setup_stage::root_entry, errno};
    return false;
  }
  return true;
}

int probe_exit_code(int value) noexcept
{
  return value == EPERM || value == EACCES ? 2 : 1;
}

bool probe_isolated_realization(const isolated_admission& admission,
                                mount_setup_failure& failure) noexcept
{
  int report[2] = {-1, -1};
  if (::pipe2(report, O_CLOEXEC) != 0) {
    failure = {mount_setup_stage::probe_preparation, errno};
    return false;
  }
  const pid_t child = ::fork();
  if (child < 0) {
    failure = {mount_setup_stage::probe_preparation, errno};
    ::close(report[0]);
    ::close(report[1]);
    return false;
  }
  if (child == 0) {
    ::close(report[0]);
    auto send_failure = [&](mount_setup_failure value) noexcept {
      if (value.error == 0) {
        value.error = EIO;
      }
      (void)::write(report[1], &value, sizeof(value));
      _exit(probe_exit_code(value.error));
    };
    mount_setup_failure child_failure{};
    if (!setup_isolated_filesystem(admission, child_failure)) {
      send_failure(child_failure);
    }
    struct stat null_info {};
    if (::stat("/dev/null", &null_info) != 0 || !S_ISCHR(null_info.st_mode)) {
      send_failure({mount_setup_stage::device_filesystem,
                    errno == 0 ? ENODEV : errno});
    }
    const int null_fd = ::open("/dev/null", O_WRONLY | O_CLOEXEC);
    if (null_fd < 0) {
      send_failure({mount_setup_stage::device_filesystem, errno});
    }
    const unsigned char byte = 0;
    if (::write(null_fd, &byte, 1) != 1 || ::close(null_fd) != 0) {
      send_failure({mount_setup_stage::device_filesystem,
                    errno == 0 ? EIO : errno});
    }
    if (!drop_process_capabilities()) {
      send_failure({mount_setup_stage::capability_drop, errno});
    }
    ::close(report[1]);
    _exit(0);
  }

  ::close(report[1]);
  mount_setup_failure reported{};
  const ssize_t report_size = ::read(report[0], &reported, sizeof(reported));
  ::close(report[0]);
  int status = 0;
  while (::waitpid(child, &status, 0) < 0 && errno == EINTR) {
  }
  if (!WIFEXITED(status)) {
    failure = {mount_setup_stage::probe_preparation, EIO};
    return false;
  }
  if (WEXITSTATUS(status) == 0) {
    return true;
  }
  failure = report_size == static_cast<ssize_t>(sizeof(reported))
      ? reported
      : mount_setup_failure{
            mount_setup_stage::probe_preparation,
            WEXITSTATUS(status) == 2 ? EPERM : EIO};
  return false;
}

} // namespace

owned_fd::owned_fd(int value) noexcept : value_(value) {}
owned_fd::~owned_fd() { reset(); }
owned_fd::owned_fd(owned_fd&& other) noexcept : value_(other.release()) {}
owned_fd& owned_fd::operator=(owned_fd&& other) noexcept
{
  if (this != &other) {
    reset(other.release());
  }
  return *this;
}
int owned_fd::get() const noexcept { return value_; }
owned_fd::operator bool() const noexcept { return value_ >= 0; }
int owned_fd::release() noexcept
{
  const int value = value_;
  value_ = -1;
  return value;
}
void owned_fd::reset(int value) noexcept
{
  if (value_ >= 0) {
    ::close(value_);
  }
  value_ = value;
}

isolated_admission::isolated_admission(
    owned_fd root_source, std::vector<isolated_binding> bindings,
    std::vector<isolated_namespace> namespaces, std::filesystem::path scratch)
    : root_source_(std::move(root_source)), bindings_(std::move(bindings)),
      namespaces_(std::move(namespaces)), scratch_(std::move(scratch))
{
}
isolated_admission::isolated_admission(isolated_admission&& other) noexcept
    : root_source_(std::move(other.root_source_)), bindings_(std::move(other.bindings_)),
      namespaces_(std::move(other.namespaces_)), scratch_(std::move(other.scratch_)),
      cleanup_attempted_(other.cleanup_attempted_)
{
  other.scratch_.clear();
  other.cleanup_attempted_ = true;
}
isolated_admission& isolated_admission::operator=(isolated_admission&& other) noexcept
{
  if (this != &other) {
    cleanup_best_effort();
    root_source_ = std::move(other.root_source_);
    bindings_ = std::move(other.bindings_);
    namespaces_ = std::move(other.namespaces_);
    scratch_ = std::move(other.scratch_);
    cleanup_attempted_ = other.cleanup_attempted_;
    other.scratch_.clear();
    other.cleanup_attempted_ = true;
  }
  return *this;
}
isolated_admission::~isolated_admission() { cleanup_best_effort(); }
int isolated_admission::root_source_fd() const noexcept { return root_source_.get(); }
const std::vector<isolated_binding>& isolated_admission::bindings() const noexcept
{ return bindings_; }
const std::vector<isolated_namespace>& isolated_admission::namespaces() const noexcept
{ return namespaces_; }
const std::filesystem::path& isolated_admission::scratch_path() const noexcept
{ return scratch_; }
void isolated_admission::cleanup_best_effort() noexcept
{
  if (!cleanup_attempted_ && !scratch_.empty()) {
    (void)verify_parent_cleanup();
  }
}
bool isolated_admission::verify_parent_cleanup() noexcept
{
  if (cleanup_attempted_) {
    return scratch_.empty();
  }
  cleanup_attempted_ = true;
  bool okay = true;
  if (!scratch_.empty()) {
    if (::rmdir((scratch_ / "root").c_str()) != 0 && errno != ENOENT) {
      okay = false;
    }
    if (::rmdir(scratch_.c_str()) != 0 && errno != ENOENT) {
      okay = false;
    }
  }
  if (okay) {
    scratch_.clear();
  }
  return okay;
}

isolated_admission admit_isolated_resources(
    const pkgexec::execution_request& request,
    const pkgexec::execution_resources& resources)
{
  const auto root_path = resources.root_view_path().lexically_normal();
  if (root_path == std::filesystem::path("/")) {
    throw error(error_code::invalid_value,
                "isolated execution requires a dedicated root-view directory");
  }
  owned_fd root(open_exact_path(root_path));
  if (!root) {
    throw error(error_code::invalid_value,
                errno_message("open exact root view", errno));
  }
  (void)identity_of(root.get());

  std::vector<std::filesystem::path> logical_paths;
  std::vector<std::filesystem::path> host_paths;
  std::vector<inode_identity> source_identities;
  std::vector<isolated_binding> admitted;
  std::vector<isolated_namespace> namespaces;
  admitted.reserve(request.resources().bindings().size());

  for (const auto& binding : request.resources().bindings()) {
    validate_role_access(binding);
    const std::filesystem::path logical(binding.mount_point().string());
    if (logical == "/") {
      throw error(error_code::invalid_value,
                  "execution resources cannot replace the root view");
    }
    if (path_contains(std::filesystem::path("/dev"), logical)) {
      throw error(error_code::invalid_value,
                  "execution resources cannot occupy backend-owned /dev");
    }
    for (const auto& existing : logical_paths) {
      if (path_contains(existing, logical) || path_contains(logical, existing)) {
        throw error(error_code::invalid_value,
                    "resource logical mount points cannot overlap");
      }
    }
    logical_paths.push_back(logical);

    const auto& materialization = resources.materialization(binding.resource());
    const auto host = materialization.host_path().lexically_normal();
    if (path_contains(root_path, host) || path_contains(host, root_path)) {
      throw error(error_code::invalid_value,
                  "execution resource host paths cannot overlap the root view");
    }
    for (const auto& existing : host_paths) {
      if (path_contains(existing, host) || path_contains(host, existing)) {
        throw error(error_code::invalid_value,
                    "resource host paths cannot overlap");
      }
    }
    host_paths.push_back(host);

    owned_fd source(open_exact_path(host));
    if (!source) {
      throw error(error_code::invalid_value,
                  errno_message("open exact execution resource", errno));
    }
    const auto source_identity = identity_of(source.get());
    if (std::find(source_identities.begin(), source_identities.end(),
                  source_identity) != source_identities.end()) {
      throw error(error_code::invalid_value,
                  "multiple resources resolve to the same directory");
    }
    source_identities.push_back(source_identity);
    if (binding.access() == pkgexec::resource_access::writable &&
        !writable_by_current_credentials(source.get())) {
      throw error(error_code::invalid_value,
                  "writable execution resource is not writable by current credentials");
    }

    const bool input = is_input_role(binding.slot().role());
    inode_identity target_identity{};
    if (input) {
      const auto parent = logical.parent_path();
      const std::string child = logical.filename().string();
      if (parent.empty() || parent == std::filesystem::path("/") ||
          child.empty()) {
        throw error(error_code::invalid_value,
                    "input resource mount point requires a dedicated parent namespace");
      }
      owned_fd target_parent(open_beneath(root.get(), parent.string()));
      if (!target_parent) {
        throw error(error_code::invalid_value,
                    errno_message("open root input namespace", errno));
      }
      if (!directory_is_empty(target_parent.get())) {
        throw error(error_code::invalid_value,
                    "root input namespace must be empty");
      }
      const auto parent_identity = identity_of(target_parent.get());
      auto found = std::find_if(
          namespaces.begin(), namespaces.end(),
          [&](const isolated_namespace& value) {
            return value.logical_path == parent.string();
          });
      if (found == namespaces.end()) {
        namespaces.push_back({parent.string(), parent_identity, {child}});
      } else {
        if (!(found->target_identity == parent_identity)) {
          throw error(error_code::invalid_value,
                      "root input namespace changed during admission");
        }
        found->children.push_back(child);
      }
    } else {
      owned_fd target(open_beneath(root.get(), binding.mount_point().string()));
      if (!target) {
        throw error(error_code::invalid_value,
                    errno_message("open root resource destination", errno));
      }
      target_identity = identity_of(target.get());
    }
    admitted.push_back({std::move(source), source_identity, target_identity,
                        binding.access(), binding.mount_point().string(), input});
  }
  for (auto& value : namespaces) {
    std::sort(value.children.begin(), value.children.end());
    value.children.erase(
        std::unique(value.children.begin(), value.children.end()),
        value.children.end());
  }
  return isolated_admission(std::move(root), std::move(admitted),
                            std::move(namespaces), make_scratch());
}

bool setup_isolated_filesystem(const isolated_admission& admission,
                               mount_setup_failure& failure) noexcept
{
  return create_private_root(admission, failure);
}

bool probe_openat2() noexcept
{
  const int fd = open_exact_path("/");
  if (fd < 0) {
    return false;
  }
  ::close(fd);
  return true;
}

bool probe_isolated_filesystem(mount_setup_failure& failure) noexcept
{
  failure = {};
  if (!probe_openat2()) {
    failure = {mount_setup_stage::probe_preparation, errno};
    return false;
  }

  std::array<char, 64> pattern{};
  const char* template_value = "/tmp/libpkgexec-linux-probe.XXXXXX";
  std::memcpy(pattern.data(), template_value,
              std::strlen(template_value) + 1U);
  char* created = ::mkdtemp(pattern.data());
  if (!created) {
    failure = {mount_setup_stage::probe_preparation, errno};
    return false;
  }
  const std::filesystem::path base(created);
  const auto view = base / "view";
  const auto target_path = view / "resource";
  const auto device_path = view / "dev";
  const auto material = base / "material";
  auto cleanup_fixture = [&]() noexcept {
    (void)::rmdir(material.c_str());
    (void)::rmdir(target_path.c_str());
    (void)::rmdir(device_path.c_str());
    (void)::rmdir(view.c_str());
    (void)::rmdir(base.c_str());
  };
  if (::mkdir(view.c_str(), 0700) != 0 ||
      ::mkdir(target_path.c_str(), 0700) != 0 ||
      ::mkdir(device_path.c_str(), 0700) != 0 ||
      ::mkdir(material.c_str(), 0700) != 0) {
    failure = {mount_setup_stage::probe_preparation, errno};
    cleanup_fixture();
    return false;
  }

  std::optional<isolated_admission> admission;
  try {
    owned_fd root(open_exact_path(view));
    owned_fd source(open_exact_path(material));
    owned_fd target(open_beneath(root.get(), "/resource"));
    if (!root || !source || !target) {
      failure = {mount_setup_stage::probe_preparation, errno};
      cleanup_fixture();
      return false;
    }
    const auto source_identity = identity_of(source.get());
    const auto target_identity = identity_of(target.get());
    std::vector<isolated_binding> probe_bindings;
    probe_bindings.push_back({std::move(source), source_identity,
                              target_identity,
                              pkgexec::resource_access::read_only,
                              "/resource"});
    admission = isolated_admission(std::move(root),
                                    std::move(probe_bindings), {},
                                    make_scratch());
  } catch (...) {
    failure = {mount_setup_stage::probe_preparation,
               errno == 0 ? EIO : errno};
    cleanup_fixture();
    return false;
  }

  bool realized = true;
  // One admission must be stable authority, not a one-shot mount payload.
  // Exercise the same retained source descriptors twice so a future attempt
  // to retain/consume detached realization mounts fails this probe.
  for (int attempt = 0; attempt < 2; ++attempt) {
    if (!probe_isolated_realization(*admission, failure)) {
      realized = false;
      break;
    }
  }
  const bool scratch_cleaned = admission->verify_parent_cleanup();
  // Release the retained exact source descriptors before proving removal of
  // the source fixture from which they were admitted.
  admission.reset();
  mount_setup_failure fixture_failure{};
  auto remove_fixture_directory = [&](const std::filesystem::path& path,
                                      mount_setup_stage stage) noexcept {
    if (::rmdir(path.c_str()) == 0 || errno == ENOENT) {
      return true;
    }
    if (fixture_failure.error == 0) {
      fixture_failure = {stage, errno};
    }
    return false;
  };
  bool fixture_cleaned = true;
  fixture_cleaned = remove_fixture_directory(
      material, mount_setup_stage::fixture_material_cleanup) && fixture_cleaned;
  fixture_cleaned = remove_fixture_directory(
      target_path, mount_setup_stage::fixture_resource_cleanup) && fixture_cleaned;
  fixture_cleaned = remove_fixture_directory(
      device_path, mount_setup_stage::fixture_device_cleanup) && fixture_cleaned;
  fixture_cleaned = remove_fixture_directory(
      view, mount_setup_stage::fixture_view_cleanup) && fixture_cleaned;
  fixture_cleaned = remove_fixture_directory(
      base, mount_setup_stage::fixture_root_cleanup) && fixture_cleaned;
  if (!scratch_cleaned) {
    failure = {mount_setup_stage::parent_cleanup, EBUSY};
    return false;
  }
  if (!fixture_cleaned) {
    failure = fixture_failure;
    return false;
  }
  return realized;
}

std::string_view mount_stage_name(mount_setup_stage stage) noexcept
{
  switch (stage) {
    case mount_setup_stage::probe_preparation: return "isolated probe preparation";
    case mount_setup_stage::mount_namespace: return "mount namespace";
    case mount_setup_stage::private_propagation: return "private mount propagation";
    case mount_setup_stage::scratch_mount: return "private scratch mount";
    case mount_setup_stage::root_tree: return "root mount tree";
    case mount_setup_stage::resource_tree: return "resource mount tree";
    case mount_setup_stage::input_namespace: return "private input namespace";
    case mount_setup_stage::device_filesystem: return "private device filesystem";
    case mount_setup_stage::root_entry: return "root-view entry";
    case mount_setup_stage::capability_drop: return "capability drop";
    case mount_setup_stage::parent_cleanup: return "parent isolation scratch cleanup";
    case mount_setup_stage::fixture_cleanup: return "isolated probe fixture cleanup";
    case mount_setup_stage::fixture_material_cleanup:
      return "isolated probe material cleanup";
    case mount_setup_stage::fixture_resource_cleanup:
      return "isolated probe resource-target cleanup";
    case mount_setup_stage::fixture_device_cleanup:
      return "isolated probe device-target cleanup";
    case mount_setup_stage::fixture_view_cleanup:
      return "isolated probe root-view cleanup";
    case mount_setup_stage::fixture_root_cleanup:
      return "isolated probe fixture-root cleanup";
  }
  return "mount isolation";
}

} // namespace pkgexec_linux::detail
