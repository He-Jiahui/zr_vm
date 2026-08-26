# Ownership And Object Member Separation Acceptance

## Status

- Design: `docs/superpowers/specs/2026-08-10-ownership-object-member-separation-design.md`
- Implementation plan: `docs/superpowers/plans/2026-08-10-ownership-object-member-separation-implementation.md`
- Review date: 2026-08-26 (UTC+08:00)
- Status: `validated_pending_full_acceptance`

## Accepted source contract

| Requirement | Implementation evidence | Focused evidence |
| --- | --- | --- |
| Ownership uses only five reserved intrinsics | `ZR_AST_OWNERSHIP_INTRINSIC_EXPRESSION` and `EZrOwnershipIntrinsicOperation` | GCC/Clang/MSVC ownership 37/37 and Shared/Weak 19/19; final full-graph replay pending |
| `.` and `?.` never classify ownership by member text | real member lookup precedes the structured migration diagnostic; compiler lowers only fact-owned intrinsic nodes | same-name object method and real-member tests |
| Weak direct access is guarded and throws `NullReferenceError` on expiry | `SZrReceiverGuardFact`, `REQUIRE_NON_NULL`, one hidden wake owner | direct-expiry test passes against the materialized named runtime prototype |
| A live weak target keeps ordinary object-member failures | guard resolves before member dispatch; standard system registration materializes the exception hierarchy | missing member follows the ordinary member-error path, not `NullReferenceError` |
| `?.member`, `?.method(args)`, and `?.(args)` skip the complete suffix | per-segment access mode and chain-level receiver-guard lowering | optional failure skips argument side effects; optional success runs once |
| Explicit `wake(weak)` returns nullable Shared | canonical intrinsic fact and `OWN_WAKE` | type/fact/opcode execution tests |
| VM, AOT C, LLVM, and artifacts use canonical operation names | `OWN_SHARE/DEGRADE/WAKE/INTO_GC_BOX/DROP` | SemIR 13/13; focused AOT C/LLVM ownership and receiver-guard replay passes |
| Old member calls are not executable aliases | diagnostic is emitted only after canonical member lookup fails, and compilation stops | structured fix tests and same-name real member tests |
| LSP consumes facts rather than spelling/AST fallback | ownership intrinsic and receiver-guard fact consumers | semantic analyzer/local query pass; remaining LSP replay pending |

The five source intrinsics are exactly:

```text
share(owner)
degrade(shared)
wake(weak)
intoGc(owner)
drop(owner)
```

The former `Module.share()` ownership escape interpretation is removed: a
guarded module payload is a scoped plain view, not an owner operand, and the
compiler alone creates and releases the hidden owner that keeps the loaded
module alive. `Module.share()` now follows ordinary member lookup, so a real
module export named `share` can be called but never lowers to `OWN_SHARE` or
promotes the guard payload. The same rule applies to ordinary user-defined
methods named `degrade`, `wake`, `intoGc`, or `drop`.

Consuming `share`, `intoGc`, and `drop` currently require a local owner binding.
The semantic layer rejects field/index projections because the current lowering
cannot atomically load and clear a projected place. This closes the review-found
case where a projected owner could be consumed through a temporary while the
original field remained populated. Non-consuming `degrade` and `wake` continue
to accept readable projections.

## Performance evidence

The Release GCC benchmark uses 16,384 iterations and three samples per variant;
all variants produced checksum 344064:

| Variant | ns/op | Ratio to direct |
| --- | ---: | ---: |
| direct non-null | 3326.579 | 1.000 |
| weak direct | 5333.008 | 1.603 |
| weak optional success | 5587.931 | 1.680 |
| weak optional failure | 1417.745 | 0.426 |
| deep nullable guard | 2059.835 | 0.619 |

The ratios are recorded evidence, not arbitrary pass thresholds. The live weak
path performs one wake for the complete suffix; the optional failure path skips
the suffix and argument evaluation.

## Test modernization

- Executable fixtures use the five intrinsic calls and canonical opcode names.
- Parser test names, summaries, and fixture source names use intrinsic,
  reference-view, and GC-bridge terminology; legacy borrow/loan/detach opcode
  names remain only in negative emission checks or artifact-compatibility tests.
- The task runtime suite no longer carries a disabled `#if 0` block of retired
  TaskRunner/coroutine compatibility cases; active percent-task forms remain
  only as parser-rejection inputs for the one-time cutover.
- The module suite drops fourteen unregistered runtime-decorator reflection
  cases plus their private helpers, keeps 78 executable registrations, and
  reports its active reflection coverage with `typeof` terminology. A
  repository scan finds no unreferenced static `test_*` functions.
