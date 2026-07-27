pkgexec_linux_isolated(3)

# NAME

pkgexec_linux_isolated - private Linux root and resource execution contract

# SYNOPSIS

*#include <libpkgexec-linux/backend.h>*

# DESCRIPTION

*isolated_backend::make()* accepts exact *interpreter_binding* values.
*execute()* requires a dedicated root-view directory and exact directory
materializations for every declared resource.

The backend opens roots and resources without following symlinks, clones
mount trees, applies read-only or writable mount attributes, creates a private
mount namespace, attaches resources at their exact logical paths, enters the
root, drops capabilities, and executes the exact interpreter descriptor.

The root tree is read-only. Source and package-input resources must be
read-only. Workspace, package-output, private-temporary, and managed-target
resources must be writable. Root and resource host paths cannot overlap.

The supplied root must contain all runtime material. Nested mounts and host
pseudo-filesystems are not inherited. The backend creates no user, PID, or
network namespace and provides no Landlock, cgroup, limit, or cancellation
guarantee.

If mount realization is unavailable or policy-restricted, the request is
rejected before start.

# SEE ALSO

*libpkgexec-linux*(3), *pkgexec_linux_backend*(3),
*pkgexec_linux_capability*(3), *pkgexec_backend*(3)
