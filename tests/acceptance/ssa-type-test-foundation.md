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
- Oracle, ExecBC, and AOT projections reject `TYPE_TEST` transactionally until
  runtime canonical subtype evaluation is implemented.

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
- `test_ssa_oracle_projections.c` proves the Oracle, ExecBC, and AOT paths fail
  closed; the materialized ExecBC and AOT projections also preserve their
  existing output on rejection.

## Deliberate boundary

A bounded single source typed catch now emits `TYPE_TEST`; no backend interprets
it, and no string-based fallback is permitted. Multiple catches, rich or
unresolved annotations, `finally`, and runtime projection remain out of scope.

## Validation

- TDD began with builder tests failing to compile because neither IR exposed a
  canonical match-type field. The first green implementation then drove
  negative missing/hidden-token checks, enum-number stability, GVN keying,
  structural hashing, DCE cleanup, and projection rejection coverage.
- Independent review found that GVN's `TYPE_TEST`-to-`COPY` rewrite initially
  retained `matchTypeToken`; the regression now requires the field to be zero
  and runs full core verification on the rewritten function.
- The SemanticIR producer passes 93/93 on Windows MSVC 19.44, WSL GCC 11.4,
  and WSL Clang 14. The builder, GVN, scalar pass manager, and Oracle/ExecBC/AOT
  projection tests pass on all three toolchains.
- WSL GCC 11.4 ASan+UBSan with leak detection and halt-on-error passes the same
  93 producer cases and four ExecIR focused tests without a sanitizer report.
