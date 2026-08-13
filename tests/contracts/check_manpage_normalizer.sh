#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
root=${1:?source root required}
fail(){ echo "manpage-normalizer-contract: $*" >&2; exit 1; }
awk_script=$root/tools/canonicalize-man-roff.awk
[ -s "$awk_script" ] || fail 'normalizer is missing'
input=$(mktemp); first=$(mktemp); second=$(mktemp)
trap 'rm -f "$input" "$first" "$second"' EXIT HUP INT TERM
printf '.EX\n\f[CR]value\f[R] \(bu\n.EE\n' > "$input"
awk -f "$awk_script" "$input" > "$first"
awk -f "$awk_script" "$first" > "$second"
cmp -s "$first" "$second" || fail 'normalizer is not idempotent'
grep -F '\[bu]' "$first" >/dev/null || fail 'two-character escape was not canonicalized'
! grep -F '\f[' "$first" >/dev/null || fail 'example highlighting escape remains'
