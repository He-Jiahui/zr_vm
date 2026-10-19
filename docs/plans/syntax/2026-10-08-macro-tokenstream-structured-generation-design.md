---
related_code:
  - zr_vm_parser/include/zr_vm_parser/lexer.h
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/include/zr_vm_parser/compile_tool.h
  - zr_vm_parser/include/zr_vm_parser/comptime_contract.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time/compile_time_executor.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_declaration_binding.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_context_lookup.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_variable_lookup.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_bindings.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_callable_lookup.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_callable_internal.h
  - zr_vm_parser/src/zr_vm_parser/types/type_system.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_call_binding.c
  - tests/language_server/test_semantic_declaration_budget.c
  - tests/language_server/test_semantic_environment_budget_cases.h
  - tests/cmake/zr_vm_lsp_declaration_budget_tests.cmake
  - tests/cmake/lsp-component-targets.cmake
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_context_lookup.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_variable_lookup.c
  - zr_vm_parser/src/zr_vm_parser/syntax/lexer.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time/compile_time_executor.c
plan_sources:
  - "user: 2026-10-08 full ZR macros, structured syntax, gpt-6.1-sol max core and gpt-6-luna max support, commit each accepted milestone"
  - docs/plans/syntax/2026-07-20-11-compile-time-attribute-decorator-typed-generation-design.md
tests:
  - tests/language_server/test_semantic_analyzer_declaration_context_cases.h
  - tests/language_server/test_semantic_declaration_budget.c
  - tests/language_server/test_semantic_environment_budget_cases.h
  - tests/compileTime/test_compile_time_execution.c
  - tests/compileTime/test_comptime_contract.c
  - tests/compileTime/test_declaration_transform_contract.c
doc_type: milestone-detail
---

# Macro Token Streams and Structured Code Generation

## Accepted Objective

Build the complete macro mechanism on the existing comptime execution, CompileTool
roles, attributes, and generation transactions. The required families are
macro_rules, function-like procedural macros, attribute macros, derive, quote,
quote_spanned, format, and module-scope comptime emission. The public boundary is
TokenStream; macro authors can also inspect and construct immutable syntax trees.
This document implements the user-approved design and does not reduce its scope.

The authoritative user objective was read from the Codex attachment on
2026-10-08. The reference Rust checkout is `lua/rust`. Work proceeds on the
current main checkout, with no worktrees or feature branches. Core implementation
and decisions use gpt-6.1-sol with max reasoning; supporting work uses gpt-6-luna
with max reasoning. About ten workers run concurrently when independent work is
available, under the user's overall thirty-worker cap. Root owns integration,
builds, reviews, and milestone commits. Workers must not independently commit or
stage files.

## Required Language Surface

Compile-only modules are `zr.compile.token` and `zr.compile.syntax`. TokenStream,
TokenTree, Group, Ident, Punct, Literal, Span, and SyntaxContext preserve exact
spelling, punctuation spacing, source provenance, and hygiene. Token streams
support construction, iteration, concatenation, and lexing strings. Printed text
is for display; compilation consumes tokens directly, never display/relex output.

`macro_rules! name { matcher => expansion; }` defines a declaration macro.
Calls support parenthesis, bracket, and brace delimiters and ZR-qualified paths.
The matcher uses ZR fragments: tt, ident, literal, expr, stmt, item, ty, pat, path,
block, meta, and vis. It supports multiple ordered arms, separators, nested
repetition with star/plus/optional operators, opaque parsed fragments, follow-set
validation, and ambiguity diagnostics. Rust-only edition and lifetime fragments
are rejected explicitly. Declaration macros have textual and imported scopes;
generated definitions participate in subsequent deterministic expansion.

Procedural definitions use ordinary comptime functions with canonical roles:

| Role | Definition | Invocation |
| --- | --- | --- |
| `zr.compile.procMacro` | `comptime fn(input: TokenStream): TokenStream` | `name!(...)` |
| `zr.compile.procMacroAttribute` | `comptime fn(args: TokenStream, item: TokenStream): TokenStream` | `#name(...)#` |
| `zr.compile.procMacroDerive("Name")` | `comptime fn(item: TokenStream): TokenStream` | `#derive(Name)#` |

