# libpkgexec-linux manual-page authority

The files under `docs/man/*.md` are the sole authored manual-page source.
`docs/man/generated/*` contains deterministic committed roff derived with Pandoc
3.1 through 3.x and `tools/canonicalize-man-roff.awk`.

Ordinary builds install the committed roff and do not require Pandoc. Maintainers
use `update-man-pages` to regenerate it and `check-man-pages` to verify that the
committed derivative is current. Generated roff is not an independent source of
truth.
