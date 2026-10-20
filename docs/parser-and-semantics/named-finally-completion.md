---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/statement/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/statement/compile_statement_flow.c
  - zr_vm_parser/src/zr_vm_parser/compiler/statement/compile_statement_try.c
  - zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_semantic_cfg.c
  - zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_semantic_cfg_finally.c
  - zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_function.c
  - zr_vm_parser/src/zr_vm_parser/compiler/expression/compile_expression_lambda.c
  - tests/exceptions/test_exceptions.c
  - tests/exceptions/test_named_finally_completion_cases.h
  - tests/parser/test_ssa_source_cleanup_cfg.c
  - tests/parser/test_cfg_finally_abrupt.c
  - tests/parser/test_pre_semantic_ir.c
  - tests/parser/test_resource_shared_weak.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/statement/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/statement/compile_statement_flow.c
  - zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_semantic_cfg.c
  - zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_semantic_cfg_finally.c
plan_sources:
  - .codex/plans/20261008-macro-tokenstream-structured-generation.md
  - docs/plans/syntax/2026-10-08-macro-tokenstream-structured-generation-design.md
tests:
  - tests/exceptions/test_exceptions.c
  - tests/exceptions/test_named_finally_completion_cases.h
  - tests/parser/test_ssa_source_cleanup_cfg.c
  - tests/parser/test_cfg_finally_abrupt.c
  - tests/parser/test_pre_semantic_ir.c
  - tests/parser/test_resource_shared_weak.c
doc_type: module-detail
---

# Named Function Finally Completion

## Return Value Ownership

A semantic finally plan belongs to the current callable's Semantic IR graph.
An accepted protected return must produce a live ValueId in that graph, record
the pending completion and route control to the cleanup block. After cleanup,
the existing completion helper returns the saved value. Whether the callable
is a script or a named function does not determine whether its plan is active.

`compile_return_statement` queries the existing active-plan helper directly.
For an active return plan it compiles the return expression in the current
graph and calls the guarded finally redirect. The helper still requires an
initialized matching return plan, valid payload state and a live value. Its
pending-completion and cleanup-entry checks remain intact.

For a named return without an active plan, the existing return-expression
isolation remains. SourceCallable identity publication still requires no
finally context or ownership cleanup. The function's outer isolation owns
its graph and restores the parent after typed metadata is finalized; nested
lambdas keep their separate graph ownership. Tail calls remain disabled around
finally and ownership cleanup. Runtime pending-return instructions are retained.

## Admission And Other Completions

Finally preflight admits a bounded protected tree and a linear cleanup body.
The current linear-expression classifier excludes binary addition. A cleanup
such as `marker = marker + 1` therefore exercises legacy compilation. A literal
assignment such as `marker = 9` can exercise the semantic plan, subject to the
protected body's other admission checks.

The earlier three return guard changes did not change preflight, void-value lowering,
conditional join construction, throw routing or cleanup helper guards.
Preflight continues to reject active ownership cleanup and unsupported shapes.
Void, conditional and throw behavior must be established using actual admitted
source programs before claiming support. Synthetic helper tests alone do not
establish that ordinary function compilation supplies valid completion state.

## Regression Fixtures

The original eight exception cases retain their source and assertions. They
cover normal, return and throw paths, named finally closures and sibling typed
metadata, catch-handler removal, stack frames and uncaught ordinary functions.
Three additional public source/VM cases cover a boolean conditional return,
both-arm returns and a named throw observed by an outer catch. Their incremented
marker checks cleanup exactly once, across both boolean inputs where applicable.
Those three use the legacy cleanup path and are compatibility controls.

The new helper frees its compiled function and destroys its own state before
behavioral assertions. The original eight retain their pre-existing assertion
abort cleanup limitations; this change does not claim whole-suite leak freedom.
No new filesystem fixture is created.

Four further source/VM companions use a literal `marker = 9` cleanup. They
exercise a conditional return, both-arm returns, an outer catch observing the
original thrown payload and a void return. Boolean cases reset the marker
between calls and check each result and cleanup. Idempotent assignment proves
cleanup ran; these four do not claim to count executions. Current fifteen-case
main and companion header are `8B80FA1E` and `59B8C6E0`. Their complete inverse
restores the previous eleven-case files without modifying the original cases.

## Current Evidence

The first eleven-case MSVC RED built successfully and reported nine passes,
two failures and no ignores. The original return-runtime and named closure
metadata cases failed with `Failed to enter semantic finally cleanup`. All
three additional compatibility controls already passed.

