# Contributing to libpkgexec-linux

Keep Linux mechanisms in this repository and semantic execution authority in
`libpkgexec`.

A backend change must state which guarantee it establishes, how admission
rejects unsupported requests, which evidence proves completion, and what
cleanup is verified. Cancellation changes must distinguish requested policy,
call-scoped control, signal delivery, natural completion, and descendant
cleanup. Resource-limit changes must distinguish exact realization from causal
termination evidence and must seal limits against later mutation. Never replace
a failed guarantee with a warning.

Use C++17, GPL-3.0-or-later SPDX headers, focused commits, strict warnings, and
model plus integration tests. Canonical Markdown manual syntax and generated-roff freshness are required.
