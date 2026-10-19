---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening_null_state.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening_null_state.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_native_call_binding.h
  - zr_vm_core/src/zr_vm_core/execution/execution_member_access.c
  - zr_vm_core/src/zr_vm_core/object/object_call.c
  - tests/parser/test_compiler_w2_performance_quickening.c
  - tests/parser/test_compiler_w2_quickening_array_add.inc
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening_null_state.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening_null_state.h
plan_sources:
  - "user: 2026-10-15 Array<int>.add null-state late-quickening checkpoint"
  - .codex/plans/20261008-macro-tokenstream-structured-generation.md
  - docs/plans/syntax/2026-10-08-macro-tokenstream-structured-generation-design.md
  - docs/plans/ssa/03-interpreter-binding/04-generated-fusion.md
tests:
  - tests/parser/test_compiler_w2_performance_quickening.c
  - tests/parser/test_compiler_w2_quickening_array_add.inc
  - tests/parser/test_span_core.c
  - tests/parser/test_call_binding_pipeline.c
  - tests/parser/test_call_binding_artifact.c
  - tests/acceptance/ssa-quickening-array-int-add.md
doc_type: module-detail
---

# Typed Array<int>.add Quickening

## Purpose

The compiler can emit `KNOWN_NATIVE_MEMBER_CALL` directly for a statically
resolved member call. That route does not leave the `GET_MEMBER_SLOT` plus
`KNOWN_NATIVE_CALL` instruction pair consumed by the older Array matcher. For
the statically selected `Array<int>.add` native provider, the late quickening
pass now recognizes the fused form and emits `SUPER_ARRAY_ADD_INT` when its
callsite and operand facts are complete.

The matcher preserves the generic call when any eligibility check fails. It
does not specialize a call based only on the member spelling or receiver type.

## Required Facts

The instruction must have a two-argument callsite cache of kind `MEMBER_GET`.
The cache must belong to the same instruction and resolve to member `add`.
The native binding must pass
`compiler_native_call_binding_is_provider_contract`, describe a direct `CALL`,
and use a module relocation whose target index is that member entry. The
compiler's slot facts must identify an `Array<int>` receiver and an `int` value.

The fused instruction stores the callsite cache index and argument count in
`operand1[0]` and `operand1[1]`, and its result slot in `operandExtra`. The
matcher uses the following two frame slots as the receiver and value inputs:

| Slot | Source |
| --- | --- |
| Receiver | `operandExtra + 1` |
| Value | `operandExtra + 2` |

The replacement instruction carries those input slots and no longer refers to
the callsite cache. Before rewriting the opcode, quickening clears the cache's
binding contract and relocation location. This prevents final callsite linking
from trying to attach the old provider relocation to the superinstruction.
Guard misses leave the original instruction and ordinary slot tracking intact.

`compiler_quickening_try_fold_dead_super_array_add_setup` also accepts an
adjacent `GET_STACK(dst <- source)` and `SET_STACK(receiver <- dst)`, with an
optional `GET_STACK(dst <- receiver)` reload. The receiver/value/destination
exclusions, block boundaries, source-slot read/write rejection and dead-result
checks remain required. Surrounding named-slot alias, native-provider and burst
conditions are unchanged. On success, the existing helper uses the source slot
directly, marks the ADD result with `ZR_INSTRUCTION_USE_RET_FLAG`, and removes
only the matched setup instructions. This is an extension of the existing
helper, not a new pass or a relaxation of the native null-return contract.

## Null-State Proof and Backedge Cleanup

The private `compiler_quickening_null_state` module answers whether a candidate
plain-NULL `GET_CONSTANT` can be omitted because its destination is already
plain NULL at that program point. Its only exported internal helper,
`compiler_quickening_slot_is_null_without_clear`, treats the candidate as a
no-op while constructing the control-flow graph. The candidate therefore
cannot provide the fact needed to delete itself.

The proof uses the ordered states `UNREACHED < NULL < UNKNOWN`. Function entry
starts at `NULL` only after the frame guard verifies checked entry
initialization: one value layout per stack slot, a valid value-frame size and
alignment, non-overlapping in-range offsets, and a non-parameter candidate
slot. The guard also rejects child functions, closures, typed closure
bindings, exports, typed exported symbols, and semantic-IR ownership tables.
Each CFG merge takes the more conservative state. The helper succeeds only
when the candidate is reached with exactly `NULL`; `UNREACHED` and
`UNKNOWN` both reject the fold.

