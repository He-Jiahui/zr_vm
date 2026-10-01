---
related_code:
  - zr_vm_core/include/zr_vm_core/capability_manifest.h
  - zr_vm_core/include/zr_vm_core/module.h
  - zr_vm_core/include/zr_vm_core/exec_ir.h
  - zr_vm_core/include/zr_vm_core/artifact_schema.h
  - zr_vm_core/include/zr_vm_core/hotpatch_generation.h
  - zr_vm_core/include/zr_vm_core/hotpatch_rollback.h
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_validate.c
  - zr_vm_core/src/zr_vm_core/module/module_exec_ir_artifact.c
  - zr_vm_core/src/zr_vm_core/artifact_identity.c
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_generation.c
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_rollback.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/capability_manifest.h
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_validate.c
  - zr_vm_core/src/zr_vm_core/module/module_exec_ir_artifact.c
  - zr_vm_core/include/zr_vm_core/hotpatch_rollback.h
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_generation.c
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_rollback.c
tests:
  - tests/CMakeLists.txt
  - tests/cmake/ssa-hotpatch-apply-sources.cmake
  - tests/library/test_ssa_capability_validation.c
  - tests/library/test_ssa_exec_ir_artifact_v6.c
  - tests/library/test_ssa_canonical_zraf_validation.inc
  - tests/library/test_ssa_canonical_zraf_eis6_guard.inc
  - tests/cmake/ssa-tests.cmake
  - tests/acceptance/ssa-hotpatch-canonical-zraf-validation.md
  - tests/core/test_ssa_generation_publication.c
  - tests/library/test_ssa_rollback_restricted.c
plan_sources:
  - docs/plans/ssa/08-artifact-hotpatch/02-capability-validation.md
doc_type: module-detail
status: partial
---

# Hotpatch capability validation contract

Hotpatch admission is a closed-world intersection:

    required = manifest.requiredCapabilities | every requirement.requiredBits
    admissible = (required & ~hostAllowedCapabilities) == 0

The capability closure is computed before generation preparation. Every
requirement must have a non-zero token and bit set, a zero reserved field, and
must fit the bounded requirement count. Unknown manifest flags, missing rows,
and capability escalation are reported with a structured diagnostic; callers
must not turn them into a generic false/success result.

ZrCore_HotPatch_ValidateCapabilityClosure delegates identity and signature
checks to the manifest validator, then uses the transactional closure seam in
hotpatch_capability.c. The output mask is written only after all rows pass.
Repeated requirements are harmless (bitwise union), while a single
out-of-policy bit rejects the complete patch.

The host must separately validate artifact schema, ABI/profile identity,
immutable content, and machine-code policy. Capability admission does not
authorize relocation or native imports by itself. iOS/WASM restricted profiles
therefore remain interpreter-only and reject relocation sections before
execution.

Validation snapshots the `patchId` and `publicContractHash` values consumed by
Apply and generation preparation, alongside the existing content hash,
capability mask, policy hash, and target profile. It also captures the original
borrowed byte address and length. Apply rehashes that span before registry
lookup or generation preparation and reports `content-mismatch` if its contents
changed since validation. This catches persistent changes made after Validate
returns and by a successful signature callback.

The validated token still borrows its artifact and manifest pointers and does
not copy or pin artifact bytes. Callers must keep the captured byte span alive
and prevent concurrent writes while Apply hashes it. This check does not make
the bytes immutable or cover later writes. The current generation manager
records identity metadata and does not install executable bytes, so any later
artifact consumer must preserve and verify the same content identity.

`ZrCore_HotPatch_ValidateZraf` is the typed outer-container entry point. It
checks limits, hashes the complete `ZRAF` byte span, validates the host policy,
and requires both the manifest `contentHash` and host `expectedContentHash` to
match. It snapshots policy and identity inputs before calling the host
signature verifier with the complete outer span. After comparing the
pre-callback and post-callback hashes, it proceeds only if they match; then it
reads the outer document, checks the copied expected public identity, opens the
canonical ExecIR module, verifies that graph, and requires exactly one function matching the explicit
`(entryFunctionToken, entrySignatureHash)` selector. It does not infer an
entry from function order or from the manifest. The current canonical reader
accepts only single-function modules, so a duplicate selector fixture is not
currently representable; the uniqueness check remains defensive.
The public input and validated token preserve `outerLength` as `TZrSize`; the
validator rejects lengths above `ZR_ARTIFACT_MAX_BYTE_LENGTH` or the `UInt32`
hash/callback limit before reading bytes. It passes the original size to the
artifact readers and narrows only for the hash and signature callback APIs. A
limit diagnostic therefore reports the original caller-supplied length
instead of a truncated value.

