#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
root=${1:?source root required}
version=$(sed -n "s/^[[:space:]]*version: '\([^']*\)'.*/\1/p" "$root/meson.build" | head -n 1)
[ "$version" = '0.3.0' ] || {
  echo "release-metadata: unexpected project version $version" >&2
  exit 1
}
grep -q "## libpkgexec-linux $version" "$root/HISTORY.md" || {
  echo "release-metadata: HISTORY omits $version" >&2
  exit 1
}
grep -q "soversion: '0'" "$root/src/meson.build" || {
  echo 'release-metadata: SONAME 0 contract missing' >&2
  exit 1
}
grep -q "libpkgexec >= 1.0.0" "$root/src/meson.build" || {
  echo 'release-metadata: execution authority floor missing' >&2
  exit 1
}
grep -q 'network_policy::denied' "$root/HISTORY.md" || {
  echo 'release-metadata: denied network scope missing' >&2
  exit 1
}
grep -q 'network_policy::loopback_only' "$root/HISTORY.md" || {
  echo 'release-metadata: loopback network scope missing' >&2
  exit 1
}
