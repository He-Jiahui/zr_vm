//
// Created by HeJiahui on 2025/6/26.
//
#include "zr_vm_core/debug.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "zr_vm_common/zr_ast_constants.h"
#include "zr_vm_common/zr_debug_conf.h"
#include "zr_vm_common/zr_meta_conf.h"
#include "zr_vm_common/zr_object_conf.h"
#include "zr_vm_common/zr_string_conf.h"
#include "zr_vm_core/constant_reference.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/meta.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

TBool ZrDebugInfoGet(struct SZrState *state, EZrDebugInfoType type, SZrDebugInfo *debugInfo) {
    ZR_TODO_PARAMETER(state);
    ZR_TODO_PARAMETER(type);
    ZR_TODO_PARAMETER(debugInfo);
    return ZR_FALSE;
}

void ZrDebugCallError(struct SZrState *state, struct SZrTypeValue *value) {
    ZR_TODO_PARAMETER(state);
    ZR_TODO_PARAMETER(value);
}

TZrDebugSignal ZrDebugTraceExecution(struct SZrState *state, const TZrInstruction *programCounter) {
    ZR_TODO_PARAMETER(state);
    ZR_TODO_PARAMETER(programCounter);

    return 0;
}

ZR_NO_RETURN void ZrDebugRunError(struct SZrState *state, TNativeString format, ...) {
    if (state == ZR_NULL || format == ZR_NULL) {
        ZR_ABORT();
    }

    // 格式化错误消息
    va_list args;
    va_start(args, format);
    TNativeString errorMessage = ZrNativeStringVFormat(state, format, args);
    va_end(args);

    if (errorMessage == ZR_NULL) {
        // 如果格式化失败，使用默认消息
        errorMessage = "Runtime error";
    }

    // 创建错误消息字符串对象
    SZrString *errorString = ZrStringCreateFromNative(state, errorMessage);
    if (errorString == ZR_NULL) {
        // 如果创建字符串失败，检查是否有 panic handling function
        SZrGlobalState *global = state->global;
        if (global != ZR_NULL && global->panicHandlingFunction != ZR_NULL) {
            ZR_THREAD_UNLOCK(state);
            global->panicHandlingFunction(state);
        }
        ZR_ABORT();
    }

    // 确保栈有足够空间
    ZrFunctionCheckStackAndGc(state, 1, state->stackTop.valuePointer);

    // 将错误消息字符串放到栈上
    SZrTypeValue *errorValue = ZrStackGetValue(state->stackTop.valuePointer);
    ZrValueInitAsRawObject(state, errorValue, ZR_CAST_RAW_OBJECT_AS_SUPER(errorString));
    errorValue->type = ZR_VALUE_TYPE_STRING;
    errorValue->isGarbageCollectable = ZR_TRUE;
    errorValue->isNative = ZR_FALSE;
    state->stackTop.valuePointer++;

    // 检查是否有已注册的异常处理函数
    SZrGlobalState *global = state->global;
    if (global != ZR_NULL && global->panicHandlingFunction != ZR_NULL) {
        // 如果有 panic handling function，先调用它
        // 注意：在调用之前需要 unlock thread
        ZR_THREAD_UNLOCK(state);
        global->panicHandlingFunction(state);
        // panic handling function 调用后，应该 abort
        ZR_ABORT();
    } else {
        // 如果没有 panic handling function，尝试抛出异常
        // 如果异常被 catch 捕获，程序可以继续
        // 如果异常没有被捕获，ZrExceptionThrow 内部会 abort
        ZrExceptionThrow(state, ZR_THREAD_STATUS_RUNTIME_ERROR);
        // 不应该到达这里（因为 ZrExceptionThrow 是 noreturn 或会 longjmp）
        ZR_ABORT();
    }
}


void ZrDebugErrorWhenHandlingError(struct SZrState *state) {
    // TODO:
}


