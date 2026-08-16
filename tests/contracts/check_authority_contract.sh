#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
root=${1:?source root required}
fail()
{
  echo "authority-contract: $1" >&2
  exit 1
}
grep -Eq 'class( PKGEXEC_LINUX_API)? host_supervisor_backend' "$root/include/libpkgexec-linux/backend.h" ||
  fail 'host supervisor API missing'
grep -Eq 'class( PKGEXEC_LINUX_API)? isolated_backend' "$root/include/libpkgexec-linux/backend.h" ||
  fail 'isolated backend API missing'
grep -q 'controlled_execution_backend' "$root/include/libpkgexec-linux/backend.h" ||
  fail 'controlled execution boundary missing'
grep -q 'cancellation_token' "$root/include/libpkgexec-linux/backend.h" ||
  fail 'request-bound cancellation token missing'
grep -q 'does not create namespaces or mounts' "$root/include/libpkgexec-linux/backend.h" ||
  fail 'restricted resource boundary missing'
grep -q 'networking' "$root/include/libpkgexec-linux/backend.h" ||
  fail 'network boundary missing'
grep -q 'pidfd_cancellation' "$root/include/libpkgexec-linux/capability.h" ||
  fail 'pidfd cancellation observation missing'
grep -q 'process_group_containment' "$root/include/libpkgexec-linux/capability.h" ||
  fail 'cleanup containment observation missing'
grep -q 'mount_setattr' "$root/include/libpkgexec-linux/capability.h" ||
  fail 'mount attribute capability missing'
grep -q 'openat2' "$root/src/mount_isolation.cpp" ||
  fail 'descriptor path admission missing'
grep -q 'OPEN_TREE_CLONE' "$root/src/mount_isolation.cpp" ||
  fail 'detached mount-tree authority missing'
grep -q 'dedicated root-view directory' "$root/src/mount_isolation.cpp" ||
  fail 'dedicated root-view admission missing'
grep -q 'cannot overlap the root view' "$root/src/mount_isolation.cpp" ||
  fail 'root/resource overlap rejection missing'
grep -q 'move_mount' "$root/src/mount_isolation.cpp" ||
  fail 'descriptor mount attachment missing'
grep -F 'realized_root_tree(clone_tree(admission.root_source_fd()))' \
    "$root/src/mount_isolation.cpp" >/dev/null ||
  fail 'root realization does not clone retained exact source authority'
grep -F 'set_tree_access(realized_root_tree.get(),' \
    "$root/src/mount_isolation.cpp" >/dev/null ||
  fail 'root realization mount is not sealed after cloning'
grep -F 'realized_tree(clone_tree(binding.source.get()))' \
    "$root/src/mount_isolation.cpp" >/dev/null ||
  fail 'resource realization does not clone retained exact source authority'
grep -F 'set_tree_access(realized_tree.get(), binding.access)' \
    "$root/src/mount_isolation.cpp" >/dev/null ||
  fail 'resource realization mount is not sealed after cloning'
root_clone_line=$(grep -n -F \
  'realized_root_tree(clone_tree(admission.root_source_fd()))' \
  "$root/src/mount_isolation.cpp" | sed -n '1s/:.*//p')
namespace_line=$(grep -n -F '::unshare(CLONE_NEWNS)' \
  "$root/src/mount_isolation.cpp" | sed -n '1s/:.*//p')
[ -n "$root_clone_line" ] && [ -n "$namespace_line" ] &&
  [ "$root_clone_line" -lt "$namespace_line" ] ||
  fail 'realization clones admitted sources only after leaving their mount namespace'
! grep -F 'attach_tree(admission.root_source_fd()' \
    "$root/src/mount_isolation.cpp" >/dev/null ||
  fail 'retained root admission is attached directly'
! grep -F 'attach_tree(binding.source.get()' \
    "$root/src/mount_isolation.cpp" >/dev/null ||
  fail 'retained resource admission is attached directly'
! grep -F 'clone_tree(admission.root_tree_fd())' \
    "$root/src/mount_isolation.cpp" >/dev/null ||
  fail 'realization requires cloning an already detached root mount'
! grep -F 'clone_tree(binding.tree.get())' \
    "$root/src/mount_isolation.cpp" >/dev/null ||
  fail 'realization requires cloning an already detached resource mount'
grep -F 'attempt < 2' "$root/src/mount_isolation.cpp" >/dev/null ||
  fail 'isolated probe does not fight one-shot admission realization'
grep -F 'fixture_failure = {stage, errno}' \
    "$root/src/mount_isolation.cpp" >/dev/null ||
  fail 'isolated probe cleanup destroys the observed errno'
grep -q 'MOUNT_ATTR_NOSUID | MOUNT_ATTR_NODEV' "$root/src/mount_isolation.cpp" ||
  fail 'root/resource device authority is not sealed nodev'
grep -q 'attributes.attr_clr = MOUNT_ATTR_NOEXEC' "$root/src/mount_isolation.cpp" ||
  fail 'root/resource execution inherits ambient noexec'
grep -q 'MS_NOSUID | MS_NOEXEC' "$root/src/mount_isolation.cpp" ||
  fail 'private device filesystem policy is absent'
grep -q 'makedev(1, 3)' "$root/src/mount_isolation.cpp" ||
  fail 'private null-device authority is absent'
grep -q 'backend-owned /dev' "$root/src/mount_isolation.cpp" ||
  fail 'executor-owned device namespace is not reserved'
