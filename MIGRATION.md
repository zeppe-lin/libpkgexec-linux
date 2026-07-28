# Migration

`libpkgexec-linux 0.3.0` extends `isolated_backend` with denied and
loopback-only network policies without changing the host supervisor or SONAME.

An isolated request may now select:

- `network_policy::allowed`, which preserves the caller's network namespace;
- `network_policy::denied`, which creates a private network namespace with only
  an administratively down loopback interface;
- `network_policy::loopback_only`, which creates a private network namespace
  and brings up only loopback.

Denied and loopback-only requests are admitted only when the current runner can
realize and verify the exact view. A restricted runner receives
`backend_unsupported` before program start. Do not retry the request with
allowed networking.

The filesystem contract introduced in 0.2 is unchanged. Callers must still
supply a dedicated root-view directory containing the exact interpreter and
runtime closure. Every logical resource destination must exist inside the root,
and host resource paths cannot overlap the root or each other.

There is no reinterpretation of 0.2 evidence. Network guarantees enter 0.3
evidence only for requests that selected and established the corresponding
policy. User namespace, PID namespace, Landlock, cgroup, credential, limit, and
cancellation guarantees remain absent.
