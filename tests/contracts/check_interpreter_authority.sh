#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
root=${1:?source root required}
fail(){ echo "interpreter-authority-contract: $*" >&2; exit 1; }
support=$root/src/support.cpp
backend=$root/src/backend.cpp
test_file=$root/tests/integration/host_admission_test.cpp
for file in "$support" "$backend" "$test_file"; do [ -s "$file" ] || fail "missing ${file#$root/}"; done
grep -F 'O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK' "$support" >/dev/null || fail 'inspection reopen is blocking'
grep -F 'interpreter descriptor is not a regular file' "$support" >/dev/null || fail 'inspection reopen lacks descriptor type validation'
grep -F 'O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK' "$backend" >/dev/null || fail 'execution reopen is blocking'
grep -F 'interpreter bytes changed after admission' "$backend" >/dev/null || fail 'execution reopen lacks exact retained digest refusal'
grep -F 'mkfifo' "$test_file" >/dev/null || fail 'FIFO replacement regression is absent'
grep -F 'alarm(2)' "$test_file" >/dev/null || fail 'FIFO replacement regression is not bounded'
