# Maintaining

Release qualification must include strict GCC and Clang builds, the full test
suite, sanitizer execution, standalone public headers, shared and static
consumers, SONAME and direct dependency inspection, generated pkg-config
metadata, manual generation where available, patch hygiene, and independent
mbox replay.

Capability changes require a deliberate ABI and release review. Diagnostic
feature probes must not silently alter the guarantees advertised by an existing
backend identity.
