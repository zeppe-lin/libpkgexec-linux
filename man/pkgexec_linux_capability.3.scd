pkgexec_linux_capability(3)

# NAME

pkgexec_linux_capability - Linux backend capability observations

# SYNOPSIS

*#include <libpkgexec-linux/capability.h>*

# DESCRIPTION

*capability_report::probe()* returns the sealed backend capability profile and
diagnostic Linux feature observations. Observation states are *available*,
*unavailable*, and *policy-restricted*.

Namespace, Landlock, cgroup, close-range, and pidfd observations describe the
current host. They do not become execution guarantees unless the backend
actually uses them to realize a request.

# SEE ALSO

*libpkgexec-linux*(3), *pkgexec_linux_backend*(3),
*pkgexec_backend*(3)