- The lexer/parser/compiler execution suite re-enables four valid struct/class
  cast and full-pipeline tests that had been silently commented out pending an
  old double-free repair; active test registrations are no longer hidden behind
  `ZR_UNUSED_PARAMETER` placeholders.
- All retained `TEST_IGNORE_MESSAGE` sites are protected by explicit platform
  capability guards. Two contradictory non-Unix branches nested inside
  Unix-only AOT helpers are removed as unreachable test code.
- The lexer/parser/compiler execution suite removes a vacuous opcode-definition
  case whose success flag was constant and whose body performed no assertion;
  concrete parser/compiler conversion cases remain executable coverage.
- Current module, LSP rename, and reference-alignment documentation uses
  `module name;`, `import(...)`, and `using(resource)` for valid examples.
  Removed percent forms remain only where the text explicitly describes a
  migration or rejection boundary; stale machine-local document links are gone.
- Current VM exception-cleanup and AOT tail-call documentation likewise names
  the active resource syntax as `using(resource)` while retaining the internal
  to-be-closed and close-meta execution terminology.
- Old ownership member syntax remains only as migration input, negative input,
  same-name member collision coverage, or quoted historical evidence.
- The stale signature-help test that expected an AST/name fallback for an
  invalid call was removed. Current signature help requires a canonical
  call-target fact and fails closed without one.
- Obsolete script/golden assets with no active consumer are deleted; generated
  snapshots are kept in build-local `tests_generated/` directories.
- Runtime-generated AOT C shared-library fixtures use the centralized
  `ZR_TESTS_AOT_C_RUNTIME_LINK_FLAGS` contract instead of 142 copies of a
  partial `zr_vm_library` / `zr_vm_core` link line. The Unix contract includes
  the libraries' complete static dependency closure; platform-specific AOT
  execution remains ignored on Windows under the existing test contract.
- Reflection-annotation fixtures allocate `typedExportedSymbols` through the
  runtime allocator because `SZrFunction` owns and releases that array. No
  fixture assigns stack storage to this owned field.

## Fresh focused validation

The 2026-08-23 weak-boundary review expanded the ownership runner to 30 cases.
The new cases prove that a hidden wake owner survives a full native GC inside
the guarded suffix, repeated direct method calls can wake the same live target,
and a live target with a missing member retains the ordinary missing-member
error. The initial missing-member test was a meaningful RED only after exposing
an older fixture blind spot: without materializing `zr.system.exception`, both
`RuntimeError` and `NullReferenceError` catch names fell back to the root
`Error` prototype. `ZrVmLibSystem_Register` now materializes the exception leaf
after registering the system descriptors, before user code executes.

Fresh isolated static Debug caches produced:

```text
                                      GCC 11.4   Clang 14   MSVC 19.44
ownership intrinsic/member separation    30/30      30/30       30/30
exceptions                                 8/8        8/8         8/8
module system                            78/78      78/78       78/78
AOT receiver guard C/LLVM                 2/2        2/2    2 expected ignores
```

Every listed process returned exit code zero. The GCC test-only RED was
`30 Tests / 1 Failure`: the live missing-member case entered the
`NullReferenceError` catch. After exception-leaf materialization, the same GCC
runner and the Clang/MSVC replays passed 30/30; GCC/Clang also executed both AOT
receiver-guard backends, while MSVC retained the suite's explicit Unix-only
ignore contract.

GCC 11.4, Clang 14, and MSVC 19.44 Debug focused direct execution each
passed the portable ownership/parser/runtime matrix:

```text
ownership intrinsic/member separation  27/27
removed percent syntax cutover            7/7
owner/borrow receiver escape checks       7/7
semantic facts                          14/14
type inference                         122/122
resource Unique/Drop                    20/20
resource Shared/Weak                    11/11
exceptions                               8/8
```

The three newest cases close review-found construct-AST compatibility routes.
Against the old implementation, a normal `new Plain()` node with
`builtinKind == NONE` but a stale `ownershipQualifier == SHARED` produced a
Shared inferred type and emitted `OWN_SHARE`; the focused runner reported
`26 Tests / 2 Failures`. Type inference, expression-fact classification, and
both compiler wrapper entries now select ownership only from explicit
`builtinKind`. The same runner then passed 27/27 on GCC 11.4, Clang 14, and
MSVC 19.44. On each toolchain, type inference, expression facts, compiler
integration, Unique/Drop, and Shared/Weak also completed with real exit code 0.

The 27th case casts the removed historical construct builtin value `8` without
referring to the legacy name. Before the cleanup it compiled successfully
through `ZR_OWNERSHIP_BUILTIN_KIND_DETACH` and lowered to
`OWN_RETURN_TO_GC`; the runner reported `27 Tests / 1 Failure`. The compiler
opcode mapper, pre-execution Semantic IR builder, inference result selection,
and ownership-fact classifier no longer contain that branch, so value `8`
reaches the unsupported-builtin failure path. The downstream instruction,
SemIR, runtime, and AOT readers remain intact exclusively for legacy artifact
compatibility under Syntax 04 M7.

