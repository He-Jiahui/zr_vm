# SSA Place promotion eligibility acceptance

## Scope

This checkpoint establishes the canonical screening contract consumed by
ExecIR SSA construction. It does not perform mem2reg rewriting or phi
insertion.

The compiler marks a SemanticIR local scalar only when its canonical type node
is primitive. During lowering, every Place result receives
`ZR_EXEC_IR_VALUE_FLAG_PLACE_ADDRESS`. A direct scalar local additionally
receives `ZR_EXEC_IR_VALUE_FLAG_PROMOTABLE_PLACE` only when it is not a
parameter and its root has no projection, loan, or escape fact. All other
Places retain explicit memory semantics.

The parser SSA pass and core verifier both reject the promotable flag without
the Place-address flag.

## Focused evidence

On 2026-09-19, the MSVC, WSL GCC 11.4, and WSL Clang 14 debug builds under
`D:\zr-ssa-verify-871bc234` built the new production source. Each toolchain
passed the same six-test builder/value gate:

```text
ssa_builder_cfg
ssa_builder_dominance
ssa_builder_control_edges
ssa_builder_fact_identity
ssa_place_eligibility
ssa_value_validation

100% tests passed, 0 tests failed out of 6
```

The producer executable also passed 19 tests with zero failures on all three
toolchains.

The eligibility fixture covers a direct scalar local, parameter, non-scalar
local, address-taken local, escaped local, and projected Place. The value
validation fixture covers the new flag dependency and a truly unknown flag.

## Remaining 01.02 work

Eligible Places are still represented by `PLACE_BASE`, `LOAD`, and `STORE`.
Dominance-frontier phi insertion, liveness pruning, renaming, loop-carried
values, and exceptional-edge availability remain open and must not be inferred
from this checkpoint.
