# Syntax 09 M4 artifact, reflection and LSP acceptance

Date: 2026-08-04

## Status

- State: `proven` for M4; Gate 09 remains open at M5.
- Plan: `docs/plans/syntax/2026-07-19-09-generational-pool-handle-ref-struct-design.md`.
- Scope: canonical artifact/native/reflection contract identity, corrupt layout
  rejection, native ref-like reflection boundaries, and capability-driven LSP
  hover/completion.

## RED evidence

1. Native `PoolRef` reflection returned ordinary `STRUCT(4)` instead of
   `REF_STRUCT(7)`. The same descriptor exposed its `runtimeOnly` guard backing
   field and hidden projection method rather than a visible property.
2. The name-independent LSP contract test failed to link because no classifier
   consumed protocol/role facts.
3. After the focused implementation, the full project-features suite exposed
   two positive fixtures still using removed keywordless function declarations.
   The parser correctly rejected `Add(...): i32` and `pub usePlugin(): int`, so
   the external-metadata semantic-token assertion failed until those fixtures
   were migrated to canonical `fn`.

## Implemented contract

- Native ref-like structs are categorized from the registered `REF_LIKE`
  protocol, not a concrete type name or a source-only modifier row.
- Native reflection omits fields marked `runtimeOnly`. A method with the
  `POOL_REF_PROJECTION` role and valid structured metadata becomes one visible
  getter-only property with exact readonly/writable reference access; the
  hidden provider method is not queryable.
- The reflected view is non-constructible. Reflection describes its getter and
  lifetime contract but never creates, boxes, or retains a direct ref/guard.
- LSP classification uses only protocol bits, handle identity roles, acquire
  roles, projection role, and structured reference access. A direct test uses
  arbitrary renamed types and acquire members to prevent name-based inference.
- Hover identifies weak identity, stable slot source, scoped readonly/writable
  ref, getter-only projection, and active stable-slot guard lifetime. Completion
  lists acquisition methods on the source prototype and does not fabricate them
  on the handle.
- Existing artifact coverage remains authoritative: source/native/binary/
  reflection contract hashes match, while corrupt, missing, zero, dangling, and
  unknown layout capability records fail closed.

## Verification

- WSL GCC 11.4 Debug `zr_vm_generational_pool_test`: 14/14.
- WSL GCC 11.4 Debug `zr_vm_language_server_lsp_project_features_test`: full
  suite completed with no `Fail -`; the external metadata semantic-token case,
  name-independent stable-slot classifier, and pooling hover/completion case all
  passed.
- Fresh WSL rerun immediately before commit: pool 14/14 and the complete LSP
  project-feature suite completed with no failing case.
- `git diff --check`: clean for the intended change set.

## Review decision

No concrete provider-name dispatch remains in the new reflection or LSP paths.
Runtime storage stays hidden, the public ref property is getter-only, and the
view cannot be reflected into a long-lived object. M4 is proven. Allocation
count and GC pause/scan work comparisons remain M5 responsibilities and keep
Gate 09 open.
