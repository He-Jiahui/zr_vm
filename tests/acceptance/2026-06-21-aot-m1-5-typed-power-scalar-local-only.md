# AOT M1.5 07-S5 typed power scalar-local result-skip

## Status

- Completed: 2026-06-22 02:28:40 +08:00
- Slice: M1.5 / 07-S5 typed power scalar-local path
- State: sub-slice complete; 07-S5 and M1.5/07 remain partially complete; 08-12 not started.

## Scope

- `POW_SIGNED`, `POW_UNSIGNED`, and `POW_FLOAT` now use scalar-local generated C when destination can skip its value slot and left/right operands are already written as matching scalar locals.
- The proven path emits `zr_aot_arith_exec_*_power_scalar_local` markers and writes `zr_aot_sN`, `zr_aot_uN`, or `zr_aot_fN` directly.
- Unproven paths keep existing frame/value-slot fallback and `ZR_VALUE_FAST_SET`.
- Scalar-local analysis declares power destinations and only the immediate constants that are actually used as typed power operands.

## Evidence

- RED: power contracts failed before implementation because typed power lowering lacked scalar-local helper/include.
- GREEN: power contracts 2/0 and power shared-library smoke 1/0 after local-only implementation.
- Regression found: broad immediate-constant scalar-local declaration broke typed scalar/call shared-library assumptions by skipping value-slot writes for constants still needed by runtime boundaries.
- Fix: immediate constant declaration is now scoped to slots used as typed power operands.
- Final focused: call shared-library smoke 3/0, typed scalar 1/0, power contracts 2/0, power smoke 1/0.
- Broad focused group passed: call contracts 4/0, call shared-library smoke 3/0, aggregate shared-library smoke 8/0, power contracts 2/0, power smoke 1/0, source contracts 19/0, generic numeric contracts/smoke 1/0 + 1/0, global smoke 9/0, logical contracts/smoke 4/0 + 4/0, typed scalar 1/0, return contracts 1/0, frame setup contracts 1/0.

## Generated C Checks

- Power smoke generated C contains direct assignments for signed, unsigned, and float typed power local results.
- It rejects generic `zr_aot_left_scalar` / `zr_aot_right_scalar` temp-template usage and destination stack lookup for local-only power result slots.
