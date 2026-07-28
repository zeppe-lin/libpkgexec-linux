#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
build_root=${1:?build root required}
pc=$(find "$build_root" -name libpkgexec-linux.pc -type f -print -quit)
if [ -z "$pc" ]; then
  echo 'metadata-test: libpkgexec-linux.pc not found' >&2
  exit 1
fi
fail()
{
  echo "metadata-test: $1" >&2
  echo '--- generated metadata ---' >&2
  cat "$pc" >&2
  exit 1
}
grep -Eq '^Name:[[:space:]]+libpkgexec-linux$' "$pc" || fail 'wrong module name'
grep -Eq '^Version:[[:space:]]+0\.4\.0$' "$pc" || fail 'wrong version'
grep -Eq '^Libs:.*-lpkgexec-linux([[:space:]]|$)' "$pc" || fail 'missing Linux backend library'
grep -Eq '(^|[[:space:],])libpkgexec[[:space:]]*>=[[:space:]]*1\.1\.0([[:space:],]|$)' "$pc" ||
  fail 'missing exact execution authority floor'
