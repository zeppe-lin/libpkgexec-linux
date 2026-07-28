# Maintaining

Release qualification must include strict GCC and Clang builds, the full test
suite, sanitizer execution, standalone public headers, shared and static
consumers, SONAME and direct dependency inspection, generated pkg-config
metadata, manual generation where available, patch hygiene, and independent
mbox replay.

The isolated backend additionally requires both outcomes from its integration
test: an explicit skip on a non-delegated runner and a real pass on a runner
that can create and populate the private mount and network namespaces. The real
pass must prove denied parent reachability and loopback-only internal
reachability. Never count a skip as isolation coverage.

Capability changes require deliberate ABI and release review. Diagnostic probes
must not silently alter guarantees advertised by an existing backend identity.
A fallback implementation is acceptable only when it establishes the identical
security contract.