Derive receives and returns a complete target type. New fields, methods,
properties, and meta operations are appended to their owning AST collections.
Validation rejects removal, renaming, signature changes, mutation, or reordering
of existing members. Struct/class gain only members their grammar already allows;
enum/union remain limited to their existing grammar. There are no external impl
blocks. Attribute macros can replace an entire declaration. Attributes apply top
to bottom; derives apply in listed order to the previous merged result. Helper
attributes are scoped to their declared derive, target, and members.

SyntaxNode exposes kind, typed fields, children, and origins through immutable
views. Category-specific parsing, replacement, appendMembers, toTokens, and
formatting preserve syntax contexts. The public example is:

```zr
let target = syntax.parseType(input);
let additions = syntax.parseMembers(quote! {
    pub fn answer(): int { return 42; }
});
return target.appendMembers(additions).toTokens();
```

Formatting retains comments, uses four-space indentation and LF endings, keeps
existing member and initialization order, and does not sort by member name.

quote supports interpolation, nested repetition with separators, and a canonical
ToTokens conversion for tokens, syntax nodes, and values. Interpolated origins
survive. Generated template tokens default to call-site hygiene; quote_spanned
allows explicit spans. Doubled hash emits a literal hash in quote templates so
ZR attributes can be generated without interpolation ambiguity.

Module comptime blocks may call `zr.compile.emit(TokenStream)`. Emitted module
declarations enter the same syntax parsing, validation, and expansion pipeline.

format parses its template during compilation. It supports positional and named
arguments, variable capture, escaped braces, Unicode, alignment, width,
precision, numeric forms, and dynamic width/precision. Arguments evaluate once
in source order. Pure comptime arguments can be evaluated immediately; runtime
arguments use the same formatting engine. Default display reuses existing string
conversion. Custom formatting uses the canonical contract
`zr.format.IFormattable.format(spec: FormatSpec): string`.

## Compilation and Ownership

The order is BuildFacts -> SyntaxExpansion -> Signature -> semantic Expansion ->
Layout -> LateCheck. New enum IDs are appended; phase order is explicit rather
than inferred from numeric enum values. Early macros can inspect syntax and
declared build inputs but cannot query later type layout or semantic views.

Existing lexer and parser operations share a generic token input. Token streams
must retain enough data for direct lookahead, punctuation assembly, locations,
and opaque syntax fragments. Hygiene contexts propagate through tokens, AST
identifiers, binding, cloning, serialization, caches, and diagnostics. Declaration
macros use mixed-site hygiene; interpolation retains its original context.
Definition references use locked package/module identity and normal visibility.

Source and binary macros use a single controlled comptime execution contract.
Complete the needed loops, mutation, member traversal, and return/break/continue
semantics in focused modules. Register new values and operations through
canonical capabilities and roles instead of source-name checks in shared paths.

Each expansion prepares a detached tree, validates it in the required category,
and then publishes it. Later signature/layout/code generation consume the merged
tree. Failure releases temporary nodes and keeps the previous valid artifacts.
Existing typed declaration generation shares the generation ownership boundary.

Retain existing default limits: one million fuel, call depth 128, 64 MiB heap,
100,000 aggregate values, 10,000 generated declarations, and 1,024 diagnostics.
Token allocation, tree operations, matching, and expansion are budgeted. Exact
recursive expansion receives a located diagnostic; decreasing recursive match
input must remain valid. Cached results serialize stable data, not native pointers.

BuildInputs declare files and environment names in the project manifest. Capture
them once at build start and expose them through `zr.compile.input`. Snapshot
digests participate in cache identity. Missing declared files and undeclared
inputs are errors; absent environment values differ from empty strings.

## Milestones and Commit Gates

- [ ] M1: TokenSource, TokenTree, Span, SyntaxContext, resource accounting, and
  comptime control flow. Verify lexical equivalence, source lifetimes, lookahead,
  traversal, nesting, budgets, scopes, and cleanup after failure.
- [ ] M2: Public immutable syntax views, parsing, replacement, appendMembers,
  token round trips, member validation, and comment-preserving formatting.
- [ ] M3: Macro registration/scopes, matcher/transcriber, expansion scheduler,
  hygiene integration, and all required invocation categories.
- [ ] M4: Procedural roles/signatures, raw attribute inputs, derive/helpers,
  quote/quote_spanned/ToTokens, emit, and internal member generation examples.
- [ ] M5: format/format protocol and declared build input snapshot/cache support.
- [ ] M6: Macro manifests and executable compile-time representation in artifacts,
  source/binary dependency imports, expanded-source CLI output, LSP diagnostics,
  navigation, generated symbol queries, and documentation/acceptance.

