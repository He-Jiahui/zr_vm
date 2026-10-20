---
related_code:
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_member_facts.c
  - zr_vm_parser/src/zr_vm_parser/compiler/extern/compiler_extern_declaration.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_type_prototypes.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/analyzer/semantic_analyzer_symbols.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_semantic_analyzer_lsp_native_range_consumer_cases.h
  - tests/language_server/test_semantic_analyzer_prototype_identity_cases.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/analyzer/semantic_analyzer_typecheck.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/analyzer/semantic_analyzer_declaration_binding.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_bindings.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_declaration_binding.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_context_lookup.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_variable_lookup.c
  - zr_vm_parser/src/zr_vm_parser/semantic/type_declaration_context.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_types.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_diagnostics.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_type_publication.h
  - zr_vm_language_server/src/zr_vm_language_server/index/symbol_table.c
  - zr_vm_parser/src/zr_vm_parser/types/type_system.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_callable_lookup.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_callable_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_call_binding.c
  - tests/language_server/test_semantic_declaration_budget.c
  - tests/language_server/test_semantic_environment_budget_cases.h
  - tests/language_server/test_semantic_analyzer_declaration_context_cases.h
  - tests/cmake/zr_vm_lsp_declaration_budget_tests.cmake
  - tests/cmake/lsp-component-targets.cmake
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_member_facts.c
  - zr_vm_parser/src/zr_vm_parser/compiler/extern/compiler_extern_declaration.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_type_prototypes.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/analyzer/semantic_analyzer_symbols.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/analyzer/semantic_analyzer_typecheck.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/analyzer/semantic_analyzer_declaration_binding.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_bindings.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_declaration_binding.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_context_lookup.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_variable_lookup.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_callable_lookup.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_callable_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_call_binding.c
  - zr_vm_parser/src/zr_vm_parser/types/type_system.c
  - zr_vm_parser/src/zr_vm_parser/semantic/type_declaration_context.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_types.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_diagnostics.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_type_publication.h
tests:
  - tests/language_server/test_semantic_analyzer_lsp_native_range_consumer_cases.h
  - tests/language_server/test_semantic_analyzer_prototype_identity_cases.h
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_semantic_analyzer_hygiene_binding_cases.h
  - tests/language_server/test_semantic_analyzer_declaration_context_cases.h
  - tests/language_server/test_semantic_declaration_budget.c
  - tests/language_server/test_semantic_environment_budget_cases.h
  - tests/language_server/test_semantic_analyzer_local_binding_identity_cases.h
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_lsp_semantic_query_parity.c
  - tests/parser/test_semantic_declaration_binding_cases.h
plan_sources:
  - docs/plans/syntax/2026-10-08-macro-tokenstream-structured-generation-design.md
  - .codex/plans/20261008-macro-tokenstream-structured-generation.md
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/astra.md
  - .codex/plans/20260926-lsp-experience-repair.md
doc_type: module-detail
---

# LSP Typecheck Canonical Local Bindings

## Purpose

The language-server typecheck pass enriches the parser-owned semantic snapshot
with inferred binding types. For a source local, this pass must attach the
existing `SymbolId` and `TypeId` to the type environment so identifier reads,
hover, navigation, references, highlights, and rename all observe one binding.

## Binding and Range Contract

Symbol collection publishes a variable symbol with the declaration node as its
location and the identifier pattern as its selection range. Typecheck resolves
the existing record by the same AST declaration and binding name within the
current snapshot and namespace-normalized syntax context. It does not locate a declaration by a position or a same-name
symbol in another scope.

For value and callable declarations, the required contract is that presentation-symbol registration and declaration type binding pass
`type_environment_declaration_context` for their actual declaration node to
the shared checked lookup with its separate out-record argument. Functions and extern functions use their
name context, parameters use the parameter name, and variables use their
identifier pattern. A hardcoded root context can choose a distinct canonical
record even when AST pointer, spelling and symbol kind all match.

The focused r1-r3 RED input deliberately supplies Root to both calls as a
temporary behavioral mutant. It is not the final implementation; r3 observes
the intended marked-declaration failures before the actual-context repair.

