# libpkgexec-linux history

## libpkgexec-linux 0.7.1

Isolation realization and capability-qualification maintenance release.

- Keep admitted root and execution-resource authority as exact retained directory
  descriptors and create/seal child-owned detached mounts before entering the
  private mount namespace. Realization no longer consumes retained admission or
  requires detached-from-detached cloning, preserving repeatable execution across
  the older and newer Linux mount APIs.
- Seal every realized detached root/resource tree with private propagation before
  attachment. This closes propagation leakage from shared host mounts exposed by
  newer anonymous-mount propagation semantics.
- Make the isolated-filesystem probe exercise one retained admission twice, retain
  the exact failing setup/cleanup stage and errno, and add a privileged public
  capability gate plus shared-source propagation stress so provider defects cannot
  hide behind a long bootstrap campaign.
- Keep API generation 2, SONAME 2, capability vocabulary, and the public
  `libpkgexec` execution contract unchanged.

## libpkgexec-linux 0.7.0

Checked-package role qualification release.

- Require `libpkgexec >= 2.2.0, < 3.0.0` and admit the new singleton
  `package_tree` resource only as read-only execution authority.
- Keep `package_tree` outside the backend-owned build/check input namespaces;
  its exact logical destination is caller-owned root-view structure.
- Qualify an isolated check that consumes `/check/package`, rejects writes to
  the checked package, and leaves the host package tree unchanged.
- Keep API generation 2 and SONAME 2 unchanged.

## libpkgexec-linux 0.6.2

Source-ABI-4 qualification release.

- Require `libpkgexec >= 2.1.1, < 3.0.0`, the first execution-core
  release whose direct source dependency is version-addressably bound to
  `libpkgsource 4.x`.
- Qualify the Linux backend against immutable `libpkgsource 4.0.0` and
  `libpkgexec 2.1.1` authority.
- Keep API generation 2 and SONAME 2 unchanged; this release closes the
  dependency graph rather than changing the Linux backend ABI.

## libpkgexec-linux 0.6.1

Nonblocking interpreter-authority maintenance release.

- Reopen admitted interpreter paths with nonblocking regular-file admission so a
  concurrent FIFO replacement cannot wedge inspection or execution before typed
  refusal.
- Preserve exact retained interpreter digest and identity verification before
  program start; no pathname replacement can silently alter admitted bytes.
- Normalize manual-page authority to canonical Markdown with committed generated
  roff and remove the scdoc source layer.
- Qualify against the published `libpkgexec 2.1.0` execution core.

## libpkgexec-linux 0.6.0

Rebuilds the Linux backend product on the reviewed `libpkgexec 2.x` execution
authority and advances the backend SONAME to 2. The execution-2 transition does
not change the x86-64 layouts of the base backend, request/result, capability,
or interpreter values retained by this library; the SONAME change instead
closes the previously published backend ABI, whose unrestricted ELF surface
exposed private `detail` machinery, private constructors, STL instantiations,
and compiler-dependent symbols. Generation 2 exports only the reviewed public
Linux backend contract.

The release requires `libpkgexec >= 2.0.0, < 3.0.0` and direct
`libpkgsource >= 3.0.1, < 4.0.0`; the Linux backend calls source-owned program
materialization and does not rely on that provider through a transitive accident. It anchors the public backend
and error RTTI/vtables in the provider DSO, rejects unsupported capability and
capability-state vocabulary at observation admission, and qualifies the actual
shared/static installed product under GCC, Clang, and ASan/UBSan. Privileged
isolation remains a separate release gate: an environmental skip is diagnostic,
not proof.

## libpkgexec-linux 0.5.2

Preserves device-node semantics from the exact caller-supplied isolated root
view. The root tree remains read-only and `nosuid`; its detached clone actively
clears an inherited `nodev` attribute while explicitly declared resource mounts
remain `nodev`. Exact roots and declared resource trees also clear inherited
`noexec`, preventing the host staging filesystem from silently changing whether
admitted executable material may run. This makes
caller-owned root devices such as `/dev/null` usable without synthesizing a
device view, binding ambient host `/dev`, or weakening package/source resource
mounts.

The isolated integration test now reports exact unavailable capability
observations before an environmental skip, and release qualification also proves
that one exact root-view character device remains usable after isolation. The
synthetic runtime fixture also retains the exact ELF `PT_INTERP` path rather than
relying on `ldd(1)`'s possibly resolved loader pathname.

SONAME remains 1 and the libpkgexec dependency floor is unchanged.

## libpkgexec-linux 0.5.1

Makes unsupported-request evidence actionable. When a sealed execution request
requires guarantees absent from the probed Linux backend profile, the returned
pre-start diagnostic names the exact missing guarantees instead of collapsing
them into a generic backend-unsupported message. Guarantee derivation, probing,
admission, and fallback policy are unchanged.

SONAME remains 1 and the libpkgexec dependency floor is unchanged.

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
