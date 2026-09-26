# Astra Ownership Supplement Acceptance

Status: in progress; integrated completion is not claimed.

Plan: `docs/plans/astra/syntax/ownership-object-member-separation.md`.
Starting main baseline: `c95e5387`; concurrent LSP commits through `55e6ba07`
were preserved. Domain-only defect commit: `7bf4cdbe`.

## Reproduced Defects

| Defect | RED evidence | Implemented support |
| --- | --- | --- |
| Pending Shared/Weak retention | Clear kept strong 2 instead of 1 and weak 3 instead of 2 | Detach then release; valued copy before replacement; reset uses protected cleanup |
| Native Drop nonlocal exit | Final strong cleanup left implicit weak and drop-in-progress; callback pending replaced caller pending | Protected resource lifecycle, callback-local pending drain, anchored caller restoration, finish final strong before reporting failure |
| AOT cleanup reentry | Catch/EndFinally kept stale relocated frame; Throw reported mutated 99 instead of original 73 | Normalize before clear, refresh after clear |
| Foreign domain release | Value reset to NONE but origin count unchanged | Domain preflight before value reset |
| Loop pending destination | Quickening moved break destination 60 to 49 without remapping; continue stayed 0 | Resolve absolute label patches and remap destinations on compaction/insertion |
| Call-result staging | Explicit drop delayed resource destruction until caller teardown | Transfer materialized owner into physical caller result slot |
| Aliased member result | A C-local stableResult retained a Shared field without release; final graph drop count 7 instead of 8 | Transfer stable ownership result before releasing old receiver in all three cache branches |
| AOT runtime layout hint | C/LLVM IR builder rejected flags 32 against canonical mask 31 | Strip derived DIRECT_VALUE hint during validation/projection; serialized ABI unchanged |

## Preliminary Execution

All configurations are static Debug, extension and Rust binding disabled.
These executions used the live main-tree overlay; the final frozen-source replay
will supersede them. Exit codes and Unity counts were inspected separately.

| Target | GCC 11.4 | Clang 14 | MSVC 19.44 |
| --- | --- | --- | --- |
| Ownership separation | 53/53, exit 0 | 53/53, exit 0 | 53/53, exit 0 |
| Unique/Drop | 20/20, exit 0 | 20/20, exit 0 | 20/20, exit 0 |
| CFG finally | 7/7, exit 0 | 7/7, exit 0 | 7/7, exit 0 |
| Exceptions | 8/8, exit 0 | 8/8, exit 0 | 8/8, exit 0 |
| Shared/Weak after final alias/reset repair | pending replay | pending replay | 54/54, exit 0, no ignored |
| Core member-access alias regression | pending replay | pending replay | 108/108, exit 0, no ignored |
| Receiver benchmark | 1/1, five scenarios | 1/1, five scenarios | 1/1, five scenarios |
| C/LLVM receiver products | initial 10/10 failures at IR validation; repair pending replay | same | 10 capability-ignored, not execution passes |
| Compiler integration | abort, native process status 6 | abort, status 6 | 200-second timeout at GC fragment stress |

The compiler integration backtrace reaches
`zr_container_storage_remove_last -> ZrCore_HashSet_Remove -> free` with an
invalid hash-node pointer. Attribution is not established from this stack alone;
no unrelated container repair or test suppression has been made.

## Runtime Parity

The shared fixture encodes finally trace `1234567789` and eight exact resource
drops as `123456778908`. It exercises Shared/Weak returns, finally exception
override, loop break/continue, and a Shared result returned through two weak
receivers. MSVC VM and binary readback both pass. The binary is written before
execution and loaded into a fresh state to avoid reusing mutated static state.
C and LLVM still require the repaired AOT prerequisite replay.

## Performance Observation

The preliminary GCC Debug run reports ns/operation:
direct 6838.053; weak direct 12343.587; optional success 11500.102;
optional failure 2212.687; deep chain 3071.086. Every scenario checks checksum
344064. These concurrent, unpinned Debug timings are observations, not a claimed
performance improvement. Structural single-wake/no-wrapper checks remain part
of the ownership separation suite and final design review.

## Open Gates

- Frozen source manifest equality for GCC, Clang and MSVC final replay.
- Sanitizer lifecycle and member-alias execution.
- C/LLVM generated products and same-source binary parity.
- Current full registered graph (initial GCC discovery: 152 entries), CLI/LSP smoke.
- Matrix regeneration twice, stable SHA-256, three consumers returning 64.
- Official migration golden regeneration twice and both Python test suites.
- Final review, detailed defect commits, and task-owned cache/log cleanup.

The syntax-status verifier was replayed: TOTAL=55, COMPLETE=55, missing status,
non-complete status, and missing completion time all zero. This checks the
historical record structure; it does not close the runtime gates above.