Before the host signature callback, the entry point snapshots the expected
identity, input scalars, and manifest scalars, and reduces the borrowed
requirement rows to a validated capability mask with the shared capability
closure helper. The synchronous callback receives the complete outer bytes and
signature before the canonical container and nested graph are parsed. The
entry point hashes the same captured span again after the callback; a differing
hash returns `ZR_HOT_PATCH_CONTENT_CHANGED` and publishes no token. Rejected
callbacks stop before structural parsing; a successful callback proceeds to
outer identity, ExecIR verification, and selector checks. Only after all checks
pass does the output token receive by-value identity, selector, and policy
snapshots. The legacy inner-ExecIR `Validate` path retains its existing
behavior and error order.

The outer-byte pointer in `SZrValidatedHotPatchZraf` is still borrowed. The
caller must keep its allocation alive through validation and every later
`RecheckZrafContent` call, and externally synchronize any writers with those
operations. The span must remain stable from its initial hash through signature
verification and structural decoding, including while the callback runs; the
callback must treat it as read-only. The post-callback hash rejects when its
result differs from the pre-callback hash. It cannot detect a callback mutation
that is restored before return, and it does not make concurrent writes atomic.
Recheck detects only changes visible to its hash while the borrowed storage
remains live; it does not pin or own bytes. The signature callback authenticates
the outer span only; the host-supplied manifest and expected identity are
separately trusted inputs whose checked values are captured before the callback.
This API validates canonical ExecIR metadata and does not relocate, install, or
execute code.

The public scalar ExecIR reader may decode typed EIS6 binding-row schemas, but
the canonical outer-artifact opener currently admits only functions using
`ZR_EXEC_IR_BINDING_ROWS_SCHEMA_LEGACY`. It checks every decoded function
immediately after scalar decoding and before contract validation or publication.
A typed function returns `ZR_ARTIFACT_STATUS_INVALID_SECTION` for
`ZR_ARTIFACT_SECTION_EXEC_IR_BUNDLE` at the nested scalar payload offset; the
temporary decoded module is freed and the caller's empty output module remains
unchanged. The ZRAF validator preserves that source offset and clears its
validated output. This keeps typed binding-row data outside the canonical
hotpatch admission path until its full validation contract is defined.

Focused CTest coverage is provided by ssa_capability_validation,
ssa_canonical_zraf_validation, and
ssa_rollback_restricted. The tests exercise a valid closure, escalation,
unknown flags, signature/content failures, idempotent application, fresh
generation rollback, and restricted-section rejection. The
[canonical ZRAF validation acceptance](../../tests/acceptance/ssa-hotpatch-canonical-zraf-validation.md),
[Apply-time content mutation acceptance](../../tests/acceptance/ssa-hotpatch-content-mutation.md)
records that sequential byte changes are rejected before registry/generation
publication. The canonical fixture also records callback ordering, full-width
length rejection, and policy/identity snapshot behavior under callback writes.
The EIS6 guard fixture requires successful public scalar decoding and Core
verification for typed-empty and typed-CALL modules before checking that the
canonical module opener and ZRAF validator reject them with the scalar payload
offset and no published output. The new guard cases passed with the EIS6
codec on MSVC 19.44, GCC 11.4 and Clang 14 in the current SSA artifact caches
under `D:/tmp/zr_vm`. The GCC/Clang 19-suite runs passed this canonical gate
while separately exposing an obsolete conditional-cleanup fixture. The guard
therefore has evidence of successful public decode followed by canonical
rejection; rejection by the previous scalar reader is not used as evidence.
Earlier current-root Clang evidence covers ten incremental build targets (85/85 build
steps) and four focused CTest cases (4/4, 3.65 seconds total): artifact writer
(2.45 s), artifact roundtrip (0.14 s), canonical ZRAF validation (0.03 s), and
legacy capability validation (0.01 s). Earlier current-source MSVC evidence also
records a five-target incremental build (352/352) and its prior foundation
CTest gate. The latest current-root GCC incremental build covered thirteen targets (107/107 steps, exit code 0); its selected CTest gate passed 13/13 cases in 71.52 seconds, including artifact writer (12.47 s), roundtrip (2.38 s), canonical ZRAF validation (0.89 s), and legacy capability validation (0.76 s). Logs: D:/tmp/zr_vm/ssa-control/gcc-current-thirteen-target-build.log and D:/tmp/zr_vm/ssa-control/gcc-current-thirteen-ctests.log. These focused results do not complete 08.02: full canonical signed manifest integration and capability-closure derivation from the complete verified IR remain open. Availability is explicit: an unsupported capability
or backend is never reported as accepted.
