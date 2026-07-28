# Maintaining

Release qualification must include strict GCC and Clang builds, the full test
suite, sanitizer execution, standalone public headers, shared and static
consumers, SONAME and direct dependency inspection, generated pkg-config
metadata, manual generation where available, patch hygiene, and independent
mbox replay.

Cancellation changes require an end-to-end host pass proving pre-start,
graceful, and forced outcomes. The forced case must include a descendant,
retain the leader unreaped until descendant cleanup is complete, and prove that
signals are delivered through pidfds. A raw syscall probe, numeric PID lookup,
ambient signal handler, or successful leader termination alone is insufficient.

The isolated backend additionally requires both outcomes from its integration
test: an explicit skip on a non-delegated runner and a real pass on a runner
that can create and populate the private mount and network namespaces. The real
pass must prove denied parent reachability, loopback-only internal reachability,
and cancellation composed with denied networking. Never count a skip as
isolation coverage.

Capability changes require deliberate ABI and release review. Diagnostic probes
must not silently alter guarantees advertised by an existing backend identity.
A fallback implementation is acceptable only when it establishes the identical
security contract.


Resource-limit changes require proof of the exact requested soft and hard
values, denial of later mutation, and execution under both host and isolated
backends. Do not infer `resource_limit_exceeded` from an exit code, allocation
failure, `EMFILE`, or signal number alone. CPU milliseconds must not be rounded
to `RLIMIT_CPU`, and process count must not be mapped to per-UID
`RLIMIT_NPROC`. Any new limit mechanism must state its accounting scope and
request-specific admission ceiling.
