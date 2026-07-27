# libpkgexec-linux

`libpkgexec-linux` is the Linux backend family for `libpkgexec`.

The 0.1 release provides a deliberately restricted host supervisor. It executes
an exact inspected POSIX shell interpreter in the current `/` root view, with
the current credentials, a closed environment, caller-prepositioned writable
resources, complete pipe capture, and verified process-group cleanup.

It is not a build sandbox. It does not create mount, user, PID, network, IPC, or
UTS namespaces; realize read-only bind mounts; apply Landlock; place processes
in cgroups; enforce resource limits; or provide call-scoped cancellation.
Requests requiring any unsupported guarantee are rejected before program start.

The semantic execution request and result remain owned by `libpkgexec`.
Concrete host paths are call-scoped materializations and do not enter execution
identity.

See `DESIGN.md`, `MIGRATION.md`, and the manual pages for the full contract.
