# Testing

The test suite covers:

- deterministic capability profiles;
- exact interpreter inspection, descriptor-execution probing, and identity;
- closed environment construction;
- current-root and current-credential admission;
- complete concurrent stdout and stderr capture;
- nonzero and signal termination classification;
- inherited descriptor closure;
- process-group descendant cleanup;
- seccomp refusal of session/process-group escape;
- explicit refusal of read-only resources and network isolation;
- resource-path and credential mismatch rejection;
- public-header and generated metadata contracts.

The backend test exits 77 when the runner prevents the process-group containment
filter from being installed. This is a skip, not a pass. Namespace, Landlock,
and cgroup observations do not convert unavailable integration coverage into a
successful isolation test.
