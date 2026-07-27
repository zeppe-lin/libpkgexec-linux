# Migration

`libpkgexec-linux 0.2.0` adds `isolated_backend` without changing the 0.1 host
supervisor or SONAME.

Callers choosing the isolated backend must supply a dedicated root-view
directory rather than `/`. The root must already contain the exact interpreter
path and every runtime object needed by the program. The backend does not copy
host `/usr`, `/lib`, `/proc`, `/dev`, `/run`, or any other undeclared tree into
the isolated view.

Every declared logical resource destination must already exist as a directory
inside the root. Source and package-input resources must be read-only;
workspace, output, private temporary, and managed-target resources must be
writable. Host resource paths cannot overlap the root or each other.

A runner without delegated mount authority receives `backend_unsupported`.
Do not fall back silently to `host_supervisor_backend` when a request requires
root-view or read-only-resource guarantees.

There is no reinterpretation of 0.1 execution evidence. Namespace, network,
Landlock, cgroup, credential, limit, and cancellation guarantees remain absent
until a later backend release establishes them explicitly.