Nominal TYPE symbols need the existing `type_declaration_context.h` contract,
not the value/callable helper's default Root. Their lookup uses a validated ITEM
context and canonical registration retains the raw context. Missing nominal
identifiers, unsupported declaration kinds or invalid contexts must not publish
an unmarked replacement. Valid Root and ITEM-normalized MixedSite contexts
remain legitimate Root identities.

The r3 Root/name-only `Compiler_RegisterTypeBinding` is a further repair
site: it must reuse a same-AST owned binding and diagnose a different declaration
only within the same ITEM context. Existing imported destructured type aliases
also use this entry, so a nominal-only selector cannot replace their behavior
without checking the alias context. These are required repairs, not claims that
the r3 RED source already satisfies the contract. The current public
TokenStream parser and analyzer can reach nominal pre-inference before symbol
collection; the defect is not deferred until M6.

The shared declaration-binding module reuses this identity when inserting the
binding into the current lexical type environment. The environment retains the
declaration's exact selection range. Source bindings without an exact type keep
a valid `SymbolId` and an invalid `TypeId`; they remain discoverable and surface
`cannot infer exact type`. Source declarations without a canonical identity no
longer create anonymous replacement bindings during typecheck.

## Extern Provenance And Preview Completion

Extern callable selection uses the actual identifier leaf retained by the
parser, checked against declaration ownership and range evidence. It does not
calculate a name position from the callable keyword or reconstruct a range
without evidence. The delegate regression compares selection and canonical
declaration/definition ranges with that real leaf.

Enum member-use publication accepts normal `CLASS_FIELD` metadata backed by
a real `ENUM_MEMBER` AST. Its canonical `FIELD` has a valid inferred type,
the original member declaration, checked identifier definition range and raw
syntax context. A parser/inference companion requires exactly one resolved use
fact; hover text is not an alternate identity source.

Bootstrap prototypes retain their actual declaration identity and an explicit
shell flag. Normal extern construction may complete a preview instead of
skipping its metadata or reporting a false duplicate. Only successful normal
construction and binding replace the same open shell. Partial failure keeps
the original metadata and reports the normal error; closed generic records
cannot be selected merely because their AST matches. The
[metadata ownership contract](type-prototype-metadata-ownership.md) defines
owned-array transfer, not a global semantic rollback guarantee.

## Lifetime and Exactness

`SymbolId` and `TypeId` are valid only in the current semantic context. The
typecheck environment stores the copied ids and declaration range; it does not
retain a borrowed symbol record. Query consumers must continue to resolve the
copied id through the same snapshot and fail closed when the identity or source
does not match. No request-time name, range, AST, or display-text matching is
part of this contract.

Declaration registration is extracted into `semantic_analyzer_declaration_binding.c`
and `type_environment_bindings.c`. Return pre-inference, symbol collection and
typecheck share the same AST declaration identity. See
[canonical declaration bindings](canonical-declaration-bindings.md) for the
pre-inference, anonymous binding and unknown-type contracts.

## Checked Lookup Implementation

The stopped ten-path candidate replaces the ambiguous pointer-only exact query
with `TZrBool type_environment_find_declaration_symbol(..., outRecord)`. Its
status and result are independent:

- FALSE means invalid input or failed normalization; callers must stop.
- TRUE with NULL output is a valid miss, not an error.
- TRUE with a record is an exact declaration hit in this semantic snapshot.

The helper requires an output pointer, clears it before validation, and rejects
invalid context, missing semantic context or missing name. A NULL declaration
with otherwise valid inputs is a successful miss without normalization. TYPE
and FUNCTION use ITEM; other declaration kinds use LOCAL. The first pass checks
all same-AST/name/kind records for valid raw context equality before allocating
any scratch. Thus an earlier Root decoy cannot consume the budget before a later
raw owner. Only the second pass normalizes: lazily once for the query and once
per relevant candidate, stopping immediately on failure. No compatibility
pointer adapter remains for the six exact-query callers.

The private [context lookup header](../../zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_context_lookup.h)
provides a separate checked pair comparator: FALSE is failure, while successful
`outEqual` distinguishes equality from inequality. It validates both raw
contexts before an allocation-free raw-equal hit, otherwise checks each
normalization. This pair operation is not the exact query's two-pass algorithm.
The [variable lookup module](../../zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_variable_lookup.c)
owns private current-scope and full-parent-chain BOOL/out-binding finders and the
four existing public Find/Lookup wrappers extracted from `type_system.c`.
Registration and callable-value shadow checks use only the current scope;
RuntimeRoot/Capture guards and public variable lookup use the full chain and
stop on comparison failure. Public signatures are unchanged: pointer-return
wrappers still return NULL for both failure and miss; only checked internal
callers retain that distinction.

