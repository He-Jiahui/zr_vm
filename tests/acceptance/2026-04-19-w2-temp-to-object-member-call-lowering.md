# 2026-04-19 W2 Temp TO_OBJECT Member-Call Lowering

## Scope

This note records the W2 compiler-side fix that unblocked specialized member-call lowering when the receiver proof
exists only on temporary `TO_OBJECT` / `TO_STRUCT` slots.

Primary production file:

- `zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c`

Focused regression surface:

- `tests/parser/call_chain_polymorphic_compile_fixture.c`
- `tests/parser/call_chain_polymorphic_compile_fixture.h`
- `tests/parser/test_compiler_call_lowering_focus_main.c`

## Root Cause

The earlier W2 work already handled two pieces correctly:

1. typed array/native member paths could pass the static member-slot gate
2. callable resolution could recover imported-native members from runtime prototypes once `GET_MEMBER_SLOT` existed

The remaining gap sat earlier in quickening:

- temp receiver values created by `CREATE_OBJECT` plus `TO_OBJECT` or `TO_STRUCT` had no named typed-local binding
- the old quickening path only trusted named binding proof
- those temp receivers therefore stayed on plain `GET_MEMBER`
- without `GET_MEMBER_SLOT`, later known-call lowering could not prove the callable and generic `FUNCTION_CALL` remained

## Implementation

`compiler_quickening.c` now reconstructs receiver type proof from the instruction stream instead of requiring only a
named binding:

1. recover a `SZrFunctionTypedTypeRef` from `TO_OBJECT` / `TO_STRUCT` type-name constants
2. walk `GET_STACK` / `SET_STACK` writer chains backward until the typed source is found
3. decide member-slot eligibility from the recovered type ref
4. reuse the same recovered type ref when runtime prototype/member metadata must be resolved for late known-call lowering

Practical result:

- temp object receivers can quicken from `GET_MEMBER` to `GET_MEMBER_SLOT`
- the existing member-call proof can resolve the callable
- known-call lowering can rewrite the call to specialized call opcodes

## Validation

### WSL gcc focused suite

Command:

```bash
cmake --build build-wsl-gcc --target zr_vm_compiler_call_lowering_focus_test -j 8
./build-wsl-gcc/bin/zr_vm_compiler_call_lowering_focus_test
```

Result:

- `6 Tests 0 Failures 0 Ignored`

### WSL clang focused suite

Command:

```bash
cmake --build build-wsl-clang --target zr_vm_compiler_call_lowering_focus_test -j 8
./build-wsl-clang/bin/zr_vm_compiler_call_lowering_focus_test
```

Result:

- `6 Tests 0 Failures 0 Ignored`

### Build integration follow-up

`tests/CMakeLists.txt` now also adds `tests/parser/call_chain_polymorphic_compile_fixture.c` to the relevant parser
test targets so the benchmark/release and focused call-lowering builds link on the current worktree.

## Residual Boundaries

The bounded residual generic-call allowance remains explicitly documented in
`tests/parser/test_compiler_regressions.c`:

- `totalGenericCallCount <= 6`
- `totalGenericTailCallCount <= 4`

The fresh tracked non-GC rerank after this W2 landing confirms why this residual is now bounded rather than dominant:

- `GET_MEMBER = 2`
- `GET_MEMBER_SLOT = 383,727`
- `FUNCTION_CALL = 327`
- `KNOWN_NATIVE_CALL = 4,167`
- `KNOWN_VM_CALL = 18,428`
- `callsite_cache_lookup = 640`

That means the remaining W2 residue is mainly:

- `call_chain_polymorphic` generic call windows
- `callsite_cache_lookup`

It is no longer the main explanation for the tracked interpreter hotspot line.

## Acceptance Decision

Accepted as the W2 closeout for the temp-receiver member-call lowering gap.

This note claims:

- temp `TO_OBJECT` / `TO_STRUCT` receivers no longer block member-slot quickening
- the focused call-lowering suite is green on both WSL toolchains with the current six-test inventory
- the current residual generic-call pocket is bounded and explicitly covered by regression assertions

This note does not claim:

- that every remaining generic call site in the tracked suite was eliminated

That stronger claim would be false. The accepted conclusion is narrower and more useful: W2 no longer owns the main
tracked non-GC hotspot story, and the remaining W2 work is a bounded residual pocket rather than the next required
mainline optimization branch.
