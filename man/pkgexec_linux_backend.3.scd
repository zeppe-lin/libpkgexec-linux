pkgexec_linux_backend(3)

# NAME

pkgexec_linux_backend - Linux host-supervisor execution contract

# SYNOPSIS

*#include <libpkgexec-linux/backend.h>*

# DESCRIPTION

*host_supervisor_backend::make()* accepts exact *interpreter_binding* values.
The ordinary *execute()* overload admits only disabled cancellation. The
controlled overload additionally requires the exact request-bound
*cancellation_token*.

The child enters a private session and process group before the final program
start gate. A seccomp filter prevents descendants from changing session,
process-group, namespace, or mount state. The parent drains requested streams,
classifies termination, terminates remaining group members, and reports
*cleanup_verified* only after the group disappears.

Cancellation observed before release of the start gate prevents execution.
After start, the backend sends *SIGTERM* through pidfds, waits the sealed grace
period, and escalates to *SIGKILL*. The leader remains unreaped while descendant
cleanup is verified. No ambient signal handler or numeric-PID cancellation
fallback is used.

This backend creates no alternate filesystem or network view. It may realize
exact address-space, file-size, and open-files limits before the final start
gate and seals them against later mutation. See *pkgexec_linux_limits*(3) for
the limit contract and *pkgexec_linux_isolated*(3) for private mount-view
execution.

# SEE ALSO

*libpkgexec-linux*(3), *pkgexec_linux_isolated*(3),
*pkgexec_linux_capability*(3), *pkgexec_linux_limits*(3),
*pkgexec_backend*(3)
