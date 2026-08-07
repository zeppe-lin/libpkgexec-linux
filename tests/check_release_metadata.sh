#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
root=${1:?source root required}
version=$(sed -n "s/^[[:space:]]*version: '\([^']*\)'.*/\1/p" "$root/meson.build" | head -n 1)
[ "$version" = '0.5.1' ] || {
  echo "release-metadata: unexpected project version $version" >&2
  exit 1
}
grep -q "## libpkgexec-linux $version" "$root/HISTORY.md" || {
  echo "release-metadata: HISTORY omits $version" >&2
  exit 1
}
grep -q "soversion: '1'" "$root/src/meson.build" || {
  echo 'release-metadata: SONAME 1 contract missing' >&2
  exit 1
}
grep -A8 "libpkgexec_dep = dependency(" "$root/meson.build" |
  grep -q "version: '>=1.2.0'" || {
  echo 'release-metadata: exact resource-limit authority floor missing' >&2
  exit 1
}
grep -q 'RLIMIT_AS' "$root/HISTORY.md" || {
  echo 'release-metadata: address-space limit scope missing' >&2
  exit 1
}
grep -q 'RLIMIT_FSIZE' "$root/HISTORY.md" || {
  echo 'release-metadata: file-size limit scope missing' >&2
  exit 1
}
grep -q 'RLIMIT_NOFILE' "$root/HISTORY.md" || {
  echo 'release-metadata: open-files limit scope missing' >&2
  exit 1
}
grep -q 'SONAME remains 1' "$root/HISTORY.md" || {
  echo 'release-metadata: ABI continuity statement missing' >&2
  exit 1
}
grep -q 'exact missing guarantees' "$root/HISTORY.md" || {
  echo 'release-metadata: unsupported-guarantee diagnostic scope missing' >&2
  exit 1
}
