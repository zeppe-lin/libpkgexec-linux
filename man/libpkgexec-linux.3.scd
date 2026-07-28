libpkgexec-linux(3)

# NAME

libpkgexec-linux - Linux execution backends for libpkgexec

# SYNOPSIS

*#include <libpkgexec-linux/libpkgexec-linux.h>*

# DESCRIPTION

*libpkgexec-linux* implements Linux backends for sealed *libpkgexec* requests.

*host_supervisor_backend* uses the current root and network views, current
credentials, exact interpreter bytes, a closed environment, complete pipe
capture, and verified process-group cleanup.

*isolated_backend* constructs a private mount namespace from one exact dedicated
root-view directory. It clones that root read-only and attaches admitted
read-only or writable directory resources at their declared logical paths.
The root must contain the interpreter and complete runtime closure.

For *allowed* networking the isolated backend preserves the caller's network
namespace. For *denied* networking it creates a private network namespace with
only administratively down loopback. For *loopback-only* networking it creates
the private namespace and brings up only loopback.

Both backends currently require current credentials, empty resource limits,
and disabled cancellation. They provide no user or PID namespace, Landlock, or
cgroup guarantee.

# SEE ALSO

*pkgexec_linux_backend*(3), *pkgexec_linux_isolated*(3),
*pkgexec_linux_capability*(3), *libpkgexec*(3), *pkgexec_semantics*(7)
