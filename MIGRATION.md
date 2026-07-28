# Migration

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
