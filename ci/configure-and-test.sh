#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
[ "$#" -ge 2 ] || { echo 'usage: configure-and-test.sh BUILD-DIR {shared|static}' >&2; exit 2; }
build_dir=$1
link_mode=$2
shift 2
case $link_mode in shared|static) ;; *) exit 2 ;; esac
root=$(CDPATH= cd "$(dirname "$0")/.." && pwd)
case $build_dir in /*) build=$build_dir ;; *) build=$(pwd)/$build_dir ;; esac
source_dir=${LIBPKGSOURCE_SOURCE:?set LIBPKGSOURCE_SOURCE}
exec_dir=${LIBPKGEXEC_SOURCE:?set LIBPKGEXEC_SOURCE}
dependency_prefix=$build/dependencies
rm -rf "$build"
mkdir -p "$build"
meson setup "$build/libpkgsource" "$source_dir" \
  --wrap-mode=nofallback --fatal-meson-warnings \
  --prefix="$dependency_prefix" --libdir=lib \
  --buildtype="${MESON_BUILDTYPE:-debug}" \
  -Ddefault_library="$link_mode" -Dlink_mode="$link_mode" \
  -Dtests=disabled -Dman_pages=disabled -Dwerror=true \
  ${MESON_SANITIZE:+-Db_sanitize="$MESON_SANITIZE"} \
  ${MESON_SANITIZE:+-Db_lundef=false}
meson compile -C "$build/libpkgsource"
meson install -C "$build/libpkgsource"
export PKG_CONFIG_PATH="$dependency_prefix/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
export LD_LIBRARY_PATH="$dependency_prefix/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
meson setup "$build/libpkgexec" "$exec_dir" \
  --wrap-mode=nofallback --fatal-meson-warnings \
  --prefix="$dependency_prefix" --libdir=lib \
  --buildtype="${MESON_BUILDTYPE:-debug}" \
  -Ddefault_library="$link_mode" -Dlink_mode="$link_mode" \
  -Dtests=disabled -Dman_pages=disabled -Dwerror=true \
  ${MESON_SANITIZE:+-Db_sanitize="$MESON_SANITIZE"} \
  ${MESON_SANITIZE:+-Db_lundef=false}
meson compile -C "$build/libpkgexec"
meson install -C "$build/libpkgexec"
meson setup "$build/product" "$root" \
  --wrap-mode=nofallback --fatal-meson-warnings \
  --prefix="$build/install" --libdir=lib \
  --buildtype="${MESON_BUILDTYPE:-debug}" \
  -Ddefault_library="$link_mode" -Dlink_mode="$link_mode" \
  -Dtests=enabled -Dman_pages=enabled -Dwerror=true \
  ${MESON_SANITIZE:+-Db_sanitize="$MESON_SANITIZE"} \
  ${MESON_SANITIZE:+-Db_lundef=false} "$@"
meson compile -C "$build/product"
meson test -C "$build/product" --no-rebuild --print-errorlogs
meson install -C "$build/product"
consumer="$root/tests/installed/consumer.cpp"
consumer_bin="$build/installed-consumer"
consumer_pc="$build/install/lib/pkgconfig:$dependency_prefix/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
flags=$(PKG_CONFIG_PATH="$consumer_pc" pkg-config --cflags libpkgexec-linux)
if [ "$link_mode" = static ]; then
  libs=$(PKG_CONFIG_PATH="$consumer_pc" pkg-config --static --libs libpkgexec-linux)
else
  libs=$(PKG_CONFIG_PATH="$consumer_pc" pkg-config --libs libpkgexec-linux)
fi
sanitize=
[ -z "${MESON_SANITIZE:-}" ] || sanitize="-fsanitize=$MESON_SANITIZE"
# shellcheck disable=SC2086
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Wpedantic -Werror \
  $sanitize $flags "$consumer" -o "$consumer_bin" $libs
interpreter=$(command -v sh)
LD_LIBRARY_PATH="$build/install/lib:$dependency_prefix/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
  "$consumer_bin" "$interpreter"
