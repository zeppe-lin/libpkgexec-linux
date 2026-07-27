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
- capability dropping before execution;
- complete concurrent stdout and stderr capture;
- nonzero and signal termination classification;
- inherited descriptor closure;
- descendant cleanup and scratch cleanup;
- explicit refusal of unsupported network, credentials, limits, cancellation,
  Landlock, and cgroup guarantees;
- public-header and generated metadata contracts.

The isolated integration test exits 77 when the runner cannot create the
required mount namespace or use the required mount APIs. This is a skip, not a
pass. A delegated or privileged test run must execute the same binary and
produce a real pass before the isolated guarantees are release-qualified.

The backend does not create a user namespace merely to make CI pass. Tests may
run under an externally delegated user/mount namespace, but must report that
condition separately from backend behavior.
