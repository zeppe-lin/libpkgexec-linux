pkgexec_linux_backend(3)

# NAME

pkgexec_linux_backend - Linux host-supervisor execution contract

# SYNOPSIS

*#include <libpkgexec-linux/backend.h>*

# DESCRIPTION

*host_supervisor_backend::make()* accepts one or more exact
*interpreter_binding* values. *execute()* admits a request only when its
required guarantees are present in the backend profile and its concrete
resources satisfy the restricted host-view contract.

The child runs in a private process group. A seccomp filter prevents session and
process-group escape and denies direct unshare/setns transitions. The parent drains requested streams,
classifies termination, terminates remaining group members, and reports
*cleanup_verified* only after the group disappears.

The backend is not a filesystem or network sandbox.

# SEE ALSO

*libpkgexec-linux*(3), *pkgexec_linux_capability*(3),
*pkgexec_backend*(3)
