# Testing libpkgexec-linux

## Test roles

The suite is separated by authority and mechanism:

- `unit` checks provider-owned value vocabulary with no execution;
- `integration` checks host execution, capability probing, interpreter admission,
  exact limits, and cancellation against real `libpkgexec` requests/results;
- `integration-privileged` checks isolated filesystem, network, limit,
  cancellation, and composed realization. Environmental inability exits 77;
- `header` compiles every installed public header independently;
- `contract` checks source/release/pkg-config/test-topology invariants.

The three programs under `tests/fixtures/payloads/` are child payload material,
not sanitizer subjects. They are built without compiler sanitizers while the
backend and test harness retain the selected sanitizer configuration.

Role-specific runs are useful when localizing a failure:

```sh
meson test -C build --suite unit --print-errorlogs
meson test -C build --suite integration --print-errorlogs
meson test -C build --suite integration-privileged --print-errorlogs -v
meson test -C build --suite header --print-errorlogs
meson test -C build --suite contract --print-errorlogs
```

The test suite covers:

- deterministic host and isolated capability profiles;
- exact interpreter inspection and descriptor execution;
- closed environment construction;
- current-root host admission;
- dedicated-root isolated admission;
- private deterministic `/dev/null` without root-device or ambient `/dev` authority;
- inherited `noexec` normalization for exact root and declared resource trees;
- exact ELF `PT_INTERP` retention in synthetic runtime roots;
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

The isolated integration cases exit 77 independently when the runner cannot
realize the guarantee family under test. Each skipped case reports the exact
unsupported request and unavailable or policy-restricted observations. The
filesystem case separately requires preparation of the exact-root device fixture.
Meson may suppress skipped output in its compact summary; use the
`integration-privileged` suite with `-v` when the reason matters.

A skip is not release proof. A delegated or privileged run must PASS every
`integration-privileged` case. The final `isolated-composition` request requires
filesystem isolation, read-only/writable resources, denied networking, exact
address-space/file-size/open-files limits, and request-bound cancellation at the
same time.

Synthetic runtime roots retain the executable's exact ELF `PT_INTERP` pathname
in addition to the dependency closure reported by `ldd(1)`, so merged-`/usr` or
loader-symlink layouts cannot make a present executable fail with `ENOENT`. The
isolated test executes one copied dynamic probe without resource limits before
using that same probe to qualify exact limits.

Network views are proved through a compiled fixture inside the supplied root.
Cancellation is proved through compiled graceful and SIGTERM-resistant
fixtures. Resource limits are proved through a compiled fixture that rereads
exact soft/hard pairs, attempts forbidden mutation, exhausts descriptors,
allocates beyond `RLIMIT_AS`, and writes beyond `RLIMIT_FSIZE`. These isolated
payload fixtures are intentionally built without compiler sanitizers even when
the backend and test harness are sanitized. Sanitizer runtimes would otherwise
become undeclared execution-root dependencies (for example LeakSanitizer needs
`/proc`), while address sanitizers also reserve virtual address ranges that
would turn the exact `RLIMIT_AS` case into a sanitizer-startup test. The tests
do not delegate policy realization to `ulimit`, `ip(8)`, shell socket
extensions, or root-supplied process utilities.

The backend does not create a user namespace merely to make CI pass. Tests may
run under externally delegated user, mount, and network authority, but must
report that condition separately from backend behavior.

## Release-product qualification

The 0.6 release additionally freezes the installed product rather than only the
in-tree test executables. Shared builds require the exact reviewed ELF ABI,
`SONAME libpkgexec-linux.so.2`, direct `NEEDED libpkgexec.so.2` and
`NEEDED libpkgsource.so.3` edges, while refusing obsolete execution/source generations.
The x86-64 layout contract freezes the exec2 carriers
embedded by the Linux backend together with the Linux-owned public values.

`ci/configure-and-test.sh` builds and installs source3, exec2, then this project
into isolated prefixes and compiles `tests/installed/consumer.cpp` only through
the generated pkg-config metadata. `ci/qualify.sh` repeats the product under GCC
and Clang, shared and static, plus ASan/UBSan shared qualification. Hosted CI
uses the same entry point. The installed consumer exercises virtual dispatch
through the exec2 base class and catches a Linux-provider error across the DSO
boundary.

The hosted runner may report environmental skips for privileged namespace
realization. Those skips remain non-proof. A release still requires the separate
delegated/root `integration-privileged` run described above with every case
PASSing.
