---
related_code:
  - zr_vm_core/include/zr_vm_core/capability_manifest.h
  - zr_vm_core/include/zr_vm_core/module.h
  - zr_vm_core/include/zr_vm_core/exec_ir.h
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_validate.c
  - zr_vm_core/src/zr_vm_core/module/module_exec_ir_artifact.c
  - zr_vm_core/src/zr_vm_core/artifact_identity.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/capability_manifest.h
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_validate.c
plan_sources:
  - docs/plans/ssa/08-artifact-hotpatch/02-capability-validation.md
tests:
  - tests/library/test_ssa_exec_ir_artifact_v6.c
  - tests/library/test_ssa_canonical_zraf_validation.inc
  - tests/cmake/ssa-tests.cmake
doc_type: testing-guide
status: focused-passed
---

# Canonical ZRAF validation boundary

## Contract

`ZrCore_HotPatch_ValidateZraf` validates the complete canonical ZRAF span. The
public input and token retain `outerLength` as `TZrSize`. Before reading bytes,
the validator rejects spans above `ZR_ARTIFACT_MAX_BYTE_LENGTH` or the
`UInt32` hash/callback limit. Artifact readers receive the original size; only
hash/callback calls use a checked narrowing. The caller's full size remains in
the structured limit diagnostic.

The manifest content hash and host expected hash must both equal the hash of
the outer bytes. The validator snapshots host policy, expected identity, and
the capability closure before invoking the signature callback with the full
outer span. It rehashes after the callback, then reads the outer artifact,
checks its public identity, opens and verifies the canonical ExecIR module,
and selects exactly one function by the explicit `(entryFunctionToken,
entrySignatureHash)` pair. The current canonical reader accepts only a
single-function module, so a duplicate matching selector cannot be represented
by this fixture; the validator's uniqueness check remains defensive.

The capability requirement limit and closure are checked before invoking the
signature callback. The callback runs before deep outer/ExecIR parsing. If the
post-callback hash differs from the pre-callback hash,
`ZR_HOT_PATCH_CONTENT_CHANGED` is returned and the output token stays zeroed.
Structural rejection cases use an accepting callback and assert it ran once;
pre-callback length/hash/policy rejections assert it did not run. The successful
token contains by-value identity, selector, and policy fields plus a borrowed
byte pointer, length, and hash.

The callback authenticates the outer bytes. The manifest and expected identity
are host-supplied inputs; capturing them does not claim they were covered by the
signed byte stream. The byte allocation is not copied or pinned. The caller
must keep it alive and stable from the initial hash through signature
verification and structural decode, including while the callback runs; the
callback must treat the span as read-only. Writers must be externally
synchronized with validation and each later
`ZrCore_HotPatch_RecheckZrafContent` call. The check rejects when the
post-callback hash differs from the pre-callback hash. It cannot detect a
mutation restored before return or make concurrent writes atomic. Recheck sees
only changes visible when it recomputes the hash while storage remains live;
it does not own or pin the bytes. This boundary does not install or execute
code, resolve relocation, or create a generation-owned payload.

## Test coverage

The `ssa_canonical_zraf_validation` CTest reuses the canonical V1 fixture and
its writer to produce a second V2 artifact with a distinct outer content hash.
It checks full-span signature callbacks and hashes for both artifacts, token
snapshots, successful recheck, and that callback writes to the caller's
manifest scalars, requirement row, and expected identity do not replace the
pre-callback token snapshot. It also covers these rejection cases:

- mismatched expected root identity;
- missing or mismatched entry selector;
- malformed outer metadata and unsupported nested ExecIR ABI;
- mismatch between the manifest full-outer hash and actual ZRAF bytes;
- capability escalation and host signature rejection;
- a still-live buffer modified after validation;
- a signature callback that mutates the outer bytes before returning;
- an over-limit `TZrSize` span represented by a one-byte sentinel, rejected
  before reading bytes or calling the signature verifier; on 64-bit targets
  the test uses `UINT32_MAX + 1` to catch premature narrowing.

Every rejection checks the structured hotpatch status and that the output token
was cleared. Rejections with a concrete mismatch also check the corresponding
expected/actual values or source offset.
The current canonical reader accepts only one function, so a duplicate
`(entryFunctionToken, entrySignatureHash)` pair cannot be represented by a
valid fixture; the implementation's uniqueness guard remains defensive.

## Validation evidence

The test-first missing-API RED was observed with the MSVC single-object build
for `zr_vm_ssa_exec_ir_artifact_v6_test`: the new `SZrValidatedHotPatchZraf`
type and `ZR_HOT_PATCH_CONTENT_CHANGED` status were undefined, producing the
expected compile failure and cascading diagnostics. No functional test ran in
that RED build.

Two current-source Core builds exposed compile integration errors before
functional tests ran. The first showed that the new public header used
`SZrArtifactPublicIdentity` and `TZrMetadataToken` without directly including
`artifact_schema.h`; that include is now explicit. The next showed the ExecIR
diagnostic enum member is `code`, not `status`; the validator now reads
`execIrDiagnostic.code`.

An earlier current-source MSVC incremental build covered five targets and completed
352/352 build steps successfully (exit code 0). A fresh foundation CTest gate
group passed 6/6, covering `ssa_exec_ir_artifact_v6_write`,
`ssa_exec_ir_artifact_v6_roundtrip`, `ssa_canonical_zraf_validation`, and
`ssa_capability_validation`. The focused canonical ZRAF and adjacent legacy
capability gates now pass; this does not mark the wider 08.02 milestone
accepted.

An earlier current-root Clang incremental build covered ten targets and completed
85/85 build steps successfully (exit code 0). The four current-source focused
CTest cases passed 4/4 in 3.65 seconds total:
`ssa_exec_ir_artifact_v6_write` (2.45 s),
`ssa_exec_ir_artifact_v6_roundtrip` (0.14 s),
`ssa_canonical_zraf_validation` (0.03 s), and
`ssa_capability_validation` (0.01 s). Root captured the build output in
`D:/tmp/zr_vm/ssa-control/clang-current-ten-target-build.log` and the test
output in `D:/tmp/zr_vm/ssa-control/clang-zraf-current-ctests.log`.
The build log contains four `-Wmissing-braces` warnings in the ExecBC VM test
fixture; they did not fail the build and are outside this ZRAF slice.

The latest current-root GCC incremental build covered thirteen targets and completed 107/107 build steps successfully (exit code 0). The selected CTest gate passed 13/13 cases in 71.52 seconds total. Its four ZRAF and capability cases passed: artifact writer (12.47 s), artifact roundtrip (2.38 s), canonical ZRAF validation (0.89 s), and legacy capability validation (0.76 s). Root captured the logs in D:/tmp/zr_vm/ssa-control/gcc-current-thirteen-target-build.log and D:/tmp/zr_vm/ssa-control/gcc-current-thirteen-ctests.log. The wider incremental build log also contains non-failing GCC warnings in binding-row, semantic-analysis, and ExecBC VM sources outside this ZRAF slice.

These are focused validator and legacy capability gates. Full canonical signed
manifest integration and capability-closure derivation from the complete
verified IR remain open under 08.02.