No milestone is complete from a C-only facade, one smoke example, unexecuted
tests, or passing tests that omit its required behaviors. Commit each complete
milestone only after spec review, quality review, and fresh validation. Stage
only owned task files; do not commit the initial unrelated dirty worktree.

## Verification Requirements

Each implementation slice starts with a failing behavioral test. Root controls
build-directory ownership and CMake registrations. The required final matrix is
WSL GCC and Clang, followed by Windows MSVC. Record commands, toolchains, test
counts, failures, and baseline differences in acceptance documents.

Required negative and boundary coverage includes delimiter mismatches, compound
punctuation, escaped strings, empty input, comments, formatting idempotence,
complete parsing consumption, repetition nesting/count mismatches, ambiguity,
recursion, cross-module import, variable capture/shadowing, invalid derive
targets, collisions, preserved originals, generated type errors, rollback and
recompilation, missing/invalid format arguments, Unicode and dynamic sizes,
once-only evaluation, input/provider cache invalidation, source/binary parity,
GC, deep trees, and resource-limit boundaries.

Upstream evidence includes Rust proc_macro/rustc_expand source and UI tests,
Roslyn SyntaxFactory/normalizer and tests, CPython AST validation and tests,
and JDK annotation-processing generation rounds. Add exact source/test ledger
entries as each API is implemented. Update module docs and language contracts
alongside implementation; preserve explicit distinctions from Rust semantics.

## Current State

Current order exception (2026-10-16): the user explicitly selected independent
Windows validation first after WSL startup failures, leaving GCC/Clang unpassed.
Windows r8 foundation52 passed, but the full build failed on two C1083 generated
outputs and source-post also detected foreign drift. Lower323/original127/
full55/budget11/LSP84/CLI were not run. The bounded build-path and libffi graph
repairs are validation prerequisites, not new macro milestones or platform
acceptance. The current selected full inventory is55, including standalone
declaration-budget11; original84 and every strict input gate remain unchanged.
M1 is still unaccepted/uncommitted and M2-M6 remain pending. Dated states below
do not supersede this order exception or provide missing current test results.

Retained Windows r9: foundation52 passes and normal full-build33132 naturally
exits0 with956 planned actions and no `FAILED:` rows. Typed-call passes, but
yield exits `0xC0000005`; source-post records12 changed inputs, so raw
receipt9D06E96E remains FAIL and HEAD/support post are not reached. Remaining
lower targets, original127/full55/budget11/LSP84/CLI were not executed in R9.

The independently reviewed typed-cache receiptD4F8443A verifies only119 C +1
MASM selected objects with actual fresh successful r9 compile edges:118 positive
`VALID` C closures, one exact84B headerless tinydir `VALID 0`, and the separate
nine-input MASM closure. All627 dependency pins and selected pre/post identities
are stable, and five inspection children naturally exit0. Normal standalone
graph receiptD563475E passes exactly2 CTest entries:6828 paths/max258 and nine
FFI inputs, with20 recorded inputs unchanged. Neither graph/cache PASS replaces
the selected full55, establishes full-tree cache health, or accepts a platform.

Debug r3 receipt4ED01C2D/rawCE696047 reproduces the same access violation:
debugger10912 and symbolizer22456 naturally exit0, inferior14856 naturally
exits `0xC0000005`, and actual module/PDB mapping identifies
`ZrCore_GlobalState_Free` at `global.c:734`. The requested shared-library stop
did not occur; planned early breakpoints and lifecycle observations are absent.
Separate read-only PDB/machine-code inspection confirms mixed layout pairs
(`State.global`184/208, `RawObject`184/208, string payload200/224) and actual
`DestroyState` reading+`0xB8` (184). Its fixture object has Ninja
`#deps 0 (VALID)` outside the audited120. The later linked-COMDAT evidence below
establishes the mixed-ABI cause without inventing GC lifecycle observations.
Full receipt identities and evidence limits are in
the [validation ledger](../../testing-and-validation/macro-foundation-validation.md).
Linux remains unpassed, M1 remains unaccepted/uncommitted and M2-M6 pending.