Constants are checked by full value and ownership shape. A null write is known
only for a non-GC `NULL` value with no ownership kind, control, or weak
reference. Signed arithmetic constants must be native `INT64` values with the
same plain ownership shape. Every instruction must have a recognized opcode
and valid operand geometry. The graph supports checked fallthrough, jumps,
conditional branches, and returns; malformed targets, out-of-range operands,
unknown opcodes, size overflow, or allocation failure return false and leave
the instruction unchanged.

The observed encoding contracts include:

| Opcode | Instruction | Checked contract |
| --- | --- | --- |
| 21 | `SUPER_ARRAY_ADD_INT4_CONST` | Requires the return flag, four in-range source slots, and a plain native signed `INT64` constant. |
| 117 | `JUMP_IF_GREATER_SIGNED` | Sign-extends the branch displacement from the signed 16-bit `B1` field and validates the target. |
| 172 | `ADD_SIGNED_CONST_PLAIN_DEST` | Requires an in-range source and a plain native signed `INT64` constant. |
| 175 | `ADD_SIGNED_LOAD_STACK` | Reads only the two defined 8-bit source slots and ignores legal high residue in the instruction word. |
| 183 | `SUB_SIGNED_CONST` | Requires an in-range source and a plain native signed `INT64` constant. |
| 222 | `GET_MEMBER_SLOT` | Requires an in-range receiver and a valid cached `MEMBER_GET`; its result slot becomes `UNKNOWN`. |

`GET_STACK` and `SET_STACK` validate their full-width source slot against
`stackSize`. `FUNCTION_CALL` and `KNOWN_NATIVE_CALL` require a valid function
slot, a matching result destination, and an in-range argument span. Their
result and argument slots become `UNKNOWN` when they overlap the queried slot;
callee storage is separate from the complete caller frame. For cached
`MEMBER_GET`, runtime getter scratch begins at
`max(savedStackTop, functionTop)` in `object_call.c`, so it cannot alias an
arbitrary slot in the caller frame. The analyzer still marks the instruction's
destination unknown.

The existing post-`SUPER_ARRAY_ADD_INT` cleanup retains its basic-block,
active-local, active-typed-binding, and CFG dead-temporary guards before using
this proof. The new `compiler_quickening_fold_dead_backedge_null_clears` pass
considers only a plain-null `GET_CONSTANT` immediately followed by `JUMP` whose
validated target is at or before the clear. It also rejects a block-start
clear and requires the same local-lifetime and future-dead checks plus the
independent null-state proof. On success it changes only the clear to `NOP`.
The pass runner compacts those NOPs before invoking the existing late strict
`SUPER_ARRAY_FILL_INT4_CONST` matcher; the matcher and its eligibility checks
are unchanged.

## Regression Boundary

`test_w2_super_array_add_variable_value_elides_dead_receiver_setup` compiles a
loop that calls `values.add((i * 3 + 1) % 17)`, stores the receiver in another
container, and computes the expected sum. It checks all of the following:

- execution still returns `51`;
- at least one `SUPER_ARRAY_ADD_INT` is emitted;
- no specialized instruction retains an active binding contract, relocation,
  or call-binding instruction-map entry;
- the dead `GET_STACK`/`SET_STACK`/`GET_STACK` receiver materialization chain
  has been removed.

Two public-source regressions exercise the additional boundary:

- `test_w2_public_four_array_zero_fill_loop_fuses` fills four real `Array<int>`
  values with zero in a live indexed loop, then reads every array in a second
  loop. Runtime must be FINE with integer checksum zero; FILL emission is a
  separate final assertion.
- `test_w2_public_array_add_live_result_is_preserved` keeps the real native
  Array.add null-return result live. The controlled 22-case replay reports PASS
  and FINE/integer 14 for this case; that observation does not replace independent
  dead-temp and named-slot alias checks for any later null-write change.

The earlier R7 snapshot is retained as history: fourteen targets reported 322
cases with 321 PASS and one W2 FILL failure. Its diagnostic also read
`operand1[2]`/`operand1[3]` beyond the two-element array, so it did not identify
the intervening opcode. A later, separately pinned R4 replay reports all 23 W2
performance-quickening cases PASS with zero failures or ignored cases. The
original test sources and registrations were unchanged for that checkpoint.
This focused result does not replace the still-open M1 acceptance inventory.
See the [M1 validation ledger](../testing-and-validation/macro-foundation-validation.md)
for the dated R7, R4, and lower-contract R8 records.

The implementation stays inside the existing Array-int late matcher. That
matcher uses file-local member-cache, slot-alias, type-kind, and control-flow
helpers from `compiler_quickening.c`. The source file is over 11,000 lines, but
extracting one conditional branch now would leave an artificial module with
those private dependencies. A cohesive follow-up boundary is the complete
Array-int late-specialization sweep and its helper family, leaving the pass
runner to invoke that module.

