pkgexec_linux_isolated(3)

# NAME

pkgexec_linux_isolated - private Linux filesystem and network execution contract

# SYNOPSIS

*#include <libpkgexec-linux/backend.h>*

# DESCRIPTION

*isolated_backend::make()* accepts exact *interpreter_binding* values.
*execute()* requires a dedicated root-view directory and exact directory
materializations for every declared resource.

The backend opens roots and resources without following symlinks, clones mount
trees, applies read-only or writable mount attributes, creates a private mount
namespace, attaches resources at their exact logical paths, enters the root,
drops capabilities, and executes the exact interpreter descriptor.

The root tree is read-only. Source and package-input resources must be
read-only. Workspace, package-output, private-temporary, and managed-target
resources must be writable. Root and resource host paths cannot overlap.

*network_policy::allowed* preserves the caller's network namespace.
*network_policy::denied* creates a private network namespace whose only link is
administratively down loopback. *network_policy::loopback_only* creates the
same private view and brings only loopback up. The backend invokes no network
configuration program and inherits no host interface or route into either
private view.

The supplied root must contain all runtime material. Nested mounts and host
pseudo-filesystems are not inherited. The backend creates no user or PID
namespace and provides no Landlock, cgroup, limit, or cancellation guarantee.

If filesystem or requested network realization is unavailable or
policy-restricted, the request is rejected before start. There is no fallback
to allowed networking.

# SEE ALSO

*libpkgexec-linux*(3), *pkgexec_linux_backend*(3),
*pkgexec_linux_capability*(3), *pkgexec_backend*(3)
