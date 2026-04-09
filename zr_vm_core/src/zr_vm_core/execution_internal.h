//
// Internal execution helpers shared across split translation units.
//

#ifndef ZR_VM_CORE_EXECUTION_INTERNAL_H
#define ZR_VM_CORE_EXECUTION_INTERNAL_H

#include "zr_vm_core/execution_control.h"
#include "zr_vm_core/execution.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "zr_vm_core/closure.h"
#include "zr_vm_core/conversion.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/math.h"
#include "zr_vm_core/meta.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/ownership.h"
#include "zr_vm_core/profile.h"
#include "zr_vm_core/reflection.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_common/zr_object_conf.h"
#include "zr_vm_common/zr_runtime_limits_conf.h"
#include "zr_vm_common/zr_string_conf.h"

typedef enum EZrExecutionNumericFallbackOp {
    ZR_EXEC_NUMERIC_FALLBACK_ADD = 0,
    ZR_EXEC_NUMERIC_FALLBACK_SUB,
    ZR_EXEC_NUMERIC_FALLBACK_MUL,
    ZR_EXEC_NUMERIC_FALLBACK_DIV,
    ZR_EXEC_NUMERIC_FALLBACK_MOD,
    ZR_EXEC_NUMERIC_FALLBACK_POW
} EZrExecutionNumericFallbackOp;

typedef enum EZrExecutionNumericCompareOp {
    ZR_EXEC_NUMERIC_COMPARE_GREATER = 0,
    ZR_EXEC_NUMERIC_COMPARE_LESS,
    ZR_EXEC_NUMERIC_COMPARE_GREATER_EQUAL,
    ZR_EXEC_NUMERIC_COMPARE_LESS_EQUAL
} EZrExecutionNumericCompareOp;

#define EXEC_DONE(N) ZR_INSTRUCTION_DONE(instruction, programCounter, N)
#define EXEC_E(INSTRUCTION) ((INSTRUCTION).instruction.operandExtra)
#define EXEC_A0(INSTRUCTION) ((INSTRUCTION).instruction.operand.operand0[0])
#define EXEC_B0(INSTRUCTION) ((INSTRUCTION).instruction.operand.operand0[1])
#define EXEC_C0(INSTRUCTION) ((INSTRUCTION).instruction.operand.operand0[2])
#define EXEC_D0(INSTRUCTION) ((INSTRUCTION).instruction.operand.operand0[3])
#define EXEC_A1(INSTRUCTION) ((INSTRUCTION).instruction.operand.operand1[0])
#define EXEC_B1(INSTRUCTION) ((INSTRUCTION).instruction.operand.operand1[1])
#define EXEC_A2(INSTRUCTION) ((INSTRUCTION).instruction.operand.operand2[0])

TZrBool execution_try_materialize_global_prototypes(SZrState *state,
                                                    SZrClosure *currentClosure,
                                                    SZrCallInfo *currentCallInfo,
                                                    const SZrTypeValue *tableValue,
                                                    const SZrTypeValue *keyValue);

TZrInt64 value_to_int64(const SZrTypeValue *value);
TZrUInt64 value_to_uint64(const SZrTypeValue *value);
TZrDouble value_to_double(const SZrTypeValue *value);

TZrBool concat_values_to_destination(SZrState *state,
                                     SZrTypeValue *outResult,
                                     const SZrTypeValue *opA,
                                     const SZrTypeValue *opB,
                                     TZrBool safeMode);
TZrBool try_builtin_add(SZrState *state,
                        SZrTypeValue *outResult,
                        const SZrTypeValue *opA,
                        const SZrTypeValue *opB);
TZrBool execution_try_builtin_sub(SZrState *state,
                                  SZrTypeValue *outResult,
                                  const SZrTypeValue *opA,
                                  const SZrTypeValue *opB);
TZrBool execution_try_builtin_mul(SZrState *state,
                                  SZrTypeValue *outResult,
                                  const SZrTypeValue *opA,
                                  const SZrTypeValue *opB);
TZrBool execution_try_builtin_div(SZrState *state,
                                  SZrTypeValue *outResult,
                                  const SZrTypeValue *opA,
                                  const SZrTypeValue *opB);

void execution_apply_binary_numeric_float_or_raise(SZrState *state,
                                                   EZrExecutionNumericFallbackOp operation,
                                                   SZrTypeValue *destination,
                                                   const SZrTypeValue *opA,
                                                   const SZrTypeValue *opB,
                                                   const TZrChar *instructionName);