void ZrDebugHook(struct SZrState *state, EZrDebugHookEvent event, TUInt32 line, TUInt32 transferStart,
                 TUInt32 transferCount) {
    FZrDebugHook hook = state->debugHook;
    if (hook && state->allowDebugHook) {
        EZrCallStatus mask = ZR_CALL_STATUS_DEBUG_HOOK;
        SZrCallInfo *callInfo = state->callInfoList;
        TZrMemoryOffset top = ZrStackSavePointerAsOffset(state, state->stackTop.valuePointer);
        TZrMemoryOffset callInfoTop = ZrStackSavePointerAsOffset(state, callInfo->functionTop.valuePointer);
        SZrDebugInfo debugInfo;
        debugInfo.event = event;
        debugInfo.currentLine = line;
        debugInfo.callInfo = callInfo;
        if (transferCount != 0) {
            mask |= ZR_CALL_STATUS_CALL_INFO_TRANSFER;
            callInfo->yieldContext.transferStart = transferStart;
            callInfo->yieldContext.transferCount = transferCount;
        }
        if (ZR_CALL_INFO_IS_VM(callInfo) && state->stackTop.valuePointer < callInfo->functionTop.valuePointer) {
            state->stackTop.valuePointer = state->stackTop.valuePointer + ZR_STACK_NATIVE_CALL_MIN;
        }
        state->allowDebugHook = ZR_FALSE;
        callInfo->callStatus |= mask;
        ZR_THREAD_UNLOCK(state);
        hook(state, &debugInfo);
        ZR_THREAD_LOCK(state);

        ZR_ASSERT(!state->allowDebugHook);
        state->allowDebugHook = ZR_TRUE;
        callInfo->functionTop.valuePointer = ZrStackLoadOffsetToPointer(state, callInfoTop);
        state->stackTop.valuePointer = ZrStackLoadOffsetToPointer(state, top);
        callInfo->callStatus &= ~mask;
    }
}

void ZrDebugHookReturn(struct SZrState *state, struct SZrCallInfo *callInfo, TZrSize resultCount) {
    if (state->debugHookSignal & ZR_DEBUG_HOOK_MASK_RETURN) {
        TZrStackValuePointer stackPointer = callInfo->functionTop.valuePointer - resultCount;
        TUInt32 totalArgumentsCount = 0;
        TInt32 transferStart = 0;
        if (ZR_CALL_INFO_IS_VM(callInfo)) {
            SZrTypeValue *functionValue = ZrStackGetValue(callInfo->functionBase.valuePointer);
            SZrFunction *function = (ZR_CAST_VM_CLOSURE(state, functionValue->value.object))->function;
            if (function->hasVariableArguments) {
                totalArgumentsCount = callInfo->context.context.variableArgumentCount + function->parameterCount + 1;
            }
        }
        callInfo->functionBase.valuePointer += totalArgumentsCount;
        transferStart = ZR_CAST_UINT(stackPointer - callInfo->functionBase.valuePointer);
        ZrDebugHook(state, ZR_DEBUG_HOOK_EVENT_RETURN, -1, transferStart, resultCount);
        callInfo->functionBase.valuePointer -= totalArgumentsCount;
    }
    callInfo = callInfo->previous;
    if (ZR_CALL_INFO_IS_VM(callInfo)) {
        SZrTypeValue *functionValue = ZrStackGetValue(callInfo->functionBase.valuePointer);
        SZrFunction *function = (ZR_CAST_VM_CLOSURE(state, functionValue->value.object))->function;
        state->previousProgramCounter =
                ZR_CAST_INT64(callInfo->context.context.programCounter - function->instructionsList) - 1;
    }
}

// 获取prototype类型名称字符串
static const char *get_prototype_type_name(EZrObjectPrototypeType type) {
    switch (type) {
        case ZR_OBJECT_PROTOTYPE_TYPE_CLASS:
            return "class";
        case ZR_OBJECT_PROTOTYPE_TYPE_STRUCT:
            return "struct";
        case ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE:
            return "interface";
        case ZR_OBJECT_PROTOTYPE_TYPE_ENUM:
            return "enum";
        case ZR_OBJECT_PROTOTYPE_TYPE_MODULE:
            return "module";
        case ZR_OBJECT_PROTOTYPE_TYPE_NATIVE:
            return "native";
        case ZR_OBJECT_PROTOTYPE_TYPE_INVALID:
        default:
            return "unknown";
    }
}