Variable registration runs current-binding and, when a semantic context exists,
exact-declaration preflight before inferred-type publication, then reuses the
cached record. Callable duplicate, alias, shadow, match and dedupe paths propagate
checked comparison failure. Exact callable-query failure frees the initialized
local function payload and raw allocation before any SymbolId reservation; it
does not free borrowed canonical records. Single-result lookup clears its output.
After multi-result initialization, comparison/dedupe failure frees and constructs
the result array again instead of returning a partial overload set. Typed-call
annotation reuse takes its existing cleanup path on comparison failure, freeing
parameter inferred types, parameter/mode arrays and the return type before
returning false.

These are implemented, statically reviewed source contracts, not native GREEN.
They add no public ABI, serialized identity, global allocator-OOM rollback or
all-pointer-consumer guarantee. In particular the four no-snapshot regressions
do not directly exercise RuntimeRoot/Capture, typed-call or external-alias
migration; those paths still need their own validation evidence.

## Regression Coverage

The analyzer regression covers inferred and explicitly typed locals, plus nested
same-name locals. It checks declaration/read roles, shared ids and type ids,
declaration-node identity, exact declaration range, and the absence of duplicate
canonical records. The LSP interface tests exercise the same identity through
structured local query and hover payloads. Parser symbol/reference tests and
source snapshot parity retain their lower-layer coverage.

The M1 hygiene companion adds two independent component tests to the existing
then-75-case analyzer inventory without a new CMake target. Each uses a real parsed
variable declaration, a marked DefSite and a same-node/name/kind root decoy with
different canonical IDs and type. The first checks exact symbol identity reuse
without adding canonical records. The second preinstalls the correct display
identity and checks the nonroot binding's IDs, context, complete source range
and absence of a root binding. Fixture initialization errors must be reported
separately from these behavioral failures. The owning fixture keeps its context
table alive until after AST destruction; these tests do not establish an AST
that escapes that fixture. Distinct prerequisite-error returns separate fixture
failure from the intended canonical-binding assertions.

The separate declaration-context companion adds nine frozen cases, bringing
the current inventory to84. It covers real unowned variable/parameter collectors,
preowned ordinary/extern callable IDs, full signatures and actual overload
membership, all six nominal declaration kinds with Root decoys, fresh nominal
misses, real nominal collection, same-AST/context-scoped owner diagnostics,
invalid-input publication and the existing Root shorthand alias path. Root,
DefSite and an ITEM-normalized MixedSite owner control remain distinct. A fresh
delegate must retain REFERENCE; existing UNKNOWN projections must not upgrade.

Collection first establishes a target presentation symbol; missing collection
is a distinct fixture prerequisite, not an identity counterexample. Invalid
publication checks cover collection lengths, ID counters and display IDs; they
are not bytewise comparisons of all preexisting semantic records. Alias tests
require the real imported module/member/type prototypes. Context tables outlive
the analyzer and AST. The actual focused results below distinguish prerequisite
failures, behavioral observations and accepted validation.

The bounded repair does not add context-aware overload grouping or marked
alias/member tooling. Those integration obligations remain explicit M6 work;
the existing parser-owned overload grouping and compiler LOCAL aliases must
not be silently changed into ITEM aliases by this nominal repair.

## Extern Provenance Regression (2026-10-18)

The original 88 analyzer cases remain ordered and unchanged. Two native-range
companions first observe exact delegate-leaf and enum-fact failures. After those
shared repairs, the next 91-case run isolates real bootstrap identity. That
repair passes in the 93-case run while enum-runtime and struct-layout assertions
fail. Those three lower runs are independently audited, not platform acceptance.

