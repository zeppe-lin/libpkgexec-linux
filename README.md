# libpkgexec-linux

`libpkgexec-linux` is the Linux backend family for `libpkgexec`.

The 0.2 release provides two truthful backends:

- `host_supervisor_backend` preserves the 0.1 host-view contract: current `/`,
  current credentials, caller-prepositioned writable resources, allowed
  networking, a closed environment, complete stream capture, and verified
  process-group cleanup;
- `isolated_backend` realizes one exact dedicated root-view directory in a
  private mount namespace, clones it read-only, and attaches exact read-only or
  writable resource directories at their declared logical paths.

The isolated backend is descriptor-oriented. Root and resource directories are
opened with `openat2(2)`, cloned with `open_tree(2)`, attached with
`move_mount(2)`, and sealed with `mount_setattr(2)`. It does not borrow missing
files from the live host root. The supplied root must contain the interpreter
and its runtime closure.

Version 0.2 still does not provide user, PID, or network namespaces, Landlock,
cgroups, arbitrary credential transitions, resource limits, or cancellation.
Requests requiring unsupported guarantees are rejected before program start.

The semantic execution request and result remain owned by `libpkgexec`.
Concrete host paths are call-scoped materializations and do not enter execution
identity.

See `DESIGN.md`, `MIGRATION.md`, and the manual pages for the full contract.
