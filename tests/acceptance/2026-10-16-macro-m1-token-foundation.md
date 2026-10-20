---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time/compile_time_frame.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time/compile_time_aggregate.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time/compile_time_aggregate.h
  - tests/compileTime/test_comptime_frame_budget_cases.h
  - tests/compileTime/test_comptime_frame_transaction_cases.h
  - tests/parser/test_type_inference.c
  - tests/parser/test_type_inference_extern_visibility_cases.h
  - zr_vm_core/src/zr_vm_core/type/type_layout_initialization.c
  - zr_vm_parser/src/zr_vm_parser/compiler/metadata/type_storage_layout.c
  - zr_vm_parser/src/zr_vm_parser/compiler/expression/compile_expression_types.c
  - tests/parser/test_reference_receiver_layout_contract_support.h
  - tests/parser/test_compiler_storage_alignment_contract_cases.h
  - zr_vm_parser/include/zr_vm_parser/token_stream.h
  - zr_vm_parser/include/zr_vm_parser/token_span.h
  - zr_vm_parser/include/zr_vm_parser/syntax_context.h
  - zr_vm_parser/include/zr_vm_parser/comptime_contract.h
  - tests/cmake/macro-foundation-tests.cmake
  - tests/cmake/macro-foundation-regressions.cmake
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
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time/compile_time_frame.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time/compile_time_aggregate.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time/compile_time_aggregate.h
  - zr_vm_core/src/zr_vm_core/type/type_layout_initialization.c
  - zr_vm_parser/src/zr_vm_parser/compiler/metadata/type_storage_layout.c
  - zr_vm_parser/src/zr_vm_parser/compiler/expression/compile_expression_types.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_context_lookup.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_variable_lookup.c
  - zr_vm_parser/src/zr_vm_parser/syntax/token_source.c
  - zr_vm_parser/src/zr_vm_parser/syntax/token_stream_clone.c
  - zr_vm_parser/src/zr_vm_parser/syntax/syntax_context_identity.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_resources.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time/compile_time_callable.c
  - zr_vm_parser/src/zr_vm_parser/compiler/semantic/compiler_semantic_cfg_try.c
plan_sources:
  - .codex/plans/20261008-macro-tokenstream-structured-generation.md
  - docs/plans/syntax/2026-10-08-macro-tokenstream-structured-generation-design.md
tests:
  - tests/compileTime/test_comptime_frame_budget_cases.h
  - tests/compileTime/test_comptime_frame_transaction_cases.h
  - tests/parser/test_type_inference.c
  - tests/parser/test_type_inference_extern_visibility_cases.h
  - tests/parser/test_reference_receiver_call_boundary.c
  - tests/parser/test_reference_receiver_layout_contract_support.h
  - tests/parser/test_compiler_features.c
  - tests/parser/test_compiler_storage_alignment_contract_cases.h
  - tests/parser/test_union.c
  - tests/core/test_type_layout_inline_copy.c
  - tests/language_server/test_semantic_analyzer_declaration_context_cases.h
  - tests/language_server/test_semantic_declaration_budget.c
  - tests/language_server/test_semantic_environment_budget_cases.h
  - tests/parser/test_token_stream.c
  - tests/parser/test_token_source.c
  - tests/parser/test_token_source_edges.c
  - tests/parser/test_token_stream_limits.c
  - tests/parser/test_token_stream_boundaries.c
  - tests/parser/test_token_span.c
  - tests/parser/test_token_hygiene_lifetime.c
  - tests/parser/test_context_budget.c
  - tests/parser/test_context_identity_queries.c
  - tests/parser/test_parser_collection_budget.c
  - tests/compileTime/test_comptime_control_flow.c
  - tests/compileTime/test_comptime_control_flow_edges.c
  - tests/compileTime/test_comptime_frame_lifetime.c
  - tests/compileTime/test_comptime_aggregate_budget.c
  - tests/compileTime/test_comptime_definition_environment.c
  - tests/compileTime/test_comptime_import_alias_environment.c
doc_type: acceptance-record
---

# Macro M1 Token Foundation

## Scope

M1 covers direct TokenSource parser input, TokenTree construction and traversal,
Span and SyntaxContext identity, resource accounting, failure cleanup and the
comptime control flow needed by later macro execution. Generated token input
must never pass through display text and re-lexing as its compiler interface.

