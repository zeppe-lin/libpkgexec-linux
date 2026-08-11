# Migration

`libpkgexec-linux 0.6.0` advances to `libpkgexec-linux.so.2` and requires
`libpkgexec >= 2.0.0, < 3.0.0` plus direct `libpkgsource >= 3.0.1, < 4.0.0`.
Recompile consumers against the generation-2
execution headers and backend DSO. No compatibility surface is provided for the
accidental private/STL exports of `libpkgexec-linux.so.1`; old toolchains may
continue to use their old backend generation. Public x86-64 carrier layouts are
unchanged across the exec1-to-exec2 dependency transition.

`libpkgexec-linux 0.5.2` keeps SONAME 1 and the `libpkgexec >= 1.2.0` floor.
The isolated root clone remains read-only and `nosuid` but no longer forces
`nodev`; device nodes explicitly present in the caller-supplied root therefore
retain their Linux semantics. Separately declared execution resources remain
`nodev`. Consumers that supplied device nodes in an exact root may now rely on
them; no device view is synthesized.

`libpkgexec-linux 0.5.0` raises the `libpkgexec` floor to 1.2.0. SONAME remains
`libpkgexec-linux.so.1`; existing 0.4 consumers remain ABI-compatible.

Both backends now admit exact address-space, file-size, and open-files limits
when the corresponding capability observation is available. The request must
carry the aggregate `resource_limits` guarantee plus the exact kind guarantees
derived by `libpkgexec 1.2`.

Requested values become both the soft and hard Linux limit before the final
start gate. Values equal to `RLIM_INFINITY`, not representable by `rlim_t`, or
above the inherited hard ceiling fail before start as `resource_admission_failed`.
The contained process may read its limits but attempts to mutate them through
`setrlimit(2)` or `prlimit64(2)` are denied.

CPU-time and process-count requests remain `backend_unsupported`. The backend
does not round milliseconds to `RLIMIT_CPU` and does not treat per-UID
`RLIMIT_NPROC` as an execution-tree limit. A program killed by `SIGXFSZ` is
reported as signal termination; the signal number alone is not admitted as
proof of resource-limit causality.

`libpkgexec-linux 0.4.0` raises the `libpkgexec` floor to 1.1.0 and changes both
backend classes to the additive `controlled_execution_backend` interface. The
old directly callable backend `execute()` symbols are not retained. SONAME
advances from 0 to 1, and consumers must be rebuilt against the 0.4 headers.

Requests with disabled cancellation continue to use:

```
backend.execute(request, resources)
```

A graceful-then-forced request must create the request-bound control authority
and use the token-bearing overload:

```
auto source = pkgexec::cancellation_source::for_request(request);
auto result = backend.execute(request, resources, source.token());
```

Calling the ordinary overload for a cancellation-enabled request remains a
core contract error. Passing a token for another request is also rejected by
`libpkgexec` before Linux realization.

The cancellation guarantee is advertised only when the current runner passes
the complete pidfd realization probe. An unsupported runner returns
`backend_unsupported` before program start; callers must not retry the same
request without cancellation.

Cancellation before the backend's final start gate produces not-started
cancelled evidence. After confirmed execution, the backend sends `SIGTERM`,
waits the sealed grace period, and escalates to `SIGKILL`. The child and its
descendants are confined to a private session and stable process group, and
cleanup is verified before the leader is reaped.

The private filesystem and network contracts from 0.2 and 0.3 are unchanged.
User namespace, PID namespace, Landlock, cgroup, arbitrary credential, and
resource-limit guarantees remain absent.
