#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"

#define ARRAY_COUNT(array_) (sizeof(array_) / sizeof((array_)[0]))

void setUp(void) {}

void tearDown(void) {}

static char *read_text_file_owned(const char *path) {
    FILE *file;
    long fileSize;
    char *buffer;

    if (path == NULL) {
        return NULL;
    }

    file = fopen(path, "rb");
    if (file == NULL) {
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    fileSize = ftell(file);
    if (fileSize < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    buffer = (char *)malloc((size_t)fileSize + 1u);
    if (buffer == NULL) {
        fclose(file);
        return NULL;
    }

    if (fileSize > 0 && fread(buffer, 1u, (size_t)fileSize, file) != (size_t)fileSize) {
        free(buffer);
        fclose(file);
        return NULL;
    }

    buffer[fileSize] = '\0';
    fclose(file);
    return buffer;
}

static char *read_repo_text_file_owned(const char *relativePath) {
    const char *sourceFile = __FILE__;
    const char *marker;
    char path[1024];
    size_t rootLength;
    size_t relativeLength;

    if (relativePath == NULL) {
        return NULL;
    }

    marker = strstr(sourceFile, "tests/parser/test_aot_c_frame_setup_contracts.c");
    if (marker == NULL) {
        marker = strstr(sourceFile, "tests\\parser\\test_aot_c_frame_setup_contracts.c");
    }
    if (marker == NULL) {
        return read_text_file_owned(relativePath);
    }

    rootLength = (size_t)(marker - sourceFile);
    relativeLength = strlen(relativePath);
    if (rootLength + relativeLength + 1u >= sizeof(path)) {
        return NULL;
    }

    memcpy(path, sourceFile, rootLength);
    memcpy(path + rootLength, relativePath, relativeLength + 1u);
    return read_text_file_owned(path);
}

static void assert_text_contains_all(const char *text, const char *const *needles, size_t needleCount) {
    size_t index;

    for (index = 0; index < needleCount; index++) {
        if (strstr(text, needles[index]) == NULL) {
            printf("Missing source contract text: %s\n", needles[index]);
            TEST_FAIL_MESSAGE("missing required source contract text");
        }
    }
}

static void assert_text_contains_all_in_any6(const char *first,
                                             const char *second,
                                             const char *third,
                                             const char *fourth,
                                             const char *fifth,
                                             const char *sixth,
                                             const char *const *needles,
                                             size_t needleCount) {
    size_t index;

    for (index = 0; index < needleCount; index++) {
        if ((first == NULL || strstr(first, needles[index]) == NULL) &&
            (second == NULL || strstr(second, needles[index]) == NULL) &&
            (third == NULL || strstr(third, needles[index]) == NULL) &&
            (fourth == NULL || strstr(fourth, needles[index]) == NULL) &&
            (fifth == NULL || strstr(fifth, needles[index]) == NULL) &&
            (sixth == NULL || strstr(sixth, needles[index]) == NULL)) {
            printf("Missing source contract text: %s\n", needles[index]);
            TEST_FAIL_MESSAGE("missing required source contract text");
        }
    }
}

static void assert_text_contains_none(const char *text, const char *const *needles, size_t needleCount) {
    size_t index;

    for (index = 0; index < needleCount; index++) {
        if (strstr(text, needles[index]) != NULL) {
            printf("Unexpected source contract text: %s\n", needles[index]);
            TEST_FAIL_MESSAGE("found forbidden source contract text");
        }
    }
}

static void test_aot_c_source_emits_direct_generated_frame_setup(void) {
    static const char *const runtimeHeaderNeedles[] = {
            "typedef struct ZrAotGeneratedModuleContext",
            "struct SZrFunction *metadataFunction;",
            "const SZrAotMethodInfo *methodInfo;",
            "const SZrAotCodeRegistration *codeRegistration;",
            "TZrUInt32 generatedFrameSlotCount;",
            "ZrLibrary_AotRuntime_ResolveGeneratedModuleContext(struct SZrState *state,",
    };
    static const char *const stateHeaderNeedles[] = {
            "typedef struct SZrAotGcRootFrame {",
            "const struct SZrAotGcRootMap *rootMap;",
            "TZrStackValuePointer frameBase;",
            "struct SZrAotGcRootFrame *previous;",
            "} SZrAotGcRootFrame;",
            "SZrAotGcRootFrame *aotGcRootFrameStack;",
            "TZrUInt32 aotGcRootFrameDepth;",
    };
    static const char *const gcHeaderNeedles[] = {
            "ZrCore_Gc_AotRootFramePush(struct SZrState *state,",
            "struct SZrAotGcRootFrame *frame,",
            "TZrStackValuePointer frameBase,",
            "const struct SZrAotGcRootMap *rootMap);",
            "ZrCore_Gc_AotRootFramePop(struct SZrState *state,",
            "ZrCore_Gc_AotRootFrameDepth(const struct SZrState *state);",
    };
    static const char *const abiHeaderNeedles[] = {
            "struct SZrFunction;",
            "struct SZrTypeValue;",
            "struct SZrAotMethodInfo;",
            "struct SZrAotGcRootMap;",
            "struct SZrTypeLayout;",
            "ZR_VM_AOT_ABI_VERSION 14u",
            "typedef void (*FZrAotReflectionInvoker)(struct SZrState *state,",
            "FZrAotEntryThunk target,",
            "const struct SZrAotMethodInfo *method,",
            "struct SZrTypeValue *self,",
            "struct SZrTypeValue *args,",
            "struct SZrTypeValue *outReturn);",
            "typedef enum EZrAotGcRootLocationKind {",
            "ZR_AOT_GC_ROOT_LOCATION_FRAME_BYTE_OFFSET = 0",
            "ZR_AOT_GC_ROOT_LOCATION_LOCAL_ADDRESS = 1",
            "} EZrAotGcRootLocationKind;",
            "typedef struct SZrAotGcRootSlot {",
            "TZrUInt32 stackSlot;",
            "TZrUInt32 frameByteOffset;",
            "TZrUInt32 typeLayoutId;",
            "TZrUInt32 fieldByteOffset;",
            "TZrUInt8 locationKind;",
            "} SZrAotGcRootSlot;",
            "typedef struct SZrAotGcRootMap {",
            "TZrUInt32 rootCount;",
            "const SZrAotGcRootSlot *roots;",
            "} SZrAotGcRootMap;",
            "typedef enum EZrAotReflectionMetadataLevel {",
            "ZR_AOT_REFLECTION_METADATA_NONE = 0",
            "ZR_AOT_REFLECTION_METADATA_RUNTIME_MAPPING = 1",
            "ZR_AOT_REFLECTION_METADATA_DESCRIPTION = 2",
            "} EZrAotReflectionMetadataLevel;",
            "typedef enum EZrAotGenericSlotKind {",
            "ZR_AOT_GENERIC_SLOT_TYPE_LAYOUT",
            "ZR_AOT_GENERIC_SLOT_METHOD",
            "ZR_AOT_GENERIC_SLOT_SIZEOF",
            "} EZrAotGenericSlotKind;",
            "typedef struct SZrAotGenericSlot {",
            "const struct SZrTypeLayout *staticTypeLayout;",
            "} SZrAotGenericSlot;",
            "typedef struct SZrAotGenericResolvedSlot {",
            "SZrAotGenericResolvedValue value;",
            "} SZrAotGenericResolvedSlot;",
            "typedef struct SZrAotGenericDictionary {",
            "const SZrAotGenericSlot *slots;",
            "SZrAotGenericResolvedSlot *resolvedSlots;",
            "} SZrAotGenericDictionary;",
            "typedef struct SZrAotSignatureType {",
            "TZrUInt16 baseType;",
            "TZrUInt16 staticCType;",
            "TZrUInt32 staticCTypeId;",
            "TZrUInt32 ownershipQualifier;",
            "TZrUInt16 elementBaseType;",
            "TZrUInt8 isNullable;",
            "TZrUInt8 isArray;",
            "} SZrAotSignatureType;",
            "typedef struct SZrAotSignature {",
            "TZrUInt32 parameterCount;",
            "const SZrAotSignatureType *returnType;",
            "const SZrAotSignatureType *parameterTypes;",
            "TZrUInt8 hasReturnValue;",
            "TZrUInt8 hasVarArgs;",
            "} SZrAotSignature;",
            "typedef struct SZrAotMethodInfo {",
            "TZrUInt32 functionIndex;",
            "const struct SZrFunction *metadataFunction;",
            "TZrUInt32 registerFrameBytes;",
            "const struct SZrAotGcRootMap *gcRootMap;",
            "const SZrAotSignature *signature;",
            "const SZrAotGenericDictionary *genericDictionary;",
            "FZrAotReflectionInvoker invoker;",
            "TZrUInt8 observationPolicy;",
            "TZrUInt8 reflectionMetadataLevel;",
            "} SZrAotMethodInfo;",
            "typedef struct SZrAotCodeRegistration {",
            "TZrUInt32 functionCount;",
            "const FZrAotEntryThunk *functionPointers;",
            "const SZrAotMethodInfo *const *methodInfos;",
            "TZrUInt32 methodInfoCount;",
            "const TZrUInt32 *methodTokens;",
            "TZrUInt32 methodTokenCount;",
            "const FZrAotReflectionInvoker *invokers;",
            "TZrUInt32 invokerCount;",
            "const struct SZrTypeLayout *const *typeLayouts;",
            "TZrUInt32 typeLayoutCount;",
            "const TZrUInt32 *typeLayoutTokens;",
            "TZrUInt32 typeLayoutTokenCount;",
            "const SZrAotGcDescriptor *const *gcDescriptors;",
            "TZrUInt32 gcDescriptorCount;",
            "const struct SZrNativeImportContract *nativeImportContracts;",
            "TZrUInt32 nativeImportContractCount;",
            "const SZrAotNativeImportRange *nativeImportRanges;",
            "TZrUInt32 nativeImportRangeCount;",
            "} SZrAotCodeRegistration;",
            "typedef struct SZrAotNativeImportRange {",
            "TZrUInt32 contractStart;",
            "TZrUInt32 contractCount;",
            "} SZrAotNativeImportRange;",
            "const SZrAotMethodInfo *const *methodInfos;",
            "TZrUInt32 methodInfoCount;",
            "const TZrUInt32 *methodTokens;",
            "TZrUInt32 methodTokenCount;",
            "const struct SZrTypeLayout *const *typeLayouts;",
            "TZrUInt32 typeLayoutCount;",
            "const TZrUInt32 *typeLayoutTokens;",
            "TZrUInt32 typeLayoutTokenCount;",
            "const SZrAotCodeRegistration *codeRegistration;",
    };
    static const char *const runtimeSourceNeedles[] = {
            "static void aot_runtime_mark_record_executed(",
            "aot_runtime_mark_record_executed(runtimeState, record);",
            "TZrBool ZrLibrary_AotRuntime_ResolveGeneratedModuleContext(SZrState *state,",
            "context->recordHandle = record;",
            "context->metadataFunction = metadataFunction;",
            "context->codeRegistration = record->codeRegistration;",
            "record->codeRegistration = descriptor->codeRegistration;",
            "record->codeRegistration->methodInfos",
            "record->codeRegistration->methodInfoCount",
            "descriptor->codeRegistration->methodTokens != descriptor->methodTokens",
            "descriptor->codeRegistration->methodTokenCount != descriptor->methodTokenCount",
            "descriptor->codeRegistration->nativeImportRanges !=",
            "descriptor->nativeImportRanges",
            "ZrLibrary_AotRuntime_ResolveNativeImportContract(",
            "method token table mismatch",
            "record->codeRegistration->functionPointers",
            "record->codeRegistration->functionCount",
            "context->methodInfo =",
            "context->module = record->module;",
            "context->generatedFrameSlotCount = generatedFrameSlotCount;",
            "ZrCore_Module_AttachMetadataRuntime(record.module, record.moduleFunction, record.codeRegistration)",
            "failed to attach AOT metadata runtime for module '%s'",
            "runtimeState->executedVia = aot_runtime_backend_to_executed_via(record->backendKind);",
    };
    static const char *const frameSetupHeaderNeedles[] = {
            "backend_aot_write_c_frame_setup(FILE *file,",
            "const SZrAotExecIrFrameLayout *frameLayout",
            "TZrUInt32 functionIndex",
            "TZrBool includeExportContext",
            "TZrBool includeFrameDescriptor",
            "TZrBool includeGcRootFrame",
    };
    static const char *const frameSetupSourceNeedles[] = {
            "#include \"backend_aot_c_frame_setup.h\"",
            "backend_aot_c_frame_setup_register_frame_bytes(",
            "ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT",
            "TZrBool includeStackFrameSetup",
            "(TZrBool)(includeFrameDescriptor || frameByteSize > 0u || includeGcRootFrame);",
            "if (!includeStackFrameSetup) {\n        return;\n    }\n\n    fprintf(file,",
            "/* zr_aot_generated_frame_setup */",
            "ZrAotGeneratedModuleContext zr_aot_context;",
            "SZrAotGcRootFrame zr_aot_gc_root_frame;",
            "TZrBool zr_aot_has_gc_root_frame = ZR_FALSE;",
            "ZrLibrary_AotRuntime_ResolveGeneratedModuleContext(state, %u, &zr_aot_context)",
            "SZrCallInfo *zr_aot_call_info = state->callInfoList;",
            "SZrFunctionStackAnchor zr_aot_base_anchor;",
            "ZrCore_Function_StackAnchorInit(state, zr_aot_function_base, &zr_aot_base_anchor);",
            "ZrCore_Function_CheckStackAndGc(state, zr_aot_frame_slot_count, zr_aot_function_base + 1);",
            "zr_aot_argument_count =",
            "TZrSize zr_aot_frame_byte_slot_count =",
            "zr_aot_frame_byte_size = (TZrSize)%uu;",
            "(zr_aot_frame_byte_size + sizeof(SZrTypeValue) - 1u) / sizeof(SZrTypeValue)",
            "if (zr_aot_frame_slot_count < zr_aot_frame_byte_slot_count)",
            "ZrCore_Value_ResetAsNull(&zr_aot_slot_base[zr_aot_slot].value);",
            "zr_aot_call_info->functionTop.valuePointer = zr_aot_frame_top;",
            "state->stackTop.valuePointer = zr_aot_frame_top;",
            "/* zr_aot_gc_root_frame_static_map_push */",
            "ZrCore_Gc_AotRootFramePush(state,",
            "&zr_aot_gc_root_frame",
            "zr_aot_slot_base",
            "&zr_aot_gc_root_map_%u",
            "ZrCore_Debug_RunError(state,",
            "generated AOT function has no call frame",
            "frame.function = zr_aot_context.metadataFunction;",
            "if (includeFrameDescriptor) {",
            "if (!includeStackFrameSetup) {",
            "if (frameByteSize > 0u) {",
            "if (includeExportContext) {",
            "frame.module = zr_aot_context.module;",
            "frame.moduleExecuted = zr_aot_context.moduleExecuted;",
            "frame.functionTable = zr_aot_context.functionTable;",
            "frame.functionCount = zr_aot_context.functionCount;",
            "frame.codeRegistration = zr_aot_context.codeRegistration;",
            "frame.functionThunks = zr_aot_context.functionThunks;",
            "frame.functionThunkCount = zr_aot_context.functionThunkCount;",
            "frame.generatedFrameSlotCount = zr_aot_context.generatedFrameSlotCount;",
    };
    static const char *const functionBodyNeedles[] = {
            "#include \"backend_aot_c_frame_setup.h\"",
            "backend_aot_write_c_frame_setup(file,",
            "functionIr != ZR_NULL ? &functionIr->frameLayout : ZR_NULL",
            "entry->flatIndex,",
            "publishExports,",
            "includeFrameDescriptor,",
            "needsGcRootFrame);",
            "TZrBool includeFrameDescriptor",
            "TZrBool needsGcRootFrame",
            "backend_aot_c_method_metadata_count_gc_roots(state, functionIr) > 0u",
            "backend_aot_c_function_body_needs_frame_descriptor(",
            "backend_aot_instruction_reads_bool_value_operand(const SZrAotExecIrFunction *functionIr,",
            "case ZR_INSTRUCTION_ENUM(LOGICAL_NOT):",
            "case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_BOOL):",
            "backend_aot_c_scalar_locals_bool_result_can_skip_value_slot(\n"
            "                                       functionIr,",
            "backend_aot_c_scalar_locals_bool_written_before(\n"
            "                                       functionIr,",
            "backend_aot_stack_copy_destination_has_upcoming_bool_value_operand(\n"
            "                                                 functionIr,",
            "TZrBool needsFrameCleanup",
            "TZrBool needsSkipDropSlot",
            "needsSkipDropSlot = (TZrBool)(",
            "needsFrameCleanup ||",
            "backend_aot_c_value_semir_needs_skip_drop_slot(module, functionIr));",
            "backend_aot_c_frame_cleanup_would_emit_for_function(state, functionIr)",
            "if (needsFrameCleanup) {",
            "if (needsSkipDropSlot) {",
    };
    static const char *const frameCleanupSourceNeedles[] = {
            "ZrCore_Gc_AotRootFramePop(state, &zr_aot_gc_root_frame);",
            "zr_aot_has_gc_root_frame = ZR_FALSE;",
    };
    static const char *const frameDescriptorSourceNeedles[] = {
            "backend_aot_c_frame_descriptor_bool_logical_can_use_local_only(",
            "backend_aot_c_frame_descriptor_conversion_can_use_local_only(",
            "backend_aot_c_frame_descriptor_generic_numeric_binary_can_use_local_only(",
            "backend_aot_c_frame_descriptor_generic_numeric_unary_can_use_local_only(",
            "case ZR_INSTRUCTION_ENUM(LOGICAL_AND):",
            "case ZR_INSTRUCTION_ENUM(LOGICAL_OR):",
            "case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL_BOOL):",
            "case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL_BOOL):",
            "case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_BOOL):",
            "case ZR_INSTRUCTION_ENUM(ADD):",
            "case ZR_INSTRUCTION_ENUM(ADD_STRING):",
            "case ZR_INSTRUCTION_ENUM(DIV):",
            "case ZR_INSTRUCTION_ENUM(MOD):",
            "case ZR_INSTRUCTION_ENUM(ADD_UNSIGNED):",
            "case ZR_INSTRUCTION_ENUM(SUB_UNSIGNED):",
            "case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED):",
            "case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED):",
            "case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED):",
            "case ZR_INSTRUCTION_ENUM(NEG):",
            "case ZR_INSTRUCTION_ENUM(TO_INT):",
            "case ZR_INSTRUCTION_ENUM(TO_INT_UNSIGNED):",
            "case ZR_INSTRUCTION_ENUM(TO_UINT):",
            "backend_aot_c_scalar_locals_bool_constant_can_skip_value_slot(",
            "backend_aot_c_scalar_locals_bool_result_can_skip_value_slot(functionIr, destinationSlot, instructionIndex)",
            "backend_aot_c_scalar_locals_bool_written_before(functionIr, operandA1, instructionIndex)",
            "backend_aot_c_scalar_locals_bool_written_before(functionIr, operandB1, instructionIndex)",
            "ZR_VALUE_IS_TYPE_UNSIGNED_INT(constantValue->type)",
            "backend_aot_c_scalar_locals_u64_constant_can_skip_value_slot(",
            "backend_aot_c_scalar_locals_i64_result_can_skip_value_slot(",
            "backend_aot_c_scalar_locals_u64_written_before(",
            "backend_aot_c_scalar_locals_f64_written_before(",
            "case ZR_INSTRUCTION_ENUM(FUNCTION_RETURN):",
            "backend_aot_c_scalar_locals_can_direct_return_i64_local(\n"
            "                                     functionIr, operandA1, instructionIndex)",
            "backend_aot_c_scalar_locals_can_direct_return_bool_local(\n"
            "                                     functionIr, operandA1, instructionIndex)",
            "backend_aot_c_scalar_locals_can_infer_return_bool_local(\n"
            "                                     functionIr, operandA1, instructionIndex)",
            "backend_aot_c_scalar_locals_can_direct_return_u64_local(\n"
            "                                     functionIr, operandA1, instructionIndex)",
            "backend_aot_c_scalar_locals_can_infer_return_u64_local(\n"
            "                                     functionIr, operandA1, instructionIndex)",
            "backend_aot_c_scalar_locals_can_direct_return_f64_local(\n"
            "                                     functionIr, operandA1, instructionIndex)",
            "backend_aot_c_scalar_locals_can_infer_return_f64_local(\n"
            "                                     functionIr, operandA1, instructionIndex)",
    };
    static const char *const scalarLocalsSourceNeedles[] = {
            "backend_aot_c_scalar_locals_instruction_reads_slot_as_any_local(",
            "backend_aot_c_scalar_locals_instruction_is_i64_local_consumer(opcode) &&",
            "backend_aot_c_scalar_locals_i64_consumer_reads_slot(",
            "backend_aot_c_scalar_locals_instruction_is_u64_local_consumer(opcode) &&",
            "backend_aot_c_scalar_locals_u64_consumer_reads_slot(functionIr, instruction, slot)",
            "backend_aot_c_scalar_locals_signed_consumer_reads_slot(instruction, slot)",
            "opcode == ZR_INSTRUCTION_ENUM(ADD_UNSIGNED)",
            "opcode == ZR_INSTRUCTION_ENUM(ADD_UNSIGNED_PLAIN_DEST)",
            "opcode == ZR_INSTRUCTION_ENUM(SUB_UNSIGNED)",
            "opcode == ZR_INSTRUCTION_ENUM(SUB_UNSIGNED_PLAIN_DEST)",
            "opcode == ZR_INSTRUCTION_ENUM(MUL_UNSIGNED)",
            "opcode == ZR_INSTRUCTION_ENUM(MUL_UNSIGNED_PLAIN_DEST)",
            "opcode == ZR_INSTRUCTION_ENUM(DIV_UNSIGNED)",
            "opcode == ZR_INSTRUCTION_ENUM(MOD_UNSIGNED)",
            "case ZR_INSTRUCTION_OP_ADD_UNSIGNED:",
            "case ZR_INSTRUCTION_OP_SUB_UNSIGNED:",
            "case ZR_INSTRUCTION_OP_MUL_UNSIGNED:",
            "case ZR_INSTRUCTION_OP_DIV_UNSIGNED:",
            "case ZR_INSTRUCTION_OP_MOD_UNSIGNED:",
            "case ZR_INSTRUCTION_OP_ADD_SIGNED_CONST:",
            "case ZR_INSTRUCTION_OP_SUB_SIGNED_CONST:",
            "case ZR_INSTRUCTION_OP_MUL_SIGNED_CONST:",
            "case ZR_INSTRUCTION_OP_DIV_SIGNED_CONST:",
            "case ZR_INSTRUCTION_OP_MOD_SIGNED_CONST:",
            "case ZR_INSTRUCTION_OP_ADD_SIGNED_LOAD_CONST:",
            "case ZR_INSTRUCTION_OP_SUB_SIGNED_LOAD_CONST:",
            "case ZR_INSTRUCTION_OP_MUL_SIGNED_LOAD_CONST:",
            "case ZR_INSTRUCTION_OP_DIV_SIGNED_LOAD_CONST:",
            "case ZR_INSTRUCTION_OP_MOD_SIGNED_LOAD_CONST:",
            "case ZR_INSTRUCTION_OP_ADD_SIGNED_LOAD_STACK_CONST:",
            "case ZR_INSTRUCTION_OP_SUB_SIGNED_LOAD_STACK_CONST:",
            "case ZR_INSTRUCTION_OP_MUL_SIGNED_LOAD_STACK_CONST:",
            "case ZR_INSTRUCTION_OP_DIV_SIGNED_LOAD_STACK_CONST:",
            "case ZR_INSTRUCTION_OP_MOD_SIGNED_LOAD_STACK_CONST:",
            "case ZR_INSTRUCTION_OP_ADD_SIGNED_LOAD_STACK_LOAD_CONST:",
            "backend_aot_c_scalar_locals_slot_has_later_scalar_consumer(",
            "backend_aot_c_scalar_locals_slot_is_later_materialized_signed_constant(",
            "return (TZrBool)(leftSlot == slot);",
            "return (TZrBool)(instruction->instruction.operand.operand0[0] == slot);",
            "backend_aot_c_scalar_locals_instruction_mentions_slot_as_source_operand(",
            "backend_aot_c_scalar_locals_instruction_is_signed_local_consumer(opcode)",
    };
    static const char *const scalarStackCopySourceNeedles[] = {
            "TZrBool backend_aot_c_scalar_stack_copy_can_use_local_only(",
            "TZrBool backend_aot_try_write_c_scalar_stack_copy(",
            "sourceStaticCType = backend_aot_c_scalar_stack_copy_static_type_for_slot(functionIr->function, sourceSlot);",
            "sourceLocalStaticCType = backend_aot_c_scalar_stack_copy_static_type_from_locals(functionIr, sourceSlot);",
            "if (sourceStaticCType != ZR_STATIC_C_TYPE_DYNAMIC &&",
            "if (sourceLocalStaticCType != ZR_STATIC_C_TYPE_DYNAMIC &&",
            "backend_aot_c_scalar_stack_copy_source_local_is_available(functionIr,",
            "backend_aot_c_scalar_stack_copy_destination_local_is_available(",
            "functionIr, destinationSlot, sourceLocalStaticCType)",
    };
    static const char *const emitterSourceNeedles[] = {
            "backend_aot_write_c_signature_type(FILE *file,",
            "#include \\\"zr_vm_core/gc.h\\\"",
            "#include \\\"zr_vm_core/value.h\\\"",
            "const SZrFunctionTypedTypeRef *typeRef",
            "#include \"backend_aot_c_scalar_locals.h\"",
            "backend_aot_c_signature_try_infer_i64_return(",
            "backend_aot_c_signature_try_infer_bool_return(",
            "backend_aot_c_signature_try_infer_u64_return(",
            "backend_aot_c_signature_try_infer_f64_return(",
            "backend_aot_c_signature_try_infer_scalar_return(",
            "backend_aot_c_signature_try_infer_static_return(",
            "backend_aot_c_signature_init_scalar_return_type(",
            "backend_aot_c_signature_init_i64_return_type(",
            "backend_aot_c_signature_init_bool_return_type(",
            "backend_aot_c_signature_init_u64_return_type(",
            "backend_aot_c_signature_init_f64_return_type(",
            "ZR_INSTRUCTION_ENUM(FUNCTION_RETURN)",
            "backend_aot_c_scalar_locals_can_direct_return_i64_local",
            "backend_aot_c_scalar_locals_can_infer_return_bool_local",
            "backend_aot_c_scalar_locals_can_infer_return_u64_local",
            "backend_aot_c_scalar_locals_can_infer_return_f64_local",
            "ZR_VALUE_TYPE_INT64",
            "ZR_VALUE_TYPE_BOOL",
            "ZR_VALUE_TYPE_UINT64",
            "ZR_VALUE_TYPE_DOUBLE",
            "ZR_STATIC_C_TYPE_I64",
            "ZR_STATIC_C_TYPE_BOOL",
            "ZR_STATIC_C_TYPE_U64",
            "ZR_STATIC_C_TYPE_F64",
            "backend_aot_write_c_signature(FILE *file,",
            "const SZrAotExecIrFunction *functionIr",
            "#include \"backend_aot_c_reflection_invokers.h\"",
            "backend_aot_write_c_reflection_invokers(FILE *file, const SZrAotFunctionTable *table)",
            "static TZrBool backend_aot_c_method_metadata_has_i64_no_arg_reflection_case(",
            "backend_aot_c_can_emit_typed_i64_no_arg_thunk(entry->function)",
            "static void backend_aot_write_c_reflection_i64_no_arg_cases(",
            "zr_aot_try_invoke_i64_no_arg",
            "static TZrBool backend_aot_c_method_metadata_has_i64_one_arg_reflection_case(",
            "backend_aot_c_can_emit_typed_i64_one_arg_thunk(entry->function)",
            "static void backend_aot_write_c_reflection_i64_one_arg_cases(",
            "zr_aot_try_invoke_i64_one_arg",
            "static TZrBool backend_aot_c_method_metadata_has_i64_two_arg_reflection_case(",
            "backend_aot_c_can_emit_typed_i64_two_arg_thunk(entry->function)",
            "backend_aot_c_can_emit_typed_i64_two_arg_state_free_thunk(entry->function)",
            "static void backend_aot_write_c_reflection_i64_two_arg_cases(",
            "zr_aot_try_invoke_i64_two_arg",
            "#include \"backend_aot_c_reflection_numeric_three_arg_invokers.h\"",
            "static TZrBool backend_aot_c_method_metadata_has_i64_three_arg_reflection_case(",
            "backend_aot_c_can_emit_typed_i64_three_arg_thunk(entry->function)",
            "backend_aot_c_can_emit_typed_i64_three_arg_state_free_thunk(entry->function)",
            "static void backend_aot_write_c_reflection_i64_three_arg_cases(",
            "zr_aot_try_invoke_i64_three_arg",
            "#include \"backend_aot_c_typed_u64_three_arg_thunks.h\"",
            "static TZrBool backend_aot_c_method_metadata_has_u64_three_arg_reflection_case(",
            "backend_aot_c_can_emit_typed_u64_three_arg_thunk(entry->function)",
            "backend_aot_c_can_emit_typed_u64_three_arg_state_free_thunk(entry->function)",
            "static void backend_aot_write_c_reflection_u64_three_arg_cases(",
            "zr_aot_try_invoke_u64_three_arg",
            "static TZrBool backend_aot_c_method_metadata_has_f64_three_arg_reflection_case(",
            "backend_aot_c_can_emit_typed_f64_three_arg_thunk(entry->function)",
            "backend_aot_c_can_emit_typed_f64_three_arg_state_free_thunk(entry->function)",
            "static void backend_aot_write_c_reflection_f64_three_arg_cases(",
            "zr_aot_try_invoke_f64_three_arg",
            "#include \"backend_aot_c_reflection_bool_three_arg_invokers.h\"",
            "#include \"backend_aot_c_typed_bool_three_arg_thunks.h\"",
            "static TZrBool backend_aot_c_method_metadata_has_bool_three_arg_reflection_case(",
            "backend_aot_c_can_emit_typed_bool_three_arg_thunk(entry->function)",
            "static void backend_aot_write_c_reflection_bool_three_arg_cases(",
            "zr_aot_try_invoke_bool_three_arg",
            "#include \"backend_aot_c_typed_bool_thunks.h\"",
            "static TZrBool backend_aot_c_method_metadata_has_bool_no_arg_reflection_case(",
            "backend_aot_c_can_emit_typed_bool_no_arg_thunk(entry->function)",
            "static void backend_aot_write_c_reflection_bool_no_arg_cases(",
            "zr_aot_try_invoke_bool_no_arg",
            "static TZrBool backend_aot_c_method_metadata_has_bool_one_arg_reflection_case(",
            "backend_aot_c_can_emit_typed_bool_one_arg_thunk(entry->function)",
            "static void backend_aot_write_c_reflection_bool_one_arg_cases(",
            "zr_aot_try_invoke_bool_one_arg",
            "static TZrBool backend_aot_c_method_metadata_has_bool_two_arg_reflection_case(",
            "backend_aot_c_can_emit_typed_bool_two_arg_thunk(entry->function)",
            "static void backend_aot_write_c_reflection_bool_two_arg_cases(",
            "zr_aot_try_invoke_bool_two_arg",
            "static TZrBool backend_aot_c_method_metadata_has_bool_i64_two_arg_reflection_case(",
            "backend_aot_c_can_emit_typed_bool_i64_two_arg_thunk(entry->function)",
            "static void backend_aot_write_c_reflection_bool_i64_two_arg_cases(",
            "zr_aot_try_invoke_bool_i64_two_arg",
            "static TZrBool backend_aot_c_method_metadata_has_bool_u64_two_arg_reflection_case(",
            "backend_aot_c_can_emit_typed_bool_u64_two_arg_thunk(entry->function)",
            "static void backend_aot_write_c_reflection_bool_u64_two_arg_cases(",
            "zr_aot_try_invoke_bool_u64_two_arg",
            "static TZrBool backend_aot_c_method_metadata_has_bool_f64_two_arg_reflection_case(",
            "backend_aot_c_can_emit_typed_bool_f64_two_arg_thunk(entry->function)",
            "static void backend_aot_write_c_reflection_bool_f64_two_arg_cases(",
            "zr_aot_try_invoke_bool_f64_two_arg",
            "#include \"backend_aot_c_typed_f64_thunks.h\"",
            "static TZrBool backend_aot_c_method_metadata_has_f64_no_arg_reflection_case(",
            "backend_aot_c_can_emit_typed_f64_no_arg_thunk(entry->function)",
            "static void backend_aot_write_c_reflection_f64_no_arg_cases(",
            "zr_aot_try_invoke_f64_no_arg",
            "static TZrBool backend_aot_c_method_metadata_has_f64_one_arg_reflection_case(",
            "backend_aot_c_can_emit_typed_f64_one_arg_thunk(entry->function)",
            "static void backend_aot_write_c_reflection_f64_one_arg_cases(",
            "zr_aot_try_invoke_f64_one_arg",
            "static TZrBool backend_aot_c_method_metadata_has_f64_two_arg_reflection_case(",
            "backend_aot_c_can_emit_typed_f64_two_arg_thunk(entry->function)",
            "backend_aot_c_can_emit_typed_f64_two_arg_state_free_thunk(entry->function)",
            "static void backend_aot_write_c_reflection_f64_two_arg_cases(",
            "zr_aot_try_invoke_f64_two_arg",
            "#include \"backend_aot_c_typed_u64_thunks.h\"",
            "static TZrBool backend_aot_c_method_metadata_has_u64_no_arg_reflection_case(",
            "backend_aot_c_can_emit_typed_u64_no_arg_thunk(entry->function)",
            "static void backend_aot_write_c_reflection_u64_no_arg_cases(",
            "zr_aot_try_invoke_u64_no_arg",
            "static TZrBool backend_aot_c_method_metadata_has_u64_one_arg_reflection_case(",
            "backend_aot_c_can_emit_typed_u64_one_arg_thunk(entry->function)",
            "static void backend_aot_write_c_reflection_u64_one_arg_cases(",
            "zr_aot_try_invoke_u64_one_arg",
            "static TZrBool backend_aot_c_method_metadata_has_u64_two_arg_reflection_case(",
            "backend_aot_c_can_emit_typed_u64_two_arg_thunk(entry->function)",
            "backend_aot_c_can_emit_typed_u64_two_arg_state_free_thunk(entry->function)",
            "static void backend_aot_write_c_reflection_u64_two_arg_cases(",
            "zr_aot_try_invoke_u64_two_arg",
            "method->signature->returnType->baseType != (TZrUInt16)ZR_VALUE_TYPE_BOOL",
            "method->signature->returnType->baseType != (TZrUInt16)ZR_VALUE_TYPE_UINT64",
            "method->signature->returnType->baseType != (TZrUInt16)ZR_VALUE_TYPE_DOUBLE",
            "method->signature->parameterCount != 0u",
            "method->signature->parameterCount != 1u",
            "method->signature->parameterCount != 2u",
            "method->signature->parameterCount != 3u",
            "method->signature->parameterTypes == ZR_NULL",
            "method->signature->parameterTypes[0].baseType != (TZrUInt16)ZR_VALUE_TYPE_INT64",
            "method->signature->parameterTypes[1].baseType != (TZrUInt16)ZR_VALUE_TYPE_INT64",
            "method->signature->parameterTypes[2].baseType != (TZrUInt16)ZR_VALUE_TYPE_INT64",
            "method->signature->parameterTypes[0].baseType != (TZrUInt16)ZR_VALUE_TYPE_UINT64",
            "method->signature->parameterTypes[1].baseType != (TZrUInt16)ZR_VALUE_TYPE_UINT64",
            "method->signature->parameterTypes[2].baseType != (TZrUInt16)ZR_VALUE_TYPE_UINT64",
            "method->signature->parameterTypes[0].baseType != (TZrUInt16)ZR_VALUE_TYPE_BOOL",
            "method->signature->parameterTypes[1].baseType != (TZrUInt16)ZR_VALUE_TYPE_BOOL",
            "method->signature->parameterTypes[2].baseType != (TZrUInt16)ZR_VALUE_TYPE_BOOL",
            "method->signature->parameterTypes[0].baseType != (TZrUInt16)ZR_VALUE_TYPE_DOUBLE",
            "method->signature->parameterTypes[1].baseType != (TZrUInt16)ZR_VALUE_TYPE_DOUBLE",
            "args == ZR_NULL",
            "args[0].type != (TZrUInt16)ZR_VALUE_TYPE_INT64",
            "args[1].type != (TZrUInt16)ZR_VALUE_TYPE_INT64",
            "args[2].type != (TZrUInt16)ZR_VALUE_TYPE_INT64",
            "args[0].type != (TZrUInt16)ZR_VALUE_TYPE_UINT64",
            "args[1].type != (TZrUInt16)ZR_VALUE_TYPE_UINT64",
            "args[2].type != (TZrUInt16)ZR_VALUE_TYPE_UINT64",
            "args[0].type != (TZrUInt16)ZR_VALUE_TYPE_BOOL",
            "args[1].type != (TZrUInt16)ZR_VALUE_TYPE_BOOL",
            "args[2].type != (TZrUInt16)ZR_VALUE_TYPE_BOOL",
            "args[0].type != (TZrUInt16)ZR_VALUE_TYPE_DOUBLE",
            "args[1].type != (TZrUInt16)ZR_VALUE_TYPE_DOUBLE",
            "args[2].type != (TZrUInt16)ZR_VALUE_TYPE_DOUBLE",
            "switch (method->functionIndex)",
            "zr_aot_return_value = zr_aot_typed_i64_fn_%u();",
            "TZrInt64 zr_aot_arg0 = args[0].value.nativeObject.nativeInt64;",
            "TZrInt64 zr_aot_arg1 = args[1].value.nativeObject.nativeInt64;",
            "TZrInt64 zr_aot_arg2 = args[2].value.nativeObject.nativeInt64;",
            "zr_aot_return_value = zr_aot_typed_i64_fn_%u(zr_aot_arg0);",
            "zr_aot_return_value = zr_aot_typed_i64_fn_%u(zr_aot_arg0, zr_aot_arg1);",
            "zr_aot_return_value = zr_aot_typed_i64_fn_%u(state, zr_aot_arg0, zr_aot_arg1);",
            "zr_aot_return_value = zr_aot_typed_i64_fn_%u(zr_aot_arg0, zr_aot_arg1, zr_aot_arg2);",
            "zr_aot_return_value = zr_aot_typed_i64_fn_%u(state, zr_aot_arg0, zr_aot_arg1, zr_aot_arg2);",
            "ZrCore_Value_InitAsInt(state, outReturn, zr_aot_return_value);",
            "TZrBool zr_aot_return_value = zr_aot_typed_bool_fn_%u();",
            "TZrBool zr_aot_arg0 = args[0].value.nativeObject.nativeBool;",
            "TZrBool zr_aot_arg1 = args[1].value.nativeObject.nativeBool;",
            "TZrBool zr_aot_arg2 = args[2].value.nativeObject.nativeBool;",
            "zr_aot_return_value = zr_aot_typed_bool_fn_%u(zr_aot_arg0);",
            "zr_aot_return_value = zr_aot_typed_bool_fn_%u(zr_aot_arg0, zr_aot_arg1);",
            "zr_aot_return_value = zr_aot_typed_bool_fn_%u(zr_aot_arg0, zr_aot_arg1, zr_aot_arg2);",
            "ZrCore_Value_InitAsBool(state, outReturn, zr_aot_return_value);",
            "TZrUInt64 zr_aot_return_value = zr_aot_typed_u64_fn_%u();",
            "TZrUInt64 zr_aot_arg0 = args[0].value.nativeObject.nativeUInt64;",
            "TZrUInt64 zr_aot_arg1 = args[1].value.nativeObject.nativeUInt64;",
            "TZrUInt64 zr_aot_arg2 = args[2].value.nativeObject.nativeUInt64;",
            "zr_aot_return_value = zr_aot_typed_u64_fn_%u(zr_aot_arg0);",
            "zr_aot_return_value = zr_aot_typed_u64_fn_%u(zr_aot_arg0, zr_aot_arg1);",
            "zr_aot_return_value = zr_aot_typed_u64_fn_%u(state, zr_aot_arg0, zr_aot_arg1);",
            "zr_aot_return_value = zr_aot_typed_u64_fn_%u(zr_aot_arg0, zr_aot_arg1, zr_aot_arg2);",
            "zr_aot_return_value = zr_aot_typed_u64_fn_%u(state, zr_aot_arg0, zr_aot_arg1, zr_aot_arg2);",
            "ZrCore_Value_InitAsUInt(state, outReturn, zr_aot_return_value);",
            "TZrFloat64 zr_aot_return_value = zr_aot_typed_f64_fn_%u();",
            "TZrFloat64 zr_aot_arg0 = args[0].value.nativeObject.nativeDouble;",
            "TZrFloat64 zr_aot_arg1 = args[1].value.nativeObject.nativeDouble;",
            "TZrFloat64 zr_aot_arg2 = args[2].value.nativeObject.nativeDouble;",
            "zr_aot_return_value = zr_aot_typed_f64_fn_%u(zr_aot_arg0);",
            "zr_aot_return_value = zr_aot_typed_f64_fn_%u(zr_aot_arg0, zr_aot_arg1);",
            "zr_aot_return_value = zr_aot_typed_f64_fn_%u(state, zr_aot_arg0, zr_aot_arg1);",
            "zr_aot_return_value = zr_aot_typed_f64_fn_%u(zr_aot_arg0, zr_aot_arg1, zr_aot_arg2);",
            "zr_aot_return_value = zr_aot_typed_f64_fn_%u(state, zr_aot_arg0, zr_aot_arg1, zr_aot_arg2);",
            "ZrCore_Value_InitAsFloat(state, outReturn, zr_aot_return_value);",
            "static void zr_aot_invoker_entry_thunk(struct SZrState *state,",
            "FZrAotEntryThunk target,",
            "const SZrAotMethodInfo *method,",
            "SZrTypeValue *self,",
            "SZrTypeValue *args,",
            "SZrTypeValue *outReturn)",
            "if (zr_aot_try_invoke_i64_no_arg(state, method, outReturn))",
            "if (zr_aot_try_invoke_i64_one_arg(state, method, args, outReturn))",
            "if (zr_aot_try_invoke_i64_two_arg(state, method, args, outReturn))",
            "if (zr_aot_try_invoke_i64_three_arg(state, method, args, outReturn))",
            "if (zr_aot_try_invoke_bool_no_arg(state, method, outReturn))",
            "if (zr_aot_try_invoke_bool_one_arg(state, method, args, outReturn))",
            "if (zr_aot_try_invoke_bool_two_arg(state, method, args, outReturn))",
            "if (zr_aot_try_invoke_bool_three_arg(state, method, args, outReturn))",
            "if (zr_aot_try_invoke_bool_i64_two_arg(state, method, args, outReturn))",
            "if (zr_aot_try_invoke_bool_u64_two_arg(state, method, args, outReturn))",
            "if (zr_aot_try_invoke_bool_f64_two_arg(state, method, args, outReturn))",
            "if (zr_aot_try_invoke_u64_no_arg(state, method, outReturn))",
            "if (zr_aot_try_invoke_u64_one_arg(state, method, args, outReturn))",
            "if (zr_aot_try_invoke_u64_two_arg(state, method, args, outReturn))",
            "if (zr_aot_try_invoke_u64_three_arg(state, method, args, outReturn))",
            "if (zr_aot_try_invoke_f64_no_arg(state, method, outReturn))",
            "if (zr_aot_try_invoke_f64_one_arg(state, method, args, outReturn))",
            "if (zr_aot_try_invoke_f64_two_arg(state, method, args, outReturn))",
            "if (zr_aot_try_invoke_f64_three_arg(state, method, args, outReturn))",
            "if (target != ZR_NULL) {",
            "(void)target(state);",
            "static const FZrAotReflectionInvoker zr_aot_reflection_invokers[] = {",
            "zr_aot_invoker_entry_thunk,",
            "backend_aot_c_method_metadata_count_gc_roots(",
            "backend_aot_write_c_gc_root_map(",
            "ZrCore_Function_ResolvePrototypeFrameTypeLayout",
            "ZR_AOT_GC_ROOT_LOCATION_FRAME_BYTE_OFFSET",
            "static const SZrAotSignatureType zr_aot_signature_%u_types[] = {",
            "static const SZrAotSignature zr_aot_signature_%u = {",
            "unsigned long long backend_aot_write_c_method_infos(FILE *file,\n                                                    SZrState *state,",
            "backend_aot_c_method_info_register_frame_bytes(",
            "backend_aot_c_generic_sharing_dictionary_id_for_function(table, entry->function);",
            "ZR_FUNCTION_FRAME_SLOT_KIND_INLINE_STRUCT",
            "const SZrAotExecIrFunction *functionIr =",
            "backend_aot_exec_ir_find_function(module, entry->flatIndex);",
            "backend_aot_write_c_gc_root_map(file, state, entry->flatIndex, functionIr);",
            "backend_aot_write_c_signature(file, entry->flatIndex, entry->function, functionIr);",
            "static const SZrAotMethodInfo zr_aot_method_info_%u = {",
            ".functionIndex = %uu,",
            ".metadataFunction = ZR_NULL,",
            ".registerFrameBytes = %uu,",
            ".gcRootMap = &zr_aot_gc_root_map_%u,",
            ".gcRootMap = ZR_NULL,",
            ".signature = &zr_aot_signature_%u,",
            ".genericDictionary = &zr_aot_generic_dict_%u,",
            ".genericDictionary = ZR_NULL,",
            ".invoker = zr_aot_invoker_entry_thunk,",
            ".observationPolicy = 0u,",
            "backend_aot_c_reflection_metadata_level_name(reflectionMetadataLevel)",
            "ZR_AOT_REFLECTION_METADATA_RUNTIME_MAPPING",
            "ZR_AOT_REFLECTION_METADATA_NONE",
            "backend_aot_write_c_generic_dictionary_macros(file);",
            "backend_aot_write_c_generic_sharing_entries(file, &functionTable, stripGeneratedSymbols);",
            "backend_aot_write_c_reflection_invokers(file, &functionTable);",
            "backend_aot_write_c_method_info_table(FILE *file,",
            "static const SZrAotMethodInfo *const zr_aot_method_infos[] = {",
            "&zr_aot_method_info_%u,",
            "backend_aot_write_c_method_token_table(FILE *file,",
            "backend_aot_c_method_token_for_function(",
            "static const TZrUInt32 zr_aot_method_tokens[] = {",
            "0x%08xu,",
            "zr_aot_method_infos,",
            "zr_aot_method_tokens,",
            "static const SZrAotCodeRegistration zr_aot_code_registration = {",
            ".functionCount = %u,",
            ".functionPointers = zr_aot_function_thunks,",
            ".methodInfos = zr_aot_method_infos,",
            ".methodInfoCount = %u,",
            ".methodTokens = zr_aot_method_tokens,",
            ".methodTokenCount = %u,",
            ".invokers = zr_aot_reflection_invokers,",
            ".invokerCount = 1u,",
            ".typeLayouts = %s,",
            ".typeLayoutCount = %u,",
            ".typeLayoutTokens = %s,",
            ".typeLayoutTokenCount = %u,",
            ".gcDescriptors = %s,",
            ".gcDescriptorCount = %u,",
            ".nativeImportContracts = %s,",
            ".nativeImportContractCount = %uu,",
            ".codeRegistration = &zr_aot_code_registration,",
            "backend_aot_c_find_function_entry_by_flat_index(",
            "TZrUInt32 functionIndexSpace",
            "backend_aot_function_table_index_space(&functionTable);",
            "backend_aot_write_c_function_table(file, &functionTable, functionIndexSpace);",
            "backend_aot_write_c_method_info_table(file, &functionTable, functionIndexSpace);",
            "backend_aot_write_c_method_token_table(file,\n                                           &functionTable,\n                                           &embeddedZrpMetadata,\n                                           functionIndexSpace);",
            "if (entry != ZR_NULL) {\n            fprintf(file, \"    zr_aot_fn_%u,\\n\", (unsigned)entry->flatIndex);",
            "if (entry != ZR_NULL) {\n            fprintf(file, \"    &zr_aot_method_info_%u,\\n\", (unsigned)entry->flatIndex);",
            "fprintf(file, \"    ZR_NULL,\\n\");",
            "functionIndexSpace",
            "backend_aot_write_c_method_infos(file, state, &functionTable, &module, reflectionMetadataLevel);",
    };
    static const char *const forbiddenFunctionBodyNeedles[] = {
            "ZrLibrary_AotRuntime_BeginGeneratedFunction(state, %u, &frame)",
    };
    static const char *const forbiddenRuntimeNeedles[] = {
            "ZrLibrary_AotRuntime_ReportGeneratedContextError",
            "ZrLibrary_AotRuntime_GetGeneratedContext",
            "ZrLibrary_AotRuntime_MarkGeneratedFunctionExecuted",
            "ZrAotGeneratedContext",
    };
    static const char *const forbiddenFrameSetupNeedles[] = {
            "ZrLibrary_AotRuntime_BeginGeneratedFunction",
            "ZrLibrary_AotRuntime_ReportGeneratedContextError",
            "ZrLibrary_AotRuntime_GetGeneratedContext",
            "ZrAotGeneratedContext",
            "ZrLibrary_AotRuntime_GetObservationPolicy",
            "ZrLibrary_AotRuntime_DefaultObservationMask()",
            "frame.recordHandle = zr_aot_context.recordHandle;",
            "frame.functionIndex = zr_aot_context.resolvedFunctionIndex;",
            "frame.currentInstructionIndex = 0;",
            "frame.lastObservedInstructionIndex = UINT32_MAX;",
            "frame.lastObservedLine = ZR_RUNTIME_DEBUG_HOOK_LINE_NONE;",
            "frame.observationMask = state->hasAotObservationPolicyOverride",
            "state->aotObservationMask",
            "ZR_AOT_GENERATED_STEP_FLAG_MAY_THROW",
            "ZR_AOT_GENERATED_STEP_FLAG_CONTROL_FLOW",
            "ZR_AOT_GENERATED_STEP_FLAG_CALL",
            "ZR_AOT_GENERATED_STEP_FLAG_RETURN",
            "frame.publishAllInstructions = state->hasAotObservationPolicyOverride",
            "state->aotPublishAllInstructions",
            "state->debugHookSignal",
            "ZR_DEBUG_HOOK_MASK_LINE",
            "zr_aot_context.methodInfo != ZR_NULL",
            "zr_aot_context.methodInfo->gcRootMap",
    };
    const char *frameDescriptorStart;
    const char *functionTableAssignment;
    const char *functionCountAssignment;
    const char *functionThunkAssignment;
    const char *functionThunkCountAssignment;
    const char *exportContextStart;
    const char *moduleAssignment;
    const char *codeRegistrationAssignment;
    char *runtimeHeaderText = read_repo_text_file_owned("zr_vm_library/include/zr_vm_library/aot_runtime.h");
    char *runtimeSourceText = read_repo_text_file_owned("zr_vm_library/src/zr_vm_library/aot_runtime.c");
    char *abiHeaderText = read_repo_text_file_owned("zr_vm_common/include/zr_vm_common/zr_aot_abi.h");
    char *stateHeaderText = read_repo_text_file_owned("zr_vm_core/include/zr_vm_core/state.h");
    char *gcHeaderText = read_repo_text_file_owned("zr_vm_core/include/zr_vm_core/gc.h");
    char *frameSetupHeaderText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_frame_setup.h");
    char *frameSetupSourceText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_frame_setup.c");
    char *frameCleanupSourceText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_frame_cleanup.c");
    char *functionBodyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c");
    char *frameDescriptorSourceText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_frame_descriptor.c");
    char *scalarLocalsText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_locals.c");
    char *scalarStackCopyText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_stack_copy.c");
    char *emitterSourceText =
            read_repo_text_file_owned("zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.c");
    char *methodMetadataSourceText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_method_metadata.c");
    char *reflectionInvokersSourceText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_reflection_invokers.c");
    char *reflectionBoolNumericInvokersSourceText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/"
            "backend_aot_c_reflection_bool_numeric_invokers.c");
    char *reflectionNumericThreeArgInvokersSourceText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/"
            "backend_aot_c_reflection_numeric_three_arg_invokers.c");
    char *reflectionBoolThreeArgInvokersSourceText = read_repo_text_file_owned(
            "zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/"
            "backend_aot_c_reflection_bool_three_arg_invokers.c");

    TEST_ASSERT_NOT_NULL(runtimeHeaderText);
    TEST_ASSERT_NOT_NULL(runtimeSourceText);
    TEST_ASSERT_NOT_NULL(abiHeaderText);
    TEST_ASSERT_NOT_NULL(stateHeaderText);
    TEST_ASSERT_NOT_NULL(gcHeaderText);
    TEST_ASSERT_NOT_NULL(frameSetupHeaderText);
    TEST_ASSERT_NOT_NULL(frameSetupSourceText);
    TEST_ASSERT_NOT_NULL(frameCleanupSourceText);
    TEST_ASSERT_NOT_NULL(functionBodyText);
    TEST_ASSERT_NOT_NULL(frameDescriptorSourceText);
    TEST_ASSERT_NOT_NULL(scalarLocalsText);
    TEST_ASSERT_NOT_NULL(scalarStackCopyText);
    TEST_ASSERT_NOT_NULL(emitterSourceText);
    TEST_ASSERT_NOT_NULL(methodMetadataSourceText);
    TEST_ASSERT_NOT_NULL(reflectionInvokersSourceText);
    TEST_ASSERT_NOT_NULL(reflectionBoolNumericInvokersSourceText);
    TEST_ASSERT_NOT_NULL(reflectionNumericThreeArgInvokersSourceText);
    TEST_ASSERT_NOT_NULL(reflectionBoolThreeArgInvokersSourceText);

    frameDescriptorStart = strstr(frameSetupSourceText, "if (includeFrameDescriptor) {");
    TEST_ASSERT_NOT_NULL(frameDescriptorStart);
    functionTableAssignment = strstr(
            frameDescriptorStart, "frame.functionTable = zr_aot_context.functionTable;");
    functionCountAssignment = strstr(
            frameDescriptorStart, "frame.functionCount = zr_aot_context.functionCount;");
    functionThunkAssignment = strstr(
            frameDescriptorStart, "frame.functionThunks = zr_aot_context.functionThunks;");
    functionThunkCountAssignment = strstr(
            frameDescriptorStart,
            "frame.functionThunkCount = zr_aot_context.functionThunkCount;");
    exportContextStart = strstr(frameDescriptorStart, "if (includeExportContext) {");
    TEST_ASSERT_NOT_NULL(exportContextStart);
    moduleAssignment = strstr(exportContextStart, "frame.module = zr_aot_context.module;");
    codeRegistrationAssignment = strstr(
            exportContextStart, "frame.codeRegistration = zr_aot_context.codeRegistration;");
    TEST_ASSERT_NOT_NULL(functionTableAssignment);
    TEST_ASSERT_NOT_NULL(functionCountAssignment);
    TEST_ASSERT_NOT_NULL(functionThunkAssignment);
    TEST_ASSERT_NOT_NULL(functionThunkCountAssignment);
    TEST_ASSERT_NOT_NULL(moduleAssignment);
    TEST_ASSERT_NOT_NULL(codeRegistrationAssignment);
    TEST_ASSERT_TRUE(functionTableAssignment < exportContextStart);
    TEST_ASSERT_TRUE(functionCountAssignment < exportContextStart);
    TEST_ASSERT_TRUE(functionThunkAssignment < exportContextStart);
    TEST_ASSERT_TRUE(functionThunkCountAssignment < exportContextStart);
    TEST_ASSERT_TRUE(exportContextStart < moduleAssignment);
    TEST_ASSERT_TRUE(exportContextStart < codeRegistrationAssignment);

    assert_text_contains_all(runtimeHeaderText, runtimeHeaderNeedles, ARRAY_COUNT(runtimeHeaderNeedles));
    assert_text_contains_all(runtimeSourceText, runtimeSourceNeedles, ARRAY_COUNT(runtimeSourceNeedles));
    assert_text_contains_all(abiHeaderText, abiHeaderNeedles, ARRAY_COUNT(abiHeaderNeedles));
    assert_text_contains_all(stateHeaderText, stateHeaderNeedles, ARRAY_COUNT(stateHeaderNeedles));
    assert_text_contains_all(gcHeaderText, gcHeaderNeedles, ARRAY_COUNT(gcHeaderNeedles));
    assert_text_contains_all(frameSetupHeaderText, frameSetupHeaderNeedles, ARRAY_COUNT(frameSetupHeaderNeedles));
    assert_text_contains_all(frameSetupSourceText, frameSetupSourceNeedles, ARRAY_COUNT(frameSetupSourceNeedles));
    assert_text_contains_all(frameCleanupSourceText,
                             frameCleanupSourceNeedles,
                             ARRAY_COUNT(frameCleanupSourceNeedles));
    assert_text_contains_all(functionBodyText, functionBodyNeedles, ARRAY_COUNT(functionBodyNeedles));
    assert_text_contains_all(frameDescriptorSourceText,
                             frameDescriptorSourceNeedles,
                             ARRAY_COUNT(frameDescriptorSourceNeedles));
    assert_text_contains_all(scalarLocalsText, scalarLocalsSourceNeedles, ARRAY_COUNT(scalarLocalsSourceNeedles));
    assert_text_contains_all(scalarStackCopyText,
                             scalarStackCopySourceNeedles,
                             ARRAY_COUNT(scalarStackCopySourceNeedles));
    assert_text_contains_all_in_any6(emitterSourceText,
                                     methodMetadataSourceText,
                                     reflectionInvokersSourceText,
                                     reflectionBoolNumericInvokersSourceText,
                                     reflectionNumericThreeArgInvokersSourceText,
                                     reflectionBoolThreeArgInvokersSourceText,
                                     emitterSourceNeedles,
                                     ARRAY_COUNT(emitterSourceNeedles));
    assert_text_contains_none(runtimeHeaderText, forbiddenRuntimeNeedles, ARRAY_COUNT(forbiddenRuntimeNeedles));
    assert_text_contains_none(runtimeSourceText, forbiddenRuntimeNeedles, ARRAY_COUNT(forbiddenRuntimeNeedles));
    assert_text_contains_none(functionBodyText, forbiddenFunctionBodyNeedles, ARRAY_COUNT(forbiddenFunctionBodyNeedles));
    assert_text_contains_none(frameSetupSourceText, forbiddenFrameSetupNeedles, ARRAY_COUNT(forbiddenFrameSetupNeedles));

    free(runtimeHeaderText);
    free(runtimeSourceText);
    free(abiHeaderText);
    free(stateHeaderText);
    free(gcHeaderText);
    free(frameSetupHeaderText);
    free(frameSetupSourceText);
    free(frameCleanupSourceText);
    free(functionBodyText);
    free(frameDescriptorSourceText);
    free(scalarLocalsText);
    free(scalarStackCopyText);
    free(emitterSourceText);
    free(methodMetadataSourceText);
    free(reflectionInvokersSourceText);
    free(reflectionBoolNumericInvokersSourceText);
    free(reflectionNumericThreeArgInvokersSourceText);
    free(reflectionBoolThreeArgInvokersSourceText);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_aot_c_source_emits_direct_generated_frame_setup);
    return UNITY_END();
}
