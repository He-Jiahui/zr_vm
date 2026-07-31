# 2026-08-01 AOT 07 Receiver Role Frame Verifier

## Scope

This A7.2E sub-milestone consumes the existing patch-38 canonical receiver role as a verified ExecIR frame input.
Validation runs before the zero-frame return and before reachability filtering, so malformed metadata on an unreachable
owner cannot disappear during code stripping.

This slice does not synthesize missing receiver roles, derive parameter direction/type, create return destinations,
spills, or address-taken slots, add a reachability schema, or complete A7.2.

## RED Evidence

The frozen code baseline originated at `3d67352` with the committed A7.2D overlays; main's later changes through
`74a15ab` were documentation-only for these validation paths. The RED used that frozen code plus the exact A7.2E test
overlay. The existing writer accepted an unknown typed-local role bit on an unreachable function, producing 34 passes
and 1 failure out of 35 on WSL GCC. The other 34 code stripping regressions remained green.

Independent review then identified a complete-frame interaction: the A7.2C parameter classifier skipped a canonical
receiver when its display name was absent, so it reversed the parameter markers for slots 0 and 1. Upgrading the
fixture from sparse to a complete two-row frame reproduced the issue at 34/35 before the classifier counted the
receiver role structurally. Final independent re-review returned `No findings.`

## Coverage Inventory

- Accepts a nameless canonical receiver in complete, sparse, and zero-frame layouts.
- Accepts a role-free older artifact when the established named parameter metadata remains available.
- Rejects a nonempty null typed-local table and every unknown role bit.
- Rejects duplicate receiver roles and receiver rows missing `SymbolId`, `TypeId`, or `PlaceId`.
- Rejects a receiver outside stack slot 0, on a zero-parameter function, or on a materialized non-parameter frame row.
- Rejects malformed unreachable owners before filtering and removes the partial generated output.

## Tooling Evidence

Frozen effective source is the `3d67352` code baseline plus the committed A7.2D and exact A7.2E production/test
overlays; main's later AOT traceability commits are documentation-only for these validation paths:

- WSL source: `/home/hejiahui/codex-validation/zr_vm-aot12-20260801-a72d-3d67352`
- Windows source: `C:\Users\HeJiahui\AppData\Local\Temp\zr_vm-aot12-20260801-a72d-3d67352`
- GCC 11.4.0, Clang 14.0.0, and MSVC 19.44.35228.0

Each compiler-specific build ran `zr_vm_aot_c_code_stripping_test`,
`zr_vm_aot_c_generic_reference_sharing_test`, and `zr_vm_debug_metadata_test`. The last target is the existing binary
and runtime roundtrip evidence for the upstream receiver role carrier.

## Results

- WSL GCC: AOT code stripping 35/0; generic reference sharing 9/0; receiver metadata roundtrip 6/0.
- WSL Clang: AOT code stripping 35/0; generic reference sharing 9/0; receiver metadata roundtrip 6/0.
- Windows MSVC x64 Debug: AOT code stripping 35/0; generic reference sharing 9/0; receiver metadata roundtrip 6/0.
- Main, WSL, and Windows copies have identical implementation/test SHA-256 values:
  `aefee7912a37525a0a0e7f236e6612abd9db9aa68d3e77d95c052831c9ebdfa9` and
  `26f3821044266b6595c11110d36ca8ee05f7f8f4bc06dbb2f55e97ffef2117e9`.
- `malformed_unreachable_receiver_role.c` is absent after the final rejected write in all three build trees.
- `git diff --check` passes. MSVC retains only the pre-existing `%TEMP%` MSB8029 warning.
- The broader source-contract static-text suite has four pre-existing text drift failures outside the frame verifier;
  its frame-layout contract passes, but the suite is not counted as A7.2E acceptance evidence.

## Acceptance Decision

Accepted at `2026-08-01 04:21:18 +08:00` as AOT 07 A7.2E's canonical receiver-role frame verifier and an AOT 12
pre-filter owner gate. A7.2, AOT 07, AOT 12, and the broader AOT 07-12 goal remain active.
