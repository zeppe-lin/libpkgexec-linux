pkgexec_linux_backend(3)

# NAME

pkgexec_linux_backend - Linux host-supervisor execution contract

# SYNOPSIS

*#include <libpkgexec-linux/backend.h>*

# DESCRIPTION

*host_supervisor_backend::make()* accepts exact *interpreter_binding* values.
*execute()* admits a request only when its guarantees and concrete resources
satisfy the restricted host-view contract.

The child runs in a private process group. A seccomp filter prevents session,
process-group, namespace, and mount operations needed to escape cleanup. The
parent drains requested streams, classifies termination, terminates remaining
group members, and reports *cleanup_verified* only after the group disappears.

This backend creates no alternate filesystem or network view. See
*pkgexec_linux_isolated*(3) for private mount-view execution.

# SEE ALSO

*libpkgexec-linux*(3), *pkgexec_linux_isolated*(3),
*pkgexec_linux_capability*(3), *pkgexec_backend*(3)
