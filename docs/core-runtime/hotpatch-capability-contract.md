---
doc_type: core-runtime-contract
status: implemented
---

# Hot-patch capability validation

`capability_manifest.h` defines the host-authoritative validation boundary for
patches. A manifest is bound to a patch id, immutable artifact content hash,
base module hash, public contract hash, ABI version, and target profile.

Validation first checks the already parsed pointer-free artifact, content/base
identity, ABI/profile and public-contract invariants. Machine-code payloads,
new native imports, and public layout/signature changes are rejected before any
publish operation. Required capabilities are checked both as an aggregate and
per token requirement; `required - hostAllowed` is an explicit escalation
error, never silently trimmed.

Signature verification is supplied by the host through a callback, keeping
trust-root ownership outside the VM. The validated result is written only
after every check succeeds and records the policy hash, content identity, and
an immutable-content marker for the later generation publisher. Callers must
retain or copy the artifact buffer so that this identity cannot be changed
between validation and apply.
