# Ownership And Object Member Separation Acceptance

## Status

- Design: `docs/superpowers/specs/2026-08-10-ownership-object-member-separation-design.md`
- Implementation plan: `docs/superpowers/plans/2026-08-10-ownership-object-member-separation-implementation.md`
- Review date: 2026-08-14 (UTC+08:00)
- Status: `validated_pending_full_acceptance`

## Accepted source contract

| Requirement | Implementation evidence | Focused evidence |
| --- | --- | --- |
| Ownership uses only five reserved intrinsics | `ZR_AST_OWNERSHIP_INTRINSIC_EXPRESSION` and `EZrOwnershipIntrinsicOperation` | parser/semantic/runtime suite 24/24 |
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

The former `Module.share()` ownership escape is rejected: a guarded module
payload is a scoped plain view, not an owner operand, and the compiler alone
creates and releases the hidden owner that keeps the loaded module alive. A
real `share` member declared by a module type still compiles as an ordinary
member call and never lowers to `OWN_SHARE`. The same rule applies to ordinary
user-defined methods named `degrade`, `wake`, `intoGc`, or `drop`.

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
- Old ownership member syntax remains only as migration input, negative input,
  same-name member collision coverage, or quoted historical evidence.
- The stale signature-help test that expected an AST/name fallback for an
  invalid call was removed. Current signature help requires a canonical
  call-target fact and fails closed without one.
- Obsolete script/golden assets with no active consumer are deleted; generated
  snapshots are kept in build-local `tests_generated/` directories.

## Fresh focused validation

GCC 11.4, Clang 14, and MSVC 19.44 Debug focused direct execution each
passed the portable ownership/parser/runtime matrix:

```text
ownership intrinsic/member separation  24/24
removed percent syntax cutover            7/7
owner/borrow receiver escape checks       7/7
semantic facts                          14/14
type inference                         122/122
resource Unique/Drop                    20/20
resource Shared/Weak                    11/11
exceptions                               8/8
```

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
`-fsanitize=address,undefined -fno-omit-frame-pointer`. With leak detection
disabled and both sanitizers configured to halt on the first error, the target
passed 24/24 with no AddressSanitizer or UndefinedBehaviorSanitizer report.

## Pending final acceptance

- The prior Clang full build completed 1460/1460 and its registered matrix passed
  124/126; only two LSP callable-value suites failed against the then-active LSP
  overlay. A fresh whole-repository replay is still required after that overlay
  is exact-committed.
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
- Remove task-owned build caches and logs after their evidence is recorded.

No plan or syntax status is promoted to completed until all pending gates pass.