Current cache follow-up: actual machine code has `State_Init` write global+208
but stale common `DestroyState` read+184. The actual linked string getter reads
length192/payload200 while the current creator writes216/224; PDB attributes
that COMDAT to the old common fixture, while the parser's own getter is new.
This establishes linked stale-object mixed ABI, not a source-level GC repair.
Seven-object r1 rebuilds successfully but still has seven `VALID 0` closures,
does not run native tests and remains FAIL (receipt0D4D43F9).

The two local support repairs normalize only line-start native showIncludes
prefixes to ASCII and defer the CMake prefix hook, preserving compiler flags,
arguments, timeouts, tests and source. HelpersF76CEF63/4668B938 have narrow
independent core acceptance plus five passing actual RAM AST controls. Normal
r2 restores seven positive closures and its saved existing-C graph has2474
positive `VALID` records plus the unique exact84B tinydir placeholder zero;
4350 absent C objects gain no test credit. Native7/3/4/127 all pass in original
source registration order, with zero failures/ignored and all12 children
naturally terminal0. Receipt558DFD25 remains raw FAIL for source4788-to4789/
six foreign ExecIR changes and HEAD e7c7cde9-to0f889998. The141 dated passes do
not accept the platform or M1; old failures are not rewritten.

R10-v2 naturally ended (unified68201/driver22008): foundation52/lower323/
original127/budget11/original LSP84 pass, but full CTest37444 exits8 with52 PASS/
3 FAIL of55. AOT source26/5, LSP ownership25/13 and CLI12/1 fail. One concurrent
CMake source change also rejects the strict input gate; HEAD-post and auxiliary
3+4/source-final were not reached. Receipt24E06251 and outer6AD77864 remain FAIL.
The focused AOT test-contract repair then passes the original26 with stable
source/HEAD/tool/binary pins and no production AOT changes. LSP/CLI diagnosis
continues, without weakening the original52/55/323/127/11/84/CLI/final gates.
Parallel foreign edits remain permitted; Linux remains unpassed, M1 remains
unaccepted/uncommitted and the full M2-M6 scope remains pending.

GCC r4 has now naturally completed: foundation build and serial52 CTest pass,
but the full build fails on two LSP callers missing the new syntax-context
argument. Its immutable raw receipt743A3F11 also records four final foreign
input changes and remains FAIL. Focused regression tests cover the LSP's
nonroot declaration identity and the new lambda range witness's signed bounds.
Lambda GREEN r3 now completes a normal 951-step build and the original ordered
ten native cases all PASS, with both own source pins and binary unchanged.
Its receipt 580B32C2 remains raw FAIL because source-post records 62 global
changes; head_post is absent. This is dated functional GREEN only, not a
source-stable GCC or M1 pass.

LSP RED r1 is terminal fixture compilation FAIL with no native run
(receipt89F08034). Five API-use corrections replace four nonexistent int kind
constants with `ZR_VALUE_TYPE_INT64` and construct the invalid context through
its public table/invalid-mark fields; the companion header is937 lines. RED r2
then builds exit0 and runs84 ordered unique cases:72 PASS and12 FAIL. All eleven
unchanged registered failures match exact reasons without fixture errors, but
the existing `Unknown Parameter Preserves Identity` also fails. All4772 source
inputs and recorded pre/post HEAD match; receipt99902011 remains raw FAIL and
the binary-post gate is not reached.

GDB r1 observes case0 PASS/type0. With an explicit `:int` return in case1, the
parameter declaration/read retain symbol3/type0 while the callable is
symbol2/type3; hover remains NULL after the callable-type assertion
short-circuits. Diagnostic receipt3FD9CDD9 is raw FAIL due three foreign ExecIR
input changes and grants no acceptance. The `symbols.c` unresolved-parameter
branch omitted `hasUnresolvedParameter`; only that one-line prerequisite
correction is pinned as a fifth phase input. The four main hygiene production
files stay RED for r3. Session12432 is terminal with build509/exit0 after7
actions and native644/exit1:84 ordered unique cases,73 PASS and the original
exact eleven FAIL names/order/reasons. `Unknown Parameter Preserves Identity`
passes; binary E4AEAB40/53963096 bytes is unchanged after execution.

