---
related_code:
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/include/zr_vm_parser/compile_tool.h
  - zr_vm_parser/src/zr_vm_parser/compiler/tool/compile_tool_execution_scope.c
  - zr_vm_parser/src/zr_vm_parser/compiler/tool/compile_tool_execution_scope.h
  - zr_vm_parser/src/zr_vm_parser/compiler/tool/compile_tool_binding.c
  - zr_vm_parser/src/zr_vm_parser/compiler/tool/compile_tool_evaluator.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time/compile_time_executor.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time/compile_time_executor_support.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time/compile_time_callable.c
  - tests/compileTime/test_comptime_import_alias_environment.c
  - tests/compileTime/test_comptime_import_alias_owner_cases.h
  - tests/cmake/macro-foundation-tests.cmake
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/tool/compile_tool_execution_scope.c
  - zr_vm_parser/src/zr_vm_parser/compiler/tool/compile_tool_binding.c
  - zr_vm_parser/src/zr_vm_parser/compiler/tool/compile_tool_evaluator.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time/compile_time_executor.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time/compile_time_executor_support.c
plan_sources:
  - .codex/plans/20261008-macro-tokenstream-structured-generation.md
  - docs/plans/syntax/2026-10-08-macro-tokenstream-structured-generation-design.md
tests:
  - tests/compileTime/test_comptime_import_alias_environment.c
  - tests/compileTime/test_comptime_import_alias_owner_cases.h
doc_type: module-detail
---

# Comptime Import Alias Environment

## Lookup Contract

A source comptime function resolves module imports in its definition
environment. Calling a provider function must expose that provider's top-level
imports, even if the consumer declares a variable with the same name. A name
imported only by the consumer must remain unknown inside the provider.

Ordinary local bindings take priority over CompileTool member dispatch inside
the function that declares them. For example, a local integer named `tool`
cannot dispatch `tool.build.feature(...)` to a top-level CompileTool import.
Calling a sibling function uses the sibling's definition environment; the
caller's local integer does not become a lexical binding in that sibling.

A callback defined in the consumer resolves the consumer's imports. Returning
from that callback restores the provider's environment before the provider
continues. Every success and failure must restore the caller's binding and
module-alias arrays, active owner, compile phase, comptime context, call depth
and GC root count.

## Shared Support Paths

`compile_tool_execution_scope` enters and leaves a function's definition
environment. `compile_tool_binding` declares imports and shadows, then resolves
provider aliases. `compile_time_executor` handles ordinary primary expressions
and imported-module aliases; `compile_tool_evaluator` recognizes typed feature
and declaration-enum operations. Local-frame lookup lives in
`compile_time_executor_support`. These shared paths must agree on owner and
local-binding precedence; a separate provider-only success path would leave
ordinary calls and callbacks inconsistent.

The scope saves both complete binding-array headers and the active module, then
creates independent scratch arrays for the callee's definition environment.
Leaving frees only the scratch arrays and restores the exact saved headers;
failed entry uses the same rollback. Nested calls therefore have separate views.

Lexical replay first predeclares function shadows, then replays imports and
ordinary declaration shadows in source order. Conditional declarations use
only branches already selected by BuildFacts. Replay never executes conditions,
initializers or bodies. Root ownership comes from actual registered function
pointer membership along compiler ancestry; ordinary source aliases resolve
already-retained canonical module records without loading the module again.

Typed member dispatch checks contextual local-frame presence first. A local
receiver returns to ordinary expression evaluation, preserving normal scalar
member errors. Source callee frames remain isolated from the caller's locals;
arguments evaluate in the caller view, while defaults and bodies use the owner
view. Existing effects, budgets and cache-key rules remain in force.

## Real Provider Fixture

The original twelve-case fixture writes a synthetic provider source and packs it through
`ZrLibrary_Zrm_WriteArchive`. Executable-source and archive content hashes,
the actual CompileTool public-contract hash, a manifest V2 build dependency,
and the normal dependency-lock reader select the provider. The consumer is
parsed, imports are canonicalized, and `PrepareBuildFactsInCompilerState`
registers actual source declarations and owner records. No AST, provider,
import, shadow or feature entry is manually fabricated.

The project declares `trace: true` and `disabled: false`. Separate controls
check both values and the existing unknown-feature diagnostic. Other cases
check consumer-only alias rejection, a consumer's ordinary homonymous shadow,
a same-owner sibling call, a real local integer, and scalar member rejection.
The callback case first verifies direct Root feature lookup and provider enum
lookup, then calls the Root callback from the provider and checks the provider
enum after returning.

Three companions use the same fixture without changing those nine cases. One
enters the real Root function's view, checks an ordinary variable's `SHADOW`
across full GC, leaves, and compares saved live rows byte-for-byte. Another
checks a selected conditional import recorded by actual BuildFacts. The last
creates a real child compiler with a different provider import, validates its
parent's registered function value, and invokes the existing private source-call
primitive. This last case does not claim public ancestor bare-name lookup.
Child resources and temporary snapshots are released before behavioral
assertions can abort the case.