Focused commands used the independent static Debug caches
`/home/hejiahui/.codex-builds/ownership-detach-red-gcc`,
`/home/hejiahui/.codex-builds/ownership-detach-clang`, and `E:\zrb\odc`:

```text
cmake --build <cache> --target zr_vm_ownership_intrinsic_member_separation_test zr_vm_type_inference_test zr_vm_expression_fact_emission_test zr_vm_compiler_integration_test zr_vm_semir_pipeline_test -j4
<cache>/bin/zr_vm_ownership_intrinsic_member_separation_test
<cache>/bin/zr_vm_type_inference_test
<cache>/bin/zr_vm_expression_fact_emission_test
<cache>/bin/zr_vm_compiler_integration_test
<cache>/bin/zr_vm_semir_pipeline_test
```

GCC 11.4, Clang 14, and MSVC 19.44 each passed ownership 27/27,
type inference 122/122, expression facts 28/28, and compiler integration
127/127. GCC and Clang passed SemIR 13/13; MSVC passed its portable 12/12
registration. Every direct process returned exit code zero.

The expanded 24th ownership case executes
`liveWeak?.add?.(bump())` and its expired counterpart. The live path returns 11
and evaluates `bump()` once; the expired path returns null and leaves the total
side-effect count at one. This directly proves that optional weak member access
and optional callable invocation guard the complete suffix, including argument
evaluation.

The lifetime regression was first reproduced with `degrade(shared)` followed by
expiry-sensitive weak access. Compiling a non-consuming identifier operand
through a temporary copied the Shared wrapper and kept the target alive. The
compiler now uses the original identifier slot for `degrade` and `wake`, and
temporary non-identifier results are explicitly cleared after lowering. The
optimizer also treats `RESET_STACK_NULL` as read/write because clearing a slot
must release the ownership wrapper previously stored there.

The earlier 23rd case was first observed RED against the old inference path:
`share(holder.ownedValue)` returned success. After the local-binding guard, the
three consuming projection cases fail during inference with the documented
diagnostic, while `degrade(holder.value)` and `wake(holder.weakValue)` remain
valid.

Earlier focused evidence remains valid:

```text
receiver-guard performance               1/1
expression fact emission                28/28
resource Unique/Drop                    20/20
resource Shared/Weak                    11/11
property ref-return                     23/23
SemIR pipeline                          13/13
exceptions                               8/8
semantic analyzer                       exit 0
local semantic query                    exit 0
```

Focused AOT execution also passes on GCC and Clang:

```text
AOT ownership contracts                   1/1
AOT ownership shared-library runtime      2/2
AOT receiver guard C and LLVM             2/2
generic bool equality local proof          5/5
typed direct u64 call                     25/25
```

The AOT review found two support defects rather than an ownership spelling
fallback. First, a scalar stack copy could keep only a C local even when a later
generic frame consumer required the copied value; the proof now requires a
downstream scalar consumer and otherwise materializes the frame slot. Second,
slot-kind inference could fall back to a historical block-entry bool after a
current-block u64 overwrite; latest-kind evidence now blocks that stale
fallback. The u64 shared-library test also held an unrooted raw function pointer
across an AOT safepoint. It now frees the compiled function before project/AOT
execution; Valgrind reports `0 errors from 0 contexts` for the corrected 25/25
run.

An independent review then raised a possible stale scalar read after an
ownership or call-result write. A minimal bool-to-`OWN_DEGRADE`-to-`GET_STACK`
regression was GREEN before any further production edit: the existing emitter
barriers reject scalar provenance for every `OWN_*` destination and every call
result, so generated C uses `ZrLibrary_AotRuntime_GetStack` with no scalar-copy
or bool-sync marker. The reviewer retracted the finding after following that
complete control flow; the regression remains to protect the barrier.

MSVC passes every portable focused target. The Unix shared-library/AOT driver
cases are registered as ignored on Windows and were executed successfully with
both GCC and Clang instead. MSVC's SemIR target reports 12/12 because the
Unix-only case is not registered in that runner; GCC and Clang report 13/13.

`backend_aot_c_scalar_locals.c` remains an oversized existing module. This fix
stays inside its current scalar provenance and result-liveness responsibility.
Splitting only the changed lines would expose the same static CFG, opcode, and
slot-kind helpers as new cross-module interfaces, so extraction is deliberately
deferred until that provenance subsystem can move as one coherent unit.

