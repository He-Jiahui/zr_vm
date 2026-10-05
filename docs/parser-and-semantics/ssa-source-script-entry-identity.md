---
related_code:
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_parser/include/zr_vm_parser/canonical_type.h
  - zr_vm_parser/include/zr_vm_parser/semantic.h
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_parser/src/zr_vm_parser/compiler/module_init_analysis.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_script_entry_metadata.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_script_callable_return.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_script_callable_return.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_script_callable_identity.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_script_callable_identity.h
  - zr_vm_parser/src/zr_vm_parser/parser.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_statements.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_state.c
  - tests/parser/test_ssa_source_script_entry_identity.c
  - tests/cmake/ssa-source-execbc-vm.cmake
  - tests/cmake/ssa-source-direct-validation/CMakeLists.txt
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_script_callable_identity.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_script_callable_identity.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_script_callable_return.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_script_callable_return.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_script_entry_metadata.c
plan_sources:
  - .codex/plans/20261004-source-aot-handoff-design-2005.md
  - docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
tests:
  - tests/parser/test_ssa_source_script_entry_identity.c
  - docs/acceptance/ssa-source-script-entry-identity.md
  - docs/acceptance/ssa-source-range-identity.md
doc_type: module-detail
status: accepted-script-entry-identity-direct-green
---

# Source SCRIPT Entry Callable Identity

## Purpose and admission

This finite producer adds canonical callable provenance to the actual SCRIPT
returned by ordinary `ZrParser_Source_Compile`. The existing
`SZrFunctionSourceCallableIdentity` record stores a real semantic FUNCTION
symbol ID, canonical FUNCTION type ID, structural signature hash, and numeric
source range after temporary compiler state is released.

Admission is restricted to a versionless, zero-export, pure ordinary SCRIPT
whose entire statement list is one explicit nonreference return of an integer
literal, such as `return 9;`. Parameters, varargs, imports/native contracts,
effects, closures, children, submissions, and a selected top-level function
are excluded. Bool, conditional paths, implicit/operandless returns, binary
expressions, local returns, and multiple statements receive no SCRIPT witness.
The preceding callable-return and entry-token producers retain their own
broader finite admission rules; their metadata alone does not admit identity.

## Source proof and finalization order

`compiler_script_entry_metadata_prepare`, called by
`ZrParser_ModuleInitAnalysis_FinalizeCurrentSourceModule` in `module_init_analysis.c`,
first invokes `compiler_script_callable_return_publish`. Its optional
`outProvenReturnTypeId` is cleared to INVALID before analysis and receives the
actual canonical i64 return TypeId only after the existing validated reachable
CFG proof and ordinary return-metadata publication succeed. The original
proof still checks every reachable terminal path and rejects reachable cycles.
Unsupported return forms leave the output invalid; structural proof failure
or scratch allocation failure follows the existing false/error path.

Preparation then calls `compiler_script_callable_identity_try_publish` with
that TypeId while the actual AST, semantic context, and semantic IR are alive.
Identity is an optional additional proof: unsupported or incomplete evidence
publishes nothing and adds no source diagnostic. The frozen implementation
places identity after the existing `functionName` assignment and GC write
barrier; the helper acquires and releases only its own temporary roots.
Metadata token refresh and module summary hashing remain later stages in
module finalization.

The identity helper checks the original AST only for supported shape and
origin. It validates and inspects the actual source-produced semantic IR:

- One entry block, ID 0, is also the exit block; it has no predecessors,
  successors, or outgoing edges, and ends in RETURN.
- Exactly three instructions are CONSTANT, inert temporary PLACE_BASE, and
  RETURN. There is one i64 value, one return operand, one temporary place, and
  one compiler slot. The constant-pool entry is actually INT64.
- The value and its defining constant have the proven canonical return TypeId;
  the slot, place, and RETURN agree. The place is rooted in that slot's real
  temporary identity, with no parent or projections.
- The sole default FUNCTION region has ID 1, no parent, and an empty source
  range. Locals, scalar scratch proofs, cleanup scopes, loans, escapes, bounds,
  contiguous views, and receiver loans are absent.
- Instruction and source-map ranges match the actual literal/return ranges.
  Instructions carry no ownership, call/accessor/constructor, branch,
  conversion, loan, cleanup, or comparison evidence outside this form.

The producer neither recreates the literal value from the AST nor derives a
callable from legacy ExecBC. It preserves the real temporary-place producer;
there is no fabricated replacement IR or analysis-only return graph.

## Canonical owner and pointer-free publication

