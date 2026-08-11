# Design

## Authority boundary

`libpkgexec-linux` realizes `libpkgexec::execution_request` values and returns
`libpkgexec::execution_result` evidence. It does not define another program,
request, result, cancellation, or package-operation model.

```
execution_request + execution_resources [+ cancellation_token]
    -> Linux realization
    -> execution_result
```

The backend owns Linux realization and observation only. It does not own source
acquisition, build semantics, lifecycle policy, artifact production,
filesystem application, or installed-state publication.

## Backends

`host_supervisor_backend` creates no mount or network namespace view. It accepts
only the current `/` root, current credentials, allowed networking,
caller-prepositioned writable resources, supported exact resource limits, and
an exact inspected interpreter.

`isolated_backend` adds descriptor-oriented filesystem and network isolation.
It accepts only:

- a dedicated root-view directory other than `/`;
- directory-valued resources whose host paths do not overlap the root or each
  other;
- logical resource destinations that already exist inside the root and do not
  overlap;
- read-only source/build-input/check-input resources;
- writable workspace/output/temporary/managed-target resources;
- the supervisor's current numeric credentials;
- allowed, denied, or loopback-only networking;
- exact address-space, file-size, and open-files limits when probed;
- `no_new_privileges` and an exact inspected interpreter.

Both backends accept disabled cancellation through the ordinary backend call.
A request with graceful-then-forced cancellation must enter through the
`libpkgexec` controlled-execution call with its exact request-bound token, and
is admitted only when the capability profile includes cancellation.

Unsupported requests are rejected before program start. No requested guarantee
is silently weakened.

## Descriptor-oriented filesystem realization

The parent opens every supplied root and resource directory once with
`openat2(2)` using no-symlink and no-magic-link resolution. It records source
and destination inode identities, creates detached mount trees with
`open_tree(2)` before the child starts. The exact root clone is made read-only
and `nosuid`, clears inherited `noexec`, and preserves device-node semantics already authorized by that root view by
clearing inherited `nodev`. Every separately declared resource tree is made
`nosuid` and `nodev`, clears inherited `noexec`, and receives its exact read-only
or writable attribute through `mount_setattr(2)`. Execution permission therefore
comes from admitted file modes and execution policy rather than the incidental
mount flags of the host path that stores a root or resource tree.

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

Detached trees are not recursively cloned. Nested mounts and host
pseudo-filesystems are not inherited. `/proc`, `/dev`, `/run`, `/tmp`, runtime
loaders, shared libraries, and other execution material exist only when the
supplied root contains them or the request declares an explicit resource at the
required logical path. Device nodes already present in the exact root remain
usable; the backend creates none and never imports ambient host `/dev`. Device
nodes from separately declared resource mounts remain disabled by `nodev`.

The backend does not create a user namespace. A caller or test environment may
provide delegated mount authority externally, but that delegation is not part
of the execution request or backend identity.

## Network isolation

Allowed networking preserves the caller's network namespace and carries no
network-isolation guarantee.

Denied and loopback-only policies create a fresh network namespace. The backend
uses rtnetlink directly to inspect the new link view and to set the loopback
administrative state. Denied policy verifies that loopback is the only link and
is down. Loopback-only verifies that loopback is the only link, is up, and can
complete an internal round trip. Neither view inherits a host interface or
route.

Network setup occurs after private-root realization and before capability
removal and seccomp containment. Once execution begins, descendants have no
capability to reconfigure the namespace and cannot create or join another
namespace. Unix-domain sockets remain governed by the filesystem resource view;
network denial means no usable IPv4 or IPv6 path, not a ban on every socket
domain.

The capability profile includes a network guarantee only after the complete
policy realization succeeds. A failed or policy-restricted probe causes
pre-start refusal, never fallback to allowed networking.

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


## Resource-limit realization

