#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
root=${1:?source root required}
fail() { echo "abi-contract: $*" >&2; exit 1; }
manifest=$root/abi/libpkgexec-linux.exports
[ -s "$manifest" ] || fail 'reviewed ELF ABI manifest is absent'
count=$(sed -n '/^_Z[A-Za-z0-9_]*$/p' "$manifest" | wc -l)
[ "$count" -eq 50 ] || fail "reviewed ELF ABI manifest contains $count symbols, expected 50"
[ "$(LC_ALL=C sort -u "$manifest" | wc -l)" -eq 50 ] ||
  fail 'reviewed ELF ABI manifest contains duplicate symbols'
! grep -E '^_ZNSt|^_ZN9__gnu_cxx|^_ZSt' "$manifest" >/dev/null ||
  fail 'standard-library implementation symbol entered public ABI manifest'
demangled=$(mktemp)
trap 'rm -f "$demangled"' EXIT HUP INT TERM
c++filt < "$manifest" > "$demangled"
! grep -F 'pkgexec_linux::detail::' "$demangled" >/dev/null ||
  fail 'private detail namespace entered public ABI manifest'
for class in capability_report interpreter_binding host_supervisor_backend isolated_backend; do
  ! grep -F "pkgexec_linux::$class::$class(" "$demangled" >/dev/null ||
    fail "private $class constructor entered public ABI manifest"
done
for required in \
  '_ZTIN13pkgexec_linux5errorE' \
  '_ZTVN13pkgexec_linux23host_supervisor_backendE' \
  '_ZTVN13pkgexec_linux16isolated_backendE'; do
  grep -Fx "$required" "$manifest" >/dev/null ||
    fail "required public ABI symbol is absent: $required"
done
grep -F "soversion: '2'" "$root/src/meson.build" >/dev/null ||
  fail 'SONAME generation is not 2'
grep -F -- '--version-script=' "$root/src/meson.build" >/dev/null ||
  fail 'reviewed ELF export manifest is not linked'
grep -F '../abi/libpkgexec-linux.exports' "$root/src/meson.build" >/dev/null ||
  fail 'Meson does not consume reviewed ABI manifest'
