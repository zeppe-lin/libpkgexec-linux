# History

## libpkgexec-linux 0.5.0

Adds exact Linux realization for the `libpkgexec 1.2` address-space,
file-size, and open-files guarantees. Both backends set the requested finite
value as the soft and hard `RLIMIT_AS`, `RLIMIT_FSIZE`, or `RLIMIT_NOFILE`
value before the final start gate. Seccomp containment permits read-only limit
inspection but denies later `setrlimit(2)` and mutating `prlimit64(2)` calls.

Capability reporting is proof-based. Each exact kind enters the profile only
after a child applies a representative soft/hard pair, installs mutation
sealing, proves an attempted raise is denied, and rereads the unchanged value.
Request values equal to `RLIM_INFINITY`, not representable by `rlim_t`, or above
the inherited hard ceiling fail before start as resource admission failures.

CPU-time and process-count guarantees remain unsupported: `RLIMIT_CPU` cannot
represent the native millisecond contract, and `RLIMIT_NPROC` is scoped to a
real UID rather than one execution tree. Signal numbers are not treated as
causal limit evidence; `SIGXFSZ` remains ordinary signal termination while the
established file-size guarantee is retained.

The release raises the `libpkgexec` floor to 1.2.0. SONAME remains 1, and the
0.4 filesystem, network, and pidfd cancellation contracts are unchanged.

## libpkgexec-linux 0.4.0

Adds truthful request-bound graceful-then-forced cancellation to both Linux
backends through the `libpkgexec 1.1` controlled-execution contract. Each child
establishes a private session before the final start gate. The supervisor keeps
the leader unreaped, observes it through a pidfd, signals exact process-group
members through pidfds, honors the sealed grace period, escalates to `SIGKILL`,
and verifies descendant cleanup before sealing cancellation evidence.

Capability reporting is proof-based. Cancellation is advertised only after an
end-to-end probe creates a descendant-bearing private execution group, signals
it through pidfds, observes the leader with `waitid(P_PIDFD)`, and proves that
no descendant remains. A raw pidfd syscall result remains diagnostic only.
Cancellation before the final gate is reported as not started; a naturally
completed program is not reclassified as cancelled.

The release raises the `libpkgexec` floor to 1.1.0 and advances the backend
SONAME to 1 because the controlled backend transition changes the C++ ABI. It
still
does not add user or PID namespaces, Landlock, cgroups, arbitrary credential
transitions, or resource limits.

## libpkgexec-linux 0.3.0

Extends `isolated_backend` with exact Linux network-policy realization.
`network_policy::denied` creates a private network namespace whose only link is
administratively down loopback. `network_policy::loopback_only` creates the
same private view, brings only loopback up through rtnetlink, and proves an
internal round trip before advertising the guarantee. Allowed networking
preserves the caller's network namespace.

Capability reporting is proof-based: a raw namespace syscall is not enough. A
runner that cannot create and configure the requested view rejects the request
before program start, without fallback to allowed networking. The release keeps
the 0.2 filesystem contract, `host_supervisor_backend`, and SONAME 0 unchanged.

The release still does not add user or PID namespaces, Landlock, cgroups,
arbitrary credential transitions, resource limits, or cancellation.

## libpkgexec-linux 0.2.0

Adds `isolated_backend`, a descriptor-oriented private mount-view executor. It
requires a dedicated supplied root, clones the root read-only, attaches exact
read-only and writable directory resources, verifies mount identities, drops
capabilities, executes the exact interpreter descriptor, and verifies process
and scratch cleanup.

The release preserves `host_supervisor_backend` unchanged and keeps SONAME 0.
It deliberately does not add user, PID, or network namespaces, Landlock,
cgroups, arbitrary credential transitions, limits, or cancellation.

## libpkgexec-linux 0.1.0

The first release establishes the restricted host supervisor, exact interpreter
inspection, truthful Linux capability observations, closed process state,
complete stream capture, signal and exit classification, and verified
process-group cleanup.

It deliberately rejects read-only resource realization, isolated root views,
network isolation, arbitrary credentials, resource limits, cancellation,
namespaces, Landlock, and cgroups.