The sanitizer build passed official-provider 9/9 with ASan+UBSan and leak
detection disabled. LeakSanitizer still reports the pre-existing core permanent
string/builtin-closure lifetime baseline (603,950 bytes in 2,681 allocations),
but the report contains no native-registry or `native_binding.c` allocation
stack after the new global teardown hook.

An independent GCC Debug cache built the ownership/member-separation target with
`-fsanitize=address,undefined -fno-omit-frame-pointer`. The final lifecycle
replay enabled leak detection and configured both sanitizers to halt on the
first error. The target passed 24/24 with no AddressSanitizer,
UndefinedBehaviorSanitizer, or LeakSanitizer report.

Valgrind independently executed the same 24/24 target with full leak checking
and definite, indirect, or possible leaks treated as errors. It observed 48,162
allocations and 48,162 frees, `0 bytes in 0 blocks` at exit, and `0 errors from
0 contexts`. The fixes pair standalone lexer initialization with an explicit
free, release malformed postfix/statement AST children, release reusable
call-info nodes at state teardown, release compiled-function prototype pointer
storage while leaving the prototypes GC-owned, and avoid overwriting an already
initialized prototype-inheritance scratch array.

The directly affected behavior suites pass under GCC 11.4, Clang 14, and MSVC
19.44 Debug with real exit code 0:

```text
ownership intrinsic/member separation       27/27
lexer/parser/compiler execution              11/11
reflection type surface                      21/21
yield syntax                                   4/4
reference syntax contract                      8/8
execution callable metadata                  18/18
```

The stale AOT-driver replay also passes on all three toolchains:

```text
previously failing AOT/aggregate CTests        11/11
language_pipeline                               pass
reflection annotation preserve                12/12
```

On GCC, the reflection-annotation target additionally passes full Valgrind
leak checking with 6,415 allocations, 6,415 frees, zero live bytes, and zero
errors. Before the fixture ownership repair, the same run reported four invalid
frees of stack-backed typed-export symbol arrays.

The reference diagnostic test now accepts a fail-closed parser result while
still requiring the current one-time-cutover diagnostic and precise source
range. The execution fixture allocates instruction storage through the runtime
allocator and captures the expected invalid-call exception through
`ZrCore_Exception_TryRun`, matching production ownership and exception
contracts instead of depending on stack storage or process abort behavior.

`compiler_semantic_ir.c`, `type_inference_core.c`, and the dedicated ownership
test runner already exceed the repository's modularization warning threshold.
This cleanup removes branches and adds one case within their existing single
responsibilities; it introduces no helper, protocol, or cross-module API.
Splitting those switch tables or the cohesive ownership contract runner solely
around deleted lines would weaken the responsibility boundary, so no structural
split is included in this removal slice.

## 2026-08-25 current-source identity cleanup pre-acceptance

At main `ea7684a`, the final current-source compatibility residue was reproduced
with a focused RED: raw historical ownership builtin value `8` was still
accepted by two public qualifier helpers even though compiler lowering already
failed closed. Call-site review then proved both qualifier helpers have no
production consumers and preserve a second, internally inconsistent ownership
rule table. The narrow cleanup therefore removes the named DETACH enum member,
both unused qualifier-helper APIs, and the unused ownership-member error-message
API rather than repairing another compatibility surface. Explicit `INTO_GC=9`
and `RETURN_LOAN=10` identities remain stable. The legacy `OWN_DETACH` and
`OWN_RETURN_TO_GC` instruction, SemIR, artifact-reader, runtime, and AOT paths
remain intentionally available for old artifacts; no current source producer
selects them.

The retained WSL GCC 11.4 static Debug cache rebuilt the complete focused target
dependency graph (`612/612`) and linked successfully. Direct execution returned
exit zero with:

```text
ownership intrinsic/member separation       30/30
```

The retained WSL Clang 14 static Debug cache then rebuilt its affected parser,
runtime, AOT C, LLVM, and test graph (`377/377`) and linked successfully. Five
overlapping/current files were SHA-256 fenced before and after the build:
`type_system.h/.c`, `type_inference_call_semantic_facts.c`, `ast.h`, and the
focused test source all remained byte-identical. Direct Clang execution also
returned exit zero with `30 Tests / 0 Failures / 0 Ignored`.

A fresh short-path MSVC 19.44.35228 static Debug cache at `E:\zrb\odf` then
configured and built the complete focused dependency graph (`636` Ninja
actions) through core, parser, AOT C, LLVM, system exceptions, libraries, and
the test executable. The same five-file SHA-256 fence remained byte-identical
across the build. Direct Windows execution returned exit zero with
`30 Tests / 0 Failures / 0 Ignored`.

