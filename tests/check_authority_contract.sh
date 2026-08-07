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
grep -q 'class host_supervisor_backend' "$root/include/libpkgexec-linux/backend.h" ||
  fail 'host supervisor API missing'
grep -q 'class isolated_backend' "$root/include/libpkgexec-linux/backend.h" ||
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
grep -q 'bool allow_devices' "$root/src/mount_isolation.cpp" ||
  fail 'root/resource device policy is not explicit'
grep -q 'if (allow_devices)' "$root/src/mount_isolation.cpp" ||
  fail 'exact root device policy missing'
grep -q 'attributes.attr_clr |= MOUNT_ATTR_NODEV' "$root/src/mount_isolation.cpp" ||
  fail 'exact root does not clear inherited nodev'
grep -q 'attributes.attr_clr = MOUNT_ATTR_NOEXEC' "$root/src/mount_isolation.cpp" ||
  fail 'root/resource execution inherits ambient noexec'
grep -q 'attributes.attr_set |= MOUNT_ATTR_NODEV' "$root/src/mount_isolation.cpp" ||
  fail 'declared resources are not sealed nodev'
grep -q 'root.get(), pkgexec::resource_access::read_only, true' \
    "$root/src/mount_isolation.cpp" ||
  fail 'exact root device semantics are not preserved explicitly'
grep -q 'root device execution' "$root/tests/isolated_test.cpp" ||
  fail 'exact root device semantics lack runtime qualification'
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
! grep -R -E '(^|[^[:alnum:]_])ulimit([^[:alnum:]_]|$)'     "$root/src" "$root/tests/resource_limit_test.cpp" >/dev/null ||
  fail 'shell limit utility entered resource-limit authority'
! grep -R -E 'pkgman\.conf|Pkgfile|fakeroot|legacy' \
    "$root/include" "$root/src" >/dev/null ||
  fail 'historical compatibility entered the backend'