Receipt19000590 remains raw FAIL because source-pre4772 becomes source-post4774
with four foreign SSA/ExecIR changes. The three tests and five RED production
inputs match across r3 and Root's pre-lease review, but the source gate fails
before HEAD-post fields are captured. This is dated functional RED, not accepted
fixed-input RED. Root authorizes the bounded four-file repair under the user's
parallel-edit/revalidation decision. Source review now freezes the implementation
with a budget-boundary finding, not GREEN. The shared lookup returns `NULL` for
both missing owners and budgeted normalization failure, so callers may publish
a replacement or publish before a later lookup fails. Original seven-case r1
now supplies dated native counterexamples, not a fixed-input acceptance result
or a claim that the budget defect is repaired.

The original 628-line r1 budget regression freezes seven ordered cases
(SHA 4859A78C), with dedicated CMake fragment 898AE2CA and single include AADB6534.
It covers constrained and wide marked-callable identity, Root no-scratch reuse,
fresh interface/delegate publication, differing raw but ITEM-equivalent context
failure, a preceding Root decoy and variable publication ordering. The original 84
analyzer cases and foundation 52 remain unchanged. Normal WSL GCC budget RED r1
is terminal/drained: build534 completes 18/18 actions, exit 0; native767 exits 5
with seven ordered cases, 2 PASS/5 exact FAIL/0 IGNORE and no fixture prerequisite
error. Receipt3888EF4C stays raw FAIL after five foreign source changes and
actual HEAD d3936df4 to 06364480. Independent review confirms seventeen owned
pre/post pins and binary4DD65559 stable; only dated functional TDD RED is credited.
Helper CE277678's isolated 8/8 and 10/10 GREEN results (receipt5E59D482) remain
validator-only evidence, not real interruption or compiler/native credit. Full
identities are in the ledger; budget production repair and original 84-case GREEN
remain pending, without relaxing full-source or HEAD gates.

Adjacent binding/callable comparisons have a separate static P2: normalization
failure becomes a miss, permitting duplicate public no-snapshot registrations
or a partial TRUE overload lookup. It is a reachable M1 context-budget
prerequisite risk, not a native result or a regression assigned to the future
exact-query patch; it cannot be silently exempted by repairing exact-query alone.
Four additional public no-snapshot cases are now authored and stopped after
Root and independent source review: main71CDCD5E/header216F2D59. Complete RAM
inverses restore original r1 SHA4859A78C; original seven assertions, original84
and CMake were unchanged at RED r2. Actual unified75735 is terminal/drained:
build517 completes6/6/exit0; native566 exits9 with11 ordered unique cases,
2 PASS/nine exact FAIL/0 IGNORE and no prerequisite failure. Receipt08934E6E
stays raw FAIL solely for HEAD06364480-to-badc069b despite identical complete
4777 source inventories09A58B41,18 stable owned pins and binary DE7F0768.
This is dated behavioral TDD, not fixed-input acceptance. The new four directly
exercise registration/multi-lookup, not RuntimeRoot/Capture/typed-call/alias.

The ten-path bounded candidate is now implemented and its author STOPPED after
Root/independent static review with no new P1/P2, not GREEN. Exact BOOL/out lookup
is raw-first/two-pass/query-once; checked context comparisons distinguish error
from inequality and reset failed overload results. Four existing variable
wrappers move into actual `type_environment_variable_lookup.c`; the actual
private `type_environment_context_lookup.h` supplies checked comparison and
current/full-chain finders. Current-scope registration/shadow and full-chain
RuntimeRoot/Capture/public queries remain distinct. See
[LSP canonical bindings](../../parser-and-semantics/lsp-typecheck-canonical-binding.md).
Original84 utility BA21308E changes only checked API plumbing: both reviewers'
nine-region RAM inverses restore8C04A6E7/50775B and preserve all assertions,
reasons and84 calls. No public ABI, all-pointer error/miss distinction or global
allocator-OOM guarantee is added; normal new-TU discovery is still unverified.

GREEN launcher7197 and minimal WSL true probe8920 naturally exit1/drained with
CreateInstance/CreateVm/0x800705b4 before any helper/compiler/native execution.
All5 GREEN outputs remain absent. Cause unknown; no machine/service restart or
configuration change. Current GCC/Clang/final MSVC and M1 remain unaccepted.
Dated nine-doc r10 PASS9/669/34 (receiptC4E44918) covers old pins only; this
revision requires fresh r11 validation, without compiler/M1 credit.