Three more append-only cases isolate normal extern field-AST publication,
failure after a valid field followed by an unavailable type, and ordinary enum
same-shell completion through its existing internal compiler entry. The normal
96-case pre-repair run has 91 PASS and exactly five intended behavioral FAIL,
no prerequisite failures, identical full source inventories and six naturally
terminal children. The normal GCC repaired replay then passes all96, including
the original true-duplicate and invalid-FFI golden cases, with six natural exit0
children and unchanged full source4908/CF3B3177, HEAD4f53caa and other inputs.
Receipt62CD3602 and native74B9D87B retain the exact raw evidence. Original LSP
interface and full matrix remain required; no historical full-matrix or GDB
failure is rewritten by this focused result.

## Focused Validation Evidence (2026-10-16)

RED r1 stops at fixture compilation and never starts native tests; its raw
receipt is89F08034. The only fixture repair replaces four nonexistent
`ZR_VALUE_TYPE_INT` uses with the actual builtin-int kind `ZR_VALUE_TYPE_INT64`
and replaces the nonexistent invalid-context constructor with the public
table-owned invalid-mark compound literal. The declaration-context header
was937 lines at that phase. The84 ordered main calls, behavior assertions, exact eleven
expected failures and four main production RED pins were preserved.

RED r2 builds successfully and executes84 ordered unique cases:72 PASS and12
FAIL. All eleven registered failures match their exact reasons without a
fixture prerequisite error, but the existing `Unknown Parameter Preserves
Identity` case also fails: `An unavailable parameter type must not expose an
outer binding or infer its return type`. All4772 source inputs and the recorded
pre/post HEAD match. The raw receipt99902011 remains FAIL because twelve
failures differ from the unchanged eleven-failure contract. That rejection
precedes the binary-post gate, so no binary-post identity result exists.

GDB r1 on the actual RED executable observes case0 passing with parameter
declaration/read symbol2/type0 and callable symbol3/type0. In case1, with an
explicit `:int` return, parameter declaration/read retain symbol3/type0 and the
same owner, but the callable has symbol2/type3. Hover remains NULL because the
callable-type assertion short-circuits before the hover query. This isolates
callable publication, not parameter identity loss or capture of the outer
binding. The diagnostic receipt3FD9CDD9 is raw FAIL after three foreign ExecIR
inputs drift; its binary-post and head-post gates are not reached. It grants
causal observation only, not acceptance.

The `symbols.c` unresolved-parameter branch emitted a diagnostic without setting
`hasUnresolvedParameter`. A one-line prerequisite correction is now pinned as a
fifth production phase input; the four main hygiene production files stayed
RED for r3. Session12432 is terminal with driver exit1: build PID509 exits0,
then native PID644 exits1 with84 ordered unique cases,73 PASS and the exact
eleven expected FAIL names/order/reasons. `Unknown Parameter Preserves Identity`
now passes. Binary E4AEAB40,53963096 bytes, is unchanged after execution.

Receipt19000590 remains raw FAIL: source-pre has4772 inputs and source-post4774,
with four foreign SSA/ExecIR changes. The three tests and five production pins
match across r3 and Root's pre-lease review, but the full source gate fails and
HEAD-post fields are absent. This is dated functional RED for TDD, not accepted
fixed-input RED or platform evidence; a later HEAD observation cannot fill the
missing receipt fields. Under the user's parallel-edit/revalidation decision,
Root authorized the bounded four-file repair. Source review has now frozen that
implementation with the budget-boundary finding below; it is not GREEN. The
original 84-case GREEN remains pending; neither raw source nor HEAD gates are
waived.

### Declaration Budget Follow-Up

Static review finds that `type_environment_find_declaration_symbol` returns
`NULL` both for an absent owner and for budgeted context-normalization failure.
Callers can therefore mistake failure for a miss: a marked preowned callable
may publish a replacement symbol/member, while a fresh nominal may publish and
then return false on its post-publication lookup. Variable registration also
needs lookup failure to precede inferred-type publication. The original
seven-case r1 now supplies dated native counterexamples below, not a claim that
production is repaired or a fixed-input acceptance result.

The original 628-line r1 `test_semantic_declaration_budget.c` freezes seven cases
(SHA 4859A78C): constrained marked-callable reuse, fresh interface/delegate
publication, wide-budget full-identity reuse, Root reuse without scratch heap,
failure for differing raw but ITEM-equivalent contexts, fresh nominal lookup
past an earlier Root decoy, and variable publication ordering. The fixture uses
a dynamically measured positive normalization allowance. Its dedicated CMake
fragment and single include register
`zr_vm_language_server_semantic_declaration_budget_test` /
`language_server_semantic_declaration_budget`; they do not change the original 84
analyzer cases or foundation 52 selection.

