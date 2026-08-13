% PKGEXEC_LINUX_CAPABILITY(3) libpkgexec-linux | Version 0.6.1


# NAME

pkgexec_linux_capability - Linux backend capability observations

# SYNOPSIS

**#include <libpkgexec-linux/capability.h>**

# DESCRIPTION

**capability_report::probe()** reports the host supervisor.
**capability_report::probe_isolated()** performs end-to-end private-root,
resource-mount, denied-network, loopback-only, pidfd-cancellation, and exact
resource-limit realization probes before advertising their guarantees.

Observation states are **available**, **unavailable**, and **policy-restricted**.
Namespace, loopback configuration, Landlock, cgroup, close-range, raw pidfd, and
mount observations describe the current host. They do not become execution
guarantees unless the selected backend actually uses them to realize a request.

A raw successful **pidfd_open**(2) is not sufficient for cancellation. The
cancellation probe creates a private execution session with a descendant,
signals exact members through pidfds, observes the leader through
**waitid(P_PIDFD)**, and verifies that descendants disappear before advertising
the guarantee.

A resource-limit probe installs one representative soft/hard pair, seals later
mutation, proves that an attempted raise is denied, and rereads the unchanged
value. A raw successful **setrlimit**(2) call is not sufficient.

A raw successful namespace syscall is not sufficient for a network guarantee.
The denied probe verifies a private link view with down loopback. The
loopback-only probe additionally configures loopback and completes an internal
round trip.

The isolated backend does not create a user namespace. External delegation may
make mount and network realization available, but that fact is diagnostic and
does not enter execution identities.

# SEE ALSO

**libpkgexec-linux**(3), **pkgexec_linux_backend**(3),
**pkgexec_linux_isolated**(3), **pkgexec_linux_limits**(3),
**pkgexec_backend**(3)
