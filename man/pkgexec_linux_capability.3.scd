pkgexec_linux_capability(3)

# NAME

pkgexec_linux_capability - Linux backend capability observations

# SYNOPSIS

*#include <libpkgexec-linux/capability.h>*

# DESCRIPTION

*capability_report::probe()* reports the host supervisor.
*capability_report::probe_isolated()* performs an end-to-end private-root and
resource-mount probe before advertising isolated guarantees.

Observation states are *available*, *unavailable*, and *policy-restricted*.
Namespace, Landlock, cgroup, close-range, pidfd, and mount observations describe
the current host. They do not become execution guarantees unless the selected
backend actually uses them to realize a request.

The isolated backend does not create a user namespace. External delegation may
make mount realization available, but that fact is diagnostic and does not
enter execution identities.

# SEE ALSO

*libpkgexec-linux*(3), *pkgexec_linux_backend*(3),
*pkgexec_linux_isolated*(3), *pkgexec_backend*(3)
