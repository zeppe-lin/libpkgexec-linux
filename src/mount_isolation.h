// SPDX-FileCopyrightText: 2026 Alexandr Savca
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <libpkgexec/backend.h>

namespace pkgexec_linux::detail {

class owned_fd final {
public:
  owned_fd() noexcept = default;
  explicit owned_fd(int value) noexcept;
  ~owned_fd();
  owned_fd(const owned_fd&) = delete;
  owned_fd& operator=(const owned_fd&) = delete;
  owned_fd(owned_fd&& other) noexcept;
  owned_fd& operator=(owned_fd&& other) noexcept;
  [[nodiscard]] int get() const noexcept;
  [[nodiscard]] explicit operator bool() const noexcept;
  [[nodiscard]] int release() noexcept;
  void reset(int value = -1) noexcept;
private:
  int value_ = -1;
};

struct inode_identity final {
  std::uint64_t device;
  std::uint64_t inode;
  friend bool operator==(const inode_identity& lhs,
                         const inode_identity& rhs) noexcept
  { return lhs.device == rhs.device && lhs.inode == rhs.inode; }
};

struct isolated_binding final {
  owned_fd tree;
  inode_identity source_identity;
  inode_identity target_identity;
  pkgexec::resource_access access;
  std::string logical_path;
  bool synthetic_destination = false;
};

struct isolated_namespace final {
  std::string logical_path;
  inode_identity target_identity;
  std::vector<std::string> children;
};

class isolated_admission final {
public:
  isolated_admission(const isolated_admission&) = delete;
  isolated_admission& operator=(const isolated_admission&) = delete;
  isolated_admission(isolated_admission&& other) noexcept;
  isolated_admission& operator=(isolated_admission&& other) noexcept;
  ~isolated_admission();

  [[nodiscard]] int root_tree_fd() const noexcept;
  [[nodiscard]] const std::vector<isolated_binding>& bindings() const noexcept;
  [[nodiscard]] const std::vector<isolated_namespace>& namespaces() const noexcept;
  [[nodiscard]] const std::filesystem::path& scratch_path() const noexcept;
  [[nodiscard]] bool verify_parent_cleanup() noexcept;
private:
  friend isolated_admission admit_isolated_resources(
      const pkgexec::execution_request&,
      const pkgexec::execution_resources&);
  friend bool probe_isolated_filesystem(struct mount_setup_failure&) noexcept;
  isolated_admission(owned_fd root_tree,
                     std::vector<isolated_binding> bindings,
                     std::vector<isolated_namespace> namespaces,
                     std::filesystem::path scratch);
  void cleanup_best_effort() noexcept;

  owned_fd root_tree_;
  std::vector<isolated_binding> bindings_;
  std::vector<isolated_namespace> namespaces_;
  std::filesystem::path scratch_;
  bool cleanup_attempted_ = false;
};

enum class mount_setup_stage : std::uint32_t {
  mount_namespace,
  private_propagation,
  scratch_mount,
  root_tree,
  resource_tree,
  input_namespace,
  device_filesystem,
  root_entry,
  probe_preparation,
  capability_drop,
  parent_cleanup,
  fixture_cleanup,
};

struct mount_setup_failure final {
  mount_setup_stage stage = mount_setup_stage::probe_preparation;
  int error = 0;
};

[[nodiscard]] isolated_admission admit_isolated_resources(
    const pkgexec::execution_request& request,
    const pkgexec::execution_resources& resources);
[[nodiscard]] bool setup_isolated_filesystem(
    const isolated_admission& admission,
    mount_setup_failure& failure) noexcept;
[[nodiscard]] bool probe_openat2() noexcept;
[[nodiscard]] bool probe_isolated_filesystem(
    mount_setup_failure& failure) noexcept;
[[nodiscard]] std::string_view mount_stage_name(mount_setup_stage stage) noexcept;

} // namespace pkgexec_linux::detail