Both backends can realize `address_space_bytes`, `file_size_bytes`, and
`open_files` through `RLIMIT_AS`, `RLIMIT_FSIZE`, and `RLIMIT_NOFILE`.
Realization is exact: the requested finite value becomes both `rlim_cur` and
`rlim_max`. The inherited hard ceiling is never raised. A value equal to
`RLIM_INFINITY`, not representable by `rlim_t`, or above the inherited hard
ceiling is rejected before the final program-start gate as resource admission
failure.

Limits are applied after filesystem, network, credential, and stream setup but
before the final gate. Seccomp containment then denies mutating `setrlimit(2)`
and `prlimit64(2)` calls while allowing read-only `prlimit64(2)` inspection.
This keeps the exact values stable even when the supervisor itself is
privileged. Descendants inherit both the limits and the mutation seal.

The capability profile contains the aggregate `resource_limits` guarantee only
with the exact kinds proved by end-to-end child probes. Each probe applies one
representative soft/hard pair, installs mutation sealing, proves that raising
it is denied, and rereads the unchanged value. A request-specific value can
still fail admission when the caller's inherited hard ceiling is lower than the
sealed request.

`cpu_time_milliseconds` is not mapped to `RLIMIT_CPU`: Linux exposes whole
seconds and rounding would alter the sealed contract. `process_count` is not
mapped to `RLIMIT_NPROC`: that limit is per real UID, not per execution tree.
Those guarantees remain absent pending an exact accounting authority, likely a
delegated cgroup contract.

A signal observed under an active limit is not, by itself, proof that the limit
caused termination. In particular, `SIGXFSZ` remains ordinary signal
termination evidence. The backend retains the established file-size guarantee
but does not manufacture `resource_limit_exceeded` causality from the signal
number alone.

## Supervision and cancellation

Before containment, the child calls `setsid(2)`. The execution therefore owns a
private session and process group whose numeric group cannot be joined by an
unrelated process in the orchestrator's session. A seccomp filter then prevents
descendants from changing session or process-group membership and from changing
namespace or mount state.

The child performs all fallible setup behind a final start gate. Cancellation
observed before the gate is released prevents program start and produces
not-started cancellation evidence only after cleanup is verified. Once the gate
is released, an execution is classified as cancelled only when the token was
requested and `pidfd_send_signal(2)` actually admitted a signal for the exact
leader. A process already observed to have completed naturally is never
reclassified as cancelled.

The supervisor retains the leader as an unreaped waitable process while cleanup
is in progress. This prevents reuse of the execution group identity. It scans
the supervisor's real `/proc` view for members of that closed group, opens a
pidfd for each candidate, rechecks membership after opening the pidfd, and sends
signals only through pidfds. The supplied execution root and any `/proc` it may
contain do not participate.

Cancellation first sends `SIGTERM` and never escalates before the sealed
grace period has elapsed. It then repeatedly sends `SIGKILL` through pidfds
until all non-leader group
members disappear or cleanup fails. The leader is reaped only after descendant
cleanup has been proved. Capture and isolated scratch cleanup remain mandatory.
There is no ambient signal authority, global flag, process-name matching, or
numeric-PID cancellation fallback.

Ordinary execution retains the existing process-group cleanup path. Controlled
execution uses pidfd observation and signaling throughout.

## Capability report

`capability_report::probe()` reports the host supervisor.
`capability_report::probe_isolated()` additionally performs end-to-end mount and
network realization probes. Both reports perform exact address-space,
file-size, and open-files realization probes. Cancellation is advertised only after an
end-to-end probe creates a private execution session with a descendant, signals
exact members through pidfds, observes the leader through `waitid(P_PIDFD)`, and
verifies descendant disappearance.

A raw successful `pidfd_open(2)` is diagnostic only. Observations distinguish
`available`, `unavailable`, and `policy-restricted`. Kernel versions, host
paths, external delegation, cancellation timing, and diagnostic strings do not
enter backend capability-profile identities.

This is not yet full package-build isolation. Version 0.6 still advertises no
arbitrary credential isolation, PID namespace, Landlock, cgroup, CPU-time, or
execution-wide process-count guarantee.
