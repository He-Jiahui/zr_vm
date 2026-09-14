#ifndef ZR_VM_PARSER_AOT_IR_LOWERING_H
#define ZR_VM_PARSER_AOT_IR_LOWERING_H

#include "zr_vm_parser/conf.h"
#include "zr_vm_core/aot_ir.h"

/* Pointer-free lowering record shared by the C and LLVM emitters. */
typedef enum EZrAotIrLoweringKind {
    ZR_AOT_IR_LOWERING_TYPED_SCALAR = 1,
    ZR_AOT_IR_LOWERING_CONTROL,
    ZR_AOT_IR_LOWERING_CALL,
    ZR_AOT_IR_LOWERING_MEMBER_BRIDGE,
    ZR_AOT_IR_LOWERING_CONTAINER_BRIDGE,
    ZR_AOT_IR_LOWERING_INLINE_LAYOUT,
    ZR_AOT_IR_LOWERING_ASYNC_BOUNDARY,
    ZR_AOT_IR_LOWERING_RUNTIME_BRIDGE,
    ZR_AOT_IR_LOWERING_UNSUPPORTED
} EZrAotIrLoweringKind;

typedef struct SZrAotIrLoweringRecord {
    TZrUInt32 functionId;
    TZrUInt32 instructionId;
    TZrUInt32 opcode;
    TZrUInt32 sourceId;
    TZrUInt32 bindingRow;
    TZrUInt32 layoutId;
    TZrUInt32 flags;
    EZrAotIrLoweringKind kind;
} SZrAotIrLoweringRecord;

typedef struct SZrAotIrLoweringResult {
    SZrAotIrLoweringRecord *records;
    TZrUInt32 capacity;
    TZrUInt32 count;
    TZrUInt32 unsupportedCount;
    TZrUInt32 runtimeBridgeCount;
    TZrUInt64 sourceHash;
    TZrUInt64 loweringHash;
} SZrAotIrLoweringResult;

typedef enum EZrAotIrEmitterTarget {
    ZR_AOT_IR_EMITTER_C = 1,
    ZR_AOT_IR_EMITTER_LLVM = 2
} EZrAotIrEmitterTarget;

typedef struct SZrAotIrEmitOptions {
    EZrAotIrEmitterTarget target;
    TZrBool strictFloatingPoint;
    TZrBool allowRuntimeBridge;
    TZrBool allowInterpreterFallback;
} SZrAotIrEmitOptions;

typedef struct SZrAotIrEmitResult {
    EZrAotIrEmitterTarget target;
    TZrUInt32 nativeCount;
    TZrUInt32 runtimeBridgeCount;
    TZrUInt32 interpreterFallbackCount;
    TZrUInt32 unsupportedCount;
    TZrUInt64 sourceHash;
    TZrUInt64 contractHash;
} SZrAotIrEmitResult;

ZR_PARSER_API TZrBool ZrParser_AotIr_LowerShared(
        const SZrAotIrModule *module,
        SZrAotIrLoweringResult *result,
        SZrAotIrDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_AotIr_LoweringIsPointerFree(
        const SZrAotIrLoweringResult *result);
ZR_PARSER_API TZrBool ZrParser_AotIr_EmitC(
        const SZrAotIrModule *module,
        const SZrAotIrEmitOptions *options,
        SZrAotIrEmitResult *result,
        SZrAotIrDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_AotIr_EmitLlvm(
        const SZrAotIrModule *module,
        const SZrAotIrEmitOptions *options,
        SZrAotIrEmitResult *result,
        SZrAotIrDiagnostic *diagnostic);

#endif /* ZR_VM_PARSER_AOT_IR_LOWERING_H */
