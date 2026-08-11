#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
root=${1:?source root required}
fail() { echo "ci-contract: $*" >&2; exit 1; }
workflow=$root/.github/workflows/ci.yml
runner=$root/ci/configure-and-test.sh
qualify=$root/ci/qualify.sh
[ -s "$workflow" ] || fail 'hosted CI workflow is absent'
[ -x "$runner" ] || fail 'qualification runner is absent or not executable'
[ -x "$qualify" ] || fail 'local qualification entry point is absent or not executable'
for text in 'GCC shared' 'GCC static' 'Clang shared' 'Clang static' 'GCC release' 'address,undefined'; do
  grep -F "$text" "$workflow" >/dev/null || fail "workflow omits $text"
done
grep -F '9a2a85c85c20bbfa77306f3eb14ccc67ac1e800c' "$workflow" >/dev/null ||
  fail 'workflow does not pin exact libpkgsource 3.0.1 authority'
grep -F 'ref: v2.0.0' "$workflow" >/dev/null || fail 'workflow does not select immutable libpkgexec v2.0.0'
grep -F 'f697d51b8884dc444e11023a4fafe64e9dab0d8a' "$workflow" >/dev/null ||
  fail 'workflow does not verify exact qualified libpkgexec 2.0.0 tree'
for text in 'meson install -C "$build/product"' 'tests/installed/consumer.cpp' 'pkg-config --static --libs libpkgexec-linux' 'LD_LIBRARY_PATH='; do
  grep -F "$text" "$runner" >/dev/null || fail "runner omits installed-product gate: $text"
done
for text in 'configure-and-test.sh' 'shared static' 'MESON_SANITIZE=address,undefined' 'LIBPKGSOURCE_SOURCE' 'LIBPKGEXEC_SOURCE'; do
  grep -F "$text" "$qualify" >/dev/null || fail "local qualification omits release gate: $text"
done
