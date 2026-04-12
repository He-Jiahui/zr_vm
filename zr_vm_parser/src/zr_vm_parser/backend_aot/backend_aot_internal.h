#ifndef ZR_VM_PARSER_BACKEND_AOT_INTERNAL_H
#define ZR_VM_PARSER_BACKEND_AOT_INTERNAL_H

#include "backend_aot_callable_provenance.h"
#include "backend_aot_exec_ir.h"
#include "backend_aot_function_table.h"
#include "zr_vm_parser/writer.h"

#define ZR_AOT_COUNT_NONE 0U
#define ZR_AOT_FUNCTION_TREE_ROOT_INDEX 0U
#define ZR_AOT_INVALID_FUNCTION_INDEX ((TZrUInt32)-1)
#define ZR_AOT_LLVM_RESUME_FALLTHROUGH ((TZrUInt32)0xFFFFFFFFu)

typedef enum EZrAotEmitterStepFlag {
    ZR_AOT_EMITTER_STEP_FLAG_NONE = 0,
    ZR_AOT_EMITTER_STEP_FLAG_MAY_THROW = 1u << 0,
    ZR_AOT_EMITTER_STEP_FLAG_CONTROL_FLOW = 1u << 1,
    ZR_AOT_EMITTER_STEP_FLAG_CALL = 1u << 2,
    ZR_AOT_EMITTER_STEP_FLAG_RETURN = 1u << 3
} EZrAotEmitterStepFlag;

void backend_aot_write_instruction_listing(FILE *file,
                                           const TZrChar *prefix,
                                           const SZrAotExecIrModule *module);
const TZrChar *backend_aot_option_text(const SZrAotWriterOptions *options,
                                       const TZrChar *candidate,
                                       const TZrChar *fallback);
TZrBool backend_aot_function_is_executable_subset(const SZrFunction *function);
TZrBool backend_aot_report_first_unsupported_instruction(const TZrChar *backendName,
                                                         const TZrChar *moduleName,
                                                         const SZrAotFunctionTable *table);
TZrUInt32 backend_aot_option_input_kind(const SZrAotWriterOptions *options);
const TZrChar *backend_aot_option_input_hash(const SZrAotWriterOptions *options,
                                             const TZrChar *sourceHash,
                                             const TZrChar *zroHash);
ZR_PARSER_API TZrUInt32 backend_aot_c_step_flags_for_instruction(const SZrFunction *function,
                                                                 const TZrInstruction *instruction);
TZrUInt32 backend_aot_get_callsite_cache_argument_count(const SZrFunction *function,
                                                        TZrUInt32 cacheIndex,
                                                        EZrFunctionCallSiteCacheKind expectedKind);

#endif
