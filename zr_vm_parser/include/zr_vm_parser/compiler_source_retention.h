#ifndef ZR_VM_PARSER_COMPILER_SOURCE_RETENTION_H
#define ZR_VM_PARSER_COMPILER_SOURCE_RETENTION_H

#include "zr_vm_parser/conf.h"
#include "zr_vm_core/exec_ir.h"
#include "zr_vm_core/aot_ir.h"

struct SZrState;
struct SZrString;
struct SZrFunction;

typedef enum EZrSourceRetention {
    ZR_SOURCE_RETENTION_NONE = 0,
    ZR_SOURCE_RETENTION_REQUIRE_SCALAR_AOT = 1
} EZrSourceRetention;

typedef struct SZrSourceCompileOptions {
    EZrSourceRetention retention;
} SZrSourceCompileOptions;

typedef enum EZrSourceCompileStage {
    ZR_SOURCE_STAGE_NONE,
    ZR_SOURCE_STAGE_OPTIONS,
    ZR_SOURCE_STAGE_PARSE,
    ZR_SOURCE_STAGE_COMPILE,
    ZR_SOURCE_STAGE_CONSTANT,
    ZR_SOURCE_STAGE_BUILD,
    ZR_SOURCE_STAGE_MODULE_CONTRACT,
    ZR_SOURCE_STAGE_COMPACTION,
    ZR_SOURCE_STAGE_LAYOUT,
    ZR_SOURCE_STAGE_FRAME,
    ZR_SOURCE_STAGE_PROJECTION,
    ZR_SOURCE_STAGE_TARGET,
    ZR_SOURCE_STAGE_DESCRIPTOR,
    ZR_SOURCE_STAGE_OWNER
} EZrSourceCompileStage;

typedef enum EZrSourceCompileResult {
    ZR_SOURCE_RESULT_OK,
    ZR_SOURCE_RESULT_INVALID_ARGUMENT,
    ZR_SOURCE_RESULT_PARSE_FAILED,
    ZR_SOURCE_RESULT_COMPILE_FAILED,
    ZR_SOURCE_RESULT_UNSUPPORTED,
    ZR_SOURCE_RESULT_OUT_OF_MEMORY,
    ZR_SOURCE_RESULT_PRODUCER_FAILED
} EZrSourceCompileResult;

typedef struct SZrSourceCompileDiagnostic {
    EZrSourceCompileStage stage;
    EZrSourceCompileResult result;
    TZrBool hasExecIr, hasAotIr, hasRange;
    SZrExecIrDiagnostic execIr;
    SZrAotIrDiagnostic aotIr;
    TZrSize startOffset, endOffset;
    TZrInt32 startLine, startColumn, endLine, endColumn;
} SZrSourceCompileDiagnostic;

/* Compile through the ordinary source wrapper with an explicit retention mode.
 * NULL options or zero options mean NONE; existing ordinary/test/submission and
 * registered host compile entrypoints remain NONE. sourceName is optional.
 * Unknown retention, NULL state/source or zero sourceLength reports
 * OPTIONS/INVALID_ARGUMENT before parsing, build facts, canonicalization, module
 * preparation or output mutation. REQUIRE combined with submission context or
 * output is rejected before module preparation by the internal pipeline.
 *
 * State/source/name/options must remain live and independently readable during
 * the call. Optional diagnostic is live writable storage, nonoverlapping with
 * inputs and returned owners. The caller serializes lifecycle and mutation.
 * The entire diagnostic is reset at entry and on success. It retains only values,
 * never source strings/messages, AST, compiler/context or source-map pointers.
 * PARSE/PARSE_FAILED copies actual callback coordinates while its range is live;
 * parser initialization failure without a callback range leaves hasRange false.
 * Offsets and signed line/column fields match the lossless parser location types.
 *
 * Generic compile failure is COMPILE/COMPILE_FAILED unless an actual diagnostic
 * certifies another result. Lower failures preserve complete ExecIR/AOT payloads
 * and stage; a lower range is copied only from the corresponding live source map.
 * No corresponding map means hasRange false. Bool/NULL/hash refusal is not
 * inferred to be OOM. Existing logging and structured callbacks are unchanged.
 *
 * REQUIRE admits only the finalized current literal SCRIPT scalar I64 profile:
 * current selected owning Function, no top-level/nested/borrowed graphs, variable
 * arity, receiver, captures, imports, exports, test manifest or submission. It
 * uses actual SemIR/canonical/source identity and independent Core metadata
 * ownership. Success returns the real Function after CompilerState/AST cleanup;
 * its retained metadata has no borrowed producer pointers. Failure returns NULL
 * without publishing a Function, owner or submission result. Transient producer
 * owners are cleaned before compiler/AST destruction. Earlier module summaries
 * may remain changed: this is not global summary/cache/effect rollback or
 * restoration across nonlocal VM exceptions. No source/backend/native execution
 * is performed by retention. Default retained output and broader programs remain
 * separate future work.
 *
 * Current finite RED implementation refuses REQUIRE at COMPILE/UNSUPPORTED,
 * with no lower record/range and no owner attachment. The producer chain requires
 * a separate GREEN implementation after actual returned-source RED acceptance.
 */
ZR_PARSER_API struct SZrFunction *ZrParser_Source_CompileWithOptions(
        struct SZrState *state, const TZrChar *source, TZrSize sourceLength,
        struct SZrString *sourceName, const SZrSourceCompileOptions *options,
        SZrSourceCompileDiagnostic *diagnostic);

#endif