After the source proof, `ZrParser_CanonicalType_InternFunction` interns zero
parameter contracts, the proven return TypeId, receiver NONE, and callable
effects NONE. The returned node must be a valid canonical FUNCTION with those
same properties and a nonzero `structuralHash`.

`ZrParser_Semantic_RegisterSymbol` then creates the actual implicit SCRIPT
FUNCTION symbol. Its name is the current module key, its AST owner is the
actual SCRIPT, and its location is that SCRIPT's source range. Lookup by the
new symbol ID must agree on kind, type, name, AST identity, and complete range.
The module key supplies an internal symbol name; it does not supply the
canonical structural signature hash.

Once all checks succeed, the helper assigns the real symbol/type IDs to
`preSemanticIr.symbolId` and `callableTypeId`, copies the complete numeric
record to `function->sourceCallableIdentity`, and finally sets
`hasSourceCallableIdentity`. Schema version is V1; return primitive is INT64;
parameter, receiver, and effect fields are zero; `hasExplicitNoArgsI64` is true.
For SCRIPT, `declarationRange` means the implicit callable's source origin,
rather than the location of a named `fn` declaration.

At this gate's historical acceptance, the parser obtained SCRIPT/RETURN starts
from `get_current_location` after scanning the first token, and RETURN was a
point range. The finite identity proof therefore did not require RETURN to
contain its expression. It preserved numeric SCRIPT origin, RETURN, and
expression positions, checked equality with real semantic IR/source-map
positions, and checked that SCRIPT contained those origins.

The subsequent [source range contract](ssa-source-range-identity.md) corrects
SCRIPT to start at the first actual token and reach EOS, and RETURN to start
at `return` and end after its consumed semicolon. Its separate direct GREEN
also verifies CRLF token ends and bounded missing-semicolon recovery. Current
identity publication consumes those actual corrected parser positions. The
historical identity receipts below remain evidence for their original finite
gate; complete statement spans are established by the separate range gate.

The IDs remain local to the original semantic context. After source compile
returns, they record provenance and cannot be resolved as persistent handles.
The structural hash describes the callable signature and does not identify a
body or provide a global definition ID. Metadata signature hashes, module ABI
hashes, tokens, and legacy callable hashes do not substitute for this value.
Core already initializes and clears the shared record in normal function
lifecycle; this SCRIPT slice adds no Core field or separately owned resource.
It retains no AST, semantic graph, IR arrays, or source-name pointer in the
record.

## Public fixture and evidence

`tests/parser/test_ssa_source_script_entry_identity.c` contains thirteen cases:
four positive publication/lifetime/retry sequences and nine refusal guards.
The completed GREEN path makes seventeen public compile calls, sixteen
successful and one expected malformed-source NULL. The positive cases inspect
the whole existing return type reference before the witness assertion, root
returned functions, and compare record fields individually across another
compile. Same-name success/guard/success also checks that the first result and
its module hash remain unchanged and that the middle unsupported result never
acquires a witness. Malformed-source retry uses the same source name and VM
state.

The frozen RED test commit is
`a9b2307196b34a9dc8cd07d0f218effe0385a0cc`. Its actual run reported thirteen
tests, four exact missing-witness feature failures, nine passing guards, and
zero ignored tests, without UBSan diagnostics. The companion acceptance
record contains receipt/log/test hashes. The focused direct GREEN run has now
passed; CMake registration in both routes still does not establish that the
ordinary top-level route passed.

The GREEN run used the direct build tree and executed
`ssa_source_script_entry_identity` together with the separate typed-binding
contract test. The script identity test reported 13/13 PASS, 0 failures and
0 ignored, with `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` and no
sanitizer diagnostic. The combined CTest command exited 8 because the
independent typed-binding contract test retained two expected reserved-flag
RED failures; those failures do not belong to this SCRIPT identity gate.
The recorded log is
`E:\cargo-targets\zr_vm\reports\ssa-20261005-01a0fe2b\script-identity-green-binding-flags-red.log`.
Its SHA-256 is
`f308a3a97ad8f179a5c2a1ac5139f412719075673683259e635c3ca6fb2fe518`.
The final identity receipt is
`E:\\cargo-targets\\zr_vm\\reports\\ssa-20261005-01a0fe2b\\script-identity-green-receipt.json`;
its SHA-256 is
`bc281339a5cbf0039ad2248601a080450d0f46410623adf66e2cb0ebbbea193f`.

This gate proves local compilation and metadata lifetime only. Retained
graphs, frames, serialization, executable AOT/token consumption, global
rollback, and the full SSA plan require separate evidence. The acceptance
record specifies environment and unrun routes without extending this scope.
