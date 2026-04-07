//
// Created by Auto on 2025/01/XX.
//

#include "zr_vm_parser/writer.h"

#include "zr_vm_core/closure.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/io.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/ownership.h"
#include "zr_vm_core/reflection.h"
#include "zr_vm_core/runtime_decorator.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/location.h"
#include "zr_vm_common/zr_io_conf.h"
#include "zr_vm_common/zr_instruction_conf.h"
#include "zr_vm_common/zr_version_info.h"
#include "zr_vm_common/zr_string_conf.h"
#include "zr_vm_common/zr_common_conf.h"
#include "zr_vm_common/zr_object_conf.h"
#include "zr_vm_common/zr_ast_constants.h"
#include "zr_vm_core/constant_reference.h"
#include "writer_binary_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 辅助函数：写入字符串（带长度）
static void write_string_with_length(SZrState *state, FILE *file, SZrString *str) {
    ZR_UNUSED_PARAMETER(state);
    if (str == ZR_NULL) {
        TZrSize strLength = 0;
        fwrite(&strLength, sizeof(TZrSize), 1, file);
        return;
    }
    
    TZrSize strLength = (str->shortStringLength < ZR_VM_LONG_STRING_FLAG) ?
                         (TZrSize)str->shortStringLength : 
                         str->longStringLength;
    fwrite(&strLength, sizeof(TZrSize), 1, file);
    if (strLength > 0) {
        TZrNativeString strStr = ZrCore_String_GetNativeString(str);
        if (strStr != ZR_NULL) {
            fwrite(strStr, sizeof(TZrChar), strLength, file);
        }
    }
}

// 辅助函数：从常量池索引获取字符串
static SZrString *get_string_from_constant(SZrState *state, SZrFunction *function, TZrUInt32 index) {
    if (function == ZR_NULL || index >= function->constantValueLength) {
        return ZR_NULL;
    }
    
    const SZrTypeValue *constant = &function->constantValueList[index];
    if (constant->type == ZR_VALUE_TYPE_STRING && constant->value.object != ZR_NULL) {
        SZrRawObject *rawObj = constant->value.object;
        if (rawObj->type == ZR_RAW_OBJECT_TYPE_STRING) {
            return ZR_CAST_STRING(state, rawObj);
        }
    }
    
    return ZR_NULL;
}

// 辅助函数：写入继承类型引用（.REFERENCE）
static void write_io_reference(SZrState *state, FILE *file, TZrUInt32 stringIndex, SZrFunction *function) {
    ZR_UNUSED_PARAMETER(state);
    ZR_UNUSED_PARAMETER(function);
    // referenceModuleName [string] (空字符串，当前模块内引用)
    TZrSize moduleNameLength = 0;
    fwrite(&moduleNameLength, sizeof(TZrSize), 1, file);
    
    // referenceModuleMd5 [string] (空字符串)
    TZrSize md5Length = 0;
    fwrite(&md5Length, sizeof(TZrSize), 1, file);
    
    // referenceIndex [8] (字符串索引)
    TZrSize referenceIndex = stringIndex;
    fwrite(&referenceIndex, sizeof(TZrSize), 1, file);
}

// 辅助函数：写入CLASS prototype的结构化数据
static void write_prototype_class(SZrState *state, FILE *file, const SZrCompiledPrototypeInfo *protoInfo, const TZrByte *data, SZrFunction *function) {
    // .CLASS: NAME [string]
    SZrString *className = get_string_from_constant(state, function, protoInfo->nameStringIndex);
    write_string_with_length(state, file, className);
    
    // SUPER_CLASS_LENGTH [8]
    TZrSize superClassLength = protoInfo->inheritsCount;
    fwrite(&superClassLength, sizeof(TZrSize), 1, file);
    
    // SUPER_CLASSES [.REFERENCE]
    if (superClassLength > 0) {
        const TZrUInt32 *inheritIndices = (const TZrUInt32 *)(data + sizeof(SZrCompiledPrototypeInfo));
        for (TZrUInt32 i = 0; i < superClassLength; i++) {
            write_io_reference(state, file, inheritIndices[i], function);
        }
    }
    
    // GENERIC_LENGTH [8] (目前设为0)
    TZrSize genericLength = 0;
    fwrite(&genericLength, sizeof(TZrSize), 1, file);
    
    // DECLARES_LENGTH [8]
    // Binary metadata only needs a stable type stub here. Member bodies are
    // restored at runtime from function->prototypeData; writing partial
    // METHOD/PROPERTY/META payloads desynchronizes the .zro reader.
    TZrSize declaresLength = 0;
    fwrite(&declaresLength, sizeof(TZrSize), 1, file);

    ZR_UNUSED_PARAMETER(data);
}

// 辅助函数：写入STRUCT prototype的结构化数据
static void write_prototype_struct(SZrState *state, FILE *file, const SZrCompiledPrototypeInfo *protoInfo, const TZrByte *data, SZrFunction *function) {
    // .STRUCT: NAME [string]
    SZrString *structName = get_string_from_constant(state, function, protoInfo->nameStringIndex);
    write_string_with_length(state, file, structName);
    
    // SUPER_STRUCT_LENGTH [8]
    TZrSize superStructLength = protoInfo->inheritsCount;
    fwrite(&superStructLength, sizeof(TZrSize), 1, file);
    
    // SUPER_STRUCTS [.REFERENCE]
    if (superStructLength > 0) {
        const TZrUInt32 *inheritIndices = (const TZrUInt32 *)(data + sizeof(SZrCompiledPrototypeInfo));
        for (TZrUInt32 i = 0; i < superStructLength; i++) {
            write_io_reference(state, file, inheritIndices[i], function);
        }
    }
    
    // GENERIC_LENGTH [8] (目前设为0)
    TZrSize genericLength = 0;
    fwrite(&genericLength, sizeof(TZrSize), 1, file);
    
    // DECLARES_LENGTH [8]
    // See write_prototype_class(): binary import metadata only consumes the
    // type stub, while runtime reconstruction uses prototypeData.
    TZrSize declaresLength = 0;
    fwrite(&declaresLength, sizeof(TZrSize), 1, file);

    ZR_UNUSED_PARAMETER(data);
}

