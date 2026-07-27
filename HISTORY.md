# History

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
