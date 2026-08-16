% PKGEXEC_LINUX_ISOLATED(3) libpkgexec-linux | Version 0.7.0


# NAME

pkgexec_linux_isolated - private Linux filesystem and network execution contract

# SYNOPSIS

**#include <libpkgexec-linux/backend.h>**

# DESCRIPTION

**isolated_backend::make()** accepts exact **interpreter_binding** values.
**execute()** requires a dedicated root-view directory and exact directory
materializations for every declared resource.

The backend opens roots and resources without following symlinks and retains
them as exact `O_PATH` admission descriptors. Before entering its private mount
namespace, each child creates fresh detached mount trees from those descriptors
and seals the child-owned realization mounts. After namespace creation it
attaches the mounts at their exact logical paths. Retained admission descriptors
are never attached or consumed; realization neither copies a source from another
mount namespace nor requires cloning an already detached anonymous mount.
The exact root is read-only and `nosuid`; declared resources receive
`nosuid,nodev` plus read-only or writable attributes. The child then enters the
root, drops capabilities, and executes the exact interpreter descriptor.

The root tree is read-only. Source and package-input resources must be
read-only. Workspace, package-output, private-temporary, and managed-target
resources must be writable. Root and resource host paths cannot overlap.

**network_policy::allowed** preserves the caller's network namespace.
**network_policy::denied** creates a private network namespace whose only link is
administratively down loopback. **network_policy::loopback_only** creates the
same private view and brings only loopback up. The backend invokes no network
configuration program and inherits no host interface or route into either
private view.

The controlled execution overload composes the same isolated root and network
view with request-bound pidfd cancellation. Cancellation before the final start
gate prevents program execution. Started cancellation preserves complete stream
capture and requires both process-tree and scratch cleanup verification.

The supplied root must contain its interpreter and runtime closure. Persistent
device nodes in the exact root are not activated because the root and declared
resources are `nodev`. The backend overlays a private execution-only `/dev`
containing deterministic `/dev/null` and imports no ambient host `/dev`. Nested
mounts and host pseudo-filesystems are not inherited. Exact address-space, file-size, and
open-files limits compose with the private filesystem and network views and are
sealed before the final start gate. The backend creates no user or PID namespace
and provides no Landlock, cgroup, arbitrary credential, CPU-time, or
execution-wide process-count guarantee.

If filesystem, network, or cancellation realization is unavailable or
policy-restricted, the request is rejected before start. There is no fallback
to a weaker execution policy.

# SEE ALSO

**libpkgexec-linux**(3), **pkgexec_linux_backend**(3),
**pkgexec_linux_capability**(3), **pkgexec_linux_limits**(3),
**pkgexec_backend**(3)
