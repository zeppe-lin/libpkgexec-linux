#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
root=$(CDPATH= cd "$(dirname "$0")/.." && pwd)
work=${1:-"$root/.qualification"}
: "${LIBPKGSOURCE_SOURCE:?set LIBPKGSOURCE_SOURCE to the exact qualified libpkgsource tree}"
: "${LIBPKGEXEC_SOURCE:?set LIBPKGEXEC_SOURCE to the exact qualified libpkgexec 2 tree}"
command -v meson >/dev/null 2>&1 || { echo 'meson is required' >&2; exit 2; }
rm -rf "$work"
for entry in 'gcc g++' 'clang clang++'; do
  set -- $entry
  cc=$1
  cxx=$2
  command -v "$cc" >/dev/null 2>&1 || continue
  command -v "$cxx" >/dev/null 2>&1 || continue
  for mode in shared static; do
    name=$(printf '%s-%s' "$cxx" "$mode" | tr + _)
    CC=$cc CXX=$cxx \
      LIBPKGSOURCE_SOURCE=$LIBPKGSOURCE_SOURCE \
      LIBPKGEXEC_SOURCE=$LIBPKGEXEC_SOURCE \
      "$root/ci/configure-and-test.sh" "$work/$name" "$mode"
  done
  name=$(printf '%s-sanitize' "$cxx" | tr + _)
  CC=$cc CXX=$cxx \
    LIBPKGSOURCE_SOURCE=$LIBPKGSOURCE_SOURCE \
    LIBPKGEXEC_SOURCE=$LIBPKGEXEC_SOURCE \
    MESON_SANITIZE=address,undefined \
    "$root/ci/configure-and-test.sh" "$work/$name" shared
 done
