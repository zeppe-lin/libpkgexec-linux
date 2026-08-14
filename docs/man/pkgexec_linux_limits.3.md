% PKGEXEC_LINUX_LIMITS(3) libpkgexec-linux | Version 0.6.2


# NAME

pkgexec_linux_limits - exact Linux resource-limit realization

# SYNOPSIS

**#include <libpkgexec-linux/backend.h>**

# DESCRIPTION

Both Linux backends may realize the native **address_space_bytes**,
**file_size_bytes**, and **open_files** limits through **RLIMIT_AS**, **RLIMIT_FSIZE**,
and **RLIMIT_NOFILE**.

For every admitted kind, the requested finite value becomes both the soft and
hard Linux limit before the final program-start gate. The inherited hard ceiling
is never raised. A value equal to **RLIM_INFINITY**, not representable by
**rlim_t**, or above the inherited hard ceiling fails before start as
**resource_admission_failed**.

After installing the limits, seccomp containment denies mutating **setrlimit**(2)
and **prlimit64**(2) calls. Read-only **prlimit64**(2) inspection remains available.
Descendants inherit both the exact limits and the mutation seal.

Capability reporting is proof-based. The aggregate **resource_limits** guarantee
is combined only with exact kind guarantees whose child probe can install a
representative soft/hard pair, seal mutation, prove that an attempted raise is
denied, and reread the unchanged value.

**cpu_time_milliseconds** is unsupported because **RLIMIT_CPU** has whole-second
granularity. **process_count** is unsupported because **RLIMIT_NPROC** is scoped to
a real UID rather than one execution tree.

A signal observed while a limit is active does not prove that the limit caused
termination. In particular, **SIGXFSZ** remains ordinary signal-termination
evidence. The established file-size guarantee is retained, but the backend does
not synthesize **resource_limit_exceeded** evidence from a signal number alone.

# SEE ALSO

**libpkgexec-linux**(3), **pkgexec_linux_backend**(3),
**pkgexec_linux_isolated**(3), **pkgexec_linux_capability**(3),
**pkgexec_result**(3)