The requirement review then extended the existing named-error runtime case to
cover both absent receiver domains: an expired `Weak<T>` still throws
`NullReferenceError` through direct member-call syntax, and `wake(weak)` first
materializes an absent `Shared<T>?` whose direct member call throws the same
named error. The short-path MSVC cache rebuilt the affected parser/test graph;
the four-file `ast.h`, `type_system.h/.c`, and focused-test SHA-256 fence stayed
unchanged throughout, and direct execution remained 30/30 with exit zero. This
is a worktree precheck; GCC/Clang and final stable-HEAD replay remain required.

`ctest -N` on that configured precheck graph reported 134 registered tests, not
the 133 from the earlier `0a46151` baseline. Subsequent concurrent work can add
targets, so the final three-toolchain matrix must re-enumerate and execute the
complete stable-HEAD graph rather than reuse either earlier count as completion
evidence.

Requirement-by-requirement review also found a normal-success lifetime defect
that the method-call and exceptional-unwind cases did not cover. A minimal
project probe against the latest available precheck CLI executed
`weak?.child.value`, released the explicit Shared owner, and then called
`wake(weak)`. The member value was correct, but the probe returned branch code
`42` because the Weak target remained live; the direct `weak.child.value`
control returned `1` and released correctly. The optional lowering success path
copied the suffix result into its merge slot but reset `guardedSlot` only on the
null branch. The focused regression now requires the post-chain wake to return
null, and `compiler_receiver_guard_finish` resets a distinct guarded slot after
the successful result copy. The regression is a separate thirty-first focused
case, so the prior 30/30 evidence did not cover it.

Commit `e2881bf` (`fix(semantics): complete ownership member cutover`) contains
the exact seven-path implementation, regression, module-contract, and language-
specification slice. The retained static Debug caches rebuilt after that commit
and directly executed the focused runner with real exit code zero:

```text
                                      GCC 11.4   Clang 14   MSVC 19.44
ownership intrinsic/member separation    31/31      31/31       31/31
```

The three runs prove both the direct nullable `NullReferenceError` addition and
the normal-success hidden-owner release case. The intentionally injected raw
builtin id `8` still prints its expected compile failure, but the Unity process
summary is `31 Tests / 0 Failures / 0 Ignored` on every toolchain.

The retained injected-AST regression requires raw id `8` to fail compiler
lowering, while numeric assertions preserve `INTO_GC=9` and `RETURN_LOAN=10`.
Static review found zero production consumers for the deleted qualifier/helper
APIs, zero current-source matches for the removed DETACH identity, zero
matches for the removed ownership-semantic member selector
`ZrParser_OwnershipMemberNameToBuiltinKind`, and zero production parser
branches for the removed percent-prefixed source forms. The retained
post-lookup migration diagnostic recognizes a removed spelling only after a
real member lookup fails; it publishes a safe edit and never selects ownership
typing or lowering. The canonical Syntax leaf selector again reported
`TOTAL=55 COMPLETE=55 MISSING_STATUS=0 MISSING_TIME=0`.

The subsequent code review found that raw historical builtin id `8` still
passed construct-expression type inference: the unknown enum produced no
operand-error message, fell through to qualifier `NONE`, and published an
`UNKNOWN` ownership fact before compiler lowering rejected it. A dedicated TDD
case first reproduced `32 Tests / 1 Failure / 0 Ignored` with `Expected FALSE
Was TRUE`. Commit `7d4029d` (`fix(semantics): reject removed ownership builtin
ids`) validates allowed construct ownership kinds before operand inference;
raw id `8` and the internal `RETURN_LOAN` cleanup identity now fail before
result rewriting or fact publication.

The focused support-first matrix then rebuilt and directly executed on all
three toolchains with real exit code zero:

```text
                                      GCC 11.4   Clang 14   MSVC 19.44
ownership intrinsic/member separation    32/32      32/32       32/32
type inference                           123/123    123/123     123/123
semantic facts                            14/14      14/14       14/14
```

The 4,000-plus-line inference core was not split for this narrow validation:
the change adds no new responsibility and belongs at the existing construct-
inference boundary; extracting only a local enum guard would increase
indirection without reducing that file's established inference scope.

This remains pre-acceptance evidence only. The ownership cleanup is committed
and `type_system.h/.c` are released, but unrelated L8 and AOT work still changes
the shared parser baseline. Full CTest, CLI, artifact regeneration, and final
status promotion must use the stable integrated baseline.

On 2026-08-26, the current Windows shared-library graph was independently
checked against two reported unresolved-external failures. Existing CMake
already links the debug expression diagnostics target to the debug library and
routes the LSP interface target through the language-server link helper. A fresh
MSVC 19.44 short-path cache linked both targets and direct execution returned
zero: debug expression diagnostics passed 56/56 and the full LSP interface
runner reported no failures. No duplicate or compensating CMake link edit was
made.

