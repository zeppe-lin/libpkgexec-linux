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
grep -q 'does not create namespaces or mounts' "$root/include/libpkgexec-linux/backend.h" ||
  fail 'restricted resource boundary missing'
grep -q 'networking' "$root/include/libpkgexec-linux/backend.h" ||
  fail 'network refusal boundary missing'
grep -q 'process_group_containment' "$root/include/libpkgexec-linux/capability.h" ||
  fail 'cleanup containment observation missing'
! grep -R -E 'pkgman\.conf|Pkgfile|fakeroot|legacy' \
    "$root/include" "$root/src" >/dev/null ||
  fail 'historical compatibility entered the backend'
