#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
root=${1:?source root required}
fail(){ echo "test-layout-contract: $*" >&2; exit 1; }
meson=$root/tests/meson.build
for directory in contracts fixtures header installed integration privileged support unit; do
  [ -d "$root/tests/$directory" ] || {
    echo "test-layout-contract: missing tests/$directory" >&2
    exit 1
  }
done
for suite in unit integration integration-privileged header contract; do
  grep -F "suite: '$suite'" "$meson" >/dev/null || {
    echo "test-layout-contract: missing $suite suite" >&2
    exit 1
  }
done
for obsolete in \
  tests/backend_test.cpp tests/cancellation_test.cpp tests/capability_test.cpp \
  tests/interpreter_test.cpp tests/isolated_test.cpp tests/resource_limit_test.cpp \
  tests/fixture.h tests/runtime_fixture.h tests/test.h; do
  [ ! -e "$root/$obsolete" ] || {
    echo "test-layout-contract: obsolete flat test remains: $obsolete" >&2
    exit 1
  }
done
grep -F "override_options: ['b_sanitize=none']" "$meson" >/dev/null || {
  echo 'test-layout-contract: isolated payloads must remain outside sanitizer runtime' >&2
  exit 1
}
for product_file in \
  "$root/tests/contracts/abi_layout_test.cpp" \
  "$root/tests/contracts/check_abi_surface.sh" \
  "$root/tests/contracts/check_dependency_abi.sh" \
  "$root/tests/contracts/check_abi_contract.sh" \
  "$root/tests/contracts/check_ci_contract.sh" \
  "$root/tests/installed/consumer.cpp"; do
  [ -s "$product_file" ] || {
    echo "test-layout-contract: missing release-product qualification: $product_file" >&2
    exit 1
  }
done
for registration in "'abi-layout'" "'abi-surface'" "'dependency-abi'" "'abi-contract'" "'ci-contract'"; do
  grep -F "$registration" "$meson" >/dev/null || {
    echo "test-layout-contract: Meson omits release-product qualification: $registration" >&2
    exit 1
  }
done
grep -F "'export.h'" "$meson" >/dev/null || {
  echo 'test-layout-contract: export header lacks standalone compilation' >&2
  exit 1
}
grep -F 'tests/installed/consumer.cpp' "$root/ci/configure-and-test.sh" >/dev/null || {
  echo 'test-layout-contract: installed consumer is not part of release qualification' >&2
  exit 1
}

for contract in "$root"/tests/contracts/check_*.sh; do
  name=${contract##*/check_}
  name=${name%.sh}
  case $name in
    abi_surface|dependency_abi|pkgconfig_metadata|manpage_generated) continue ;;
  esac
  if ! grep -F "'$name'" "$root/tests/meson.build" >/dev/null &&
     ! grep -F "check_${name}.sh" "$root/tests/meson.build" >/dev/null; then
    fail "unregistered contract: check_${name}.sh"
  fi
done
