---
related_code:
  - tests/parser/test_ssa_source_execbc_vm.c
  - tests/parser/ssa_source_execbc_vm_compare.inc
  - tests/parser/ssa_source_execbc_vm_loop_break.inc
  - tests/parser/ssa_source_execbc_vm_diagnostics.inc
  - tests/cmake/ssa-source-execbc-vm.cmake
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
  - tests/CMakeLists.txt
  - tests/harness/runtime_support.c
implementation_files:
  - tests/parser/test_ssa_source_execbc_vm.c
  - tests/parser/ssa_source_execbc_vm_compare.inc
plan_sources:
  - .codex/plans/20261004-source-comparison-producer-lowering-design.md
  - docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
  - docs/plans/ssa/01-execir-ssa/03-effects-verifier.md
  - docs/plans/ssa/03-interpreter-binding/01-dispatch-boundaries.md
  - docs/plans/ssa/07-aot-backends/01-aotir-contract.md
tests:
  - tests/parser/test_ssa_source_execbc_vm.c
  - tests/parser/ssa_source_execbc_vm_compare.inc
doc_type: testing-guide
status: actual-red-established
---

# Source LT/GT producer RED fixture

## Purpose and source contract

The four comparison tests use literal source, the actual parser and statement compiler, preSemanticIr validation, BuildModule, ExecIR Oracle, canonical typed ExecBC projection/materialization and the core VM dispatcher. They add no fixture-built typed instruction arrays and no fallback to the source compiler's legacy emitted function.

| Unity case suffix | Source condition | Expected signed return |
| --- | --- | --- |
| signed_less_true_reaches_core_dispatcher | `1 < 2` | 9 |
| signed_less_false_reaches_core_dispatcher | `2 < 1` | 8 |
| signed_greater_true_reaches_core_dispatcher | `2 > 1` | 9 |
| signed_greater_false_reaches_core_dispatcher | `1 > 2` | 8 |

Each full test name begins `test_source_`. Source arms are `return 9` and `return 8`. The first assertions require ordinary parsing/statement compilation and ordinary preSemanticIr validation to succeed. The next assertion requires an executable source CFG, with message `source comparison producer did not establish an executable source CFG`. Root's actual V46 run reached this third assertion in all four cases after both prerequisites passed, establishing genuine producer RED with natural exit 4. A failure at either earlier assertion must be diagnosed first and cannot be reported as the intended producer RED.

Once CFG production succeeds, the shared report assertions require no structured stage diagnostic, verified source-built ExecIR, published fixture module identity, conditional branch, signed-i64 Oracle and VM returns, matching results, a valid VM PC/source map and valid trace successors/terminal Oracle block. The report helper accepts an already collected report so the comparison case executes the source pipeline only once. The actual production compiler still does not automatically publish this artifact; the fixture's identity values are test-local.

## Four-consumer coverage boundary

| Consumer | Source comparison connection |
| --- | --- |
| ExecIR Oracle | Actual harness route exists; comparison success not established until source producer and RED/Green execution |
| Canonical typed ExecBC/core VM | Actual harness route exists; comparison success not established until source producer and RED/Green execution |
| C AOT | Not established by this fixture; no manufactured emitter input/output is used to claim coverage |
| LLVM AOT | Not established by this fixture; no manufactured emitter input/output is used to claim coverage |

Current existing AOT conditional helpers support LT/GT, not six predicates. EQ/NE/LE/GE, equal inputs and signed extremes remain later coordinated coverage; this initial fixture introduces no unary/minimum-literal assumptions.

## Translation unit and link closure

The formal target remains `zr_vm_ssa_source_execbc_vm_test`, registered CTest `ssa_source_execbc_vm` in tests/cmake/ssa-source-execbc-vm.cmake. It compiles tests/parser/test_ssa_source_execbc_vm.c; its included fixture units are diagnostics, loop-break and comparison `.inc` files. Comparison cases live in their own cohesive include.

The target uses `zr_vm_add_unity_test_target`, which includes configured common, Unity and reference harness sources, common test settings and the existing zr_unity dependency. `zr_vm_link_parser_core_plus_library` links the configured shared or static variants of parser, core, library, math, system, container and FFI. This existing link closure is unchanged. Linked FFI/library support does not authorize executing network, FFI, providers, security or async tests; these selected cases execute only local integer source and core interpretation.

The observed V46 RED instead used the independent actual-checkout driver in
`tests/cmake/ssa-source-direct-validation`: the historical 666 actual C inputs,
nine ordinary static archives, actual Unity and runtime harness, and kernel32.
It did not configure the broader formal provider/FFI graph. See
[the direct-route guide](ssa-source-direct-validation.md) for source membership,
toolchain settings and archive provenance. Three later parser TUs produce a
future 669-TU pool; the sealed V46/V48 evidence does not validate those additions.

## Root execution and named output contract

Root built the existing target through the focused actual-checkout native driver.
No source snapshot or copied tree was used. Its actual executable selects cases with:

```text
zr_vm_ssa_source_execbc_vm_test.exe --comparisons-only
zr_vm_ssa_source_execbc_vm_test.exe --regressions-only
```

`--comparisons-only` selects exactly four new Unity cases; `--regressions-only` selects the original thirteen cases. No argument selects all seventeen, including through the unchanged registered CTest. Unknown or extra arguments print usage and exit 2 before Unity runs. A selective successful run must report 4 or 13 tests respectively with no failures/ignores; ordinary full CTest uses 17. A producer RED must report four comparison tests with the specific CFG assertion after both prerequisite assertions pass. Unity exit status remains the test result; no wrapper converts RED to success.

Root's reports are under
`E:/cargo-targets/zr_vm/reports/ssa-20261004-01a0fe2b`. V46 compiled the historical
666 inputs, created nine archives and linked a 41 MB PE. The four-comparison run
established third-CFG-assertion RED with natural status 4. The same attempt's
13-regression run overflowed the default stack (`0xC00000FD`), so V46's aggregate
receipt remains false. That failed receipt is not rewritten as success merely
because the four-case RED is independently accepted.

V47 native unwind data identifies a dispatcher frame of 1,169,720 + 48 bytes,
exceeding the PE's default 1,048,576-byte stack reserve. Root's V48 freshly relinked
the sealed V46 666 objects and nine libraries into
`E:/cargo-targets/zr_vm/build/ssa-20261004-01a0fe2b/source-stack-baseline-v48`
with reserve 8,388,608 and commit 4,096 bytes. All original 13 cases PASS, natural
exit 0, EMPTY process state, closed handle and no actions; whole run 81.454 seconds.
The TRUE receipt is `source-stack-baseline-v48/Root-receipt.json` under the report
root, 599,239 bytes, SHA256
`23c1ac9f1b703453509be030fa6ba1e1f21de41ae14292c466d4232aa1213c40`.
V48 validates the sealed historical baseline and did not compile producer changes
being edited. No compiler, runtime, Git or prohibited test command was executed
by the fixture/document author.

## Local reference semantics

Lua's actual checked-out `lua/src/lvm.c:499` keeps integer LT in the numeric comparison path, and OP_LT dispatch at 1703 calls that comparison machinery. CPython's `lua/CPython/Python/bytecodes.c:3033` integer comparison obtains integer values and returns bool singleton values at the end of the comparison operation. These are local reference grounds for integer comparison/bool-result separation; their opcode encodings are not copied into zr_vm's canonical predicate ABI.

This fixture slice does not complete 01.02, 01.03, 03.01 or 07.01. Automatic normal-compiler publication and same-source C/LLVM consumers require separate implementation and acceptance evidence.
