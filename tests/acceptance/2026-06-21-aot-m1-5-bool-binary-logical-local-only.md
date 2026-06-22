# AOT M1.5 / 07-S5 Bool Binary Logical Local-Only Opcode Path

Timestamp: 2026-06-22 01:42:03 +08:00

Status: complete for this slice; 07-S5 remains partial; 08-12 not started.

## Scope

- Optimize `LOGICAL_AND` and `LOGICAL_OR` bytecode lowering when both operands and the destination are proven bool scalar locals.
- Keep the existing frame-slot fallback for unproven operands or destinations.
- Preserve source-level `&&` / `||` behavior, which usually lowers to short-circuit branches before these opcodes.

## Changes

- Added `backend_aot_c_write_bool_binary_scalar_local()` in the generic logical C lowering module.
- `backend_aot_write_c_direct_logical_and()` and `backend_aot_write_c_direct_logical_or()` now receive the instruction index and emit local-only `zr_aot_bN` assignments when bool result-skip and left/right written-before proofs hold.
- Scalar-local write and consumer analysis now recognizes `LOGICAL_AND` and `LOGICAL_OR` for declared bool destinations and bool source reads.
- The logical source contract now locks the scalar-local marker, proof checks, call-site instruction index, and scalar-local analysis coverage.

## RED / GREEN

- RED: `zr_vm_aot_c_logical_contracts_test` failed 1/4 with missing `backend_aot_c_write_bool_binary_scalar_local`.
- GREEN: focused validation passed logical contracts 4/0, logical shared-library smoke 4/0, source contracts 19/0, and aggregate shared-library smoke 8/0.
- GREEN: broader focused AOT validation passed call contracts 4/0, call shared-library smoke 3/0, aggregate shared-library smoke 8/0, power contracts 2/0, power shared-library smoke 1/0, source contracts 19/0, generic numeric contracts 1/0, generic numeric shared-library smoke 1/0, global shared-library smoke 9/0, logical contracts 4/0, logical shared-library smoke 4/0, typed scalar 1/0, return contracts 1/0, and frame setup contracts 1/0.
- Note: the first long validation command timed out after 244 seconds in the tool harness; the same build and test binaries passed when split into shorter commands.

## Static Checks

- Generator source contains `zr_aot_bool_binary_scalar_local`, bool result-skip proof, and left/right bool written-before proof.
- Function-body dispatch passes `instructionIndex` into `backend_aot_write_c_direct_logical_and/or()`.
- Scalar-local analysis records and consumes bool evidence for `LOGICAL_AND` and `LOGICAL_OR`.
- Fallback `zr_aot_bool_logical_and` / `zr_aot_bool_logical_or` and `ZR_VALUE_FAST_SET` remain for unproven paths.
- `git diff --check` exited 0 with only existing LF/CRLF warnings.

## Notes

This is an opcode generator-shape slice. Source `&&` / `||` generally lowers to short-circuit branches before `LOGICAL_AND` / `LOGICAL_OR`, so source-level shared-library smoke is used as regression coverage rather than proof that generated source necessarily contains the new marker.