// 输出Prototype信息（增强版本，显示详细信息）
ZR_CORE_API void ZrDebugPrintPrototype(struct SZrState *state, struct SZrObjectPrototype *prototype, FILE *output) {
    if (state == ZR_NULL || prototype == ZR_NULL || output == ZR_NULL) {
        return;
    }

    // 输出名称和类型
    const char *typeName = get_prototype_type_name(prototype->type);
    TNativeString nameStr = ZR_NULL;
    if (prototype->name != ZR_NULL) {
        nameStr = ZrStringGetNativeStringShort(prototype->name);
        if (nameStr == ZR_NULL) {
            nameStr = *ZrStringGetNativeStringLong(prototype->name);
        }
    }
    fprintf(output, "%s %s", typeName, nameStr != ZR_NULL ? nameStr : "<unnamed>");

    // 输出继承链（递归显示所有基类）
    fprintf(output, " ");
    TBool hasInheritance = ZR_FALSE;
    struct SZrObjectPrototype *current = prototype->superPrototype;
    while (current != ZR_NULL) {
        if (hasInheritance) {
            fprintf(output, ", ");
        } else {
            fprintf(output, ": ");
        }
        if (current->name != ZR_NULL) {
            TNativeString superName = ZrStringGetNativeStringShort(current->name);
            if (superName == ZR_NULL) {
                superName = *ZrStringGetNativeStringLong(current->name);
            }
            fprintf(output, "%s", superName != ZR_NULL ? superName : "<unknown>");
        } else {
            fprintf(output, "<unnamed>");
        }
        hasInheritance = ZR_TRUE;
        current = current->superPrototype;
    }
    fprintf(output, "\n");
    fprintf(output, "{\n");

    // 输出Struct的字段信息（包含偏移量）
    if (prototype->type == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT) {
        SZrStructPrototype *structProto = (SZrStructPrototype *) prototype;
        if (structProto->keyOffsetMap.isValid && structProto->keyOffsetMap.buckets != ZR_NULL &&
            structProto->keyOffsetMap.elementCount > 0) {
            fprintf(output, "  // Fields (with offsets):\n");
            for (TZrSize i = 0; i < structProto->keyOffsetMap.capacity; i++) {
                SZrHashKeyValuePair *pair = structProto->keyOffsetMap.buckets[i];
                while (pair != ZR_NULL) {
                    if (pair->key.type == ZR_VALUE_TYPE_STRING && pair->key.value.object != ZR_NULL) {
                        SZrString *fieldName = ZR_CAST_STRING(state, pair->key.value.object);
                        TNativeString fieldNameStr = ZrStringGetNativeStringShort(fieldName);
                        if (fieldNameStr == ZR_NULL) {
                            fieldNameStr = *ZrStringGetNativeStringLong(fieldName);
                        }
                        TUInt64 offset = pair->value.value.nativeObject.nativeUInt64;
                        fprintf(output, "    %s (offset: %llu bytes)\n",
                                fieldNameStr != ZR_NULL ? fieldNameStr : "<unknown>", (unsigned long long) offset);
                    }
                    pair = pair->next;
                }
            }
        } else {
            fprintf(output, "  // Fields: (none)\n");
        }
    }

    // 输出Meta方法（包含函数信息）
    fprintf(output, "  // Meta Methods:\n");
    TBool hasMeta = ZR_FALSE;
    for (EZrMetaType metaType = 0; metaType < ZR_META_ENUM_MAX; metaType++) {
        if (prototype->metaTable.metas[metaType] != ZR_NULL) {
            hasMeta = ZR_TRUE;
            const char *metaName = CZrMetaName[metaType];
            SZrMeta *meta = prototype->metaTable.metas[metaType];
            if (meta->function != ZR_NULL && meta->function->functionName != ZR_NULL) {
                TNativeString funcName = ZrStringGetNativeStringShort(meta->function->functionName);
                if (funcName == ZR_NULL) {
                    funcName = *ZrStringGetNativeStringLong(meta->function->functionName);
                }
                fprintf(output, "    @%s -> %s (params: %u)\n", metaName != ZR_NULL ? metaName : "<unknown>",
                        funcName != ZR_NULL ? funcName : "<anonymous>", (unsigned int) meta->function->parameterCount);
            } else {
                fprintf(output, "    @%s -> <anonymous> (params: %u)\n", metaName != ZR_NULL ? metaName : "<unknown>",
                        meta->function != ZR_NULL ? (unsigned int) meta->function->parameterCount : 0);
            }
        }
    }
    if (!hasMeta) {
        fprintf(output, "    (none)\n");
    }

    fprintf(output, "}\n");
}

