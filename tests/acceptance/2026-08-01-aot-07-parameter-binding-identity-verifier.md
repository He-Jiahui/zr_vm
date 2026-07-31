# 2026-08-01 AOT 07 Parameter Binding Identity Verifier

## Scope

This A7.2F sub-milestone validates the producer-ordered canonical parameter binding prefix before the zero-frame return
and before reachability filtering. It consumes the existing typed-local `SymbolId`/`TypeId`/`PlaceId` carrier and does
not alter the function schema or the version 1 frame manifest.

This slice does not compare parameter TypeId with TypeRef, TypeLayout, or CallableContract, derive passing direction or
defaults, create return destinations, spills, or address-taken slots, modify the parser producer, or complete A7.2.

## RED Evidence

The frozen source originated at `3d67352` and carried the committed A7.2D/A7.2E overlays. The RED used the unchanged
A7.2E backend (`aefee791...9ebdfa9`) plus only the A7.2F focused test. WSL GCC reported 35 passes and 1 failure out of
36 because an unreachable function with a partial parameter identity tuple was accepted. The other 35 code-stripping
regressions remained green.

Independent review found no implementation or extraction defect and identified two P3 coverage gaps: nameless
non-receiver rows had no explicit skip positive, and the all-zero legacy set had no slot-failure negatives. Those cases
were added before the final matrix; independent re-review returned `No findings.`

## Coverage Inventory

- Accepts complete canonical tuples, an all-zero legacy tuple set, equal parameter TypeIds, and incomplete identity on
  an ordinary local after the parameter prefix.
- Accepts nameless non-receiver rows before and between selected parameter rows without consuming the prefix.
- Rejects mixed canonical/legacy availability and each missing SymbolId, TypeId, or PlaceId component.
- Rejects out-of-range or duplicate parameter stack slots for both canonical and all-zero legacy identity sets.
- Rejects duplicate canonical SymbolId or PlaceId, insufficient eligible rows, and a receiver after the prefix.
- Rejects malformed unreachable owners before filtering and removes the partial generated output after every failure.

## Tooling Evidence

Frozen effective source is the `3d67352` baseline plus committed A7.2D/A7.2E and the exact A7.2F production/test
overlays:

- WSL source: `/home/hejiahui/codex-validation/zr_vm-aot12-20260801-a72d-3d67352`
- Windows source: `C:\Users\HeJiahui\AppData\Local\Temp\zr_vm-aot12-20260801-a72d-3d67352`
- GCC 11.4.0, Clang 14.0.0, and MSVC 19.44.35228.0

Each compiler-specific build ran `zr_vm_aot_c_code_stripping_test`,
`zr_vm_aot_c_generic_reference_sharing_test`, `zr_vm_debug_metadata_test`, `zr_vm_semir_pipeline_test`, and
`zr_vm_aot_c_value_semir_contracts_test`.

## Results

- WSL GCC: code stripping 36/0; generic sharing 9/0; receiver metadata 6/0; SemIR pipeline 10/0; value-SemIR 8/0.
- WSL Clang: code stripping 36/0; generic sharing 9/0; receiver metadata 6/0; SemIR pipeline 10/0; value-SemIR 8/0.
- Windows MSVC x64 Debug: code stripping 36/0; generic sharing 9/0; receiver metadata 6/0; SemIR pipeline 10/0;
  value-SemIR 8/0.
- Main, WSL, and Windows copies have identical SHA-256 values for the six changed implementation/test files:
  `296e5d2be3edad2340c384122ad54c421e339f8c3be236543edec7a9d0a7d486`,
  `187d2752d233c93e24cde4da10d4325999e9ec12fa6cbcf741308f2e2a7142b2`,
  `4574c1273aac27c3b1b12b8758eab4ebe685e444d46b44052bccb24cf2a6c06b`,
  `316b00431c42a0883b811e845292aff0e46fd02fc74704779b694d4981baea42`,
  `db58abce287e9dc7aa327032a1b503cf7e3be2868b16d6c72960b4e7d5b2627a`, and
  `63d32624a2a2ce4e6209d82d721f2fe49ad7cae418656c1e6aec927676075a1c`.
- `malformed_unreachable_parameter_binding_identity.c` is absent after the final rejected write in all three build
  trees. `git diff --check` passes; MSVC retains only the pre-existing `%TEMP%` MSB8029 warning.

## Acceptance Decision

Accepted at `2026-08-01 05:11:35 +08:00` as AOT 07 A7.2F's canonical parameter binding identity verifier and an AOT
12 pre-filter owner gate. A7.2, AOT 07, AOT 12, and the broader AOT 07-12 goal remain active.
