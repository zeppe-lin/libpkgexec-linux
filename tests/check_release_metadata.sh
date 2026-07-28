#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
root=${1:?source root required}
version=$(sed -n "s/^[[:space:]]*version: '\([^']*\)'.*/\1/p" "$root/meson.build" | head -n 1)
[ "$version" = '0.4.0' ] || {
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
grep -q "libpkgexec >= 1.1.0" "$root/src/meson.build" || {
  echo 'release-metadata: controlled execution authority floor missing' >&2
  exit 1
}
grep -q 'graceful-then-forced cancellation' "$root/HISTORY.md" || {
  echo 'release-metadata: cancellation scope missing' >&2
  exit 1
}
grep -q 'waitid(P_PIDFD)' "$root/HISTORY.md" || {
  echo 'release-metadata: pidfd observation scope missing' >&2
  exit 1
}
