# Design

## Authority boundary

`libpkgexec-linux` realizes `libpkgexec::execution_request` values and returns
`libpkgexec::execution_result` evidence. It does not define another program,
request, result, or package-operation model.

```
execution_request + execution_resources
    -> Linux realization
    -> execution_result
```

The backend owns Linux realization and observation only. It does not own source
acquisition, build semantics, lifecycle policy, artifact production,
filesystem application, or installed-state publication.

## Backends

`host_supervisor_backend` retains the 0.1 contract. It creates no mount or
namespace view and accepts only the current `/` root, current credentials,
allowed networking, caller-prepositioned writable resources, empty resource
limits, and disabled cancellation.

`isolated_backend` adds descriptor-oriented filesystem isolation. It accepts
only:

- a dedicated root-view directory other than `/`;
- directory-valued resources whose host paths do not overlap the root or each
  other;
- logical resource destinations that already exist inside the root and do not
  overlap;
- read-only source/build-input/check-input resources;
- writable workspace/output/temporary/managed-target resources;
- the supervisor's current numeric credentials;
- allowed networking;
- empty resource limits and disabled cancellation;
- `no_new_privileges` and an exact inspected interpreter.

Unsupported requests are rejected before program start. No requested guarantee
is silently weakened.

## Descriptor-oriented filesystem realization

The parent opens every supplied root and resource directory once with
`openat2(2)` using no-symlink and no-magic-link resolution. It records source
and destination inode identities, creates detached mount trees with
`open_tree(2)`, and applies read-only, `nosuid`, and `nodev` attributes with
`mount_setattr(2)` before the child starts.

The child creates a private mount namespace, makes propagation private, mounts
a private scratch filesystem, attaches the detached root and resource trees
with `move_mount(2)`, verifies the visible inode identities, enters the exact
root with `chroot(2)`, drops Linux capabilities, and executes the already-opened
interpreter descriptor.

The root tree is read-only. Writable access exists only through explicitly
writable resource mounts. Read-only source and input trees are enforced by the
mount layer. A host resource may not be inside the supplied root, because that
would expose it both at its undeclared original path and at its declared logical
path.

Detached trees are not recursively cloned. Nested mounts and host pseudo-filesystems
are not inherited. `/proc`, `/dev`, `/run`, `/tmp`, runtime loaders, shared
libraries, and other execution material exist only when the supplied root
contains them or the request declares an explicit resource at the required
logical path.

The backend does not create a user namespace. A caller or test environment may
provide delegated mount authority externally, but that delegation is not part
of the execution request or backend identity.

## Interpreter authority

`interpreter_binding::inspect()` resolves the supplied path, rejects a resolved
path with symlink components, verifies a regular executable, hashes its exact
bytes, and derives the Linux interpreter identity from that content identity.
The executable is opened and hashed immediately before execution. Descriptor
execution is probed before it enters the capability profile, and the same open
descriptor is passed to `execveat(2)`, so pathname replacement cannot change
the bytes that are executed.

The POSIX-shell invocation is fixed:

```
INTERPRETER -c PROGRAM pkgexec
```

No shell startup file or inherited shell environment is admitted.

## Closed process state

The child environment is constructed only from the sealed policy: `PATH`,
`HOME`, `LANG`, `LC_ALL`, `TZ`, `TMPDIR`, `PKGEXEC_JOBS`, optional
`SOURCE_DATE_EPOCH`, and exact additional variables admitted by `libpkgexec`.
The parent environment is never copied. The requested umask and working
directory are installed before execution, and inherited descriptors are
closed.

## Supervision and cleanup

Each child leads a private process group. A seccomp filter prevents session,
process-group, and namespace/mount operations that could defeat cleanup or
alter the isolated root after setup. The parent drains stdout and stderr
concurrently, classifies termination, terminates remaining group members, and
verifies that the process group disappears.

For isolated execution the parent also verifies removal of the private scratch
directory. `cleanup_verified` is reported only when both process and filesystem
cleanup succeed. Cleanup failure cannot produce successful evidence.

This is not yet full package-build isolation. Version 0.2 advertises no network
denial, arbitrary credential isolation, PID isolation, Landlock, cgroup,
resource-limit, or cancellation guarantee.

## Capability report

`capability_report::probe()` reports the host supervisor.
`capability_report::probe_isolated()` performs an end-to-end mount realization
probe before advertising root-view, read-only-resource, writable-resource, or
cleanup guarantees. Observations distinguish `available`, `unavailable`, and
`policy-restricted`.

Kernel versions, host paths, external delegation, and diagnostic strings do not
enter backend capability-profile identities.