// 输出Object信息
ZR_CORE_API void ZrDebugPrintObject(struct SZrState *state, struct SZrObject *object, FILE *output) {
    if (state == ZR_NULL || object == ZR_NULL || output == ZR_NULL) {
        return;
    }

    // 输出prototype信息
    if (object->prototype != ZR_NULL && object->prototype->name != ZR_NULL) {
        TNativeString protoName = ZrStringGetNativeStringShort(object->prototype->name);
        if (protoName == ZR_NULL) {
            protoName = *ZrStringGetNativeStringLong(object->prototype->name);
        }
        const char *typeName = get_prototype_type_name(object->prototype->type);
        fprintf(output, "%s %s", typeName, protoName != ZR_NULL ? protoName : "<unnamed>");
    } else {
        fprintf(output, "object");
    }

    fprintf(output, " {\n");

    // 输出字段值
    if (object->nodeMap.isValid && object->nodeMap.buckets != ZR_NULL && object->nodeMap.elementCount > 0) {
        fprintf(output, "  // Fields:\n");
        TZrSize count = 0;

        for (TZrSize i = 0; i < object->nodeMap.capacity && count < kZrDebugMaxFields; i++) {
            SZrHashKeyValuePair *pair = object->nodeMap.buckets[i];
            while (pair != ZR_NULL && count < kZrDebugMaxFields) {
                // 输出键名
                if (pair->key.type == ZR_VALUE_TYPE_STRING && pair->key.value.object != ZR_NULL) {
                    SZrString *keyStr = ZR_CAST_STRING(state, pair->key.value.object);
                    TNativeString keyName = ZrStringGetNativeStringShort(keyStr);
                    if (keyName == ZR_NULL) {
                        keyName = *ZrStringGetNativeStringLong(keyStr);
                    }
                    fprintf(output, "    %s = ", keyName != ZR_NULL ? keyName : "<unknown>");

                    // 输出值
                    SZrString *valueStr = ZrValueConvertToString(state, &pair->value);
                    if (valueStr != ZR_NULL) {
                        TNativeString valueName = ZrStringGetNativeStringShort(valueStr);
                        if (valueName == ZR_NULL) {
                            valueName = *ZrStringGetNativeStringLong(valueStr);
                        }
                        fprintf(output, "%s", valueName != ZR_NULL ? valueName : "<unknown>");
                    } else {
                        // 如果无法转换为字符串，输出类型信息
                        fprintf(output, "<%s>", pair->value.type < ZR_VALUE_TYPE_ENUM_MAX ? "value" : "unknown");
                    }
                    fprintf(output, "\n");
                    count++;
                }
                pair = pair->next;
            }
        }

        if (count >= kZrDebugMaxFields && object->nodeMap.elementCount > kZrDebugMaxFields) {
            fprintf(output, "    ... (and %zu more fields)\n",
                    (size_t) (object->nodeMap.elementCount - kZrDebugMaxFields));
        }
    } else {
        fprintf(output, "  // (no fields)\n");
    }

    fprintf(output, "}\n");
}

// 获取访问修饰符名称
static const char *get_access_modifier_name(TUInt32 modifier) {
    switch (modifier) {
        case ZR_ACCESS_CONSTANT_PUBLIC:
            return "public";
        case ZR_ACCESS_CONSTANT_PRIVATE:
            return "private";
        case ZR_ACCESS_CONSTANT_PROTECTED:
            return "protected";
        default:
            return "unknown";
    }
}

