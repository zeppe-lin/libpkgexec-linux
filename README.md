# libpkgexec-linux

`libpkgexec-linux` is the Linux backend family for `libpkgexec`.

The 0.4 release provides two truthful controlled backends:

- `host_supervisor_backend` preserves the host-view contract: current `/`,
  current credentials, caller-prepositioned writable resources, allowed
  networking, a closed environment, complete stream capture, and verified
  process-tree cleanup;
- `isolated_backend` realizes one exact dedicated root-view directory in a
  private mount namespace, attaches exact read-only or writable resource
  directories, and enforces allowed, denied, or loopback-only networking.

Both backends realize request-bound graceful-then-forced cancellation when the
current runner passes the end-to-end pidfd cancellation probe. The child enters
a private session before the final start gate. The supervisor observes the
call-scoped cancellation token, signals exact processes through pidfds, waits
the sealed grace period, escalates through pidfds, and verifies that no member
of the closed execution group remains. There is no ambient signal handler,
global cancellation flag, `kill(-pgid)` cancellation fallback, or reuse of an
unverified numeric PID.

The isolated backend remains descriptor-oriented. Root and resource directories
are opened with `openat2(2)`, cloned with `open_tree(2)`, attached with
`move_mount(2)`, and sealed with `mount_setattr(2)`. It does not borrow missing
files from the live host root. The supplied root must contain the interpreter
and its runtime closure.

Denied networking creates a private network namespace whose only interface is
administratively down loopback. Loopback-only networking creates the same
private view and brings only loopback up through rtnetlink. Allowed networking
preserves the caller's network namespace. No policy silently degrades to
allowed networking.

Version 0.4 still does not provide user or PID namespaces, Landlock, cgroups,
arbitrary credential transitions, or resource limits. Requests requiring
unsupported guarantees are rejected before program start.

The semantic execution request, cancellation authority, and result remain owned
by `libpkgexec`. Concrete host paths and cancellation timing are call-scoped
operational state and do not enter execution identity.

See `DESIGN.md`, `MIGRATION.md`, and the manual pages for the full contract.
