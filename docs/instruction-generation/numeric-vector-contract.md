---
related_code:
  - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c
  - zr_vm_parser/include/zr_vm_parser/exec_ir_profile.h
implementation_files:
  - zr_vm_core/include/zr_vm_core/exec_ir_numeric.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c
plan_sources:
  - docs/plans/ssa/09-language-simd/02-numeric-vector-ir.md
  - docs/plans/ssa/index.md
  - lua/rust/tests/auxiliary/minisimd.rs
  - lua/QuickJS-master/tests/test_bjson.js
tests:
  - tests/parser/test_ssa_numeric_vector_ir.c
doc_type: module-detail
status: implemented
---

# Strict numeric and portable vector contract

`exec_ir_numeric.h` is the parser/backend boundary for numeric semantics.  It
is a pointer-free, fixed-width side-table contract: a backend can put the
policy and legalization result in an artifact or cache key without retaining
an AST, runtime object, host address, or variable-width ExecBC instruction.
The implementation is split between policy intersection and target
legalization so that a target cannot accidentally redefine language semantics.

## Strict baseline

`ZrParser_ExecIr_NumericPolicyInit` establishes the conservative baseline:

| Concern | Baseline contract |
| --- | --- |
| integer overflow | checked (wrapping and saturating are explicit profiles) |
| shift count | masked by destination width |
| floating divide | IEEE infinities/NaN and signed-zero rules |
| NaN | preserve (payload handling remains an operation/backend detail) |
| signed zero | preserve `+0` and `-0` |
| rounding | nearest-even |
| exceptions | preserve/observe the language boundary |
| reduction | source order unless an unordered permission is present |

The `BITWISE` determinism profile is stronger than ordinary strictness for
operations represented by the contract, but it does not claim that every host
`libm` call is bit-for-bit portable.  Callers must use an explicit oracle row
for operations whose implementation is platform dependent.  This preserves
the observability tested by the local QuickJS NaN/`-0` fixtures while keeping
the fixed-width SIMD shape evidence from the Rust `minisimd` fixtures.

`SZrNumericSemanticContract` records the operation-level row (overflow, shift,
divide, NaN, rounding, exception, and signed-zero modes).  It is deliberately
data-only; arithmetic implementations must avoid relying on C signed-overflow
or compiler-wide `-ffast-math` behavior.

## Fast-math permission algebra

Fast math is five independent bits:

* `REASSOCIATE`
* `FMA`/contract
* `APPROX_RECIPROCAL`
* `NO_SIGNED_ZERO`
* `UNORDERED_REDUCTION`

`ZrParser_ExecIr_NumericPolicyIntersect` computes:

```text
effective = requested & project.allowed & package.allowed & target.allowed
```

The package mask is mandatory in this equation.  A strict package therefore
cannot inherit a project-wide opt-in, and an FMA-only package cannot acquire
reassociation or unordered reductions.  `deniedFastMath` records requested
bits that were not jointly permitted, allowing diagnostics and optimization
remarks to explain why a transformation stayed on the baseline path.

Strict IEEE/bitwise profiles reject effective weakening flags.  In a relaxed
profile, accepting `NO_SIGNED_ZERO` clears the signed-zero preservation bit;
reassociation, approximation, and unordered reduction clear exception
preservation.  These changes are visible in the returned policy and its
64-bit FNV-1a-style hash.  Hash inputs include semantic modes, all permission
masks, capability bits, and project/package/target identities.

## Vector shape and fallback

`SZrVectorLegalizationRequest` describes an operation, fixed element kind
(`f32`, `f64`, fixed-width integers, `bool`, or `mask`), logical length,
alignment, target lane count, capabilities, and optional effective policy.
The hardware width is selected here; ExecBC remains fixed width.

`ZrParser_ExecIr_LegalizeVector` returns a `SZrVectorLegalizationResult` with
the selected lanes, full-vector iterations, tail lanes, applied permissions,
semantic-preservation bits, and a contract hash.  Unsupported vector hardware,
an unsupported element/operation, insufficient alignment, denied required
permission, or an ordered reduction produces an explicit scalar fallback when
`allowScalarFallback` is true.  The fallback has `chosenLanes == 1`, preserves
the logical length and source order, and leaves a machine-readable reason;
these are successful legalizations, not silent failures.  If fallback is
forbidden, the diagnostic reports target/unsupported status.

Ordered reductions never become unordered merely because a reduction unit is
available.  A target may vectorize only when the policy and request both allow
the changed association.  Tail counts use quotient/remainder on `TZrUInt64`
lengths, so zero, one, and width-boundary lengths are deterministic and do not
depend on host pointer arithmetic.

## Validation and integration boundary

The focused fixture is `tests/parser/test_ssa_numeric_vector_ir.c`.  It checks
strict defaults, malformed policy diagnostics, permission intersection and
cross-package isolation, FMA independence, NaN/zero preservation, ordered
reduction behavior, target/alignment fallback, lane boundaries, and target
hash invalidation.  The source intentionally does not edit shared CMake or
umbrella headers; the owning SSA test registration can add the source files to
the parser target and register CTest `ssa_numeric_vector_ir`.

Standalone strict validation (before CMake registration) is:

```text
gcc -std=c11 -Wall -Wextra -Wpedantic -Wstrict-prototypes \
  -Wmissing-prototypes -Werror \
  -Izr_vm_common/include -Izr_vm_core/include \
  tests/parser/test_ssa_numeric_vector_ir.c \
  zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c \
  zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c
```

Once the parent task registers the target, the plan's CTest command remains the
source of truth for build-directory and sanitizer matrix coverage.
