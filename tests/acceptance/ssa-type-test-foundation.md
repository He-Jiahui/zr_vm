# SSA canonical type-test foundation acceptance

## Scope

This phase introduced a reusable canonical type-membership fact without
claiming executable subtype dispatch. The later bounded source producer is
recorded in `ssa-source-typed-catch-cfg.md`.

- SemanticIR appends `TYPE_TEST` without renumbering existing opcodes.
- `typeId` is the result type; `matchTypeId` is the resolved type being tested.
- ExecIR appends the matching pure, one-operand/one-result opcode and stores the
  target independently in `matchTypeToken`.
- Semantic emission, semantic validation, ExecIR construction, and core
  verification require the target exactly for `TYPE_TEST` and reject a hidden
  target on every other opcode.
- Structural and analysis hashes include the target. GVN may common equal tests
  but never tests with different targets. DCE clears the target when replacing
  a dead test with `NOP`.
- The direct Oracle executes `TYPE_TEST` through an explicit
  `FZrExecIrOracleTypeTest` provider; missing providers remain unsupported and
  provider rejection has a dedicated diagnostic. ExecBC and AOT projections
  now preserve its `matchTypeToken` metadata but mark the result non-runnable
  until their executable subtype ABI exists.

## Language evidence

The local reference trees converge on resolved type identity and source order,
not textual type-name comparison:

- CPython's `Python/errors.c` implements exception matching over resolved
  exception objects through `PyErr_GivenExceptionMatches`; interpreter users
  consume that semantic predicate rather than compare names.
- HotSpot's `oops/method.cpp` walks the exception table in order, treats catch
  type index zero as catch-all, and otherwise applies `is_subtype_of` to the
  resolved exception class.
- Mono's `mini/mini-exceptions.c` walks handlers in source order and applies
  `mono_object_isinst_checked` for typed catches; verifier tests separately
  cover typed catch clauses.

The resulting zr_vm foundation therefore carries one canonical match token.
The bounded single-handler producer now takes the canonical match edge and
rethrows when it misses. Future multiple-catch lowering must extend this in
source order, take the first canonical subtype match, and represent catch-all
explicitly.

## Focused coverage

- `test_pre_semantic_ir.c` locks golden formatting and both missing/hidden
  `matchTypeId` rejection.
- `test_ssa_builder_cfg.c` locks direct canonical-token preservation and core
  verifier rejection of missing or misplaced tokens.
- `test_ssa_gvn_range.c` proves equal-target tests may be commoned while
  different-target tests remain distinct.
- `test_ssa_pass_manager_scalar.c` proves the target participates in structural
  hashing and is cleared by dead-code elimination.
- `test_ssa_oracle_projections.c` proves missing-provider rejection, successful
  true/false Oracle membership, provider-error diagnostics, and transactional
  projection transport of the canonical target token. The materialized
  projections remain non-runnable until a subtype backend is connected.

## Deliberate boundary

A bounded single source typed catch now emits `TYPE_TEST`; only the direct
Oracle provider seam interprets it, and no string-based fallback is permitted.
Initial projections preserve the operation as non-runnable metadata. Multiple
catches, rich or unresolved annotations, `finally`, and executable backend
projection remain out of scope.

## Validation

- TDD began with builder tests failing to compile because neither IR exposed a
  canonical match-type field. The first green implementation then drove
  negative missing/hidden-token checks, enum-number stability, GVN keying,
  structural hashing, DCE cleanup, and projection rejection/transport
  coverage.
- Independent review found that GVN's `TYPE_TEST`-to-`COPY` rewrite initially
  retained `matchTypeToken`; the regression now requires the field to be zero
  and runs full core verification on the rewritten function.
- The SemanticIR producer passes 93/93 on Windows MSVC 19.44, WSL GCC 11.4,
  and WSL Clang 14. The builder, GVN, scalar pass manager, and Oracle/ExecBC/AOT
  projection tests pass on all three toolchains.
- WSL GCC 11.4 ASan+UBSan with leak detection and halt-on-error passes the same
  93 producer cases and four ExecIR focused tests without a sanitizer report.

## Oracle provider follow-up

The direct Oracle now has a caller-owned `FZrExecIrOracleTypeTest` seam for
canonical subtype membership. The callback receives the pointer-free operand
and the instruction's `matchTypeToken`; returning `false` in the output slot
is a successful non-match, while returning failure publishes
`ZR_EXEC_IR_DIAGNOSTIC_ORACLE_TYPE_TEST_ERROR`. Without a callback, the
operation remains `ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED`. This keeps runtime type
identity out of the Oracle scalar value model and leaves executable ExecBC/AOT
subtype lowering for a later backend contract. The current projections retain
the match token but advertise `runnable == false` for the operation.

TDD first added the missing-provider and true/false/rejection assertions; the
pre-change build failed because the callback fields and diagnostic did not
exist. The implementation then passed the focused projection test on Windows
MSVC 19.44, WSL GCC 11.4, and WSL Clang 14 (1/1 each). GCC ASan+UBSan with
leak detection passed the same focused test five consecutive times. Wiki
validation passed 116 Markdown files, 115 manifest pages, and 644 local links;
the validator unit suite passed 5/5.

## Projection transport follow-up

The initial ExecBC/AOT projection records now carry `TYPE_TEST` instead of
rejecting it during structural lowering. `SZrExecBcInstruction.matchTypeToken`
is copied from ExecIR, and any projection containing the operation sets
`runnable == false`; projection construction still remains transactional for
malformed input. This is metadata transport only, not executable catch
dispatch.

TDD changed the existing unsupported-projection assertion to require the
preserved opcode/token and non-runnable flag; the pre-change implementation
failed because it rejected the operation and had no projection-side target
field. Focused `ssa_oracle_projections` passed 1/1 on Windows MSVC 19.44, WSL
GCC 11.4, and WSL Clang 14 after the transport change. GCC ASan+UBSan with
leak detection passed the same focused test five consecutive times.
