#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
root=${1:?source root required}
fail(){ echo "manpage-source-contract: $*" >&2; exit 1; }
for page in libpkgexec-linux.3 pkgexec_linux_backend.3 pkgexec_linux_capability.3 pkgexec_linux_isolated.3 pkgexec_linux_limits.3; do
  source=$root/docs/man/$page.md
  generated=$root/docs/man/generated/$page
  [ -s "$source" ] || fail "missing canonical source: $page.md"
  [ -s "$generated" ] || fail "missing committed generated roff: $page"
  first=$(sed -n '1p' "$source")
  printf '%s\n' "$first" | grep -F '| Version 0.7.1' >/dev/null || fail "wrong manual version title: $page"
  grep -F '# NAME' "$source" >/dev/null || fail "NAME section missing: $page"
done
[ ! -e "$root/man" ] || fail 'legacy root man/ authority remains'
if find "$root" -type f \( -name '*.scd' -o -name '*.scdoc' \) | grep . >/dev/null; then fail 'scdoc manual authority remains'; fi
