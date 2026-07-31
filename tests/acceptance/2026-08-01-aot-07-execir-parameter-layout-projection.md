# 2026-08-01 AOT 07 ExecIR Parameter Layout Projection

## Scope

This A7.2G sub-milestone projects the A7.2F-verified runtime parameter prefix into an internal ExecIR frame sidecar and
makes MethodInfo signatures consume that slot-aligned projection. It also validates the structural bounds of legacy
parameter metadata before code stripping. No public function, artifact, or manifest schema changes.

This slice does not prove metadata completeness or name order, compare TypeId with TypeRef, TypeLayout, or
CallableContract, derive passing direction/defaults, define return/destination/spill/address-taken ABI, or complete A7.2.

## RED And Review Evidence

- With the unchanged A7.2F backend, the receiver-alignment test reported 1 pass and 1 failure because the explicit AST
  metadata row occupied runtime parameter slot 0 and the receiver type was absent.
- The code-stripping suite reported 36 passes and 1 failure because a nonzero metadata count with a null table was
  accepted on an unreachable function.
- After the first implementation, independent review found that partial legacy metadata was still copied from slot 0
  and that equal typed-local/metadata test types did not prove authority. A conflicting U64 metadata row and a
  no-typed-local 2-to-1 count case produced a 2/3 RED result.
- Legacy copying was restricted to equal counts; both runtime slots stay unknown when the mapping is ambiguous.
  Independent final re-review returned `No findings.`

## Coverage Inventory

- Projects receiver and explicit typed-local rows in verified producer order with stack slot, canonical identity, role,
  and TypeRef fields.
- Proves typed-local TypeRef authority with conflicting valid metadata while preserving receiver/explicit slot order.
- Keeps complete equal-count legacy metadata index-addressable and zero-count metadata legal.
- Leaves every projected type unknown when a partial legacy metadata table cannot prove its receiver/synthetic offset.
- Rejects nonzero/null metadata storage and metadata count overflow before filtering an unreachable owner; each rejected
  write immediately checks that partial generated output was removed.
- Owns and releases parameter and frame-slot sidecars through the same frame-layout lifetime boundary.

## Tooling Evidence

Frozen effective source is the `3d67352` baseline plus the committed A7.2D-A7.2F overlays and the exact A7.2G
production/test overlay:

- WSL source: `/home/hejiahui/codex-validation/zr_vm-aot12-20260801-a72d-3d67352`
- Windows source: `C:\Users\HeJiahui\AppData\Local\Temp\zr_vm-aot12-20260801-a72d-3d67352`
- GCC 11.4.0, Clang 14.0.0, and MSVC 19.44.35228.0 x64 Debug

The Windows matrix was accepted only after `cmake --build ... --target clean` and a full rebuild. A prior incremental
overlay preserved old source timestamps and mixed objects compiled against different private ExecIR layouts.

## Results

- WSL GCC: MethodInfo 3/0; code stripping 37/0; SemIR 10/0; generic sharing 9/0; debug metadata 6/0;
  value-SemIR 8/0; typed-call 4/0.
- WSL Clang: MethodInfo 3/0; code stripping 37/0; SemIR 10/0; generic sharing 9/0; debug metadata 6/0;
  value-SemIR 8/0; typed-call 4/0.
- Windows MSVC x64 Debug: MethodInfo 3/0; code stripping 37/0; SemIR 10/0; generic sharing 9/0; debug metadata 6/0;
  value-SemIR 8/0; typed-call 4/0.
- Main, WSL, and Windows copies have identical SHA-256 values for all eight implementation/test files:
  `0587071e1bccbe4722a89dfcc5a61c96e81794f4921c294bc47087e846be1a0f`,
  `280b598372cb44770fd11d14c4a172d6a945599ad49182443ad036f400cc0350`,
  `15db3a15180a13fc1f710c597876cec849c7d8a4a798d697a1c9d274a533497f`,
  `c24f0f9e6cf31eaeb360e176f1ae9d78ce66e05edfbde437e20cd0f3791c37a3`,
  `ffdf3188c193ed3223c3ad25dc186e41e8fe609ea746f75c00f3a2c5ad9a77f1`,
  `af04d80530ab4749b751b730bb2d3707bdea5523326760a998726dd5529e6ade`,
  `3d33e64c314cf2c8f455a62cdd07f5ee3e575cbcaaf940c5b3a38cc42c86d422`, and
  `f8f13d3549ef024ede804c0cee5ec11d2a4e4f6834d9f0345703b0ee1ac0d145`.
- `git diff --check` passes. MSVC retains only the existing temporary-directory MSB8029 and third-party warnings.

## Baseline Deviations

- GCC `zr_vm_aot_c_call_shared_library_smoke_test` remains 4/5. The binary-input dynamic-call writer failure reproduces
  with A7.2F production files from `c09091b`, and A7.2G metadata validation is not reached.
- The frame-setup static source contract remains 0/1 on its pre-existing `includeStackFrameSetup` text expectation.
  Neither deviation is part of the parameter projection contract or changed by this sub-milestone.

## Acceptance Decision

Accepted at `2026-08-01 06:46:40 +08:00` as AOT 07 A7.2G's slot-aligned ExecIR parameter projection and an AOT 12
pre-filter metadata-shape gate. A7.2, AOT 07, AOT 12, and the broader AOT 07-12 goal remain active.