Normal WSL GCC budget RED r1 (session 75983) is terminal and drained. Build
PID 534 completes 18/18 actions with exit 0; native PID 767 exits 5 with seven
ordered unique cases, 2 PASS, the exact five expected FAIL reasons and 0 IGNORE,
without fixture prerequisite failure. Receipt3888EF4C remains raw FAIL after
five foreign source changes and HEAD d3936df4 to 06364480. Independent review
confirms seventeen owned source-pre/post pins and binary4DD65559 stable. This
is dated functional TDD RED, not fixed-input acceptance. Helper CE277678's
isolated 8/8 and 10/10 GREEN results remain validator-only evidence, not real
child-interruption or compiler credit. Full identities and boundaries are in the
ledger; budget repair, original 84-case GREEN, GCC/Clang/final MSVC and M1
acceptance/commit remain pending.

### Adjacent Context Comparison Budget Risk

Independent static review also identifies a P2 in the binding and callable
context comparators: normalization failure becomes a nonmatch. A real public
`ZrParser_TypeEnvironment_New(state)` environment with NULL parent and semantic
snapshot can append a duplicate marked variable or callable when scratch is
exhausted. `LookupFunctionsWithContext` can skip a failed later candidate and
return TRUE with only part of the overload set. Invalid contexts do not compare
equal. These are reachable M1 context-budget prerequisite risks, not native
results or regressions attributed to the future checked exact-query repair.
Fixing only exact declaration lookup does not resolve or exempt this P2 from M1.

Four additional public no-snapshot regressions are now authored and frozen:
marked-variable reuse, marked-callable same-AST rejection,
Root/MixedSite ITEM-equivalent same-AST rejection, and fail-closed two-signature
overload lookup. The stopped 637-line main71CDCD5E and 426-line companion
`test_semantic_environment_budget_cases.h` (216F2D59) pass Root and independent
source review; a complete main RAM inverse restores original r1 SHA4859A78C.
Before the checked migration, original seven functions/assertions, original 84
inputs and CMake were unchanged. Normal WSL GCC RED r2, unified75735, is now
terminal: build517 completes six actions/exit0 and native566 exits9 with eleven
ordered unique cases, 2 PASS, nine exact FAIL and 0 IGNORE, without fixture
prerequisite failure.
Receipt08934E6E has identical complete4777 source inventories (09A58B41), eighteen
owned pins and a stable binary, but stays raw FAIL solely for HEAD06364480 to
badc069b. It is dated functional TDD RED, not fixed-input acceptance.

The ten-path checked candidate above is implemented and its author STOPPED;
both new private modules now exist. Root and independent source review report
no new P1/P2, not GREEN. The original84 declaration-context utility header
honestly changes to BA21308E; Root's nine-region RAM inverse restores
8C04A6E7/50775 bytes, preserving every assertion/reason and all84 main calls.
The first GREEN launcher, unified7197, exits1/drained on WSL
CreateInstance/CreateVm/0x800705b4. The helper never starts and all five registered
GREEN outputs are absent after that attempt; no compiler/native result or
helper receipt exists. Minimal WSL `/usr/bin/true` probe8920 also exits1/drained
with the same error, before any Linux command. No machine/service restart or
configuration change is performed. GREEN, platform gates and M1 remain pending;
the timeout cause is not inferred. The ledger retains full raw identities and
dated documentation-only r10 evidence.

The matrix helpers' missing HEAD-false rejection is separately repaired with a
two-line guard in each helper. The unchanged isolated eight-case checker moves
from4 PASS/4 FAIL to8 PASS (receipt3A6A5EC3), with four inputs stable. This is
actual final-verification control-flow evidence only, not native or platform
validation. Full receipts and hashes are in the
[M1 validation ledger](../testing-and-validation/macro-foundation-validation.md).
The budget follow-up, platform matrix and M1 acceptance remain open.
The new standalone budget target is also explicitly registered in each pending
platform helper's full build/CTest/binary inventory (54 becomes55); foundation52
and all original case inventories remain unchanged. This is registration, not
platform acceptance.