This record does not accept public syntax editing, macro matching, procedural
macros, derive, quote, format or macro artifact/tooling work. Those remain M2-M6.
The [module contract](../../docs/parser-and-semantics/macro-token-foundation.md)
and [accepted design](../../docs/plans/syntax/2026-10-08-macro-tokenstream-structured-generation-design.md)
describe the implementation and reference-language evidence.

## Current Gate

At the 2026-10-18 frame checkpoint, M1 remains OPEN and uncommitted. Actual
closed RED r3 observes seven original PASS plus four exact FAIL. Repair GREEN
r2 returns native0 and eleven PASS, but concurrent Canonical test edits fail
the fixed-source closure; receipt
`58C209CE353E7F1765DE78860EE74CC20B94D6E3E67596D3A2D6CBD7201A24FB`
is retained as FAIL, not adopted. The implementation now admits complete
candidate capacity before allocation and preserves live rows, roots, usage and
mutability on rejection. Actual GREEN r3 now passes all 21 ordered cases,
including ten OOM/GC/exact-charge/interleave controls, with no failures/ignored
and six natural0 children. Receipt
`B6535EB4B3476D4867AF0D43C32DFE2CC518830E5C4E232E5B4904130E6B53B2`
keeps source4917/ED17EA0D, HEAD7c006421 and all strict closures fixed. The
revised fourteen-case aggregate suite and three FFI visibility controls still
require current native validation. Fresh 26-document and complete
GCC/Clang/MSVC gates remain required. The older
storage results below cover their recorded inputs, not these new revisions.

