# History

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
