% LIBPKGEXEC-LINUX(3) libpkgexec-linux | Version 0.7.0


# NAME

libpkgexec-linux - Linux execution backends for libpkgexec

# SYNOPSIS

**#include <libpkgexec-linux/libpkgexec-linux.h>**

# DESCRIPTION

**libpkgexec-linux** implements Linux backends for sealed **libpkgexec** requests.

**host_supervisor_backend** uses the current root and network views, current
credentials, exact interpreter bytes, a closed environment, complete pipe
capture, and verified process-tree cleanup.

**isolated_backend** constructs a private mount namespace from one exact dedicated
root-view directory. It clones that root read-only, **nosuid**, and **nodev**, and
attaches admitted read-only or writable directory resources at their declared
logical paths with **nodev**. The root must contain the interpreter and complete
runtime closure. The backend overlays a private execution-only _/dev_ containing
deterministic _/dev/null_ and imports no ambient host _/dev_.

For **allowed** networking the isolated backend preserves the caller's network
namespace. For **denied** networking it creates a private network namespace with
only administratively down loopback. For **loopback-only** networking it creates
the private namespace and brings up only loopback.

Both backends implement **controlled_execution_backend**. Disabled cancellation
uses the ordinary execution call. Graceful-then-forced cancellation requires
the exact request-bound token. Where the end-to-end pidfd probe succeeds, the
backend creates a private execution session, sends signals through pidfds,
honors the sealed grace period, and verifies descendant cleanup.

Both backends require current credentials. Where the realization probes
succeed, they admit exact address-space, file-size, and open-files limits and
seal them against later mutation. They provide no user or PID namespace,
Landlock, cgroup, CPU-time, or execution-wide process-count guarantee.

# SEE ALSO

**pkgexec_linux_backend**(3), **pkgexec_linux_isolated**(3),
**pkgexec_linux_capability**(3), **pkgexec_linux_limits**(3),
**libpkgexec**(3), **pkgexec_semantics**(7)
