---
related_code:
  - zr_vm_parser/include/zr_vm_parser/compiler_source_retention.h
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_source_retention.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_source_retention_internal.h
  - zr_vm_core/src/zr_vm_core/function_aot_ir.c
  - zr_vm_core/src/zr_vm_core/aot_ir_owned_module.c
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/compiler_source_retention.h
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_source_retention.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_source_retention_internal.h
plan_sources:
  - .codex/plans/20261008-ssa-function-retention-next-finite-v4.md
  - .codex/plans/20261009-ssa-source-returned-retention-source-tests-design.md
tests:
  - tests/parser/test_ssa_source_returned_retention.c
  - tests/parser/support/ssa_source_returned_retention_fixture.c
  - docs/acceptance/ssa-source-returned-retention.md
doc_type: module-documentation
status: finite-green-accepted
---

# Returned source metadata retention

## Purpose and entrypoint

`ZrParser_Source_CompileWithOptions` compiles through the real source wrapper.
Its public contract is in `zr_vm_parser/compiler_source_retention.h`.
NULL options and a zero options record select NONE. The ordinary, test,
submission, cache and registered host compiler entrypoints also select NONE.
Only explicit REQUIRE_SCALAR_AOT requests independent Core AOT metadata for
the finalized literal SCRIPT I64 profile. It does not execute the Function.

The options entrypoint takes state, source bytes/length, optional source name,
optional options and optional diagnostic; it returns an actual `SZrFunction*`.
Unknown options and invalid required arguments return NULL at OPTIONS before
parsing. The legacy cache entry retains its original invalid-argument output
reset behavior. Inputs must remain readable, the diagnostic writable and
nonoverlapping, and lifecycle changes serialized by the caller.

## Lifecycle and module boundaries

The source wrapper parses, prepares build facts, canonicalizes textual module
identity, prepares module summaries and enters the existing compiler mode.
The REQUIRE hook runs after finalized call bindings and before submission
publication, while the real compiler, AST, canonical types, pools and SemIR are
live. A separate private compiler module owns the retention producer chain.
The retention changes in the large compiler entry module contain signatures,
option checks, diagnostic plumbing and the single hook.

On refusal the active compiler frees the selected Function and compiler state;
the wrapper then clears AST identity and frees the AST. The enclosing compiler
mode restores the previous provider phase. On retention success,
transient producers are released before compiler and AST destruction, and the
returned Function owns its independent Core arena. Generic Core ownership
contains no Parser-specific source-profile checks.

The helper calls the actual scalar-constant bridge before metadata lookup or
module allocation. It then builds the module, appends the constant, verifies
the original function, binds the source module contract and compacts dead source
places. The host primitive layout is appended to the module, the compacted frame
is attached and verified, and canonical callable lowering creates the projection.
The host target and descriptor producers validate the module and executable ABI
before one `ZrCore_Function_SnapshotAotIr` publishes an independent owner.
Every exit releases descriptor, projection, compacted function and original
module in that order, before the caller destroys compiler/AST storage.

Admission remains the finalized no-argument, nonnullable I64 literal SCRIPT
profile: no receiver, captures, nested or borrowed graphs, imports, exports,
test manifest or submission. These checks use current source/canonical facts;
the helper does not construct replacement identities or execute the source.

Earlier prepared/finalized module summaries may persist after a late refusal.
This API promises no returned owner and complete transient cleanup on explicit
failure; it does not promise global summary/cache/effect rollback or restoration
across nonlocal VM allocator exceptions.

## Diagnostic values

The entire diagnostic resets on entry and success. It records a stage/result,
complete copied ExecIR/AOT records and optional numeric source coordinates.
Offsets use `TZrSize`; signed lines and columns use `TZrInt32`, matching the
actual parser location types. No source string, message, AST, compiler or map
pointer survives the call.

PARSE/PARSE_FAILED copies the first actual error-callback range while live.
A failure without such a callback leaves hasRange false. Generic compilation
failure is COMPILE/COMPILE_FAILED and does not overwrite a specific retention
failure. A lower range needs its corresponding live source map; no map means
no range. Bool/NULL/hash-provider refusal is never guessed to be OOM.

The implemented D helper calls the real nonallocating scalar-constant bridge
before module allocation. Legal bool and float literals report
CONSTANT/UNSUPPORTED with the actual lower
UNSUPPORTED record, sourceId0, expectedHash1 and actualHash0; other fields stay
zero and no range is invented.

Bool constant/layout append APIs provide no lower diagnostic. Their refusal
reports BUILD/PRODUCER_FAILED or LAYOUT/PRODUCER_FAILED without guessing OOM.
Lower payloads are copied before cleanup. Source ranges retain original SemIR
coordinates; an AOT instruction ID is mapped through the verified compacted
source map. A target diagnostic index is a type ID and is not used as an
instruction index.

## Approved validation scope

The finite gate compiles only `return 9;`, `return 8;`, `return true;`,
`return 9.0;` and `return 9 + ;`. It uses relative source names and fresh Globals
with optional host callbacks and project/source-loader userdata NULL.
Compiler construction attaches NativeRegistry and registers internal static
builtin/reflection metadata. The reviewed literal closure validates/copies
those records without invoking external loaders, plugins or providers.

Tests compile, inspect metadata and destroy objects. They do not call source
execution, Oracle, generated native programs, import resolution, network, FFI
or security services. Presence of a cold linked function is not execution
coverage. The full ordinary returned-output, broader programs, Linux/full MSVC,
backend/native parity and all 47 SSA leaf acceptance gates remain OPEN.

## Related evidence

- [Finite tests](../testing-and-validation/ssa-source-returned-retention.md)
- [Acceptance record](../acceptance/ssa-source-returned-retention.md)
- [Source constant support](ssa-source-scalar-constant.md)
- [Descriptor allocation support](ssa-source-descriptor-oom.md)
- [AOT contract plan](../plans/ssa/07-aot-backends/01-aotir-contract.md)