void execution_apply_binary_numeric_compare_or_raise(SZrState *state,
                                                     EZrExecutionNumericCompareOp operation,
                                                     SZrTypeValue *destination,
                                                     const SZrTypeValue *opA,
                                                     const SZrTypeValue *opB,
                                                     const TZrChar *instructionName);
void execution_try_binary_numeric_float_fallback_or_raise(SZrState *state,
                                                          EZrExecutionNumericFallbackOp operation,
                                                          SZrTypeValue *destination,
                                                          const SZrTypeValue *opA,
                                                          const SZrTypeValue *opB,
                                                          const TZrChar *instructionName);

SZrObjectPrototype *find_type_prototype(SZrState *state,
                                        SZrString *typeName,
                                        EZrObjectPrototypeType expectedType);
TZrBool convert_to_struct(SZrState *state,
                          SZrTypeValue *source,
                          SZrObjectPrototype *targetPrototype,
                          SZrTypeValue *destination);
TZrBool convert_to_class(SZrState *state,
                         SZrTypeValue *source,
                         SZrObjectPrototype *targetPrototype,
                         SZrTypeValue *destination);
TZrBool convert_to_enum(SZrState *state,
                        SZrTypeValue *source,
                        SZrObjectPrototype *targetPrototype,
                        SZrTypeValue *destination);

TZrSize close_scope_cleanup_registrations(SZrState *state, TZrSize cleanupCount);
TZrBool execution_prepare_meta_call_target(SZrState *state, TZrStackValuePointer stackPointer);
TZrBool execution_meta_get_member(SZrState *state,
                                  SZrTypeValue *receiver,
                                  SZrString *memberName,
                                  SZrTypeValue *result);
const SZrFunctionCallSiteCacheEntry *execution_get_callsite_cache_entry(SZrFunction *function,
                                                                        TZrUInt16 cacheIndex,
                                                                        EZrFunctionCallSiteCacheKind expectedKind);
TZrBool execution_prepare_meta_call_target_cached(SZrState *state,
                                                  SZrFunction *function,
                                                  TZrUInt16 cacheIndex,
                                                  TZrStackValuePointer stackPointer,
                                                  EZrFunctionCallSiteCacheKind expectedKind);
TZrBool execution_try_prepare_dyn_call_target_cached(SZrState *state,
                                                     SZrFunction *function,
                                                     TZrUInt16 cacheIndex,
                                                     TZrStackValuePointer stackPointer,
                                                     EZrFunctionCallSiteCacheKind expectedKind);
TZrBool execution_meta_get_cached_member(SZrState *state,
                                         SZrFunction *function,
                                         TZrUInt16 cacheIndex,
                                         SZrTypeValue *receiver,
                                         SZrTypeValue *result);
TZrBool execution_meta_get_cached_static_member(SZrState *state,
                                                SZrFunction *function,
                                                TZrUInt16 cacheIndex,
                                                SZrTypeValue *receiver,
                                                SZrTypeValue *result);
TZrBool execution_meta_set_member(SZrState *state,
                                  SZrTypeValue *receiverAndResult,
                                  SZrString *memberName,
                                  const SZrTypeValue *assignedValue);
TZrBool execution_meta_set_cached_member(SZrState *state,
                                         SZrFunction *function,
                                         TZrUInt16 cacheIndex,
                                         SZrTypeValue *receiverAndResult,
                                         const SZrTypeValue *assignedValue);
TZrBool execution_meta_set_cached_static_member(SZrState *state,
                                                SZrFunction *function,
                                                TZrUInt16 cacheIndex,
                                                SZrTypeValue *receiverAndResult,
                                                const SZrTypeValue *assignedValue);
TZrBool execution_invoke_meta_call(SZrState *state,
                                   SZrCallInfo *savedCallInfo,
                                   TZrStackValuePointer savedStackTop,
                                   TZrStackValuePointer requestedScratchBase,
                                   SZrMeta *meta,
                                   const SZrTypeValue *arg0,
                                   const SZrTypeValue *arg1,
                                   TZrSize argumentCount,
                                   TZrStackValuePointer *outMetaBase,
                                   TZrStackValuePointer *outSavedStackTop);

TZrBool execution_has_exception_handler_for_callinfo(SZrState *state, SZrCallInfo *callInfo);
const SZrFunctionExceptionHandlerInfo *execution_lookup_exception_handler_info(
        SZrState *state,
        const SZrVmExceptionHandlerState *handlerState,
        SZrFunction **outFunction);
TZrBool execution_try_reuse_tail_call_frame(SZrState *state,
                                            SZrCallInfo *callInfo,
                                            TZrStackValuePointer functionPointer);

#endif // ZR_VM_CORE_EXECUTION_INTERNAL_H