After the source type-hierarchy relation work was integrated at `3a36ddf`, the
two ownership-focused runners were rebuilt again on GCC 11.4, Clang 14, and
MSVC 19.44. A thirteen-file SHA-256 fence covered both test sources and the
shared compiler, relation, type-system, and native-inference inputs; every hash
was identical before and after the builds. The retained GCC cache initially
linked an old `semantic_relations.c.o` that did not export the newly integrated
relation publisher. `nm` distinguished that stale object from the Clang object,
which did export it. Removing only the stale GCC object and letting Ninja
rebuild it closed the cache issue without a source change. Direct execution on
all three toolchains then returned zero:

```text
                                      GCC 11.4   Clang 14   MSVC 19.44
ownership intrinsic/member separation    32/32      32/32       32/32
owner/borrow receiver guards                7/7        7/7         7/7
```

The seven-case receiver suite includes the negative method- and property-ref
escape checks for a temporary Weak wake owner. This is fresh focused evidence
on the current committed relation baseline, but the reserved L8 external
callable-value paths still require integration before the final full graph can
be accepted.

Four subsequent requirement gaps were closed with test-only exact commits.
`56ea4b5` adds a computed-index suffix case: a live
`weak?.values[bump()]` evaluates `bump` exactly once, while an expired target
returns null without evaluating the index. `abc681d` proves that the receiver
expression itself is evaluated exactly once for both live and expired optional
chains. `80f0476` proves that a live optional property access enters its
throwing getter and catch path, while an expired receiver returns null without
invoking the getter. `71b914e` executes a canonical nullable callable through
`?.(params)`: the live path evaluates its argument once, the absent optional
path skips the argument and returns null, and the absent direct call raises
catchable `NullReferenceError` before argument evaluation. Direct GCC execution
passes the expanded ownership runner with `36 Tests / 0 Failures / 0 Ignored`
and exit zero. These additions are not yet three-toolchain evidence; Clang,
MSVC, and the full registered graph remain part of the final stable-baseline
replay.

The artifact requirement was then reviewed against the real `.zro` writer and
reader. `SZrOwnershipIntrinsicFact` and `SZrReceiverGuardFact` are AST-backed
compiler facts and are not artifact records. Their canonical executable
projection is serialized instead: ownership/guard opcodes, patched jump bounds,
merge/reset slots, SemIR TypeRefs and effects, typed-binding TypeId/PlaceId,
exception tables, and cleanup instructions. The current five intrinsic facts
always publish `loanId == 0`, so serializing that compiler-local placeholder
would add no consumer-visible contract.

A new modular round-trip case compiles and executes live and expired Weak
member paths, writes `.zro`, reloads the complete function graph, compares the
recursive ExecBC, SemIR, TypeRef, TypeId/PlaceId, exception-table, and child-
function projection, then executes the imported graph. Both source and imported
functions return `2`; the optional path retains/skips correctly and the direct
expired path catches `NullReferenceError`. Current GCC directly reports
`37 Tests / 0 Failures / 0 Ignored` with exit zero. The initial test runs also
locked two writer contracts: zero-length tables are valid, and non-struct
`staticCTypeId` is normalized to
`ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE` during serialization.

The checked-in `lsp_language_feature_matrix` artifact set was then rebuilt with
the production CLI entry:

```text
zr_vm_cli --compile tests/fixtures/projects/lsp_language_feature_matrix/lsp_language_feature_matrix.zrp --intermediate
```

The WSL GCC 11.4 compiler rebuilt all four modules. Exactly seven tracked
outputs changed semantically: `async_native.zri`, `core_semantics.zri`,
`oop_meta.zri`, and all four module `.zro` files; `main.zri` content and every
source fixture remained unchanged. A second compile from the canonical
repository path changed zero SHA-256 hashes. The regenerated
`async_native.zro` contains `wakeView` and no historical `upgraded` spelling.
GCC 11.4, Clang 14, and MSVC 19.44 each loaded the same regenerated project with
`--execution-mode binary --emit-executed-via`, printed `matrix`, returned `64`,
reported `executed_via=binary`, and exited zero.

The producer choice is material with the current execution-layout schema.
`SZrTypeValue` has a larger private host layout on WSL than under MSVC; the WSL
artifact's larger inline value slots are accepted by all three loaders, while a
diagnostic MSVC-produced artifact used 16/8 aggregate layout and returned the
wrong value under the WSL loaders. The checked-in portable fixture is therefore
generated by WSL GCC. This acceptance does not claim that arbitrary `.zro`
execution layouts are producer-ABI independent. `.zro` hashes also differ when
the same project is generated below another worktree because the artifact
retains absolute source/project mappings, so determinism is fenced by repeated
output at one resolved root.

