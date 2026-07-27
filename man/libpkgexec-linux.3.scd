libpkgexec-linux(3)

# NAME

libpkgexec-linux - restricted Linux backend for libpkgexec

# SYNOPSIS

*#include <libpkgexec-linux/libpkgexec-linux.h>*

# DESCRIPTION

*libpkgexec-linux* implements a Linux execution backend for sealed
*libpkgexec* requests. Version 0.1 provides *host_supervisor_backend*, which
uses the current root view and credentials, exact inspected interpreter bytes,
a closed environment, complete pipe capture, and verified process-group
cleanup.

The backend accepts only writable resources already present at their logical
paths. It rejects requests for read-only mounts, network isolation, arbitrary
credentials, resource limits, cancellation, namespaces, Landlock, or cgroups.

# SEE ALSO

*pkgexec_linux_backend*(3), *pkgexec_linux_capability*(3),
*libpkgexec*(3), *pkgexec_semantics*(7)
