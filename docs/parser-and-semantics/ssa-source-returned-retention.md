---
doc_type: module-documentation
status: finite-red-established
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
The large compiler entry module contains only signatures, option checks,
diagnostic plumbing and the single hook.

On refusal the active compiler frees the selected Function and compiler state;
the wrapper then clears AST identity and frees the AST. The enclosing compiler
mode restores the previous provider phase. On future retention success,
transient producers are released before compiler and AST destruction, and the
returned Function owns its independent Core arena. Generic Core ownership
contains no Parser-specific source-profile checks.

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

The current C implementation is a safe COMPILE/UNSUPPORTED refusal stub with
all lower/range fields zero. The separately planned D implementation calls the
real nonallocating scalar-constant bridge before module allocation. Legal bool
and float literals then report CONSTANT/UNSUPPORTED with the actual lower
UNSUPPORTED record, sourceId0, expectedHash1 and actualHash0; other fields stay
zero and no range is invented.

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
