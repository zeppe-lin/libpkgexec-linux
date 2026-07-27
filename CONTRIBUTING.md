# Contributing

Keep Linux mechanisms in this repository and semantic execution authority in
`libpkgexec`.

A backend change must state which guarantee it establishes, how admission
rejects unsupported requests, which evidence proves completion, and what
cleanup is verified. Never replace a failed guarantee with a warning.

Use C++17, GPL-3.0-or-later SPDX headers, focused commits, strict warnings, and
model plus integration tests. Correct scdoc continuation syntax is required.
