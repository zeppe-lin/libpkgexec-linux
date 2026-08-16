#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
root=${1:?source root required}
fail() { echo "release-metadata: $*" >&2; exit 1; }
grep -F "version: '0.7.1'" "$root/meson.build" >/dev/null || fail 'project version is not 0.7.1'
grep -F "soversion: '2'" "$root/src/meson.build" >/dev/null || fail 'SONAME is not 2'
source_block=$(sed -n '/^libpkgsource_dep = dependency(/,/^)/p' "$root/meson.build")
printf '%s\n' "$source_block" | grep -F "  version: ['>=4.0.0', '<5.0.0']," >/dev/null ||
  fail 'source dependency interval is not >=4.0.0,<5.0.0'
block=$(sed -n '/^libpkgexec_dep = dependency(/,/^)/p' "$root/meson.build")
printf '%s\n' "$block" | grep -F "  version: ['>=2.2.0', '<3.0.0']," >/dev/null ||
  fail 'execution dependency interval is not >=2.2.0,<3.0.0'
grep -F '## libpkgexec-linux 0.7.1' "$root/HISTORY.md" >/dev/null || fail '0.7.1 history entry is absent'
grep -F 'libpkgexec-linux.so.2' "$root/MIGRATION.md" >/dev/null || fail 'generation-2 migration is absent'
grep -F 'libpkgexec >= 2.0.0, < 3.0.0' "$root/HISTORY.md" >/dev/null || fail 'history omits exact exec2 interval'
grep -F 'libpkgsource >= 3.0.1, < 4.0.0' "$root/HISTORY.md" >/dev/null || fail 'history omits historical source3 interval'