TZrBool ZrParser_Writer_FunctionTreeHasDebugInfo(const SZrFunction *function) {
    TZrUInt32 childIndex;

    if (function == ZR_NULL) {
        return ZR_FALSE;
    }

    if (function->executionLocationInfoList != ZR_NULL && function->executionLocationInfoLength > 0) {
        return ZR_TRUE;
    }

    if (function->childFunctionList == ZR_NULL || function->childFunctionLength == 0) {
        return ZR_FALSE;
    }

    for (childIndex = 0; childIndex < function->childFunctionLength; childIndex++) {
        if (ZrParser_Writer_FunctionTreeHasDebugInfo(&function->childFunctionList[childIndex])) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static SZrFunctionExecutionLocationInfo function_find_debug_location_for_instruction(const SZrFunction *function,
                                                                                     TZrUInt32 instructionIndex) {
    SZrFunctionExecutionLocationInfo bestLocation;
    TZrUInt32 index;

    ZrCore_Memory_RawSet(&bestLocation, 0, sizeof(bestLocation));
    if (function == ZR_NULL || function->executionLocationInfoList == ZR_NULL || function->executionLocationInfoLength == 0) {
        return bestLocation;
    }

    for (index = 0; index < function->executionLocationInfoLength; index++) {
        const SZrFunctionExecutionLocationInfo *info = &function->executionLocationInfoList[index];
        if (info->currentInstructionOffset > instructionIndex) {
            break;
        }
        bestLocation = *info;
    }

    if (bestLocation.lineInSource == 0 && function->lineInSourceStart > 0) {
        bestLocation.lineInSource = function->lineInSourceStart;
        bestLocation.lineInSourceEnd =
                function->lineInSourceEnd > 0 ? function->lineInSourceEnd : function->lineInSourceStart;
    }

    return bestLocation;
}

static void write_native_string_with_length(FILE *file, const TZrChar *text) {
    TZrSize length = 0;

    if (file == ZR_NULL) {
        return;
    }

    if (text != ZR_NULL) {
        length = strlen(text);
    }

    fwrite(&length, sizeof(TZrSize), 1, file);
    if (length > 0) {
        fwrite(text, sizeof(TZrChar), length, file);
    }
}

static const TZrChar *writer_get_stable_debug_source_id(const SZrFunction *function,
                                                        const SZrBinaryWriterOptions *options) {
    const TZrChar *sourceFile = ZR_NULL;

    if (function == ZR_NULL || function->sourceCodeList == ZR_NULL) {
        if (options != ZR_NULL && options->moduleName != ZR_NULL && options->moduleName[0] != '\0') {
            return options->moduleName;
        }
        return ZR_NULL;
    }

    sourceFile = ZrCore_String_GetNativeString(function->sourceCodeList);
    if (sourceFile == ZR_NULL || sourceFile[0] == '\0') {
        if (options != ZR_NULL && options->moduleName != ZR_NULL && options->moduleName[0] != '\0') {
            return options->moduleName;
        }
        return ZR_NULL;
    }

    return sourceFile;
}

static void write_function_debug_infos(FILE *file,
                                       const SZrFunction *function,
                                       const SZrBinaryWriterOptions *options) {
    TZrUInt64 debugInfoLength = 0;
    const TZrChar *sourceId = ZR_NULL;
    const TZrChar *sourceHash = ZR_NULL;

    if (file == ZR_NULL || function == ZR_NULL) {
        return;
    }

    if (function->instructionsLength > 0 && function->executionLocationInfoList != ZR_NULL &&
        function->executionLocationInfoLength > 0) {
        debugInfoLength = 1;
    }

    fwrite(&debugInfoLength, sizeof(TZrUInt64), 1, file);
    if (debugInfoLength == 0) {
        return;
    }

    sourceId = writer_get_stable_debug_source_id(function, options);
    if (options != ZR_NULL) {
        sourceHash = options->moduleHash;
    }

    {
        write_native_string_with_length(file, sourceId);
        write_native_string_with_length(file, sourceHash);

        TZrUInt64 instructionsLength = function->instructionsLength;
        fwrite(&instructionsLength, sizeof(TZrUInt64), 1, file);
        for (TZrUInt32 index = 0; index < function->instructionsLength; index++) {
            SZrFunctionExecutionLocationInfo location = function_find_debug_location_for_instruction(function, index);
            TZrUInt64 line = location.lineInSource;
            fwrite(&line, sizeof(TZrUInt64), 1, file);
        }

        for (TZrUInt32 index = 0; index < function->instructionsLength; index++) {
            SZrFunctionExecutionLocationInfo location = function_find_debug_location_for_instruction(function, index);
            fwrite(&location.lineInSource, sizeof(TZrUInt32), 1, file);
            fwrite(&location.columnInSourceStart, sizeof(TZrUInt32), 1, file);
            fwrite(&location.lineInSourceEnd, sizeof(TZrUInt32), 1, file);
            fwrite(&location.columnInSourceEnd, sizeof(TZrUInt32), 1, file);
        }
    }
}

static TZrBool write_io_function_internal(SZrState *state,
                                          FILE *file,
                                          SZrFunction *function,
                                          const TZrChar *defaultName,
                                          const SZrBinaryWriterOptions *options);

ZR_PARSER_API TZrUInt64 ZrParser_Writer_GetSerializableNativeHelperId(FZrNativeFunction function) {
    if (function == ZR_NULL) {
        return ZR_IO_NATIVE_HELPER_NONE;
    }

    if (function == ZrCore_Module_ImportNativeEntry) {
        return ZR_IO_NATIVE_HELPER_MODULE_IMPORT;
    }

    if (function == ZrCore_Ownership_NativeUnique) {
        return ZR_IO_NATIVE_HELPER_OWNERSHIP_UNIQUE;
    }

    if (function == ZrCore_Ownership_NativeShared) {
        return ZR_IO_NATIVE_HELPER_OWNERSHIP_SHARED;
    }

    if (function == ZrCore_Ownership_NativeWeak) {
        return ZR_IO_NATIVE_HELPER_OWNERSHIP_WEAK;
    }

    if (function == ZrCore_Ownership_NativeUsing) {
        return ZR_IO_NATIVE_HELPER_OWNERSHIP_USING;
    }

    if (function == ZrCore_Reflection_TypeOfNativeEntry) {
        return ZR_IO_NATIVE_HELPER_REFLECTION_TYPEOF;
    }

    if (function == ZrCore_RuntimeDecorator_ApplyNativeEntry) {
        return ZR_IO_NATIVE_HELPER_RUNTIME_DECORATOR_APPLY;
    }

    if (function == ZrCore_RuntimeDecorator_ApplyMemberNativeEntry) {
        return ZR_IO_NATIVE_HELPER_RUNTIME_MEMBER_DECORATOR_APPLY;
    }

    return ZR_IO_NATIVE_HELPER_NONE;
}

static void write_function_name(SZrState *state, FILE *file, SZrFunction *function, const TZrChar *defaultName) {
    if (function != ZR_NULL && function->functionName != ZR_NULL) {
        write_string_with_length(state, file, function->functionName);
        return;
    }

    if (defaultName != ZR_NULL) {
        SZrString *fallbackName = ZrCore_String_Create(state, (TZrNativeString)defaultName, strlen(defaultName));
        write_string_with_length(state, file, fallbackName);
        return;
    }

    write_string_with_length(state, file, ZR_NULL);
}

static void write_function_local_variables(FILE *file, SZrFunction *function) {
    TZrUInt64 localLength = function->localVariableLength;
    fwrite(&localLength, sizeof(TZrUInt64), 1, file);

    for (TZrUInt64 i = 0; i < localLength; i++) {
        SZrFunctionLocalVariable *local = &function->localVariableList[i];
        TZrMemoryOffset instructionStart = local->offsetActivate;
        TZrMemoryOffset instructionEnd = local->offsetDead;
        TZrUInt64 startLineLocal = 0;
        TZrUInt64 endLineLocal = 0;
        if (function->executionLocationInfoList != ZR_NULL && function->executionLocationInfoLength > 0) {
            for (TZrUInt32 j = 0; j < function->executionLocationInfoLength; j++) {
                SZrFunctionExecutionLocationInfo *locInfo = &function->executionLocationInfoList[j];
                if (locInfo->currentInstructionOffset == instructionStart) {
                    startLineLocal = locInfo->lineInSource;
                    break;
                }
            }
            for (TZrUInt32 j = 0; j < function->executionLocationInfoLength; j++) {
                SZrFunctionExecutionLocationInfo *locInfo = &function->executionLocationInfoList[j];
                if (locInfo->currentInstructionOffset == instructionEnd) {
                    endLineLocal = locInfo->lineInSource;
                    break;
                }
            }
            if (startLineLocal == 0) {
                for (TZrUInt32 j = 0; j < function->executionLocationInfoLength; j++) {
                    SZrFunctionExecutionLocationInfo *locInfo = &function->executionLocationInfoList[j];
                    if (locInfo->currentInstructionOffset <= instructionStart) {
                        startLineLocal = locInfo->lineInSource;
                    } else {
                        break;
                    }
                }
            }
            if (endLineLocal == 0) {
                for (TZrUInt32 j = 0; j < function->executionLocationInfoLength; j++) {
                    SZrFunctionExecutionLocationInfo *locInfo = &function->executionLocationInfoList[j];
                    if (locInfo->currentInstructionOffset <= instructionEnd) {
                        endLineLocal = locInfo->lineInSource;
                    } else {
                        break;
                    }
                }
            }
        }

        {
            TZrUInt64 instructionStartValue = (TZrUInt64)instructionStart;
            TZrUInt64 instructionEndValue = (TZrUInt64)instructionEnd;
            fwrite(&instructionStartValue, sizeof(TZrUInt64), 1, file);
            fwrite(&instructionEndValue, sizeof(TZrUInt64), 1, file);
        }
        fwrite(&startLineLocal, sizeof(TZrUInt64), 1, file);
        fwrite(&endLineLocal, sizeof(TZrUInt64), 1, file);
    }
}

static void write_function_closure_variables(SZrState *state, FILE *file, SZrFunction *function) {
    TZrUInt64 closureLength = function->closureValueLength;
    fwrite(&closureLength, sizeof(TZrUInt64), 1, file);

    for (TZrUInt64 i = 0; i < closureLength; i++) {
        SZrFunctionClosureVariable *closure = &function->closureValueList[i];
        TZrUInt8 inStack = closure->inStack ? 1 : 0;
        TZrUInt32 index = closure->index;
        TZrUInt32 valueType = (TZrUInt32)closure->valueType;

        write_string_with_length(state, file, closure->name);
        fwrite(&inStack, sizeof(TZrUInt8), 1, file);
        fwrite(&index, sizeof(TZrUInt32), 1, file);
        fwrite(&valueType, sizeof(TZrUInt32), 1, file);
    }
}

static void write_function_exception_metadata(SZrState *state, FILE *file, SZrFunction *function) {
    TZrUInt64 catchClauseCount = function->catchClauseCount;
    TZrUInt64 exceptionHandlerCount = function->exceptionHandlerCount;

    fwrite(&catchClauseCount, sizeof(TZrUInt64), 1, file);
    for (TZrUInt64 i = 0; i < catchClauseCount; i++) {
        SZrFunctionCatchClauseInfo *clause = &function->catchClauseList[i];
        TZrUInt64 targetInstructionOffset = (TZrUInt64)clause->targetInstructionOffset;
        write_string_with_length(state, file, clause->typeName);
        fwrite(&targetInstructionOffset, sizeof(TZrUInt64), 1, file);
    }

    fwrite(&exceptionHandlerCount, sizeof(TZrUInt64), 1, file);
    for (TZrUInt64 i = 0; i < exceptionHandlerCount; i++) {
        SZrFunctionExceptionHandlerInfo *handler = &function->exceptionHandlerList[i];
        TZrUInt64 protectedStartInstructionOffset = (TZrUInt64)handler->protectedStartInstructionOffset;
        TZrUInt64 finallyTargetInstructionOffset = (TZrUInt64)handler->finallyTargetInstructionOffset;
        TZrUInt64 afterFinallyInstructionOffset = (TZrUInt64)handler->afterFinallyInstructionOffset;
        TZrUInt8 hasFinally = handler->hasFinally ? 1 : 0;

        fwrite(&protectedStartInstructionOffset, sizeof(TZrUInt64), 1, file);
        fwrite(&finallyTargetInstructionOffset, sizeof(TZrUInt64), 1, file);
        fwrite(&afterFinallyInstructionOffset, sizeof(TZrUInt64), 1, file);
        fwrite(&handler->catchClauseStartIndex, sizeof(TZrUInt32), 1, file);
        fwrite(&handler->catchClauseCount, sizeof(TZrUInt32), 1, file);
        fwrite(&hasFinally, sizeof(TZrUInt8), 1, file);
    }
}

static TZrBool write_embedded_value(FILE *file, SZrState *state, const SZrTypeValue *value);

static TZrInt32 writer_compare_native_strings(TZrNativeString left, TZrNativeString right) {
    if (left == right) {
        return 0;
    }
    if (left == ZR_NULL) {
        return -1;
    }
    if (right == ZR_NULL) {
        return 1;
    }
    return ZrCore_NativeString_Compare(left, right);
}

static TZrInt32 writer_compare_values_for_binary_order(SZrState *state,
                                                       const SZrTypeValue *left,
                                                       const SZrTypeValue *right) {
    if (left == right) {
        return 0;
    }
    if (left == ZR_NULL) {
        return -1;
    }
    if (right == ZR_NULL) {
        return 1;
    }
    if (left->type != right->type) {
        return left->type < right->type ? -1 : 1;
    }

    switch (left->type) {
        case ZR_VALUE_TYPE_NULL:
            return 0;

        case ZR_VALUE_TYPE_BOOL:
            if (left->value.nativeObject.nativeBool == right->value.nativeObject.nativeBool) {
                return 0;
            }
            return left->value.nativeObject.nativeBool < right->value.nativeObject.nativeBool ? -1 : 1;

        case ZR_VALUE_TYPE_INT8:
        case ZR_VALUE_TYPE_INT16:
        case ZR_VALUE_TYPE_INT32:
        case ZR_VALUE_TYPE_INT64:
            if (left->value.nativeObject.nativeInt64 == right->value.nativeObject.nativeInt64) {
                return 0;
            }
            return left->value.nativeObject.nativeInt64 < right->value.nativeObject.nativeInt64 ? -1 : 1;

        case ZR_VALUE_TYPE_UINT8:
        case ZR_VALUE_TYPE_UINT16:
        case ZR_VALUE_TYPE_UINT32:
        case ZR_VALUE_TYPE_UINT64:
            if (left->value.nativeObject.nativeUInt64 == right->value.nativeObject.nativeUInt64) {
                return 0;
            }
            return left->value.nativeObject.nativeUInt64 < right->value.nativeObject.nativeUInt64 ? -1 : 1;

        case ZR_VALUE_TYPE_FLOAT:
        case ZR_VALUE_TYPE_DOUBLE:
            if (left->value.nativeObject.nativeDouble == right->value.nativeObject.nativeDouble) {
                return 0;
            }
            return left->value.nativeObject.nativeDouble < right->value.nativeObject.nativeDouble ? -1 : 1;

        case ZR_VALUE_TYPE_STRING:
            return writer_compare_native_strings(
                    left->value.object != ZR_NULL ? ZrCore_String_GetNativeString(ZR_CAST_STRING(state, left->value.object))
                                                  : ZR_NULL,
                    right->value.object != ZR_NULL ? ZrCore_String_GetNativeString(ZR_CAST_STRING(state, right->value.object))
                                                   : ZR_NULL);

        default: {
            TZrUInt64 leftHash;
            TZrUInt64 rightHash;

            if (ZrCore_Value_CompareDirectly(state, left, right)) {
                return 0;
            }

            leftHash = ZrCore_Value_GetHash(state, left);
            rightHash = ZrCore_Value_GetHash(state, right);
            if (leftHash != rightHash) {
                return leftHash < rightHash ? -1 : 1;
            }

            if (left->value.object == right->value.object) {
                return 0;
            }
            return left->value.object < right->value.object ? -1 : 1;
        }
    }
}

static void writer_sort_object_pairs_for_binary_order(SZrState *state,
                                                      SZrHashKeyValuePair **pairs,
                                                      TZrSize pairCount) {
    if (state == ZR_NULL || pairs == ZR_NULL || pairCount < 2) {
        return;
    }

    for (TZrSize index = 1; index < pairCount; index++) {
        SZrHashKeyValuePair *current = pairs[index];
        TZrSize insertIndex = index;

        while (insertIndex > 0 &&
               writer_compare_values_for_binary_order(state, &pairs[insertIndex - 1]->key, &current->key) > 0) {
            pairs[insertIndex] = pairs[insertIndex - 1];
            insertIndex--;
        }
        pairs[insertIndex] = current;
    }
}

static TZrBool write_object_constant_payload(FILE *file, SZrState *state, const SZrTypeValue *value) {
    SZrObject *object;
    TZrUInt64 entryCount = 0;

    if (file == ZR_NULL || state == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    object = value->value.object != ZR_NULL ? ZR_CAST_OBJECT(state, value->value.object) : ZR_NULL;
    if (object != ZR_NULL && object->nodeMap.isValid && object->nodeMap.buckets != ZR_NULL) {
        entryCount = (TZrUInt64)object->nodeMap.elementCount;
    }

    fwrite(&entryCount, sizeof(TZrUInt64), 1, file);
    if (object == ZR_NULL || entryCount == 0) {
        return ZR_TRUE;
    }

    SZrHashKeyValuePair **pairs = (SZrHashKeyValuePair **)malloc(sizeof(*pairs) * (TZrSize)entryCount);
    TZrSize pairIndex = 0;

    if (pairs == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize bucketIndex = 0; bucketIndex < object->nodeMap.capacity; bucketIndex++) {
        SZrHashKeyValuePair *pair = object->nodeMap.buckets[bucketIndex];
        while (pair != ZR_NULL) {
            if (pairIndex >= (TZrSize)entryCount) {
                free(pairs);
                return ZR_FALSE;
            }
            pairs[pairIndex++] = pair;
            pair = pair->next;
        }
    }

    writer_sort_object_pairs_for_binary_order(state, pairs, pairIndex);
    for (TZrSize index = 0; index < pairIndex; index++) {
        if (!write_embedded_value(file, state, &pairs[index]->key) ||
            !write_embedded_value(file, state, &pairs[index]->value)) {
            free(pairs);
            return ZR_FALSE;
        }
    }

    free(pairs);

    return ZR_TRUE;
}

static TZrBool write_array_constant_payload(FILE *file, SZrState *state, const SZrTypeValue *value) {
    SZrObject *arrayObject;
    TZrUInt64 elementCount = 0;

    if (file == ZR_NULL || state == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    arrayObject = value->value.object != ZR_NULL ? ZR_CAST_OBJECT(state, value->value.object) : ZR_NULL;
    if (arrayObject != ZR_NULL && arrayObject->nodeMap.isValid && arrayObject->nodeMap.buckets != ZR_NULL) {
        elementCount = (TZrUInt64)arrayObject->nodeMap.elementCount;
    }

    fwrite(&elementCount, sizeof(TZrUInt64), 1, file);
    for (TZrUInt64 index = 0; index < elementCount; index++) {
        SZrTypeValue indexValue;
        const SZrTypeValue *elementValue;
        SZrTypeValue nullValue;

        ZrCore_Value_InitAsInt(state, &indexValue, (TZrInt64)index);
        elementValue = arrayObject != ZR_NULL ? ZrCore_Object_GetValue(state, arrayObject, &indexValue) : ZR_NULL;
        if (elementValue == ZR_NULL) {
            ZrCore_Value_ResetAsNull(&nullValue);
            elementValue = &nullValue;
        }

        if (!write_embedded_value(file, state, elementValue)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool write_value_payload(FILE *file,
                                   SZrState *state,
                                   const SZrTypeValue *value,
                                   TZrBool allowCallablePayload,
                                   TZrUInt64 *outHelperId) {
    if (file == ZR_NULL || state == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    if (outHelperId != ZR_NULL) {
        *outHelperId = 0;
    }

    switch (value->type) {
        case ZR_VALUE_TYPE_NULL:
            return ZR_TRUE;
        case ZR_VALUE_TYPE_BOOL: {
            TZrUInt8 boolValue = value->value.nativeObject.nativeBool ? ZR_TRUE : ZR_FALSE;
            fwrite(&boolValue, sizeof(TZrUInt8), 1, file);
            return ZR_TRUE;
        }
        case ZR_VALUE_TYPE_INT8:
        case ZR_VALUE_TYPE_INT16:
        case ZR_VALUE_TYPE_INT32:
        case ZR_VALUE_TYPE_INT64:
            fwrite(&value->value.nativeObject.nativeInt64, sizeof(TZrInt64), 1, file);
            return ZR_TRUE;
        case ZR_VALUE_TYPE_UINT8:
        case ZR_VALUE_TYPE_UINT16:
        case ZR_VALUE_TYPE_UINT32:
        case ZR_VALUE_TYPE_UINT64:
            fwrite(&value->value.nativeObject.nativeUInt64, sizeof(TZrUInt64), 1, file);
            return ZR_TRUE;
        case ZR_VALUE_TYPE_FLOAT:
        case ZR_VALUE_TYPE_DOUBLE:
            fwrite(&value->value.nativeObject.nativeDouble, sizeof(TZrDouble), 1, file);
            return ZR_TRUE;
        case ZR_VALUE_TYPE_STRING: {
            SZrString *str = ZR_NULL;
            if (value->value.object != ZR_NULL) {
                SZrRawObject *rawObj = value->value.object;
                if (rawObj->type == ZR_RAW_OBJECT_TYPE_STRING) {
                    str = ZR_CAST_STRING(state, rawObj);
                }
            }
            write_string_with_length(state, file, str);
            return ZR_TRUE;
        }
        case ZR_VALUE_TYPE_OBJECT:
            return write_object_constant_payload(file, state, value);
        case ZR_VALUE_TYPE_ARRAY:
            return write_array_constant_payload(file, state, value);
        case ZR_VALUE_TYPE_FUNCTION:
        case ZR_VALUE_TYPE_CLOSURE: {
            TZrBool hasFunctionValue = ZR_FALSE;
            SZrFunction *functionValue = ZR_NULL;
            TZrUInt64 helperId = ZR_IO_NATIVE_HELPER_NONE;
            if (!allowCallablePayload) {
                return ZR_FALSE;
            }

            if (value->value.object != ZR_NULL) {
                SZrRawObject *rawObj = value->value.object;
                if (rawObj->type == ZR_RAW_OBJECT_TYPE_FUNCTION) {
                    functionValue = ZR_CAST(SZrFunction *, rawObj);
                    hasFunctionValue = ZR_TRUE;
                } else if (rawObj->type == ZR_RAW_OBJECT_TYPE_CLOSURE) {
                    if (value->isNative) {
                        SZrClosureNative *nativeClosure = ZR_CAST_NATIVE_CLOSURE(state, rawObj);
                        if (nativeClosure != ZR_NULL) {
                            helperId = ZrParser_Writer_GetSerializableNativeHelperId(nativeClosure->nativeFunction);
                        }
                    } else {
                        SZrClosure *closure = ZR_CAST_VM_CLOSURE(state, rawObj);
                        if (closure != ZR_NULL && closure->function != ZR_NULL) {
                            functionValue = closure->function;
                            hasFunctionValue = ZR_TRUE;
                        }
                    }
                }
            }
            fwrite(&hasFunctionValue, sizeof(TZrBool), 1, file);
            if (hasFunctionValue) {
                if (!write_io_function_internal(state, file, functionValue, ZR_NULL, ZR_NULL)) {
                    return ZR_FALSE;
                }
            }
            if (outHelperId != ZR_NULL) {
                *outHelperId = helperId;
            }
            return ZR_TRUE;
        }
        case ZR_VALUE_TYPE_NATIVE_POINTER: {
            if (!allowCallablePayload) {
                return ZR_FALSE;
            }

            if (outHelperId != ZR_NULL) {
                FZrNativeFunction nativeFunction = value->value.nativeFunction;
                *outHelperId = ZrParser_Writer_GetSerializableNativeHelperId(nativeFunction);
            }
            return ZR_TRUE;
        }
        default:
            return ZR_FALSE;
    }
}

static TZrBool write_embedded_value(FILE *file, SZrState *state, const SZrTypeValue *value) {
    SZrTypeValue nullValue;
    TZrUInt32 type;

    if (value == ZR_NULL) {
        ZrCore_Value_ResetAsNull(&nullValue);
        value = &nullValue;
    }

    type = (TZrUInt32)value->type;
    fwrite(&type, sizeof(TZrUInt32), 1, file);
    return write_value_payload(file, state, value, ZR_FALSE, ZR_NULL);
}

static TZrBool write_function_constant(FILE *file, SZrState *state, SZrTypeValue *constant) {
    TZrUInt32 type;
    TZrUInt64 helperId = 0;
    TZrUInt64 endLineConst = 0;

    if (file == ZR_NULL || state == ZR_NULL || constant == ZR_NULL) {
        return ZR_FALSE;
    }

    type = (TZrUInt32)constant->type;
    fwrite(&type, sizeof(TZrUInt32), 1, file);
    if (!write_value_payload(file, state, constant, ZR_TRUE, &helperId)) {
        return ZR_FALSE;
    }

    fwrite(&helperId, sizeof(TZrUInt64), 1, file);
    fwrite(&endLineConst, sizeof(TZrUInt64), 1, file);
    return ZR_TRUE;
}

static void write_function_typed_type_ref(FILE *file, SZrState *state, const SZrFunctionTypedTypeRef *typeRef) {
    TZrUInt32 baseType = ZR_VALUE_TYPE_OBJECT;
    TZrUInt8 isNullable = ZR_FALSE;
    TZrUInt32 ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
    TZrUInt8 isArray = ZR_FALSE;
    TZrUInt32 elementBaseType = ZR_VALUE_TYPE_OBJECT;

    if (typeRef != ZR_NULL) {
        baseType = (TZrUInt32)typeRef->baseType;
        isNullable = typeRef->isNullable ? ZR_TRUE : ZR_FALSE;
        ownershipQualifier = (TZrUInt32)typeRef->ownershipQualifier;
        isArray = typeRef->isArray ? ZR_TRUE : ZR_FALSE;
        elementBaseType = (TZrUInt32)typeRef->elementBaseType;
    }

    fwrite(&baseType, sizeof(TZrUInt32), 1, file);
    fwrite(&isNullable, sizeof(TZrUInt8), 1, file);
    fwrite(&ownershipQualifier, sizeof(TZrUInt32), 1, file);
    fwrite(&isArray, sizeof(TZrUInt8), 1, file);
    write_string_with_length(state, file, typeRef != ZR_NULL ? typeRef->typeName : ZR_NULL);
    fwrite(&elementBaseType, sizeof(TZrUInt32), 1, file);
    write_string_with_length(state, file, typeRef != ZR_NULL ? typeRef->elementTypeName : ZR_NULL);
}

static void write_function_typed_local_bindings(FILE *file, SZrState *state, SZrFunction *function) {
    TZrUInt64 typedLocalCount = function != ZR_NULL ? function->typedLocalBindingLength : 0;

    fwrite(&typedLocalCount, sizeof(TZrUInt64), 1, file);
    for (TZrUInt64 index = 0; index < typedLocalCount; index++) {
        SZrFunctionTypedLocalBinding *binding = &function->typedLocalBindings[index];
        write_string_with_length(state, file, binding->name);
        fwrite(&binding->stackSlot, sizeof(TZrUInt32), 1, file);
        write_function_typed_type_ref(file, state, &binding->type);
    }
}

static void write_function_typed_export_symbols(FILE *file, SZrState *state, SZrFunction *function) {
    TZrUInt64 symbolCount = function != ZR_NULL ? function->typedExportedSymbolLength : 0;

    fwrite(&symbolCount, sizeof(TZrUInt64), 1, file);
    for (TZrUInt64 index = 0; index < symbolCount; index++) {
        SZrFunctionTypedExportSymbol *symbol = &function->typedExportedSymbols[index];
        TZrUInt64 parameterCount = symbol->parameterCount;

        write_string_with_length(state, file, symbol->name);
        fwrite(&symbol->stackSlot, sizeof(TZrUInt32), 1, file);
        fwrite(&symbol->accessModifier, sizeof(TZrUInt8), 1, file);
        fwrite(&symbol->symbolKind, sizeof(TZrUInt8), 1, file);
        fwrite(&symbol->exportKind, sizeof(TZrUInt8), 1, file);
        fwrite(&symbol->readiness, sizeof(TZrUInt8), 1, file);
        fwrite(&symbol->reserved0, sizeof(TZrUInt16), 1, file);
        fwrite(&symbol->callableChildIndex, sizeof(TZrUInt32), 1, file);
        write_function_typed_type_ref(file, state, &symbol->valueType);
        fwrite(&parameterCount, sizeof(TZrUInt64), 1, file);
        for (TZrUInt64 paramIndex = 0; paramIndex < parameterCount; paramIndex++) {
            const SZrFunctionTypedTypeRef *parameterType =
                    symbol->parameterTypes != ZR_NULL ? &symbol->parameterTypes[paramIndex] : ZR_NULL;
            write_function_typed_type_ref(file, state, parameterType);
        }
        fwrite(&symbol->lineInSourceStart, sizeof(TZrUInt32), 1, file);
        fwrite(&symbol->columnInSourceStart, sizeof(TZrUInt32), 1, file);
        fwrite(&symbol->lineInSourceEnd, sizeof(TZrUInt32), 1, file);
        fwrite(&symbol->columnInSourceEnd, sizeof(TZrUInt32), 1, file);
    }
}

static void write_function_module_effect(FILE *file, SZrState *state, const SZrFunctionModuleEffect *effect) {
    TZrUInt8 kind = 0;
    TZrUInt8 exportKind = 0;
    TZrUInt8 readiness = 0;
    TZrUInt8 reserved0 = 0;
    TZrUInt32 lineInSourceStart = 0;
    TZrUInt32 columnInSourceStart = 0;
    TZrUInt32 lineInSourceEnd = 0;
    TZrUInt32 columnInSourceEnd = 0;

    if (effect != ZR_NULL) {
        kind = effect->kind;
        exportKind = effect->exportKind;
        readiness = effect->readiness;
        reserved0 = effect->reserved0;
        lineInSourceStart = effect->lineInSourceStart;
        columnInSourceStart = effect->columnInSourceStart;
        lineInSourceEnd = effect->lineInSourceEnd;
        columnInSourceEnd = effect->columnInSourceEnd;
    }

    fwrite(&kind, sizeof(TZrUInt8), 1, file);
    fwrite(&exportKind, sizeof(TZrUInt8), 1, file);
    fwrite(&readiness, sizeof(TZrUInt8), 1, file);
    fwrite(&reserved0, sizeof(TZrUInt8), 1, file);
    write_string_with_length(state, file, effect != ZR_NULL ? effect->moduleName : ZR_NULL);
    write_string_with_length(state, file, effect != ZR_NULL ? effect->symbolName : ZR_NULL);
    fwrite(&lineInSourceStart, sizeof(TZrUInt32), 1, file);
    fwrite(&columnInSourceStart, sizeof(TZrUInt32), 1, file);
    fwrite(&lineInSourceEnd, sizeof(TZrUInt32), 1, file);
    fwrite(&columnInSourceEnd, sizeof(TZrUInt32), 1, file);
}

static void write_function_static_imports(FILE *file, SZrState *state, SZrFunction *function) {
    TZrUInt64 importCount = function != ZR_NULL ? function->staticImportLength : 0;

    fwrite(&importCount, sizeof(TZrUInt64), 1, file);
    for (TZrUInt64 index = 0; index < importCount; index++) {
        write_string_with_length(state, file, function->staticImports[index]);
    }
}

static void write_function_module_entry_effects(FILE *file, SZrState *state, SZrFunction *function) {
    TZrUInt64 effectCount = function != ZR_NULL ? function->moduleEntryEffectLength : 0;

    fwrite(&effectCount, sizeof(TZrUInt64), 1, file);
    for (TZrUInt64 index = 0; index < effectCount; index++) {
        write_function_module_effect(file, state, &function->moduleEntryEffects[index]);
    }
}

static void write_function_exported_callable_summaries(FILE *file, SZrState *state, SZrFunction *function) {
    TZrUInt64 summaryCount = function != ZR_NULL ? function->exportedCallableSummaryLength : 0;

    fwrite(&summaryCount, sizeof(TZrUInt64), 1, file);
    for (TZrUInt64 index = 0; index < summaryCount; index++) {
        const SZrFunctionCallableSummary *summary = &function->exportedCallableSummaries[index];
        TZrUInt64 effectCount = summary->effectCount;

        write_string_with_length(state, file, summary->name);
        fwrite(&summary->callableChildIndex, sizeof(TZrUInt32), 1, file);
        fwrite(&effectCount, sizeof(TZrUInt64), 1, file);
        for (TZrUInt64 effectIndex = 0; effectIndex < effectCount; effectIndex++) {
            const SZrFunctionModuleEffect *effect =
                    summary->effects != ZR_NULL ? &summary->effects[effectIndex] : ZR_NULL;
            write_function_module_effect(file, state, effect);
        }
    }
}

static void write_function_top_level_callable_bindings(FILE *file, SZrState *state, SZrFunction *function) {
    TZrUInt64 bindingCount = function != ZR_NULL ? function->topLevelCallableBindingLength : 0;

    fwrite(&bindingCount, sizeof(TZrUInt64), 1, file);
    for (TZrUInt64 index = 0; index < bindingCount; index++) {
        const SZrFunctionTopLevelCallableBinding *binding = &function->topLevelCallableBindings[index];

        write_string_with_length(state, file, binding->name);
        fwrite(&binding->stackSlot, sizeof(TZrUInt32), 1, file);
        fwrite(&binding->callableChildIndex, sizeof(TZrUInt32), 1, file);
        fwrite(&binding->accessModifier, sizeof(TZrUInt8), 1, file);
        fwrite(&binding->exportKind, sizeof(TZrUInt8), 1, file);
        fwrite(&binding->readiness, sizeof(TZrUInt8), 1, file);
        fwrite(&binding->reserved0, sizeof(TZrUInt8), 1, file);
    }
}

static TZrBool write_function_metadata_parameters(FILE *file,
                                                  SZrState *state,
                                                  SZrFunctionMetadataParameter *parameters,
                                                  TZrUInt32 parameterCount) {
    TZrUInt64 count = parameterCount;

    fwrite(&count, sizeof(TZrUInt64), 1, file);
    for (TZrUInt32 index = 0; index < parameterCount; index++) {
        TZrUInt8 hasDefaultValue = parameters[index].hasDefaultValue ? ZR_TRUE : ZR_FALSE;
        TZrUInt8 hasDecoratorMetadata = parameters[index].hasDecoratorMetadata ? ZR_TRUE : ZR_FALSE;
        TZrUInt64 decoratorCount = parameters[index].decoratorCount;
        write_string_with_length(state, file, parameters[index].name);
        write_function_typed_type_ref(file, state, &parameters[index].type);
        fwrite(&hasDefaultValue, sizeof(TZrUInt8), 1, file);
        if (hasDefaultValue) {
            if (!write_function_constant(file, state, &parameters[index].defaultValue)) {
                return ZR_FALSE;
            }
        }
        fwrite(&hasDecoratorMetadata, sizeof(TZrUInt8), 1, file);
        if (hasDecoratorMetadata) {
            if (!write_function_constant(file, state, &parameters[index].decoratorMetadataValue)) {
                return ZR_FALSE;
            }
        }
        fwrite(&decoratorCount, sizeof(TZrUInt64), 1, file);
        for (TZrUInt64 decoratorIndex = 0; decoratorIndex < decoratorCount; decoratorIndex++) {
            write_string_with_length(state, file, parameters[index].decoratorNames[decoratorIndex]);
        }
    }

    return ZR_TRUE;
}

static TZrBool write_function_parameter_metadata(FILE *file, SZrState *state, SZrFunction *function) {
    if (function == ZR_NULL) {
        return write_function_metadata_parameters(file, state, ZR_NULL, 0);
    }

    return write_function_metadata_parameters(file,
                                              state,
                                              function->parameterMetadata,
                                              function->parameterMetadataCount);
}

static TZrBool write_function_compile_time_metadata(FILE *file, SZrState *state, SZrFunction *function) {
    TZrUInt64 variableCount = function != ZR_NULL ? function->compileTimeVariableInfoLength : 0;
    TZrUInt64 functionCount = function != ZR_NULL ? function->compileTimeFunctionInfoLength : 0;

    fwrite(&variableCount, sizeof(TZrUInt64), 1, file);
    for (TZrUInt64 index = 0; index < variableCount; index++) {
        SZrFunctionCompileTimeVariableInfo *info = &function->compileTimeVariableInfos[index];
        TZrUInt64 pathBindingCount = info->pathBindingCount;

        write_string_with_length(state, file, info->name);
        write_function_typed_type_ref(file, state, &info->type);
        fwrite(&info->lineInSourceStart, sizeof(TZrUInt32), 1, file);
        fwrite(&info->lineInSourceEnd, sizeof(TZrUInt32), 1, file);
        fwrite(&pathBindingCount, sizeof(TZrUInt64), 1, file);
        for (TZrUInt64 bindingIndex = 0; bindingIndex < pathBindingCount; bindingIndex++) {
            SZrFunctionCompileTimePathBinding *binding = &info->pathBindings[bindingIndex];

            write_string_with_length(state, file, binding->path);
            fwrite(&binding->targetKind, sizeof(TZrUInt8), 1, file);
            write_string_with_length(state, file, binding->targetName);
        }
    }

    fwrite(&functionCount, sizeof(TZrUInt64), 1, file);
    for (TZrUInt64 index = 0; index < functionCount; index++) {
        SZrFunctionCompileTimeFunctionInfo *info = &function->compileTimeFunctionInfos[index];
        write_string_with_length(state, file, info->name);
        write_function_typed_type_ref(file, state, &info->returnType);
        if (!write_function_metadata_parameters(file, state, info->parameters, info->parameterCount)) {
            return ZR_FALSE;
        }
        fwrite(&info->lineInSourceStart, sizeof(TZrUInt32), 1, file);
        fwrite(&info->lineInSourceEnd, sizeof(TZrUInt32), 1, file);
    }

    return ZR_TRUE;
}

static TZrBool write_function_decorator_metadata(FILE *file, SZrState *state, SZrFunction *function) {
    TZrUInt8 hasDecoratorMetadata = (function != ZR_NULL && function->hasDecoratorMetadata) ? ZR_TRUE : ZR_FALSE;
    TZrUInt64 decoratorCount = function != ZR_NULL ? function->decoratorCount : 0;

    fwrite(&hasDecoratorMetadata, sizeof(TZrUInt8), 1, file);
    if (hasDecoratorMetadata) {
        if (!write_function_constant(file, state, &function->decoratorMetadataValue)) {
            return ZR_FALSE;
        }
    }

    fwrite(&decoratorCount, sizeof(TZrUInt64), 1, file);
    for (TZrUInt64 index = 0; index < decoratorCount; index++) {
        write_string_with_length(state, file, function->decoratorNames[index]);
    }

    return ZR_TRUE;
}

static TZrBool write_function_test_metadata(FILE *file, SZrState *state, SZrFunction *function) {
    TZrUInt64 testCount = function != ZR_NULL ? function->testInfoLength : 0;

    fwrite(&testCount, sizeof(TZrUInt64), 1, file);
    for (TZrUInt64 index = 0; index < testCount; index++) {
        SZrFunctionTestInfo *info = &function->testInfos[index];
        TZrUInt8 hasVariableArguments = info->hasVariableArguments ? ZR_TRUE : ZR_FALSE;

        write_string_with_length(state, file, info->name);
        if (!write_function_metadata_parameters(file, state, info->parameters, info->parameterCount)) {
            return ZR_FALSE;
        }
        fwrite(&hasVariableArguments, sizeof(TZrUInt8), 1, file);
        fwrite(&info->lineInSourceStart, sizeof(TZrUInt32), 1, file);
        fwrite(&info->lineInSourceEnd, sizeof(TZrUInt32), 1, file);
    }

    return ZR_TRUE;
}

static void write_function_member_entries(FILE *file, SZrState *state, SZrFunction *function) {
    TZrUInt64 memberCount = function != ZR_NULL ? function->memberEntryLength : 0;

    fwrite(&memberCount, sizeof(TZrUInt64), 1, file);
    for (TZrUInt64 index = 0; index < memberCount; index++) {
        const SZrFunctionMemberEntry *entry = &function->memberEntries[index];

        write_string_with_length(state, file, entry->symbol);
        fwrite(&entry->entryKind, sizeof(TZrUInt8), 1, file);
        fwrite(&entry->reserved0, sizeof(TZrUInt8), 1, file);
        fwrite(&entry->reserved1, sizeof(TZrUInt16), 1, file);
        fwrite(&entry->prototypeIndex, sizeof(TZrUInt32), 1, file);
        fwrite(&entry->descriptorIndex, sizeof(TZrUInt32), 1, file);
    }
}

static void write_function_semir_metadata(FILE *file, SZrState *state, SZrFunction *function) {
    TZrUInt64 typeCount = function != ZR_NULL ? function->semIrTypeTableLength : 0;
    TZrUInt64 ownershipCount = function != ZR_NULL ? function->semIrOwnershipTableLength : 0;
    TZrUInt64 effectCount = function != ZR_NULL ? function->semIrEffectTableLength : 0;
    TZrUInt64 blockCount = function != ZR_NULL ? function->semIrBlockTableLength : 0;
    TZrUInt64 instructionCount = function != ZR_NULL ? function->semIrInstructionLength : 0;
    TZrUInt64 deoptCount = function != ZR_NULL ? function->semIrDeoptTableLength : 0;

    fwrite(&typeCount, sizeof(TZrUInt64), 1, file);
    for (TZrUInt64 index = 0; index < typeCount; index++) {
        write_function_typed_type_ref(file, state, &function->semIrTypeTable[index]);
    }

    fwrite(&ownershipCount, sizeof(TZrUInt64), 1, file);
    for (TZrUInt64 index = 0; index < ownershipCount; index++) {
        fwrite(&function->semIrOwnershipTable[index].state, sizeof(TZrUInt32), 1, file);
    }

    fwrite(&effectCount, sizeof(TZrUInt64), 1, file);
    for (TZrUInt64 index = 0; index < effectCount; index++) {
        SZrSemIrEffectEntry *entry = &function->semIrEffectTable[index];
        fwrite(&entry->kind, sizeof(TZrUInt32), 1, file);
        fwrite(&entry->instructionIndex, sizeof(TZrUInt32), 1, file);
        fwrite(&entry->ownershipInputIndex, sizeof(TZrUInt32), 1, file);
        fwrite(&entry->ownershipOutputIndex, sizeof(TZrUInt32), 1, file);
    }

    fwrite(&blockCount, sizeof(TZrUInt64), 1, file);
    for (TZrUInt64 index = 0; index < blockCount; index++) {
        SZrSemIrBlockEntry *entry = &function->semIrBlockTable[index];
        fwrite(&entry->blockId, sizeof(TZrUInt32), 1, file);
        fwrite(&entry->firstInstructionIndex, sizeof(TZrUInt32), 1, file);
        fwrite(&entry->instructionCount, sizeof(TZrUInt32), 1, file);
    }

    fwrite(&instructionCount, sizeof(TZrUInt64), 1, file);
    for (TZrUInt64 index = 0; index < instructionCount; index++) {
        SZrSemIrInstruction *entry = &function->semIrInstructions[index];
        fwrite(&entry->opcode, sizeof(TZrUInt32), 1, file);
        fwrite(&entry->execInstructionIndex, sizeof(TZrUInt32), 1, file);
        fwrite(&entry->typeTableIndex, sizeof(TZrUInt32), 1, file);
        fwrite(&entry->effectTableIndex, sizeof(TZrUInt32), 1, file);
        fwrite(&entry->destinationSlot, sizeof(TZrUInt32), 1, file);
        fwrite(&entry->operand0, sizeof(TZrUInt32), 1, file);
        fwrite(&entry->operand1, sizeof(TZrUInt32), 1, file);
        fwrite(&entry->deoptId, sizeof(TZrUInt32), 1, file);
    }

    fwrite(&deoptCount, sizeof(TZrUInt64), 1, file);
    for (TZrUInt64 index = 0; index < deoptCount; index++) {
        SZrSemIrDeoptEntry *entry = &function->semIrDeoptTable[index];
        fwrite(&entry->deoptId, sizeof(TZrUInt32), 1, file);
        fwrite(&entry->execInstructionIndex, sizeof(TZrUInt32), 1, file);
    }
}

static void write_function_callsite_cache_metadata(FILE *file, SZrFunction *function) {
    TZrUInt64 cacheCount = function != ZR_NULL ? function->callSiteCacheLength : 0;

    fwrite(&cacheCount, sizeof(TZrUInt64), 1, file);
    for (TZrUInt64 index = 0; index < cacheCount; index++) {
        const SZrFunctionCallSiteCacheEntry *entry = &function->callSiteCaches[index];
        fwrite(&entry->kind, sizeof(TZrUInt32), 1, file);
        fwrite(&entry->instructionIndex, sizeof(TZrUInt32), 1, file);
        fwrite(&entry->memberEntryIndex, sizeof(TZrUInt32), 1, file);
        fwrite(&entry->deoptId, sizeof(TZrUInt32), 1, file);
        fwrite(&entry->argumentCount, sizeof(TZrUInt32), 1, file);
    }
}

static void write_function_prototypes(SZrState *state, FILE *file, SZrFunction *function) {
    TZrUInt64 prototypesLength = 0;
    TZrUInt64 classCount = 0;
    TZrUInt64 structCount = 0;
    TZrUInt64 prototypeBlobLength = 0;

    if (function->prototypeData != ZR_NULL && function->prototypeCount > 0 && function->prototypeDataLength > 0) {
        prototypeBlobLength = function->prototypeDataLength;
    }

    if (function->prototypeData != ZR_NULL && function->prototypeCount > 0) {
        prototypesLength = function->prototypeCount;

        const TZrByte *prototypeData = function->prototypeData + sizeof(TZrUInt32);
        TZrSize remainingDataSize = function->prototypeDataLength - sizeof(TZrUInt32);
        const TZrByte *currentPos = prototypeData;

        for (TZrUInt32 i = 0; i < prototypesLength; i++) {
            if (remainingDataSize < sizeof(SZrCompiledPrototypeInfo)) {
                break;
            }

            const SZrCompiledPrototypeInfo *protoInfo = (const SZrCompiledPrototypeInfo *) currentPos;
            TZrUInt32 inheritsCount = protoInfo->inheritsCount;
            TZrUInt32 membersCount = protoInfo->membersCount;
            TZrUInt32 decoratorsCount = protoInfo->decoratorsCount;
            TZrSize currentPrototypeSize = sizeof(SZrCompiledPrototypeInfo) +
                                           inheritsCount * sizeof(TZrUInt32) +
                                           decoratorsCount * sizeof(TZrUInt32) +
                                           membersCount * sizeof(SZrCompiledMemberInfo);
            if (remainingDataSize < currentPrototypeSize) {
                break;
            }

            if (protoInfo->type == ZR_OBJECT_PROTOTYPE_TYPE_CLASS) {
                classCount++;
            } else if (protoInfo->type == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT) {
                structCount++;
            }

            currentPos += currentPrototypeSize;
            remainingDataSize -= currentPrototypeSize;
        }
    }

    fwrite(&prototypesLength, sizeof(TZrUInt64), 1, file);

    if (prototypesLength == 0 || function->prototypeData == ZR_NULL || function->prototypeDataLength == 0) {
        TZrUInt64 zero = 0;
        fwrite(&zero, sizeof(TZrUInt64), 1, file);
        fwrite(&zero, sizeof(TZrUInt64), 1, file);
        fwrite(&zero, sizeof(TZrUInt64), 1, file);
        return;
    }

    {
        const TZrByte *prototypeData = function->prototypeData + sizeof(TZrUInt32);
        TZrSize remainingDataSize = function->prototypeDataLength - sizeof(TZrUInt32);
        const TZrByte *currentPos = prototypeData;

        fwrite(&classCount, sizeof(TZrUInt64), 1, file);
        if (classCount > 0) {
            for (TZrUInt32 i = 0; i < prototypesLength; i++) {
                if (remainingDataSize < sizeof(SZrCompiledPrototypeInfo)) {
                    break;
                }

                const SZrCompiledPrototypeInfo *protoInfo = (const SZrCompiledPrototypeInfo *) currentPos;
                TZrUInt32 inheritsCount = protoInfo->inheritsCount;
                TZrUInt32 membersCount = protoInfo->membersCount;
                TZrUInt32 decoratorsCount = protoInfo->decoratorsCount;
                TZrSize currentPrototypeSize = sizeof(SZrCompiledPrototypeInfo) +
                                               inheritsCount * sizeof(TZrUInt32) +
                                               decoratorsCount * sizeof(TZrUInt32) +
                                               membersCount * sizeof(SZrCompiledMemberInfo);
                if (remainingDataSize < currentPrototypeSize) {
                    break;
                }

                if (protoInfo->type == ZR_OBJECT_PROTOTYPE_TYPE_CLASS) {
                    write_prototype_class(state, file, protoInfo, currentPos, function);
                }

                currentPos += currentPrototypeSize;
                remainingDataSize -= currentPrototypeSize;
            }
        }

        currentPos = prototypeData;
        remainingDataSize = function->prototypeDataLength - sizeof(TZrUInt32);
        fwrite(&structCount, sizeof(TZrUInt64), 1, file);
        if (structCount > 0) {
            for (TZrUInt32 i = 0; i < prototypesLength; i++) {
                if (remainingDataSize < sizeof(SZrCompiledPrototypeInfo)) {
                    break;
                }

                const SZrCompiledPrototypeInfo *protoInfo = (const SZrCompiledPrototypeInfo *) currentPos;
                TZrUInt32 inheritsCount = protoInfo->inheritsCount;
                TZrUInt32 membersCount = protoInfo->membersCount;
                TZrUInt32 decoratorsCount = protoInfo->decoratorsCount;
                TZrSize currentPrototypeSize = sizeof(SZrCompiledPrototypeInfo) +
                                               inheritsCount * sizeof(TZrUInt32) +
                                               decoratorsCount * sizeof(TZrUInt32) +
                                               membersCount * sizeof(SZrCompiledMemberInfo);
                if (remainingDataSize < currentPrototypeSize) {
                    break;
                }

                if (protoInfo->type == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT) {
                    write_prototype_struct(state, file, protoInfo, currentPos, function);
                }

                currentPos += currentPrototypeSize;
                remainingDataSize -= currentPrototypeSize;
            }
        }

        fwrite(&prototypeBlobLength, sizeof(TZrUInt64), 1, file);
        if (prototypeBlobLength > 0) {
            fwrite(function->prototypeData, 1, (TZrSize)prototypeBlobLength, file);
        }
    }
}

static TZrBool write_io_function_internal(SZrState *state,
                                          FILE *file,
                                          SZrFunction *function,
                                          const TZrChar *defaultName,
                                          const SZrBinaryWriterOptions *options) {
    if (state == ZR_NULL || file == ZR_NULL || function == ZR_NULL) {
        return ZR_FALSE;
    }

    write_function_name(state, file, function, defaultName);

    {
        TZrUInt64 startLine = function->lineInSourceStart;
        TZrUInt64 endLine = function->lineInSourceEnd;
        TZrUInt64 parametersLength = function->parameterCount;
        TZrUInt64 hasVarArgs = function->hasVariableArguments ? ZR_TRUE : ZR_FALSE;
        TZrUInt32 stackSize = function->stackSize;
        TZrUInt64 instructionsLength = function->instructionsLength;

        fwrite(&startLine, sizeof(TZrUInt64), 1, file);
        fwrite(&endLine, sizeof(TZrUInt64), 1, file);
        fwrite(&parametersLength, sizeof(TZrUInt64), 1, file);
        fwrite(&hasVarArgs, sizeof(TZrUInt64), 1, file);
        fwrite(&stackSize, sizeof(TZrUInt32), 1, file);
        fwrite(&instructionsLength, sizeof(TZrUInt64), 1, file);

        for (TZrUInt64 i = 0; i < instructionsLength; i++) {
            TZrUInt64 rawValue = function->instructionsList[i].value;
            fwrite(&rawValue, sizeof(TZrUInt64), 1, file);
        }
    }

    write_function_local_variables(file, function);
    write_function_closure_variables(state, file, function);
    write_function_exception_metadata(state, file, function);

    {
        TZrUInt64 constantsLength = function->constantValueLength;
        fwrite(&constantsLength, sizeof(TZrUInt64), 1, file);
        for (TZrUInt64 i = 0; i < constantsLength; i++) {
            if (!write_function_constant(file, state, &function->constantValueList[i])) {
                return ZR_FALSE;
            }
        }
    }

    {
        TZrUInt64 exportedVariablesLength = function->exportedVariableLength;
        fwrite(&exportedVariablesLength, sizeof(TZrUInt64), 1, file);
        for (TZrUInt64 i = 0; i < exportedVariablesLength; i++) {
            struct SZrFunctionExportedVariable *exported = &function->exportedVariables[i];
            write_string_with_length(state, file, exported->name);
            fwrite(&exported->stackSlot, sizeof(TZrUInt32), 1, file);
            fwrite(&exported->accessModifier, sizeof(TZrUInt8), 1, file);
            fwrite(&exported->exportKind, sizeof(TZrUInt8), 1, file);
            fwrite(&exported->readiness, sizeof(TZrUInt8), 1, file);
            fwrite(&exported->reserved0, sizeof(TZrUInt8), 1, file);
            fwrite(&exported->callableChildIndex, sizeof(TZrUInt32), 1, file);
        }
    }

    write_function_typed_local_bindings(file, state, function);
    write_function_typed_export_symbols(file, state, function);
    write_function_static_imports(file, state, function);
    write_function_module_entry_effects(file, state, function);
    write_function_exported_callable_summaries(file, state, function);
    write_function_top_level_callable_bindings(file, state, function);
    if (!write_function_parameter_metadata(file, state, function)) {
        return ZR_FALSE;
    }
    if (!write_function_compile_time_metadata(file, state, function)) {
        return ZR_FALSE;
    }
    if (!write_function_test_metadata(file, state, function)) {
        return ZR_FALSE;
    }
    if (!write_function_decorator_metadata(file, state, function)) {
        return ZR_FALSE;
    }
    write_function_member_entries(file, state, function);
    write_function_semir_metadata(file, state, function);
    write_function_callsite_cache_metadata(file, function);

    write_function_prototypes(state, file, function);

    {
        TZrUInt64 closuresLength = function->childFunctionLength;
        fwrite(&closuresLength, sizeof(TZrUInt64), 1, file);
        for (TZrUInt64 i = 0; i < closuresLength; i++) {
            if (!write_io_function_internal(state, file, &function->childFunctionList[i], ZR_NULL, options)) {
                return ZR_FALSE;
            }
        }
    }

    write_function_debug_infos(file, function, options);

    return ZR_TRUE;
}

TZrBool ZrParser_Writer_WriteIoFunction(SZrState *state,
                                        FILE *file,
                                        SZrFunction *function,
                                        const TZrChar *defaultName,
                                        const SZrBinaryWriterOptions *options) {
    return write_io_function_internal(state, file, function, defaultName, options);
}

// 写入明文中间文件 (.zri)
