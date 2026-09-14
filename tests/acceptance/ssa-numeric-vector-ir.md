# SSA numeric/vector IR contract acceptance

## Scope

This slice implements the 09.02 parser-owned numeric policy and target
legalization contract:

- strict checked/wrapping/saturating integer and IEEE/NaN/signed-zero modes;
- independent reassociation, FMA, approximate reciprocal, no-signed-zero, and
  unordered-reduction permissions;
- project/package/target permission intersection with identity hashing;
- fixed-width vector operation/type rows and explicit ordered scalar fallback.

## Focused fixture

`tests/parser/test_ssa_numeric_vector_ir.c` covers:

- strict defaults and deterministic policy hashes;
- malformed enum/bit/semantic combinations and diagnostic codes;
- FMA-only permission isolation (no implicit reassociation/reduction);
- strict package isolation from project permissions;
- NaN/`+0`/`-0` preservation flags;
- target, lane, alignment, empty/tail shape, and ordered-reduction fallback;
- target-contract hash invalidation.

## Reproducible commands

The fixture is intentionally standalone until its owner registers the planned
CTest target.  From the repository root:

```text
gcc -std=c11 -Wall -Wextra -Wpedantic -Wstrict-prototypes \
  -Wmissing-prototypes -Werror \
  -Izr_vm_common/include -Izr_vm_core/include \
  tests/parser/test_ssa_numeric_vector_ir.c \
  zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_numeric_policy.c \
  zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_vector_legalize.c \
  -o build/test_ssa_numeric_vector_ir
build/test_ssa_numeric_vector_ir
```

The same source set should be run with Clang and GCC sanitizer builds after
the parent task adds `zr_vm_ssa_numeric_vector_ir_test` and CTest
`ssa_numeric_vector_ir` in `tests/cmake/ssa-tests.cmake`.  This slice does not
edit that shared registration point.

## Evidence and remaining gate

The implementation has no dynamic allocation, ownership transfer, cancellation
state, or partial-initialization cleanup path.  Null inputs are rejected and
diagnostics are optional; all successful fallback decisions remain explicit in
the result.  Performance claims are intentionally omitted until the complete
SSA differential/performance matrix is wired by the parent milestone.
