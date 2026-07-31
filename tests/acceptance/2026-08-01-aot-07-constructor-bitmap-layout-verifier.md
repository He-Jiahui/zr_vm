# 2026-08-01 AOT 07 Constructor Bitmap Layout Verifier

## Scope

This A7.2D sub-milestone makes the runtime-owned constructor receiver initialization bitmap a verified AOT frame ABI
input. The public core query derives the uint64 tail from the canonical receiver TypeLayout, and ExecIR calls it before
code stripping. A valid row is a direct inline-struct constructor parameter at stack slot 0. Every physical frame-slot
storage envelope must end before the aligned bitmap tail.

This slice does not add field-initialization dataflow, per-field cleanup lowering, receiver/return/destination slot
derivation, a new reachability manifest schema, or complete A7.2.

## RED Evidence

Effective baseline was commit `3d67352` plus the final test overlay. The initial public layout query accepted a
receiver TypeLayout after its schema hash was corrupted, so the core suite reported 37 passes and 1 failure out of 38;
the AOT stripping suite remained 34/34 because its initial fixture matrix did not mutate canonical layout identity.

The first hardening attempt applied canonical `cTypeId` and payload-shape checks to the generic resolver path. It fixed
the new public-query test but regressed the existing custom-resolver constructor unwind test, again producing 37/38.
The final implementation keeps schema/hash and payload-shape validation universal while requiring canonical `cTypeId`
identity only for the two public APIs used by AOT and normal runtime lookup.

Independent review then found that the generic switch also relaxed payload size/alignment. A hash-valid custom
size-drift case reproduced this as 37/38: malformed shape still entered partial Drop. Shape matching is now universal,
while only `cTypeId` is resolver-local. A second review requested direct coverage for duplicate bitmap flags after
successful layout resolution; the added physical-envelope-valid second row is rejected before any Drop.

## Coverage Inventory

- Accepts a canonical constructor receiver with a uint64-aligned bitmap tail.
- Rejects non-parameter and non-slot-0 bitmap rows, non-constructors, zero-field receivers, missing tails, and portable
  under-alignment when the platform's uint64 alignment permits a smaller alignment.
- Rejects receiver TypeLayout schema/hash corruption, `cTypeId` drift, byte-size drift, and byte-alignment drift.
- Accepts direct, indirect-alias, and borrowed-alias storage that ends before the tail; rejects each storage envelope
  when it overlaps the tail, using the runtime binding size for alias rows.
- Resets public offset/count/pointer outputs on failure.
- Preserves the existing custom-resolver `cTypeId` behavior while requiring schema/hash and exact payload shape.
- Rejects custom size/alignment drift and duplicate bitmap flags before custom or field Drop can touch receiver storage.
- Rejects every malformed unreachable owner before filtering and leaves the generated output absent.

## Tooling Evidence

Frozen effective source is commit `3d67352` plus the exact A7.2D production/test overlays:

- WSL source: `/home/hejiahui/codex-validation/zr_vm-aot12-20260801-a72d-3d67352`
- Windows source: `C:\Users\HeJiahui\AppData\Local\Temp\zr_vm-aot12-20260801-a72d-3d67352`
- GCC 11.4.0, Clang 14.0.0, and MSVC 19.44.35228.0

Focused commands built and ran `zr_vm_type_layout_inline_copy_test` and
`zr_vm_aot_c_code_stripping_test` from each compiler-specific build directory. GCC also built and ran
`zr_vm_aot_c_generic_reference_sharing_test` as the adjacent generic dictionary regression.

## Results

- WSL GCC: core inline-copy/layout 38/0; AOT code stripping 34/0; generic reference sharing 9/0.
- WSL Clang: core inline-copy/layout 38/0; AOT code stripping 34/0.
- Windows MSVC x64 Debug: core inline-copy/layout 38/0; AOT code stripping 34/0.
- The three changed code/test files have identical SHA-256 hashes in the main tree, WSL snapshot, and Windows snapshot:
  `127429be...d3d7be`, `098b7268...5f4f96`, and `635b9047...d04d`.
- `constructor_bitmap_layout.c` is absent after the final rejected writer attempt in all three build trees.
- `git diff --check` passes for the implementation and focused tests; MSVC retains only pre-existing build warnings,
  including MSB8029 because the isolated build is below `%TEMP%`.
- Independent review's custom-resolver shape P2 and duplicate-flag coverage P3 were reproduced and fixed; final
  re-review returned `No findings`.

## Acceptance Decision

Accepted at `2026-08-01 03:28:59 +08:00` as AOT 07 A7.2D's constructor bitmap layout verifier and an AOT 12
pre-filter graph-input gate. A7.2, AOT 07, AOT 12, and the broader AOT 07-12 goal remain active.
