#!/bin/sh
# SPDX-FileCopyrightText: 2026 Alexandr Savca
# SPDX-License-Identifier: GPL-3.0-or-later
set -eu
root=${1:?source root required}
fail(){ echo "documentation-source-contract: $*" >&2; exit 1; }
for file in README.md DESIGN.md TESTING.md HISTORY.md MAINTAINING.md CONTRIBUTING.md MIGRATION.md docs/manpage-markdown.md; do
  path=$root/$file
  [ -s "$path" ] || fail "missing $file"
  first=$(sed -n '/[^[:space:]]/ { p; q; }' "$path")
  case "$first" in '# '*) ;; *) fail "$file does not begin with an ATX level-one heading" ;; esac
  count=$(grep -c '^# ' "$path" || true)
  [ "$count" -eq 1 ] || fail "$file must contain exactly one ATX level-one heading"
done
if grep -R -n -E --include='*.md' --exclude-dir=.git '^(=+|-+|~+)$' "$root"/*.md "$root"/docs/*.md >/dev/null 2>&1; then fail 'maintained project prose contains Setext headings'; fi