The requirement-by-requirement code review then identified a remaining
fact-consumer defect. `SZrReceiverGuardFact` published `chainSegmentEnd` and
`resultLift`, but compiler lowering used only `chainSegmentStart` and closed all
optional frames from the AST chain end. An injected shortened-suffix fact first
produced the expected RED result, `40 Tests / 1 Failure / 0 Ignored`, because
lowering did not reject the drift. The fix validates guard kind/mode, node and
start identity, the full dominated-suffix end, and mode-compatible result lift;
the lowering frame retains the fact-owned end/lift, finalization checks the
reached end, and absent-result emission explicitly consumes `NULLABLE` versus
`VOID_NOOP`.

The new cases live in
`tests/parser/test_ownership_receiver_guard_contract_cases.h` rather than
expanding the already oversized runner. They compare live and expired
`weak?.a.b` with `weak?.a?.b`, assert the complete emitted wake count, verify
direct inner expiry throws while optional inner expiry returns null, prove an
expired outer receiver skips the whole mixed suffix, and execute 32 repeated
live/expire/wake transitions.

The first MSVC execution exposed a lifetime defect after the inner direct guard
threw and was caught: normal chain finalization was bypassed, so the outer hidden
Shared wake retained `parentShared`'s target. The earlier transient-stack
hypothesis was falsified by instruction and exception-scope diagnostics. The
lowering fix emits `MARK_TO_BE_CLOSED` immediately after every guard-owned
`OWN_WAKE`, closes each registration in LIFO order on normal live/absent paths,
and relies on the handler's saved scope boundary to close the same registrations
during exception unwind. The mixed-chain regression also registers
`zr.system.exception` before execution; otherwise the named
`NullReferenceError` prototype is absent and the test enters generic status
normalization instead of the contract under test.

The follow-up review suspected that a direct guard could close an
ownership-valued result when member lowering reused the marked slot. New direct
and outer-optional/inner-direct cases both return `Shared<Leaf>`, release every
other strong owner, and then read the retained leaf. They passed against the
pre-response lowering, which demonstrates that the VM's registered owner mirror
releases the hidden wake without clearing the copied final result. No speculative
production change was made for that disproved finding.

The same review found valid fail-closed gaps. The fact-injection matrix now
covers a nonzero shortened suffix, AST access-mode drift, nullable-versus-Weak
kind drift, value-versus-void result-lift drift, and a missing guarded member
segment. Lowering validates receiver identity/type, guarded type, syntax mode,
full bounds, and the canonical chain result. A follow-up review showed that
`receiverType` was still self-certifying and that the member-only missing-fact
gate allowed a nullable/Weak callable segment to fail open when another guard
fact suppressed inference repair. Three independent cases now cover canonical
receiver drift, guarded-type drift, and a missing nullable callable guard fact.
Guard inference publishes the receiver expression fact, lowering compares the
guard's receiver type against it before deriving kind/guarded type, and missing
member or function-call facts fail closed for canonical nullable/Weak receivers.
Known non-null optional callable lowering remains valid without a fabricated
guard fact.

Fresh isolated snapshots representing main `075d68c` plus the exact ownership
overlay report the following serial direct results on GCC 11.4, Clang 14, and
MSVC 19.44:

```text
Shared/Weak                               19 Tests / 0 Failures / 0 Ignored
ownership intrinsic/member separation    37 Tests / 0 Failures / 0 Ignored
type inference                           123 Tests / 0 Failures / 0 Ignored
expression facts                          28 Tests / 0 Failures / 0 Ignored
compiler integration                     127 Tests / 0 Failures / 0 Ignored
SemIR pipeline (GCC/Clang)                13 Tests / 0 Failures / 0 Ignored
SemIR pipeline (MSVC registered set)      12 Tests / 0 Failures / 0 Ignored
```

This accepts the focused receiver-guard correction on all three toolchains. It
does not replace the pending full-graph replay on the stable integrated L8
baseline.

The current first-party test-source audit found zero disabled `#if 0` blocks,
zero commented `RUN_TEST` registrations, 165 `TEST_IGNORE_MESSAGE` sites with a
nearby explicit platform/capability guard, and zero globally unreferenced
`static test_*` candidates across 4,544 declarations. The six executable `.zr`
paths that still contain removed percent spellings are intentionally scoped as
negative migration input or literal/comment filtering fixtures. They remain
current rejection coverage and are not stale positive syntax tests.

### Pre-final completion-criteria audit