Both matrix helpers now have the minimal HEAD-false rejection. The unchanged
isolated eight-case regression moves from4 PASS/4 FAIL to8 PASS
(receipt3A6A5EC3, PID7884/exit0), with four inputs stable. It verifies actual
final-verification control flow only, not compiler/native matrix behavior.
Initial r5/r6 registrations adopted reviewed WSL E7A887BE/MSVC ABA4614F hashes.
The standalone budget target now expands full CTest54 to55 with explicit build,
discovery and binary pins; current WSL AEC3755E/MSVC254D2275 retain the gates and
superseded identities. The unchanged isolated eight-case checker passes again
(PID19004/exit0, receiptB4705C35); no platform credit. The budget follow-up, original 84-case
GREEN and complete GCC/Clang/final MSVC gates remain pending; M1 is not accepted or
committed. Full immutable evidence is in the
[M1 validation ledger](../../testing-and-validation/macro-foundation-validation.md).

As of 2026-10-16, M1 remains unaccepted and uncommitted; M2-M6 have not started.
The latest token Concat fix reports initial context-allocation exhaustion through
the common resource diagnostic without modifying either input. Its recorded
Windows regression run passes 35 cases across seven token/resource targets.
The earlier lower14, original127, and original51 Windows results retain their
recorded source identities; they do not substitute for the final current matrix.

WSL is now available. The actual GCC Debug/static foundation configuration
completes exit 0 against the current checkout in the registered E-root build,
including compiler identification, include probes and Ninja generation. The
GCC r3 validation was stopped deliberately at 628/1271 to remove an unused
root-context CFG wrapper introduced by the hygiene migration. Its terminal
receipt records no test execution and eight foreign source-input changes.
The wrapper is removed; both callers still use the context-aware helper.
The unchanged driver validates the revised checkout as GCC r4, reusing the
same build and retaining strict source hashes. Its dated foundation52 results
are recorded above; the failed full build prevents the complete Linux gate.
After the remaining LSP focused repair/GREEN, validation must still cover
foundation-only52 and the relevant full graph, then Clang and final MSVC.
The full graph includes lower14/native127, language_pipeline, provider/artifact
contracts, CLI incremental behavior, language_server, and hello_world. Detailed
commands, identities, and retained failures belong to the
[M1 validation ledger](../../testing-and-validation/macro-foundation-validation.md).
Other sessions' source changes and staged entries remain preserved. A milestone
commit must contain only this task's accepted changes.

### Prior Array Receiver Validation Checkpoint

The finite Array.add receiver-setup candidate 4C5B070D has Root/core SOURCE PASS
with complete RAM inverse/forward byte equality. It adds 1085 bytes/21 LF to an
existing helper, accepting adjacent LOAD+STAGE with an optional reload while
retaining liveness, named-slot alias, native and burst conditions. The controlled
prepatch 22-case RED has one FILL failure; the live native-null-result case
reports PASS with FINE/integer 14.

Lower-r7 completes fourteen targets and 322 cases: 321 PASS/one failure/zero
ignored, build 0/wrapper 1, with 100 source pins and HEAD stable. All thirteen
non-W2 targets pass 300 cases, including escape metadata 14/0. W2 remains 22/1:
four ADD results are discarded but FILL is absent. Its operand1[2]/[3] diagnostic
reads exceed the actual two-element array; gap-opcode evidence and a fresh
22-case replay are pending. No later null-write/FILL repair or final W2 GREEN
is credited.

Original127-r2 and original51-r5 have not started. The complete pre-475D
original127 120/7 and prior lower replays remain history. Original51's 50/1 and
34/17-timeout records and WSL E_ACCESSDENIED leave acceptance open. M1 remains
unaccepted, M2-M6 pending, with no Root milestone commit. Full receipts are in the
[M1 validation ledger](../../testing-and-validation/macro-foundation-validation.md).

### Retained Historical Checkpoint

