# Migration

`libpkgexec-linux 0.1.0` has no historical compatibility surface.

Do not wrap an existing package shell runner and label its output as native
execution evidence. Callers must construct a sealed `libpkgexec` request,
admit exact resources, bind an inspected interpreter, and accept that the 0.1
backend rejects read-only resources, denied networking, arbitrary credentials,
limits, cancellation, and isolated root views.

Later namespace, Landlock, and cgroup backends may broaden the capability
profile. They must not reinterpret 0.1 results or claim guarantees that were not
established during the original execution.