| Design criterion | Current evidence | Gate state |
| --- | --- | --- |
| Ownership control has only the five intrinsic source calls | Dedicated intrinsic AST/facts and focused parse/type/lower tests; production percent branches and ownership-lowering member selector searches are empty | Implemented; final matrix pending |
| `.` and `?.` always perform target access | Same-name object/module member regressions use ordinary member lowering; the post-failed-lookup migration diagnostic cannot select ownership semantics | Implemented; final matrix pending |
| Direct absent nullable/Weak access throws `NullReferenceError` | Direct weak runtime case verifies the named error; live missing-member regression preserves its distinct error | Implemented; final matrix pending |
| Optional absence skips the complete suffix and returns null or a void no-op | Optional member/call, nullable callable, argument-side-effect, computed-index, receiver single-evaluation, getter-skip, mixed-boundary, repeated-transition, nullable-lift, and void-noop focused cases pass; injected fact drift and missing member/call guards fail closed | GCC/Clang/MSVC Shared/Weak 19/19; full matrix pending |
| A Weak chain wakes once, retains the hidden Shared owner through the suffix, and releases it afterward | Every guard wake has an adjacent close marker; deep-chain, suffix-throw cleanup, caught-inner-throw cleanup, ownership-valued direct/mixed results, repeated-call, mixed `weak?.a.b`/`weak?.a?.b`, 32-transition loop, native-GC-pressure, and performance cases pass | GCC/Clang/MSVC Shared/Weak 19/19 and ownership 37/37; full matrix pending |
| Intrinsic and guard facts drive VM and AOT backends | Lowering validates and consumes guard bounds/lift; semantic-fact, SemIR, interpreter, AOT C, LLVM, artifact-reader, and cleanup regressions exist; recursive `.zro` comparison proves the lowered execution projection rather than serializing AST-backed facts | three-toolchain 123/28/127 plus SemIR 13/13 (GCC/Clang), 12/12 (MSVC); final integrated replay pending |
| Intrinsic spellings remain legal member names | The focused collision case executes members named `share`, `degrade`, `wake`, `intoGc`, and `drop` through normal dispatch | Implemented; final matrix pending |
| Old source syntax and string compatibility routes are removed | Production searches are empty; migration cases require canonical receiver facts and structured fixes | Implemented; final inventory pending |
| Artifacts, LSP, docs, tests, and status describe one language | The tracked matrix artifacts were regenerated deterministically; `async_native.zri/.zro` both contain `wakeView`, and binary execution returns `64` through the regenerated graph | Implemented; final stable-HEAD consumer replay pending |
| Evidence is fresh on GCC, Clang, and MSVC | Isolated snapshots on all three directly pass Shared/Weak 19/19, ownership 37/37, type inference 123/123, expression facts 28/28, and compiler integration 127/127; SemIR is 13/13 on GCC/Clang and 12/12 on MSVC | Focused correction accepted; full integrated replay pending |

## Pending final acceptance

- Clean detached GCC 11.4, Clang 14, and MSVC 19.44 Debug builds at intermediate
  baseline `0a46151` each passed all 133 registered CTests with zero failures.
  The three CLI smokes printed `hello world` and exited zero. This closes the
  earlier stdio/document-sync and MSVC project-pressure failures, but it
  predates the current L8 callable-value support and therefore is not yet the
  final stable-HEAD replay.
- The migration-inventory protocol currently passes 9/10. Its only failure is
  the intentionally stale repository golden; regeneration is deferred until the
  concurrent tracked LSP overlay is exact-committed so no intermediate state is
  frozen into the deterministic baseline.
- Complete the LSP focused replay after unrelated concurrent LSP source reaches a
  compilable baseline.
- Complete full matrices and CLI smoke on the final integrated main baseline.
- Replay tracked matrix artifact generation and binary consumption on the final
  integrated main baseline; the current deterministic generation evidence is
  from the fixed pre-L8 compiler snapshot.
- Regenerate inventory after concurrent LSP work stops changing the shared HEAD.
- Run final source/alias search, `git diff --check`, exact-path review, and status
  promotion.
- Old `ownership-post-stdio-*` WSL caches plus `E:\zrs\of` and `E:\zrb\ofm`
  were removed after newer evidence superseded them. After recording the final
  serial focused matrix, cleanup also removed and verified absent:
  `E:\zrs\ownership-review-f77`, `E:\zrb\orm`,
  `/home/hejiahui/.codex-snapshots/ownership-review-f77`, and the corresponding
  `ownership-review-f77-gcc` and `ownership-review-f77-clang` build roots. The
  final matrix created no persistent log. Existing `.codex/logs` artifacts are
  owned by L8, Q6, or other sessions and are preserved rather than conflated
  with this task's cleanup scope.

No plan or syntax status is promoted to completed until all pending gates pass.