grep -q 'private null-device execution' "$root/tests/privileged/isolated_filesystem_test.cpp" ||
  fail 'private null-device runtime qualification is absent'
grep -q 'setup_network_policy' "$root/src/backend.cpp" ||
  fail 'network policy is not wired into child setup'
grep -q 'CLONE_NEWNET' "$root/src/network_isolation.cpp" ||
  fail 'private network namespace authority missing'
grep -q 'NETLINK_ROUTE' "$root/src/network_isolation.cpp" ||
  fail 'rtnetlink loopback authority missing'
! grep -E 'system\(|popen\(|execl|execv' \
    "$root/src/network_isolation.cpp" >/dev/null ||
  fail 'external utility entered network authority'
grep -q 'setsid()' "$root/src/backend.cpp" ||
  fail 'private execution session missing'
grep -q 'open_pidfd' "$root/src/backend.cpp" ||
  fail 'pidfd leader observation missing'
grep -q 'pidfd_send_signal' "$root/src/process_control.cpp" ||
  fail 'pidfd signal authority missing'
grep -q 'signal_process_group_members' "$root/src/backend.cpp" ||
  fail 'pidfd process-group realization is not wired'
grep -q 'waitid(type, identifier' "$root/src/backend.cpp" ||
  fail 'waitid pidfd observation missing'
grep -q 'cancellation_requested' "$root/src/backend.cpp" ||
  fail 'call-scoped cancellation observation missing'
! grep -E '::sigaction\(|::signal\(' "$root/src/backend.cpp" >/dev/null ||
  fail 'ambient signal handler entered backend control'
grep -q 'address_space_limit' "$root/include/libpkgexec-linux/capability.h" ||
  fail 'address-space limit observation missing'
grep -q 'file_size_limit' "$root/include/libpkgexec-linux/capability.h" ||
  fail 'file-size limit observation missing'
grep -q 'open_files_limit' "$root/include/libpkgexec-linux/capability.h" ||
  fail 'open-files limit observation missing'
grep -q 'setup_resource_limits' "$root/src/backend.cpp" ||
  fail 'resource-limit realization is not wired into child setup'
grep -q 'RLIMIT_AS' "$root/src/resource_limits.cpp" ||
  fail 'address-space realization authority missing'
grep -q 'RLIMIT_FSIZE' "$root/src/resource_limits.cpp" ||
  fail 'file-size realization authority missing'
grep -q 'RLIMIT_NOFILE' "$root/src/resource_limits.cpp" ||
  fail 'open-files realization authority missing'
grep -q 'requested{exact, exact}' "$root/src/resource_limits.cpp" ||
  fail 'soft and hard limits are not sealed to the exact value'
grep -q '__NR_setrlimit' "$root/src/process_control.cpp" ||
  fail 'setrlimit mutation seal missing'
grep -q '__NR_prlimit64' "$root/src/process_control.cpp" ||
  fail 'prlimit mutation seal missing'
for payload in network-probe cancellation-probe resource-limit-probe; do
  sed -n "/'$payload'/,/install: false/p" "$root/tests/meson.build" |
    grep -q "override_options: \['b_sanitize=none'\]" ||
    fail "$payload sanitizer exclusion missing"
done
! grep -E 'RLIMIT_CPU|RLIMIT_NPROC' "$root/src/resource_limits.cpp" >/dev/null ||
  fail 'inexact CPU or per-UID process limits entered backend authority'
! grep -R -E '(^|[^[:alnum:]_])ulimit([^[:alnum:]_]|$)'     "$root/src" "$root/tests/integration/host_resource_limit_test.cpp" "$root/tests/privileged/isolated_resource_limit_test.cpp" >/dev/null ||
  fail 'shell limit utility entered resource-limit authority'
! grep -R -E 'pkgman\.conf|Pkgfile|fakeroot|legacy' \
    "$root/include" "$root/src" >/dev/null ||
  fail 'historical compatibility entered the backend'

grep -q 'network_policy::denied' "$root/tests/privileged/isolated_composition_test.cpp" ||
  fail 'isolated composition does not require denied networking'
grep -q 'resource_limits::make' "$root/tests/privileged/isolated_composition_test.cpp" ||
  fail 'isolated composition does not require exact resource limits'
grep -q 'graceful_then_forced' "$root/tests/privileged/isolated_composition_test.cpp" ||
  fail 'isolated composition does not require cancellation'
grep -q 'auto established = request.required_guarantees()' "$root/src/backend.cpp" ||
  fail 'started result evidence is not request-bounded'
! grep -q 'established = profile.guarantees()' "$root/src/backend.cpp" ||
  fail 'backend profile leaked into request-scoped result evidence'

grep -F 'unsupported Linux capability vocabulary' "$root/src/capability.cpp" >/dev/null ||
  fail 'capability observation does not reject unsupported capability vocabulary'
grep -F 'unsupported Linux capability-state vocabulary' "$root/src/capability.cpp" >/dev/null ||
  fail 'capability observation does not reject unsupported state vocabulary'
grep -F 'static_cast<capability_kind>(255)' "$root/tests/unit/capability_value_test.cpp" >/dev/null ||
  fail 'invalid capability vocabulary lacks a direct negative test'
grep -F 'static_cast<capability_state>(255)' "$root/tests/unit/capability_value_test.cpp" >/dev/null ||
  fail 'invalid capability-state vocabulary lacks a direct negative test'
