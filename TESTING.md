# Testing

The test suite covers:

- deterministic host and isolated capability profiles;
- exact interpreter inspection and descriptor execution;
- closed environment construction;
- current-root host admission;
- dedicated-root isolated admission;
- no-symlink root and resource opening through `openat2(2)`;
- root/resource and resource/resource overlap rejection;
- detached `open_tree(2)` cloning and `move_mount(2)` attachment;
- read-only source and input mounts;
- writable workspace mounts;
- read-only undeclared root content;
- absence of undeclared host files;
- allowed networking through the caller's network namespace;
- denied networking with only down loopback and no parent reachability;
- loopback-only networking with an internal round trip and no parent
  reachability;
- capability dropping before execution;
- complete concurrent stdout and stderr capture;
- nonzero and signal termination classification;
- inherited descriptor closure;
- descendant cleanup and scratch cleanup;
- explicit refusal of unsupported credentials, limits, cancellation, Landlock,
  and cgroup guarantees;
- public-header and generated metadata contracts.

The isolated integration test exits 77 when the runner cannot create the
required mount or network namespaces or use the required mount and rtnetlink
operations. This is a skip, not a pass. A delegated or privileged test run must
execute the same binary and produce a real pass before the isolated guarantees
are release-qualified.

The test proves network views through a compiled fixture inside the supplied
root. It does not rely on `ip(8)`, `/proc`, `/sys`, shell-specific socket
extensions, or host network utilities.

The backend does not create a user namespace merely to make CI pass. Tests may
run under externally delegated user, mount, and network authority, but must
report that condition separately from backend behavior.
