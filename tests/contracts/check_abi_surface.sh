#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
nm_tool=${1:?nm required}
library=${2:?shared library required}
manifest=${3:?ABI manifest required}
fail() { echo "abi-surface-test: $*" >&2; exit 1; }
[ -s "$manifest" ] || fail 'reviewed ELF ABI manifest is absent'
raw=$($nm_tool -D --defined-only --format=posix "$library") ||
  fail 'cannot inspect shared library'
expected=$(mktemp)
actual=$(mktemp)
trap 'rm -f "$expected" "$actual"' EXIT HUP INT TERM
sed -n '/^_Z[A-Za-z0-9_]*$/p' "$manifest" | LC_ALL=C sort -u > "$expected"
printf '%s\n' "$raw" |
  awk '$2 != "A" { symbol=$1; sub(/@.*/, "", symbol); if (symbol ~ /^_Z/) print symbol }' |
  LC_ALL=C sort -u > "$actual"
if ! cmp -s "$expected" "$actual"; then
  diff -u "$expected" "$actual" || true
  fail 'dynamic symbol set differs from reviewed ELF ABI manifest'
fi
