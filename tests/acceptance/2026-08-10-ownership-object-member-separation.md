# Ownership And Object Member Separation Acceptance

## Status

- Design: `docs/superpowers/specs/2026-08-10-ownership-object-member-separation-design.md`
- Implementation plan: `docs/superpowers/plans/2026-08-10-ownership-object-member-separation-implementation.md`
- Review date: 2026-08-23 (UTC+08:00)
- Status: `validated_pending_full_acceptance`

## Accepted source contract

| Requirement | Implementation evidence | Focused evidence |
| --- | --- | --- |
| Ownership uses only five reserved intrinsics | `ZR_AST_OWNERSHIP_INTRINSIC_EXPRESSION` and `EZrOwnershipIntrinsicOperation` | parser/semantic/runtime suite 27/27 |
| `.` and `?.` never classify ownership by member text | real member lookup precedes the structured migration diagnostic; compiler lowers only fact-owned intrinsic nodes | same-name object method and real-member tests |
| Weak direct access is guarded and throws `NullReferenceError` on expiry | `SZrReceiverGuardFact`, `REQUIRE_NON_NULL`, one hidden wake owner | direct-expiry test passes and catches the named runtime error |
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

## Pending final acceptance

- Clean detached GCC 11.4, Clang 14, and MSVC 19.44 Debug builds at intermediate
  baseline `0a46151` each passed all 133 registered CTests with zero failures.
  The three CLI smokes printed `hello world` and exited zero. This closes the
  earlier stdio/document-sync and MSVC project-pressure failures, but it
  predates the still-unreleased L8 external callable-value parser overlay and
  therefore is not yet the final stable-HEAD replay.
- The migration-inventory protocol currently passes 9/10. Its only failure is
  the intentionally stale repository golden; regeneration is deferred until the
  concurrent tracked LSP overlay is exact-committed so no intermediate state is
  frozen into the deterministic baseline.
- Complete the LSP focused replay after unrelated concurrent LSP source reaches a
  compilable baseline.
- Complete full matrices and CLI smoke on the final integrated main baseline.
- Regenerate inventory after concurrent LSP work stops changing the shared HEAD.
- Run final source/alias search, `git diff --check`, exact-path review, and status
  promotion.
- Old `ownership-post-stdio-*` WSL caches plus `E:\zrs\of` and `E:\zrb\ofm`
  were removed after the newer full-matrix evidence superseded them. Retain only
  the current final-replay caches until L8 integration is verified, then remove
  those remaining task-owned build/source directories and logs.

No plan or syntax status is promoted to completed until all pending gates pass.