// 从常量池中解析并字符串化prototype信息
ZR_CORE_API void ZrDebugPrintPrototypeFromConstants(struct SZrState *state, struct SZrFunction *entryFunction,
                                                    FILE *output) {
    if (state == ZR_NULL || entryFunction == ZR_NULL || output == ZR_NULL) {
        return;
    }

    // 检查常量池是否有效
    if (entryFunction->constantValueList == ZR_NULL || entryFunction->constantValueLength == 0) {
        fprintf(output, "// No constants found\n");
        return;
    }

    // 优先使用新的prototypeConstantIndices机制
    TUInt32 prototypeCount = 0;

    if (entryFunction->prototypeConstantIndices != ZR_NULL && entryFunction->prototypeConstantIndicesLength > 0) {
        prototypeCount = entryFunction->prototypeConstantIndicesLength;
        fprintf(output, "// ========== PROTOTYPES FROM CONSTANTS (new format, count: %u) ==========\n",
                (unsigned int) prototypeCount);

        // 遍历每个prototype索引
        for (TUInt32 i = 0; i < prototypeCount; i++) {
            TUInt32 constantIndex = entryFunction->prototypeConstantIndices[i];
            if (constantIndex >= entryFunction->constantValueLength) {
                fprintf(output, "// Error: Invalid prototype constant index %u (>= %u)\n", (unsigned int) constantIndex,
                        (unsigned int) entryFunction->constantValueLength);
                continue;
            }

            const SZrTypeValue *constant = &entryFunction->constantValueList[constantIndex];
            if (constant->type != ZR_VALUE_TYPE_STRING) {
                fprintf(output, "// Error: Prototype constant at index %u is not a string (type: %u)\n",
                        (unsigned int) constantIndex, (unsigned int) constant->type);
                continue;
            }

            // 解析prototype二进制数据
            SZrString *serializedString = ZR_CAST_STRING(state, constant->value.object);
            if (serializedString == ZR_NULL) {
                fprintf(output, "// Error: Failed to cast prototype constant at index %u to string\n",
                        (unsigned int) constantIndex);
                continue;
            }

            TNativeString strData = ZR_NULL;
            TZrSize strLength = 0;
            if (serializedString->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
                strData = ZrStringGetNativeStringShort(serializedString);
                strLength = (TZrSize) serializedString->shortStringLength;
            } else {
                TNativeString *longStrPtr = ZrStringGetNativeStringLong(serializedString);
                if (longStrPtr != ZR_NULL) {
                    strData = *longStrPtr;
                    strLength = serializedString->longStringLength;
                }
            }

            if (strData == ZR_NULL || strLength < sizeof(SZrCompiledPrototypeInfo)) {
                fprintf(output, "// Error: Invalid prototype binary data at index %u (size: %zu, expected: >= %zu)\n",
                        (unsigned int) constantIndex, (TZrSize) strLength, sizeof(SZrCompiledPrototypeInfo));
                continue;
            }

            // 解析SZrCompiledPrototypeInfo
            const SZrCompiledPrototypeInfo *protoInfo = (const SZrCompiledPrototypeInfo *) strData;
            TUInt32 nameStringIndex = protoInfo->nameStringIndex;
            TUInt32 type = protoInfo->type;
            TUInt32 accessModifier = protoInfo->accessModifier;
            TUInt32 inheritsCount = protoInfo->inheritsCount;
            TUInt32 membersCount = protoInfo->membersCount;

            // 读取类型名称
            TNativeString typeNameStr = "<unknown>";
            if (nameStringIndex < entryFunction->constantValueLength) {
                const SZrTypeValue *nameConstant = &entryFunction->constantValueList[nameStringIndex];
                if (nameConstant->type == ZR_VALUE_TYPE_STRING) {
                    SZrString *typeName = ZR_CAST_STRING(state, nameConstant->value.object);
                    if (typeName != ZR_NULL) {
                        TNativeString tmpStr = ZrStringGetNativeStringShort(typeName);
                        if (tmpStr == ZR_NULL) {
                            tmpStr = *ZrStringGetNativeStringLong(typeName);
                        }
                        if (tmpStr != ZR_NULL) {
                            typeNameStr = tmpStr;
                        }
                    }
                }
            }

            const char *prototypeTypeName = "unknown";
            if (type == ZR_OBJECT_PROTOTYPE_TYPE_CLASS)
                prototypeTypeName = "class";
            else if (type == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT)
                prototypeTypeName = "struct";
            else if (type == ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE)
                prototypeTypeName = "interface";
            else if (type == ZR_OBJECT_PROTOTYPE_TYPE_MODULE)
                prototypeTypeName = "module";
            else if (type == ZR_OBJECT_PROTOTYPE_TYPE_ENUM)
                prototypeTypeName = "enum";
            else if (type == ZR_OBJECT_PROTOTYPE_TYPE_NATIVE)
                prototypeTypeName = "native";

            fprintf(output, "// --- Prototype %u (constant index: %u) ---\n", (unsigned int) (i + 1),
                    (unsigned int) constantIndex);
            fprintf(output, "%s %s", prototypeTypeName, typeNameStr);

            // 读取继承类型
            if (inheritsCount > 0) {
                fprintf(output, " : ");
                const TUInt32 *inheritIndices = (const TUInt32 *) (strData + sizeof(SZrCompiledPrototypeInfo));
                for (TUInt32 j = 0; j < inheritsCount; j++) {
                    if (j > 0)
                        fprintf(output, ", ");
                    TUInt32 inheritStringIndex = inheritIndices[j];
                    if (inheritStringIndex < entryFunction->constantValueLength) {
                        const SZrTypeValue *inheritConstant = &entryFunction->constantValueList[inheritStringIndex];
                        if (inheritConstant->type == ZR_VALUE_TYPE_STRING) {
                            SZrString *inheritTypeName = ZR_CAST_STRING(state, inheritConstant->value.object);
                            if (inheritTypeName != ZR_NULL) {
                                TNativeString inheritStr = ZrStringGetNativeStringShort(inheritTypeName);
                                if (inheritStr == ZR_NULL) {
                                    inheritStr = *ZrStringGetNativeStringLong(inheritTypeName);
                                }
                                fprintf(output, "%s", inheritStr != ZR_NULL ? inheritStr : "<unknown>");
                            }
                        }
                    }
                }
            }

            fprintf(output, " {\n");

            // 输出访问修饰符名称
            const char *accessName = "unknown";
            if (accessModifier == ZR_ACCESS_CONSTANT_PUBLIC)
                accessName = "public";
            else if (accessModifier == ZR_ACCESS_CONSTANT_PRIVATE)
                accessName = "private";
            else if (accessModifier == ZR_ACCESS_CONSTANT_PROTECTED)
                accessName = "protected";
            fprintf(output, "  access: %s (%u),\n", accessName, (unsigned int) accessModifier);

            // 计算成员数据的起始位置
            // 布局：SZrCompiledPrototypeInfo(20字节) + [inheritsCount * 4字节] + [membersCount *
            // SZrCompiledMemberInfo(44字节)]
            TZrSize inheritArraySize = inheritsCount * sizeof(TUInt32);
            const TByte *membersData = (const TByte *) (strData + sizeof(SZrCompiledPrototypeInfo) + inheritArraySize);
            TZrSize expectedTotalSize =
                    sizeof(SZrCompiledPrototypeInfo) + inheritArraySize + membersCount * sizeof(SZrCompiledMemberInfo);

            if (strLength < expectedTotalSize) {
                fprintf(output, "  // Warning: Binary data size (%zu) < expected size (%zu), cannot read all members\n",
                        (TZrSize) strLength, expectedTotalSize);
                fprintf(output, "  members: %u (partial data)\n", (unsigned int) membersCount);
                fprintf(output, "}\n\n");
                continue;
            }

            fprintf(output, "  members: [\n");

            // 遍历每个成员
            for (TUInt32 j = 0; j < membersCount; j++) {
                const SZrCompiledMemberInfo *memberInfo =
                        (const SZrCompiledMemberInfo *) (membersData + j * sizeof(SZrCompiledMemberInfo));

                TUInt32 memberType = memberInfo->memberType;
                TUInt32 nameStringIndex = memberInfo->nameStringIndex;
                TUInt32 memberAccess = memberInfo->accessModifier;
                TUInt32 isStatic = memberInfo->isStatic;
                TUInt32 fieldTypeNameStringIndex = memberInfo->fieldTypeNameStringIndex;
                TUInt32 fieldOffset = memberInfo->fieldOffset;
                TUInt32 fieldSize = memberInfo->fieldSize;
                TUInt32 isMetaMethod = memberInfo->isMetaMethod;
                TUInt32 metaType = memberInfo->metaType;
                TUInt32 functionConstantIndex = memberInfo->functionConstantIndex;
                TUInt32 parameterCount = memberInfo->parameterCount;

                // 读取成员名称
                TNativeString memberNameStr = "<unnamed>";
                if (nameStringIndex > 0 && nameStringIndex < entryFunction->constantValueLength) {
                    const SZrTypeValue *nameConstant = &entryFunction->constantValueList[nameStringIndex];
                    if (nameConstant->type == ZR_VALUE_TYPE_STRING) {
                        SZrString *memberName = ZR_CAST_STRING(state, nameConstant->value.object);
                        if (memberName != ZR_NULL) {
                            TNativeString tmpStr = ZrStringGetNativeStringShort(memberName);
                            if (tmpStr == ZR_NULL) {
                                tmpStr = *ZrStringGetNativeStringLong(memberName);
                            }
                            if (tmpStr != ZR_NULL) {
                                memberNameStr = tmpStr;
                            }
                        }
                    }
                }

                // 根据成员类型输出信息（使用常量值）
                const char *memberTypeName = "unknown";
                // 使用switch或if-else匹配所有可能的类型
                if (memberType == ZR_AST_CONSTANT_STRUCT_FIELD)
                    memberTypeName = "STRUCT_FIELD";
                else if (memberType == ZR_AST_CONSTANT_STRUCT_METHOD)
                    memberTypeName = "STRUCT_METHOD";
                else if (memberType == ZR_AST_CONSTANT_STRUCT_META_FUNCTION)
                    memberTypeName = "STRUCT_META_FUNCTION";
                else if (memberType == ZR_AST_CONSTANT_CLASS_FIELD)
                    memberTypeName = "CLASS_FIELD";
                else if (memberType == ZR_AST_CONSTANT_CLASS_METHOD)
                    memberTypeName = "CLASS_METHOD";
                else if (memberType == ZR_AST_CONSTANT_CLASS_PROPERTY)
                    memberTypeName = "CLASS_PROPERTY";
                else if (memberType == ZR_AST_CONSTANT_CLASS_META_FUNCTION)
                    memberTypeName = "CLASS_META_FUNCTION";
                // 暂时兼容其他值（可能是不同的enum值或错误的存储）
                else {
                    // 尝试根据常见值猜测类型（调试用）
                    if (memberType == 10)
                        memberTypeName = "STRUCT_FIELD? (10)"; // 可能是错误的值
                    else if (memberType == 11)
                        memberTypeName = "STRUCT_METHOD? (11)"; // 可能是错误的值
                    else if (memberType == 12)
                        memberTypeName = "STRUCT_META_FUNCTION? (12)"; // 可能是错误的值
                }

                fprintf(output, "    {\n");
                fprintf(output, "      type: %s (%u),\n", memberTypeName, (unsigned int) memberType);
                fprintf(output, "      name: \"%s\",\n", memberNameStr);

                // 访问修饰符
                const char *memberAccessName = "unknown";
                if (memberAccess == ZR_ACCESS_CONSTANT_PUBLIC)
                    memberAccessName = "public";
                else if (memberAccess == ZR_ACCESS_CONSTANT_PRIVATE)
                    memberAccessName = "private";
                else if (memberAccess == ZR_ACCESS_CONSTANT_PROTECTED)
                    memberAccessName = "protected";
                fprintf(output, "      access: %s (%u),\n", memberAccessName, (unsigned int) memberAccess);
                fprintf(output, "      static: %s,\n", isStatic ? "true" : "false");

                // 判断是否为字段类型（包括可能的错误值10）
                TBool isFieldType = (memberType == ZR_AST_CONSTANT_STRUCT_FIELD ||
                                     memberType == ZR_AST_CONSTANT_CLASS_FIELD || memberType == 10);
                // 判断是否为方法类型（包括可能的错误值11）
                TBool isMethodType = (memberType == ZR_AST_CONSTANT_STRUCT_METHOD ||
                                      memberType == ZR_AST_CONSTANT_CLASS_METHOD || memberType == 11);
                // 判断是否为元方法类型（包括可能的错误值12）
                TBool isMetaFunctionType = (memberType == ZR_AST_CONSTANT_STRUCT_META_FUNCTION ||
                                            memberType == ZR_AST_CONSTANT_CLASS_META_FUNCTION || memberType == 12);

                // 如果是字段
                if (isFieldType) {
                    // 读取字段类型名称
                    TNativeString fieldTypeNameStr = "<unknown>";
                    if (fieldTypeNameStringIndex > 0 && fieldTypeNameStringIndex < entryFunction->constantValueLength) {
                        const SZrTypeValue *fieldTypeConstant =
                                &entryFunction->constantValueList[fieldTypeNameStringIndex];
                        if (fieldTypeConstant->type == ZR_VALUE_TYPE_STRING) {
                            SZrString *fieldTypeName = ZR_CAST_STRING(state, fieldTypeConstant->value.object);
                            if (fieldTypeName != ZR_NULL) {
                                TNativeString tmpStr = ZrStringGetNativeStringShort(fieldTypeName);
                                if (tmpStr == ZR_NULL) {
                                    tmpStr = *ZrStringGetNativeStringLong(fieldTypeName);
                                }
                                if (tmpStr != ZR_NULL) {
                                    fieldTypeNameStr = tmpStr;
                                }
                            }
                        }
                    }
                    fprintf(output, "      fieldType: \"%s\",\n", fieldTypeNameStr);
                    fprintf(output, "      fieldOffset: %u,\n", (unsigned int) fieldOffset);
                    fprintf(output, "      fieldSize: %u\n", (unsigned int) fieldSize);
                }
                // 如果是方法
                else if (isMethodType) {
                    fprintf(output, "      isMetaMethod: false,\n");
                    fprintf(output, "      functionConstantIndex: %u,\n", (unsigned int) functionConstantIndex);
                    fprintf(output, "      parameterCount: %u\n", (unsigned int) parameterCount);
                }
                // 如果是元方法
                else if (isMetaFunctionType) {
                    fprintf(output, "      isMetaMethod: true,\n");
                    // 输出元方法类型名称（使用CZrMetaName数组）
                    const char *metaTypeName = "unknown";
                    if (metaType < ZR_META_ENUM_MAX) {
                        metaTypeName = CZrMetaName[metaType];
                    }
                    fprintf(output, "      metaType: %s (%u),\n", metaTypeName != ZR_NULL ? metaTypeName : "unknown",
                            (unsigned int) metaType);
                    fprintf(output, "      functionConstantIndex: %u,\n", (unsigned int) functionConstantIndex);
                    fprintf(output, "      parameterCount: %u\n", (unsigned int) parameterCount);
                }
                // 如果是属性
                else if (memberType == ZR_AST_CONSTANT_CLASS_PROPERTY) {
                    fprintf(output,
                            "      // Property: getter/setter function indices not yet stored in member info\n");
                } else {
                    // 未知类型，输出所有可用信息
                    fprintf(output, "      // Unknown member type, showing all fields:\n");
                    fprintf(output, "      fieldTypeNameStringIndex: %u,\n", (unsigned int) fieldTypeNameStringIndex);
                    fprintf(output, "      fieldOffset: %u,\n", (unsigned int) fieldOffset);
                    fprintf(output, "      fieldSize: %u,\n", (unsigned int) fieldSize);
                    fprintf(output, "      isMetaMethod: %s,\n", isMetaMethod ? "true" : "false");
                    fprintf(output, "      metaType: %u,\n", (unsigned int) metaType);
                    fprintf(output, "      functionConstantIndex: %u,\n", (unsigned int) functionConstantIndex);
                    fprintf(output, "      parameterCount: %u\n", (unsigned int) parameterCount);
                }

                fprintf(output, "    }%s\n", (j < membersCount - 1) ? "," : "");
            }

            fprintf(output, "  ]\n");
            fprintf(output, "}\n\n");
        }
        return;
    }

    // 没有新格式数据，直接返回
    fprintf(output, "// No prototypes found in constants\n");
    fprintf(output, "// ========== END OF PROTOTYPES ==========\n");
}