At the2026-10-18 storage checkpoint, M1 remains OPEN and uncommitted. The current
focused GCC receiver repair passes all31 original cases with no ignored cases,
native exit0 and independently checked source/HEAD/support/environment/cache/
binary closure. Receipt `m1-wsl-gcc-receiver-storage-layout-green-r2-receipt.json`
has SHA-256 `89D4D643675509C73B9F00A007D6119D5F0F91DD5D400820BC1070A31E392F4D`.
The real RED31/4 and alignment-only31/3 FAIL histories remain in the
[validation ledger](../../docs/testing-and-validation/macro-foundation-validation.md#receiver-storage-and-dispatch-gates).

Seven adjacent storage suites193 cases and the original compiler127 companion
now pass all320 original cases in GCC R6, with0 FAIL/0 IGNORE, eight native
exits0 and all18 direct children naturally terminal. Receipt SHA-256 is
`511E8B48C1186187E73AAE413D7F52032EFB9D2CE49499331F4B711213F6F168`;
strict4890/E36006EF source, HEAD33c3f43e, support/environment/cache/ELF closure
PASS. The prior R5 closed RED isolates signed-i64 precision; R6 adds only the
reviewed tagged64 argument producer repair. Real closed-generic/implicit-extern
layout metadata is compared with platform C mirrors after independent VM
cleanup. This is focused validation, not the full milestone.

The reviewed full inventory is282 build targets and68 selected CTest entries;
all current MSVC/GCC/Clang matrices remain required. The prior20-document check
passed, but revised22 documents need fresh validation. M2-M6 remain pending and
no milestone commit is authorized by this focused checkpoint.

### Historical Windows r8

The user selected independent Windows validation first; Linux GCC and Clang
remain unpassed. Windows r8 foundation CTest passed52/52 with unchanged limits,
but full-build36132 naturally failed on two generated-file C1083 errors.
Receipt `m1-msvc-current-r8-receipt.json`, SHA-256
`FA26E2CCF17C02EDCAEBCFBD307D538C6C288FB77BEC3F4CDB8ABE1BE95D1737`,
also records foreign source-post drift; HEAD/support post were not reached.
Lower323/original127/full55/budget11/LSP84/CLI were not executed in that run.
Strict input gates are not waived while parallel editing continues.

The actual old-graph six-path RED and bounded internal CMake target repair are
recorded in the [Windows build-path contract](../../docs/testing-and-validation/windows-test-build-paths.md).
The independent libffi graph CTest passed1/1, not the full platform matrix.
This is retained R8 history; the latest R9 result follows. No later Linux gate
is credited, and results below remain dated evidence rather than current-input
acceptance.

### Retained Windows r9

Foundation CTest passes52/52. Full-build33132 naturally exits0 with956 planned
actions and zero `FAILED:` rows. The typed-call target passes; yield then exits
`0xC0000005`. Source-post records12 changed inputs and HEAD/support post are not
reached. R9 receipt9D06E96E remains raw FAIL. Remaining lower targets,
original127/full55/budget11/LSP84/CLI were not executed in R9.

The independent selected-cache audit passes119 C +1 MASM fresh successful r9
compile edges:118 positive `VALID` header closures, the exact84B tinydir
placeholder `VALID 0`, and the separate nine-input MASM closure. All627
dependency pins and selected pre/post identities are stable; five inspection
children naturally exit0. Receipt `m1-msvc-cache-post-r9.json` has SHA-256
`D4F8443A9E0C672F59D1C81F9D8BD41E0A11568F7F5B34921FADAAC8A75398E5`.
Normal standalone graph CTest passes2/2:6828 paths/max258 and all nine FFI
inputs, with20 input pins unchanged. Receipt
`m1-msvc-build-graph-ctest-r2-receipt.json` has SHA-256
`D563475EF1AD7F794D8D9CE41620E467E48B30B616561DDB59ECA5B1383AAB79`.
Neither result is full55, full-tree cache, platform or M1 acceptance.

Debug r3 receipt4ED01C2D/rawCE696047 reproduces the same fault: debugger10912
and symbolizer22456 naturally exit0; inferior14856 naturally exits
`0xC0000005`, mapped to `ZrCore_GlobalState_Free` at `global.c:734`.
The requested shared-library stop did not occur, so planned early breakpoints
and lifecycle observations were not collected. Separate PDB/machine-code
inspection confirms mixed layout pairs (`State.global`184/208,
`RawObject`184/208, string payload200/224); `DestroyState` reads+`0xB8` (184)
from a fixture object with Ninja `#deps 0 (VALID)` outside the audited120.
The later linked-COMDAT investigation below establishes the mixed-ABI cause;
these missing early stops do not establish GC lifecycle behavior. Full receipts
and limits are in the
[validation ledger](../../docs/testing-and-validation/macro-foundation-validation.md).
Linux remains unpassed; M1 is unaccepted and uncommitted, with M2-M6 pending.

### Current Cache Repair And r10

Actual PDB/machine code confirms incompatible linked accesses: current
`State_Init` global+208 versus stale `DestroyState`+184, and current string
length216/payload224 versus linked old getter192/200. PDB attributes the old
getter COMDAT to the zero-dependency common fixture outside the audited120.
This establishes the mixed-ABI stale-object cause, not a new GC logic repair.

Seven-object r1 compiles all seven successfully but retains seven `VALID 0`
closures, stops before native tests and remains FAIL (receipt0D4D43F9). Local
ASCII showIncludes normalization plus deferred CMake prefix hook then restores
all seven positive closures in r2. HelpersF76CEF63/4668B938 have narrow core
review acceptance and five passing RAM AST controls, not platform acceptance.
The saved r2 graph has2474 positive C closures and the unique exact84B headerless
tinydir zero; 4350 absent C objects receive no execution credit. Original native
7/3/4/127 rows match source registration order, all PASS, with one zero-failure/
zero-ignored footer each. All12 children naturally exit0 and helper pins remain
stable. Receipt `m1-msvc-zero-deps-r2-receipt.json`, SHA-256
`558DFD252894A57C84E4ED689BFDE7B72AEB7C75EA3D9BA1F4E2E31FB8C53674`,
nevertheless remains raw FAIL: source4788-to4789/six foreign ExecIR changes and
HEAD e7c7cde9-to0f889998. The141 passes are dated evidence only.

R10-v2 naturally ended: foundation52/lower323/original127/budget11/original
LSP84 pass; full CTest37444 exits8 with52 PASS/3 FAIL of55. Failed groups are
AOT source26/5, LSP ownership25/13 and CLI incremental12/1. Receipt24E06251 also
rejects one concurrent CMake source change; HEAD-post was not reached. Outer
6AD77864 confirms all owned children terminal and no fixed/extra leftovers.
Standalone CLI/auxiliary3+4/source-final were not executed. Original strict
gates remain required. The focused AOT test-contract repair passes26/26 with
stable source/HEAD/tool/binary pins, but does not accept the full matrix. LSP/CLI
diagnosis continues. Linux remains unpassed; M1 is unaccepted/uncommitted, with
all M2-M6 scope pending.

## Baseline

Earlier native MSVC results are dated evidence, not the final three-platform
gate. In particular, the Concat budget regression moved from a real failing
case to seven targets and 35 passing cases on its recorded stable inputs.

GCC r3 was deliberately stopped for the confirmed unused root-context CFG
wrapper. It executed no tests and remains FAIL, including its eight changed
source inputs. The wrapper is removed before the current r4 input inventory.
Existing expression-helper warnings remain visible and are separately proven
to match baseline code; no warning-free result is claimed.

## Test Inventory

| Layer | Required selection | Checks |
| --- | --- | --- |
| Foundation | Exact 52 CTest entries | Current foundation tests, five existing comptime regressions and failure-log probe |
| Shared support | Exact 14 native targets / 325 cases | Original ordered registration, all PASS, zero failures and zero ignored |
| Compiler integration | Original 127 native cases | Original ordered registration and actual behavioral rows |
| Full graph | Exact 68 CTest entries and 282 build targets | Language pipeline, CLI incremental, language server and standalone declaration-budget11 included |
| UNIX AOT integers | Separate 2-case / 19-scenario gate | Real compiler/linker/loader chain and generated fixture cleanup; not counted in full68 |
| CLI | Actual hello_world project | Bare interpreted project invocation from the registered build |

CTest runs serially with its original timeouts. Counts are not interchangeable:
the five dedicated comptime entries belong to the foundation-only configuration,
while full-graph aggregates use their actual generated commands and executable lists.
The current required inventory above is refreshed for 2026-10-18; earlier dated
run tables keep their actual discovery counts. Three supplemental numeric-literal
cases are checked separately against their unchanged ordered registration.

Boundary coverage includes empty and malformed token input, nested groups,
compound punctuation, literal spellings, token/source boundaries, span identity,
name shadowing and aliases, repeated context queries, deep trees and budgets.
Failure coverage includes allocation failure, peak and aggregate charging,
interleaved budgets, parser collections, ownership transfer, GC-sensitive frame
lifetimes and control-flow completion. Existing return/break/continue and
exception/finally regressions remain in the required lower-to-upper selection.

## Tooling Evidence

The default order is WSL GCC 11.4, WSL Clang 14, then native Windows MSVC 19.44.
The user's current explicit exception runs independent Windows first, retaining
both Linux gates as unpassed. It changes order only, not acceptance criteria.
Both Linux configurations use the actual checkout, Ninja Debug/static builds,
language server ON, and Rust binding/editor extension/host JIT OFF. Products,
caches, generated fixtures and receipts stay in the registered E-root outputs.

The historical GCC r4 command was:

```text
wsl.exe --exec /usr/bin/python3 -B /mnt/e/cargo-targets/zr_vm/tmp/m1-wsl-matrix-validation.py gcc --attempt r4
```

That historical Clang registration uses `clang --attempt r4`. Current Windows
uses its separately registered native driver under the explicit order exception.
Input and helper hashes,
actual child exit codes, discovered commands, binary hashes, test output and
final source comparison are part of the receipts. See the
[validation ledger](../../docs/testing-and-validation/macro-foundation-validation.md)
for commands, historical repairs and immutable evidence locations.

## Results

GCC r4 is terminal. Foundation configuration and build exit zero, followed by
52/52 serial CTest passes in 9.59 seconds with all 52 binary hashes unchanged.
Full configuration exits zero, but full build exits one: two LSP declaration
lookup callers omit the new syntax-context argument. Lower14, original127,
full54 and CLI are not executed. Immutable raw receipt
`m1-wsl-gcc-current-r4-receipt.json` has SHA-256
`743A3F11A320EF93685B26ECE095398B77922F4647A37D80321581A5690182E3`
and remains **FAIL**.

Final source-post records four changed foreign inputs: the SSA differential
test, SSA state-map test, public state-map header and `exec_ir_materialize.c`.
The earlier two-path preflight and one-header documentation proof do not cover
that final inventory. No complete GCC, Clang or final MSVC gate is credited.

The M1-new lambda metadata witness adds four negative-coordinate boundaries
without weakening its original real-source checks. LSP declaration lookup
still needs the declaration's actual hygiene context,
tested against a same-AST/name/kind root decoy. The first focused RED launcher
fails at WSL CreateVm (`0x800705b4`) before Python, compiler or tests start.
A subsequent `/bin/true` availability-only probe exits zero in 66368 ms; that
probe grants no compiler/test credit. The first focused attempt subsequently
compiles through 80/554 before its log reports `interrupted by user`. On resume,
the handle is missing, WSL has a new boot and old descendants are absent. Root
sent no stop signal; the actor and process exit are unknown. No native test log,
source-post or driver receipt exists, so this is not functional RED. The compact
interruption observation has SHA-256
`99B7B7A31D8F820438CF79650510C6D05D77B6CE84315B9A7DA3443831739DE8`.

Attempt r2 reuses the same actual build and completed objects, with new output
stems rather than overwriting r1. Actual build949/949 exits0 and the complete
ten-case native run exits1: nine PASS, only the intended final negative-coordinate
assertion FAIL, exact mask0xF, zero ignored. The binary and both lambda source
pre/post pins remain unchanged. Raw receipt7B2F9186 remains FAIL due66 global
changed inputs; this is dated behavioral evidence, not source-stable GCC/M1.
The comparator now rejects negatives before safe unsigned comparisons. Normal
same-build GREEN r2 is terminal FAIL at837/838: ranlib reports No space left
on device; build490 exits1 and no native tests run. Source-post3922 exits0
with61 global changed inputs and stable own lambda pins. Raw receipt SHA-256
is FF5CFF5C0B536EDFB884EF7D7753E1AC7050F14342F6ABB6DAD12E67958C9C6C.
After capacity recovers, normal same-build GREEN r3 completes 951/951 steps
with build PID 539 exiting 0. Native PID 4455 exits 0 with all original ten
ordered cases PASS, zero failures and zero ignored. Both own lambda source
pre/post pins and binary 6630345A (41217544 bytes) remain unchanged.
Source-post PID 4456 exits 0 but records 62 global changed inputs; raw receipt
`E:/cargo-targets/zr_vm/reports/m1-gcc-range-witness-green-r3-receipt.json`
remains FAIL, SHA-256
`580B32C2C240F2C720C238BEF4644066AD6D40F126AA528A54357A3C61B0AECB`,
with head_post absent. Root/core accept only dated functional GREEN, not a
source-stable GCC or M1 pass.

LSP's original two hygiene tests plus nine new declaration-context cases are
frozen, with84 unique main calls, distinct prerequisites and exact expected
behavior reasons. The temporary Root-argument mutant is frozen through r3 for
its actual RED, not final behavior. RED r1 is terminal fixture
compilation FAIL with no native run (receipt89F08034). Exactly five API uses are
corrected: four builtin-int constants become `ZR_VALUE_TYPE_INT64`, and the
invalid context uses the public table-owned invalid-mark compound literal.
The declaration-context header is937 lines; all84 calls and eleven expected
behavior failures remain unchanged.

RED r2 builds exit0, then executes84 ordered unique cases with72 PASS and12 FAIL.
The eleven intended failures match exact reasons with no fixture prerequisite
error; the existing `Unknown Parameter Preserves Identity` adds a twelfth
failure. All4772 source inputs and recorded pre/post HEAD match. Receipt99902011
is raw FAIL, not accepted exact RED, and rejection precedes the binary-post gate.
No binary-post result exists. GDB r1 observes case0 PASS/type0; case1 keeps the
parameter declaration/read on symbol3/type0 but publishes callable symbol2/type3.
Hover stays NULL because the preceding callable-type assertion short-circuits.
Diagnostic receipt3FD9CDD9 is also raw FAIL after three foreign ExecIR inputs
drift; it grants causal observation only, not acceptance.

The unresolved-parameter branch in `symbols.c` omitted `hasUnresolvedParameter`.
Only that one-line prerequisite correction is now inserted and pinned as a fifth
production phase input; the four main hygiene production files stay RED for r3.
Session12432 is terminal with driver exit1: build509 exits0 after7 actions,
native644 exits1 with84 ordered unique cases,73 PASS and the original exact
eleven FAIL names/order/reasons. `Unknown Parameter Preserves Identity` passes.
Binary E4AEAB40,53963096 bytes, is unchanged after execution.

Receipt19000590 remains raw FAIL: source-pre4772 becomes source-post4774 with
four foreign SSA/ExecIR changes. The three tests and five RED production pins
match across r3 and Root's pre-lease review, but HEAD-post is absent after the
full source gate fails. This is dated functional RED for TDD, not accepted
fixed-input RED. Root authorizes the bounded four-file repair under the user's
parallel-edit/revalidation decision. Source review freezes that implementation
with a budget-boundary finding, not GREEN: shared lookup can mistake budgeted
normalization failure for a miss, allowing replacement or premature publication.
The original seven-case native RED r1 now reproduces its registered behavior
failures as dated TDD evidence, not fixed-input acceptance or a production fix.

The original 628-line r1 budget regression has seven ordered cases (SHA 4859A78C),
registered through dedicated fragment 898AE2CA and single include AADB6534. It is
independent of the unchanged original 84 analyzer cases and foundation 52. Root
has drained normal WSL GCC budget RED r1: build534 completes 18/18 actions with
exit 0; native767 exits 5, seven cases = 2 PASS/5 exact FAIL/0 IGNORE, with no
fixture prerequisite error. Receipt3888EF4C stays raw FAIL after five foreign
source changes and actual HEAD d3936df4 to 06364480. Seventeen owned pre/post
pins and binary4DD65559 are independently stable; this is dated functional RED
only. Helper CE277678's isolated 8/8 and 10/10 GREEN checker results retain no
real interruption or compiler/native credit. Full identities are in the ledger.

The prior binding/callable comparators also collapse normalization failure into
a miss: public no-snapshot registration can append duplicates, and overload
lookup can return partial TRUE. This static P2 is an M1 context-budget
prerequisite risk, not a native result or an automatic M1 exemption after an
exact-query-only repair. Four added public no-snapshot cases are now authored,
stopped and source-reviewed: main71CDCD5E/header216F2D59. Root and independent
full RAM inverses recover original r1 SHA4859A78C; the original seven assertions,
original84 and CMake were unchanged at RED r2. Normal unified75735 is terminal:
build517 completes6/6/exit0; native566 exits9 with11 unique ordered cases,
2 PASS/nine exact FAIL/0 IGNORE/no prerequisite failure. Receipt08934E6E remains
raw FAIL solely for HEAD06364480-to-badc069b; complete4777 source inventories
are identical09A58B41, all18 owned pins and binary DE7F0768 are stable. This is
dated TDD for all nine failures, not fixed-input acceptance. The four new cases
directly exercise registration/multi-lookup, not RuntimeRoot/Capture, typed-call
or external-alias migration.

The ten-path checked candidate is implemented and its author STOPPED after Root
and independent core review with no new P1/P2. Separate exact BOOL/out,
comparison status/outEqual and current/full-chain lookup contracts are documented
in [LSP canonical bindings](../../docs/parser-and-semantics/lsp-typecheck-canonical-binding.md).
The two new private modules exist; public ABI and callable-payload ownership
are unchanged. Original84 utility BA21308E is honestly refrozen: two independent
nine-region RAM inverses restore8C04A6E7/50775B and preserve assertions/reasons/
all84 calls. No all-pointer error/miss distinction or global OOM guarantee follows.

GREEN launcher7197 and minimal WSL true probe8920 naturally exit1/drained at
CreateInstance/CreateVm/0x800705b4. The helper/build/native never starts; all5
GREEN output paths are absent and there is no GREEN receipt. Cause unknown,
no machine/service restart or config change. Original84 GREEN, full platform
matrix and M1 acceptance/commit remain pending with strict Source/HEAD gates.
Dated r10 documentation PASS9/669/34 (PID2272, receiptC4E44918) covers old pins
only; this revision requires fresh r11 metadata/link validation.

The two matrix helpers now explicitly reject changed HEAD. The unchanged
isolated eight-case checker moves from4 PASS/4 FAIL to8 PASS, PID7884/exit0
(GREEN receipt3A6A5EC3), with four input hashes stable. It exercises actual final
verification control flow only, not source enumeration or compiler/native
matrix execution. Initial r5/r6 registrations adopted WSL E7A887BE and MSVC
ABA4614F while retaining superseded hashes. The standalone budget target now
expands only full CTest54 to55, including build/discovery/binary pins; the current
reviewed WSL AEC3755E/MSVC254D2275 hashes retain those HEAD gates. The unchanged
isolated eight-case checker passes again (PID19004, receiptB4705C35), without
compiler or platform credit. Full receipt paths and hashes are
in the validation ledger; neither these runs nor the pending budget follow-up
establish the complete platform matrix or M1 acceptance.

## Acceptance Decision

**Not accepted.** Complete all required platform runs, inspect every failure,
verify final source and binary identities, and document any independently
proven source-identity assessment without relabeling raw receipts. The foreign
mechanical layout baseline is now committed as ec8debec with merge b3217253;
recheck current HEAD/index and isolate only accepted M1 work without taking
unrelated changes. M2 remains pending.

The task lifecycle remains active. Only the failed Parser archive is removed
by an explicit fingerprint/preview/guarded Apply plan, releasing100524694bytes.
Reusable objects, current builds, all failure evidence, foreign products,
sources and shared caches remain. The user keeps parallel editing and accepts
later current-input revalidation; this does not relax any acceptance gate.