A thirteenth compatibility control uses the existing in-memory source loader
and the normal parser, BuildFacts and `compile_script` producers. Its ordinary
`sizing` import retains a real module with the exported `staticPlanSize`
comptime declaration. A Root `readOwned` declaration resolves the module before
and after full GC inside its owner view. Leaving must restore complete array
headers and live rows, without invoking the loader again. Snapshots and the
loader override are released before assertions; teardown owns the temporary
compiled functions. Its externally held runtime function is registered with a
standard GC root before compilation. After collection, the fixture resolves
that root to refresh its borrowed `currentFunction`; teardown resolves and
releases the root before freeing the function. The control creates no extra
filesystem fixture.

The outer callback argument is a native function pointer and remains
uncacheable under the existing scalar cache-key policy. Only the no-argument
Root callback's cache hits are asserted. The provider body must therefore run
again on repeated calls, including after full GC. Borrowed array rows are
re-fetched after evaluation and collection.

Generated source and archive paths use the actual CMake binary root beneath
`E:/cargo-targets/zr_vm/build`. The virtual consumer manifest is never written.
Creation refuses pre-existing unowned files. Teardown releases compiler,
provider, AST, project and runtime ownership before removing attributed
fixtures, and reports removed files, bytes and failures. Completed-created-file
content hashes are recorded and verified before deletion. Changed or unpinned
files remain as failure evidence, including incomplete writes without a pin.

## Validation Status

The target is `zr_vm_comptime_import_alias_environment_test`, registered as
`macro_comptime_import_alias_environment` with the existing 30-second timeout.
The current thirteen-case main fixture is `64B0151D`, with companion header
`9480AF4B`. The original twelve cases remain intact. The added ordinary-source
control initially lacked a root for its externally held runtime function.
A bounded Windows native observer traced the access violation to
`Function_Free` during that control's teardown. Explicit standard rooting,
resolution after collection and release before free correct that fixture's
lifetime. A whole-file inverse restores the pre-rooting header `D17DF9A3`;
the earlier inverse restores migrated twelve-case main `FAA81657` and header
`59831362`. No production GC behavior was changed for this correction.

The complete MSVC RED-r9 exits naturally with build exit 0 and test exit 5:
`13 Tests 5 Failures 0 Ignored`. The ordinary-source loader, canonical module,
alias across full GC, complete binding-array restoration and actual result 11
control passes. The same five owner-view/local-receiver failures remain.
Both independent callback prerequisites pass; later callback cache and GC
checks remain unreached. This complete RED authorizes the bounded private
scope/evaluator repair. This RED precedes the production candidate below.

The RED-r9 runtime log is
`m1-msvc-import-alias-environment-red-r9-zr_vm_comptime_import_alias_environment_test.log`,
SHA256 `9E68C3BD372C616C1CB732C5B1910CC505FDA421315B65110AE23B954FF5FB91`.
Hash-guarded cleanup removes 24 files and 33,198 bytes with zero errors;
the exact owned source and archive are absent after the run.

The fresh thirteen-case MSVC GREEN-r1 exits naturally with build, test and
wrapper exit 0: `13 Tests 0 Failures 0 Ignored`, followed by `OK`.
All five earlier failures pass. The callback reaches its repeated-call cache
and full-GC assertions, and the actual ancestor-call and ordinary-source import
controls pass. Independent core review finds no actionable candidate delta.
The four production files are scope `B5DA5B75`, header `E942290F`, evaluator
`1981F78B` and executor `57405E9E`. The complete test log is
`m1-msvc-import-alias-environment-green-r1-zr_vm_comptime_import_alias_environment_test.log`,
SHA256 `64944DAF14B1F65399DD804DBADB19EA74D01BFA33C3B95286F53CFAE751D774`.
Cleanup again reports 24 files, 33,198 bytes and zero errors; exact source and
archive paths are independently confirmed absent. Broader M1 replay remains
required; this focused GREEN does not close the obligations listed below.

The following RED-r3 result retains its historical `050C2CB6`/`7F8F623F` inputs.
Independent prerequisite review accepts the original nine cases and the three
companions. The whole-main inverse restores `C22365C4` without changing the
original cases, source constants, callback checkpoints or cleanup guards.

The actual MSVC RED-r3 builds successfully, then reports twelve tests, five
failures and zero ignores. Own true/false feature lookup, unknown-feature range,
sibling invocation, local value, Root saved-row/GC and selected conditional
import controls pass. Consumer-only alias capture, consumer shadow interference,
typed local receiver bypass, cross-owner callback and the validated ancestor
call fail. Both independent callback controls pass before that callback failure.
Later callback cache/GC assertions remain unreached in this earlier RED;
it supplies no production GREEN evidence.

The run exits naturally with test exit 5. Its test log is
`m1-msvc-import-alias-environment-red-r3-zr_vm_comptime_import_alias_environment_test.log`
with SHA256 `19C2F87C6A51475AFAF3EFFEE591C87E1ACCC1C0283C86DC7F23C3BA0E070287`.
Hash-guarded cleanup reports 24 files, 33,198 bytes and zero errors; exact owned
source and archive paths are absent afterward. The same native build is reused.

Separate open obligations include two source-project providers, marked alias
hygiene, cross-owner global-variable lookup and source/binary execution parity.
This focused fixture cannot establish acceptance for the whole M1 milestone.

See [definition environment](comptime-definition-environment.md) and
[foundation validation](../testing-and-validation/macro-foundation-validation.md).