M1 foundation code and the direct-token parser path are present. Focused MSVC
validation covers lexical owner replay, finally completion graphs, and the
current canonical pre-semantic IR contracts. The latest validation executes all
51 foundation entries with 34 passes and 17 timeouts; the earlier current-source
run reached 50 passes and one timeout. Both complete failed runs and exact
source hashes remain recorded. The original 126-program
aggregate fails at compiler integration. The complete remaining121 inventory
has111 passes and10 failures; comparison identifies eight new source-contract
failures alongside two resolved exception failures. Those eight new AOT test
contract failures were subsequently repaired without changing runtime code.
Seven lower suites pass311 cases. The primitive return repair subsequently
passes61 focused cases and the binary typed suite passes17; the complete
original127 replay has12 failures, with only the decorator project changing to
PASS. The imported META owner repair then passes54 focused cases and another
ten-case module replay after warning cleanup. Its original integration replay
stops at a native-callable assertion in the first LSP case. Among89 completed
standard cases, only the network project changes to PASS; no full127 result is
claimed. The ordinary VM member predicate repair subsequently passes 34 focused
cases, including the real imported method result 42. The generator copy-pointer
repair makes the new public closure-graph case pass; the original three yield
semantic failures remain. Its module and pipeline companions pass 29 cases.
Cyclic source tracing proves successful summary registration and full function
compilation, but its typed cache remains missing while runtime and consumer
calls return 17. The native callback classification repair passes all 13 escape
cases plus 26 pipeline/artifact companions. Receiver placement now passes the
inline layout and alias gates; execution still fails with `layout-mismatch`.
The runtime repair and zero-argument metadata contract remain under review.
The top-level `const fn` rejection test is being aligned with the parser's
null-AST fatal recovery contract. LLDB attachment
is denied; no stack evidence or timeout-cause claim is made. Historical
23/28 and20/31 foundation results also remain retained. Platform acceptance
remains open. See the
[M1 validation ledger](../../testing-and-validation/macro-foundation-validation.md)
for actual test results and limits.

M1 is active and unaccepted. The public macro language surface remains pending
the later milestones. Existing unrelated AOT ownership, runtime registration,
source migration, and other session work are preserved. No milestone has yet
passed its acceptance gate or been committed by Root.

## Array Null-State Implementation Checkpoint (2026-10-15)

The Array<int>.add follow-up adds a private compiler CFG proof with the state
order `UNREACHED < NULL < UNKNOWN`. It treats the candidate plain-NULL clear as
a no-op during analysis, requires checked isolated value-frame entry, and
rejects unknown opcodes, invalid operand/frame geometry, overflow, and
allocation failure. The new late pass considers only a clear immediately
before a valid backward `JUMP`; active local and typed-local bindings,
block-start clears, live temporaries, or a failed independent NULL proof keep
the clear. NOP compaction runs before the existing strict late FILL matcher,
whose implementation and tests were unchanged.

The R4 source proof is byte-exact and pins 119 files, including the newly
recorded `object_call.c` dependency; the historical R3 set had 118 and did not
pin it. The separate MSVC R4 quickening run passes 23/23 cases. Lower-contract
R8 passes 14 targets and 323/323 cases with stable registrations and binaries.
Fresh original127 R2 passes 127/127 with ordered registrations, no custom
failure rows, 127 stable source pins, a stable binary, and unchanged HEAD.
Original51 R5 passes all 51 tests in 24.79 seconds with CTest exit 0 under the
original `-j1` invocation and no timeout override. The older 50/1 and
34/17-timeout runs remain historical; no timeout recurred and no cause is
inferred. R5 has 119 consistent source pins and HEAD
`63dd36d0c72db5aaf9bee15a2d81107f823a5cc6` before and after the run. A later
documentation commit from another session moved the checkout to HEAD
`7a3e20119a5afe51f4625182833caa4ab8511b1b`; Root reviewed its full diff and
found only comments and whitespace added in `math/module.h` and
`math/module.c`, with no declaration or code changes. R5 remains evidence for
its recorded source pins, not for this later HEAD. These are Windows results.
The retained WSL recheck reports
`Wsl/Service/CreateInstance/E_ACCESSDENIED`, with neither compiler running.
The bounded MSVC control-flow run at HEAD
`7a3e20119a5afe51f4625182833caa4ab8511b1b` passes 9/0/0 and edges passes
11/0/0. Ordered registrations match, binaries are stable, and all 163 source
pins are unchanged. The receipt `m1-msvc-comptime-control-negative-r1-receipt.json`
has SHA-256
`5A0640C1D9524D6AAAC129D20588695B627E493C9C92C92A3887809A27F1774E`. Earlier
R4/R8/original127/original51 results remain at HEAD `63dd36d...`, without
retroactive credit from this run. WSL GCC/Clang is the only remaining
unaccepted M1 gate; M2-M6 remain pending.

See the [M1 validation ledger](../../testing-and-validation/macro-foundation-validation.md)
for receipt hashes, dated R3/R7 and original51 history, and exact R4/R8 and
original127/original51 evidence.