After the three guard changes, the complete GREEN-r2 runtime log reports
`11 Tests 0 Failures 0 Ignored` and `OK`. Both original failed cases pass.
Current statement SHA256 is
`9D8F9894F5899FDC81B846CB8DF3CC79F77E560B8A89632D6E23493C9C37F75E`;
the unchanged exception fixture and private header are `6A0CB182` and `23DDBFB6`.
The runtime log SHA256 is
`1460ECA8B4717AE66076D9BA7587FBC777E208BCEEE7A7161327C86791BF1FBC`.

Normal CMake regeneration selects the migrated statement, executor and scope
paths. A wrong migrated residual-source path first blocked configuration; its
directory value was corrected while retaining all source exclusions and count
guards. The native handle disappeared during automatic continuation; the
wrapper's natural exit code was not recovered. The complete runtime footer is
the evidence for these eleven results, not a claimed observed wrapper exit.

The complete fifteen-case MSVC RED exits naturally with wrapper exit 1,
build exit 0 and test exit 3: `15 Tests 3 Failures 0 Ignored`. All original
eleven and the new conditional-return companion pass. The admitted both-arm
and throw cases fail entering semantic finally cleanup. Void return first
fails routing its semantic value through finally, then entering cleanup.
Statement `9D8F9894`, try compiler `928393C9` and CFG `A1349011` remain unchanged
through this RED. The runtime log is
`m1-msvc-named-finally-admitted-red-r1-zr_vm_exceptions_test.log`, SHA256
`FB16FD083800EE285B14BEEFF052B4FB7B0F19CD40BE2C46CAEE2578F843C878`.

The admitted-plan follow-up preserves the earlier guards and keeps conditional
graphs only for an existing matching return/throw finally plan. Named throw
uses the active-plan helper, and an active void return emits the existing
semantic null literal. Ordinary named conditionals and inactive returns keep
their prior isolation; admission and helper failure guards are unchanged.
These three production files are frozen and independently reviewed.

Peers identify a separate cleanup-entry issue: both terminal branches set the
protected-body Terminated flag. Entering the reachable cleanup block leaves
that flag set, causing unreachable-statement isolation to discard cleanup
code. Later completion LOAD emission is also suppressed before the flag is
cleared. Existing runtime-only controls and synthetic graph validation do not
prove retained cleanup instructions.

The two existing terminal repeated return/throw fixtures now inspect real
cleanup source line6, a defined payload LOAD and actual ExecIR construction.
All62 cases and original source/assertions remain; whole-file inverse restores
the pre-strengthening fixture. New assertions run after output/source cleanup.
The complete strengthened MSVC RED exits naturally1/build0/test2:
`62 Tests 2 Failures 0 Ignored`. Both new cleanup-line assertions fail;
the remaining60 pass. Later LOAD/ExecIR assertions are not credited on abort.
The runtime log SHA256 is
`AFEF341195C90C627308C9F215BA0A244AAFE5A18AF90B613E7BA852DACC6A14`.
This evidence authorizes clearing Terminated only after successful guarded
cleanup entry. The existing helper's inactive and failure paths remain before
the reset. Independent peer review finds no actionable boundary issue. Only
the approved assignment changes the helper; Root's whole-file inverse in RAM
reproduces its complete preimage. The frozen helper is `8A15E6F6`.

Fresh source62/runtime15/helper7 verification exits naturally0/build0, with
all three complete native footers showing zero failures and zero ignores.
Both terminal source cases reach and pass cleanup-line, defined-LOAD and
ExecIR assertions. All fifteen runtime cases pass, including admitted both-arm
return, throw payload and void cleanup; all seven abrupt CFG cases pass.
The source and runtime log SHA256 values are
`BB22EBE0E85F4C436735F3533847F7C1E2B9863BE1014407CA313F42B12B8D52` and
`0A4DEB88CE5CF64DAC64826A2ADBFE31E1891CA12AEA4FD5E479B16F4C117A76`.
No filesystem fixtures are introduced by these targets.

The independent review confirms live graph ownership and preserved nonactive
return behavior. Four other lower-layer failures remain baseline-unattributed;
source-level diagnosis, full M1 replay and the WSL matrix remain open.
This focused result does not establish M1 acceptance.

See [lambda isolation](lambda-semantic-ir-isolation.md),
[definition environments](comptime-definition-environment.md) and
[foundation validation](../testing-and-validation/macro-foundation-validation.md).
