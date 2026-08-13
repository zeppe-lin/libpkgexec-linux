#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
root=${1:?source root required}
fail(){ echo "repository-contract: $*" >&2; exit 1; }
for f in README.md DESIGN.md TESTING.md HISTORY.md MAINTAINING.md CONTRIBUTING.md MIGRATION.md meson.build meson.options docs/man/meson.build docs/manpage-markdown.md tools/update-man-pages.sh tools/canonicalize-man-roff.awk; do
  [ -s "$root/$f" ] || fail "missing $f"
done
[ ! -e "$root/meson_options.txt" ] || fail 'legacy meson_options.txt remains'
[ ! -e "$root/man" ] || fail 'legacy root man/ authority remains'
if find "$root" -type f \( -name '*.scd' -o -name '*.scdoc' \) | grep . >/dev/null; then fail 'scdoc manual authority remains'; fi
grep -F "subdir('docs/man')" "$root/meson.build" >/dev/null || fail 'docs/man is not the Meson manual authority'
grep -F "input: 'generated/' + page" "$root/docs/man/meson.build" >/dev/null || fail 'ordinary man installation does not use committed roff'
grep -F "'update-man-pages'" "$root/docs/man/meson.build" >/dev/null || fail 'update-man-pages target is absent'
grep -F "'check-man-pages'" "$root/docs/man/meson.build" >/dev/null || fail 'check-man-pages target is absent'
[ -x "$root/tools/update-man-pages.sh" ] || fail 'update-man-pages.sh is not executable'
if grep -R -F 'scdoc' "$root/meson.build" "$root/meson.options" "$root/.github/workflows" "$root/ci" 2>/dev/null | grep . >/dev/null; then fail 'active build/CI still depends on scdoc'; fi