This subtask covers only the guarded `Array<int>.add` call form. SSA 03.04
remains open for its other matcher and dispatch integration work.

## Verification

The 2026-10-15 source proof receipt
`E:/cargo-targets/zr_vm/reports/m1-array-null-state-source-proof-r4.json`
(SHA-256 `49BAABFFD2282E96FA86149239201A17795A5CFF60611B7BD51C870285AB508C`)
records byte-exact forward and inverse proofs for the analyzer and parent
quickening source. The new C source is SHA-256
`1BB3BDCB15D7AF4D2654A4AB82136D9AE7F9F04AAB78DF89ED554D3F6C3A5EAB`, its
header is `B7635CDAA0E51367347AF68F4834D4E8480C06B7F5943009238FDE68285ED1CB`,
and the parent source is
`864A72383D127DD5237E1D9C1710B49607440D5342D1598AECE86C9A8F56AA0E`. The
receipt freezes 119 sources, including `object_call.c`; the earlier R3 set had
118 and did not pin that dependency.

The MSVC R4 run of `zr_vm_compiler_w2_performance_quickening_test` reports
23/23 cases, zero failures, and zero ignored cases. Its native log is
`E:/cargo-targets/zr_vm/reports/m1-msvc-array-null-state-r4-zr_vm_compiler_w2_performance_quickening_test.log`
(SHA-256 `4A2D9A7A33499B161EDA92EFF9F5C03D69127138E2A2D3276935AD774CF4A81F`).
The follow-up lower-contract R8 run reports 14 targets and 323 cases, all
passing with zero ignored cases, exact ordered registrations, stable binaries,
119 stable source pins, and unchanged HEAD `63dd36d0c72db5aaf9bee15a2d81107f823a5cc6`;
its build log is
`E:/cargo-targets/zr_vm/reports/m1-msvc-m1-lower-contracts-r8-build.log`
(SHA-256 `39F39AAF696E621D35CC603EB9003C80BA657ECC3C2C491CB6BB59D1D8DC9ED6`).

The fresh MSVC original127 R2 integration run reports 127/0/0 with exit 0,
exact ordered registrations, zero custom failure rows, 127 stable source pins,
stable binary, and unchanged HEAD. Its build log
`m1-msvc-original127-after-lower-r2-build.log` is SHA-256
`F205B65E867C597BAA7F376E14EB2C268DE747130FF3EE921E5B557F6028FDE9`; its
native log is SHA-256
`28CC63BC1341A81FE8847B516F02699937143815241B41DA4757F357CD826D19`.

Fresh original51 R5 reports 51/51 with CTest exit 0 in 24.79 seconds under the
original `-j1` invocation and no timeout override. The receipt
`m1-msvc-foundation-current-r5-receipt.json` is SHA-256
`E8DFAE09D85E182DC3B864C87BC55D4BAC3173C68B3EE46CE6B5ACC317409443`; its
CTest log is SHA-256
`7B1BC51F1AD22E959D0CDCA4CD74D696562C7BA83B184454C1A23D5BB5B68F7E`. The
earlier 50/1 and 34/17-timeout observations remain history. The latest run did
not reproduce a timeout, and no cause is inferred.

The original127 and original51 source pins and before/after HEAD are
`63dd36d0c72db5aaf9bee15a2d81107f823a5cc6`. After R5, another session's
documentation commit moved the checkout to HEAD
`7a3e20119a5afe51f4625182833caa4ab8511b1b`; Root reviewed its complete diff,
which only adds comments and whitespace in `math/module.h` and
`math/module.c`, with no declaration or code changes. These test results remain
tied to the recorded `63dd...` source pins and do not claim validation at the
later HEAD. The R4, lower-contract R8, original127, and original51 Windows
slices are green on their recorded sources. The retained WSL availability
receipt records `Wsl/Service/CreateInstance/E_ACCESSDENIED`; neither compiler
ran. The focused MSVC control-flow run passes 9/0/0 and the edges run passes
11/0/0 at HEAD `7a3e20119a5afe51f4625182833caa4ab8511b1b`, with 163 unchanged
source pins, ordered registrations, and stable binaries. The R4/R8/original127/
original51 runs remain evidence at their recorded HEAD `63dd36d...`. WSL
GCC/Clang is the only remaining unaccepted M1 gate. M2-M6 remain pending. See
[`ssa-quickening-array-int-add.md`](../../tests/acceptance/ssa-quickening-array-int-add.md)
for the earlier call-binding RED and projection regression.
