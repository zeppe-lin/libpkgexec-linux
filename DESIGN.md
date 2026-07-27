# Design

## Authority boundary

`libpkgexec-linux` realizes `libpkgexec::execution_request` values and returns
`libpkgexec::execution_result` evidence. It does not define another program,
request, result, or package-operation model.

The backend owns Linux realization and observation only:

```
execution_request + execution_resources
    -> Linux realization
    -> execution_result
```

The backend does not own source acquisition, build semantics, lifecycle policy,
artifact production, filesystem application, or installed-state publication.

## 0.1 host supervisor

The initial backend accepts only requests whose guarantees can be established
without constructing a new filesystem or network view:

- the process-visible root is the current `/`;
- every resource is writable and already present at its exact logical path;
- every resource path is absolute, existing, directory-valued, and free of
  symlink components;
- requested credentials exactly equal the supervisor's current UID, GID, and
  supplementary groups;
- `no_new_privileges` is required;
- networking is allowed rather than isolated;
- resource limits are empty;
- cancellation is disabled;
- the interpreter is an exact inspected regular executable;
- stdout and stderr are either completely captured or discarded.

Unsupported requests return `backend_unsupported` or `request_rejected` before
program start. No requested guarantee is silently weakened.

## Interpreter authority

`interpreter_binding::inspect()` resolves the supplied path, rejects a resolved
path with symlink components, verifies a regular executable, hashes its exact
bytes, and derives the Linux interpreter identity from that content identity.
The executable is opened and hashed immediately before execution. Descriptor
execution is probed before it enters the capability profile, and the same open
descriptor is passed to `execveat(2)`, so pathname replacement cannot
change the bytes that are executed.

The 0.1 POSIX-shell invocation is fixed:

```
INTERPRETER -c PROGRAM pkgexec
```

No shell startup file or inherited shell environment is admitted.

## Closed process state

The child environment is constructed only from the sealed policy:

- `PATH`;
- `HOME`;
- `LANG=C.UTF-8` and `LC_ALL=C.UTF-8`;
- `TZ=UTC`;
- `TMPDIR`;
- `PKGEXEC_JOBS`;
- optional `SOURCE_DATE_EPOCH`;
- exact additional variables admitted by `libpkgexec`.

The parent environment is never copied. The requested umask and working
directory are installed before `execve(2)`. Inherited descriptors are closed.

## Supervision and cleanup

The child becomes leader of a private process group. Before `execveat(2)`, a
seccomp filter denies `setsid(2)`, `setpgid(2)`, `unshare(2)`, and `setns(2)`.
The filter is inherited by descendants and prevents session or process-group
escape. It is not a complete namespace filter and advertises no namespace
guarantee.

The parent drains stdout and stderr concurrently with nonblocking pipes, waits
for the leader, terminates any remaining group members, and verifies that the
process group no longer exists. `cleanup_verified` is reported only after that
verification. A failed cleanup remains a failed execution even after a zero
leader exit.

This is process-group containment, not a security sandbox. The 0.1 backend does
not claim mount, network, Landlock, cgroup, or privilege isolation.

## Capability report

`capability_report` seals the actual `libpkgexec` guarantee profile and retains
Linux feature observations for diagnostics. Namespace, Landlock, cgroup, and
pidfd probes do not advertise execution guarantees that the backend does not
use. Feature observations may be `available`, `unavailable`, or
`policy-restricted`.

Kernel versions, host paths, and diagnostic strings do not enter the core
backend capability-profile identity.
