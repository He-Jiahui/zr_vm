# SSA 01.02: builder definition identity preflight

## Scope

`ZrParser_ExecIr_Build` now requires the bidirectional SemanticIR definition
contract already enforced by `ZrParser_SemanticIr_Validate`. A zero cached
definition denotes an external-entry value only when no instruction writes
it. This is a builder preflight, not phi insertion or local promotion.

## Regression

The focused `ssa_builder_fact_identity` fixture checks a missing cache on an
instruction result, a cache pointing at a nonproducer, a cache with no writer,
two instructions writing one ValueId, and an out-of-range cached instruction
ID. All five report `INVALID_VALUE` with the offending instruction ID and
expected/actual identity; a nonexistent instruction has source ID zero rather
than a fabricated source map entry. The caller's previously allocated block
and value counts remain intact. The positive fixture covers a matching
constant result and a genuinely external value. CFG, dominance,
iterator-invoke, control-edge, cleanup-dispatch, and exception-state fixtures
now declare actual producer IDs; their negative cases still exercise their
original downstream diagnostics.

## Evidence

Before the production change, the GCC fixture failed with `builder accepted
an inconsistent definition or changed output`. After compiling the real
builder and adjacent tests with the existing WSL GCC 11.4 Ninja-cache compiler
and linker commands, the adjacent eleven-test CTest selection passed 11/11:

```text
ctest --test-dir /home/hejiahui/zrvm-ssa-nested-gcc.4pVemu \
  -R '^(ssa_builder_|ssa_cleanup_exception_state|ssa_linear_effects_builder|ssa_cfg_effects_builder|ssa_place_eligibility|ssa_place_promotion)' \
  --output-on-failure --no-tests=error
```

Clang 14 syntax-checked the changed builder and five fixture sources with
`-std=c11 -Wall -Wextra -Wpedantic -Werror -Wno-error=missing-braces` (exit
0); the three existing initializer-brace warnings remain. Full source-program
integration and sanitizer suites were not run for this focused preflight.

No source-program parity, phi/rename, or full 01.02 acceptance is claimed by
these builder tests. The builder file remains under 1000 lines; a future split
should extract the coherent canonical-input preflight when it grows another
responsibility.
