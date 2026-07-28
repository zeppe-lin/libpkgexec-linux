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
- allowed, denied, and loopback-only networking;
- capability dropping before execution;
- complete concurrent stdout and stderr capture;
- exact `RLIMIT_AS`, `RLIMIT_FSIZE`, and `RLIMIT_NOFILE` soft/hard values;
- inability to raise or replace admitted limits after containment;
- `ENOMEM`, `EMFILE`, and `SIGXFSZ` enforcement behavior;
- conservative signal classification without invented limit causality;
- refusal of CPU-time and process-count limits;
- nonzero and signal termination classification;
- inherited descriptor closure;
- private-session descendant cleanup and scratch cleanup;
- request-bound cancellation before the final start gate;
- graceful cancellation after confirmed program start;
- forced cancellation of a descendant-bearing execution group;
- composition of isolated root, exact limits, networking, and cancellation;
- explicit refusal of unsupported credentials, CPU-time, process-count,
  Landlock, and cgroup guarantees;
- public-header and generated metadata contracts.

The host cancellation test exits 77 when the end-to-end pidfd cancellation
probe cannot establish exact leader observation, pidfd signaling, and process
group cleanup. A raw `pidfd_open(2)` result is not sufficient coverage.

The isolated integration test exits 77 when the runner cannot create the
required mount or network namespaces, use the required mount and rtnetlink
operations, or establish pidfd cancellation. This is a skip, not a pass. A
delegated or privileged test run must execute the same binary and produce a real
pass before the composed isolated guarantees are release-qualified.

Network views are proved through a compiled fixture inside the supplied root.
Cancellation is proved through compiled graceful and SIGTERM-resistant
fixtures. Resource limits are proved through a compiled fixture that rereads
exact soft/hard pairs, attempts forbidden mutation, exhausts descriptors,
allocates beyond `RLIMIT_AS`, and writes beyond `RLIMIT_FSIZE`. The tests do not
delegate policy realization to `ulimit`, `ip(8)`, shell socket extensions, or
root-supplied process utilities.

The backend does not create a user namespace merely to make CI pass. Tests may
run under externally delegated user, mount, and network authority, but must
report that condition separately from backend behavior.
