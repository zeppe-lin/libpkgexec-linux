#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu

root=$1
meson="$root/tests/meson.build"

for directory in contracts fixtures header integration privileged support unit; do
  test -d "$root/tests/$directory" || {
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
  tests/backend_test.cpp \
  tests/cancellation_test.cpp \
  tests/capability_test.cpp \
  tests/interpreter_test.cpp \
  tests/isolated_test.cpp \
  tests/resource_limit_test.cpp \
  tests/fixture.h \
  tests/runtime_fixture.h \
  tests/test.h; do
  test ! -e "$root/$obsolete" || {
    echo "test-layout-contract: obsolete flat test remains: $obsolete" >&2
    exit 1
  }
done

grep -F "override_options: ['b_sanitize=none']" "$meson" >/dev/null || {
  echo "test-layout-contract: isolated payloads must remain outside sanitizer runtime" >&2
  exit 1
}

echo "test-layout-contract: ok"
