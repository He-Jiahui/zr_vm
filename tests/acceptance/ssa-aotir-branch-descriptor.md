# Two-block ExecIR AOTIR descriptor edge ownership

## Scope

The parser AOT projection has separate successor and predecessor arrays. AOTIR
uses one edge pool for both range kinds. The descriptor builder now owns a
combined in-memory pool and rebases predecessor ranges without changing the
public descriptor layout or persisted AOTIR schema. The legacy lowering entry
still declares callable ABI `UNKNOWN`.

## Test-first evidence

The dedicated `ssa_aot_projection_branch_descriptor` test constructs a verified
`BRANCH → CONSTANT → RETURN` ExecIR function and lowers it through the existing
AOT projection. With the old descriptor builder, its first descriptor assertion
failed: `invalid_cfg block=1 expected=1 actual=0`. The AOTIR edge reciprocity
check saw the outgoing 1→2 edge but read block 2's predecessor range from the
successor-only pool, so it could not find 2←1.

The GREEN test checks a descriptor-owned pool `{2, 1}`, a predecessor range
rebased to offset 1, unchanged instruction successor range, and successful
`ZrCore_AotIr_ValidateModule`. It corrupts a predecessor range, an instruction
successor range, and a predecessor ID in turn; each attempted rebuild fails
without replacing the previous descriptor. Freeing the descriptor clears the
owned pool pointer.

## Validation

The focused target was built in the released GCC cache at
`D:\tmp\zr_vm\ssa-artifact-v6-gcc`. The final ownership refinement keeps the
combined allocation in `function.successorPool` and frees it through
`FreeDescriptor`, so the public descriptor struct size stays unchanged.

| Check | Result |
| --- | --- |
| Final focused and adjacent CTest | 4/4 passed: `ssa_aot_projection_branch_descriptor`, `ssa_aot_projection_descriptor`, `ssa_aot_callable_abi`, `ssa_aotir_contract` |
| Scoped `git diff --check` | Passed |

This support change validates AOTIR CFG representation and ownership. It does
not claim an executable callable ABI, native emitter integration, or artifact
availability.
