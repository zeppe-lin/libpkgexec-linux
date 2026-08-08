# Test fixtures

This directory contains deterministic test-side authority and payload material.

- `host.h` constructs ordinary current-root execution requests/resources.
- `isolated.h` constructs dedicated-root execution requests/resources and owns
  temporary isolated trees.
- `runtime_root.h` copies an executable's exact ELF interpreter and dynamic
  runtime closure into a synthetic execution root.
- `payloads/` contains compiled programs executed inside sealed requests.

Fixtures construct inputs only. Assertions and result interpretation belong in
`tests/support/`; behavioral scenarios belong in `tests/integration/` or
`tests/privileged/`.
