//
// Created by Auto on 2025/01/XX.
//

#include "zr_vm_parser/writer.h"

#include "zr_vm_core/closure.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/io.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/ownership.h"
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

#include <stdio.h>
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
        if (rawObj->type == ZR_VALUE_TYPE_STRING) {
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

// 辅助函数：写入成员声明（FIELD/METHOD/PROPERTY/META）
static void write_member_declare(SZrState *state, FILE *file, const SZrCompiledMemberInfo *memberInfo, SZrFunction *function) {
    TZrUInt32 memberType = memberInfo->memberType;
    TZrUInt32 nameStringIndex = memberInfo->nameStringIndex;
    
    // 确定EZrIoMemberDeclareType
    TZrUInt32 ioMemberType;
    if (memberType == ZR_AST_CONSTANT_STRUCT_FIELD || memberType == ZR_AST_CONSTANT_CLASS_FIELD) {
        ioMemberType = ZR_IO_MEMBER_DECLARE_TYPE_FIELD;
    } else if (memberType == ZR_AST_CONSTANT_STRUCT_METHOD || memberType == ZR_AST_CONSTANT_CLASS_METHOD) {
        ioMemberType = ZR_IO_MEMBER_DECLARE_TYPE_METHOD;
    } else if (memberType == ZR_AST_CONSTANT_CLASS_PROPERTY) {
        ioMemberType = ZR_IO_MEMBER_DECLARE_TYPE_PROPERTY;
    } else if (memberType == ZR_AST_CONSTANT_STRUCT_META_FUNCTION || memberType == ZR_AST_CONSTANT_CLASS_META_FUNCTION) {
        ioMemberType = ZR_IO_MEMBER_DECLARE_TYPE_META;
    } else {
        // 未知类型，跳过
        return;
    }
    
    // TYPE [4]
    fwrite(&ioMemberType, sizeof(EZrIoMemberDeclareType), 1, file);
    
    // 根据类型写入不同的数据
    switch (ioMemberType) {
        case ZR_IO_MEMBER_DECLARE_TYPE_FIELD: {
            // .FIELD: NAME [string]
            SZrString *fieldName = get_string_from_constant(state, function, nameStringIndex);
            write_string_with_length(state, file, fieldName);
            break;
        }
        case ZR_IO_MEMBER_DECLARE_TYPE_METHOD: {
            // .METHOD: NAME [string], FUNCTIONS_LENGTH [8], FUNCTIONS [.FUNCTION]
            // 函数引用需要从childFunctionList获取
            SZrString *methodName = get_string_from_constant(state, function, nameStringIndex);
            write_string_with_length(state, file, methodName);
            
            // 从functionConstantIndex获取函数引用
            // functionConstantIndex可能指向：
            // 1. 常量池中的函数对象（ZR_VALUE_TYPE_FUNCTION）
            // 2. 常量池中的引用路径（需要解析）
            TZrUInt32 functionConstantIndex = memberInfo->functionConstantIndex;
            TZrSize functionsLength = 0;
            
            if (functionConstantIndex > 0 && functionConstantIndex < function->constantValueLength) {
                const SZrTypeValue *funcConstant = &function->constantValueList[functionConstantIndex];
                if (funcConstant->type == ZR_VALUE_TYPE_FUNCTION && funcConstant->value.object != ZR_NULL) {
                    // 常量池中直接存储了函数对象
                    functionsLength = 1;
                } else if (funcConstant->type == ZR_VALUE_TYPE_STRING) {
                    // 可能是引用路径（序列化为字符串）
                    // 需要解析引用路径来找到childFunctionList中的函数
                    // TODO: 这里暂时标记为有函数引用，但实际写入需要解析路径
                    functionsLength = 1;
                }
            }
            
            // 如果functionConstantIndex为0或无效，尝试通过方法名在childFunctionList中查找
            if (functionsLength == 0 && methodName != ZR_NULL && function->childFunctionLength > 0) {
                for (TZrUInt32 i = 0; i < function->childFunctionLength; i++) {
                    SZrFunction *childFunc = &function->childFunctionList[i];
                    if (childFunc->functionName != ZR_NULL && ZrCore_String_Equal(childFunc->functionName, methodName)) {
                        functionsLength = 1;
                        break;
                    }
                }
            }
            
            fwrite(&functionsLength, sizeof(TZrSize), 1, file);
            
            // 写入函数引用（如果有）
            if (functionsLength > 0) {
                // 注意：完整的函数序列化需要递归调用write_function
                // TODO: 这里暂时写入函数索引或占位符
                // 实际实现需要：
                // 1. 如果常量池中是函数对象，直接序列化
                // 2. 如果是引用路径，解析路径找到函数后序列化
                // 3. 如果通过方法名找到，序列化对应的childFunction
                // TODO: 由于函数序列化比较复杂，这里暂时跳过，后续可以完善
            }
            break;
        }
        case ZR_IO_MEMBER_DECLARE_TYPE_PROPERTY: {
            // .PROPERTY: NAME [string], PROPERTY_TYPE [4], GETTER_FUNCTION [.FUNCTION], SETTER_FUNCTION [.FUNCTION]
            SZrString *propertyName = get_string_from_constant(state, function, nameStringIndex);
            write_string_with_length(state, file, propertyName);
            
            // PROPERTY_TYPE [4] (目前设为0)
            TZrUInt32 propertyType = 0;
            fwrite(&propertyType, sizeof(TZrUInt32), 1, file);
            
            // GETTER和SETTER函数引用需要从childFunctionList获取
            // 注意：property的getter/setter可能通过不同的机制存储
            // TODO: 这里暂时写入空的函数占位符，因为property的getter/setter存储方式可能不同
            // 实际实现需要：
            // 1. 从memberInfo中获取getter/setter的函数引用索引
            // 2. 从常量池或childFunctionList中查找对应的函数
            // 3. 序列化函数对象
            // TODO: 由于property的getter/setter可能使用不同的存储机制，这里暂时跳过
            // TODO: GETTER_FUNCTION [.FUNCTION] - 跳过（需要完整的函数序列化）
            // TODO: SETTER_FUNCTION [.FUNCTION] - 跳过（需要完整的函数序列化）
            break;
        }
        case ZR_IO_MEMBER_DECLARE_TYPE_META: {
            // .META: META_TYPE [4], FUNCTIONS_LENGTH [8], FUNCTIONS [.FUNCTION]
            TZrUInt32 metaType = memberInfo->metaType;
            fwrite(&metaType, sizeof(TZrUInt32), 1, file);
            
            // FUNCTIONS_LENGTH [8] (目前设为0，函数引用暂不支持)
            TZrSize functionsLength = 0;
            fwrite(&functionsLength, sizeof(TZrSize), 1, file);
            break;
        }
        default:
            break;
    }
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
    TZrSize declaresLength = protoInfo->membersCount;
    fwrite(&declaresLength, sizeof(TZrSize), 1, file);
    
    // DECLARES [.CLASS_DECLARE] [.FIELD|.PROPERTY|.METHOD|.META]
    if (declaresLength > 0) {
        TZrSize inheritArraySize = superClassLength * sizeof(TZrUInt32);
        const SZrCompiledMemberInfo *members = (const SZrCompiledMemberInfo *)(data + sizeof(SZrCompiledPrototypeInfo) + inheritArraySize);
        for (TZrUInt32 i = 0; i < declaresLength; i++) {
            write_member_declare(state, file, &members[i], function);
        }
    }
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
    TZrSize declaresLength = protoInfo->membersCount;
    fwrite(&declaresLength, sizeof(TZrSize), 1, file);
    
    // DECLARES [.STRUCT_DECLARE] [.FIELD|.METHOD|.META]
    if (declaresLength > 0) {
        TZrSize inheritArraySize = superStructLength * sizeof(TZrUInt32);
        const SZrCompiledMemberInfo *members = (const SZrCompiledMemberInfo *)(data + sizeof(SZrCompiledPrototypeInfo) + inheritArraySize);
        for (TZrUInt32 i = 0; i < declaresLength; i++) {
            write_member_declare(state, file, &members[i], function);
        }
    }
}

static TZrBool write_io_function(SZrState *state, FILE *file, SZrFunction *function, const TZrChar *defaultName);

static TZrUInt64 writer_get_serializable_native_helper_id(FZrNativeFunction function) {
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

    return ZR_IO_NATIVE_HELPER_NONE;
}

static void write_function_name(SZrState *state, FILE *file, SZrFunction *function, const TZrChar *defaultName) {
    if (function != ZR_NULL && function->functionName != ZR_NULL) {
        write_string_with_length(state, file, function->functionName);
        return;
    }

    if (defaultName != ZR_NULL) {
        SZrString *fallbackName = ZrCore_String_Create(state, defaultName, strlen(defaultName));
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
        TZrUInt64 instructionStart = local->offsetActivate;
        TZrUInt64 instructionEnd = local->offsetDead;
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

        fwrite(&instructionStart, sizeof(TZrUInt64), 1, file);
        fwrite(&instructionEnd, sizeof(TZrUInt64), 1, file);
        fwrite(&startLineLocal, sizeof(TZrUInt64), 1, file);
        fwrite(&endLineLocal, sizeof(TZrUInt64), 1, file);
    }
}

static void write_function_constant(FILE *file, SZrState *state, SZrTypeValue *constant) {
    TZrUInt32 type = (TZrUInt32) constant->type;
    TZrUInt64 startLineConst = 0;
    TZrUInt64 endLineConst = 0;
    fwrite(&type, sizeof(TZrUInt32), 1, file);

    switch (constant->type) {
        case ZR_VALUE_TYPE_NULL:
            break;
        case ZR_VALUE_TYPE_BOOL: {
            TZrUInt8 boolValue = constant->value.nativeObject.nativeBool ? ZR_TRUE : ZR_FALSE;
            fwrite(&boolValue, sizeof(TZrUInt8), 1, file);
            break;
        }
        case ZR_VALUE_TYPE_INT8:
        case ZR_VALUE_TYPE_INT16:
        case ZR_VALUE_TYPE_INT32:
        case ZR_VALUE_TYPE_INT64:
            fwrite(&constant->value, sizeof(TZrInt64), 1, file);
            break;
        case ZR_VALUE_TYPE_FLOAT:
        case ZR_VALUE_TYPE_DOUBLE:
            fwrite(&constant->value, sizeof(TZrDouble), 1, file);
            break;
        case ZR_VALUE_TYPE_STRING: {
            SZrString *str = ZR_NULL;
            if (constant->value.object != ZR_NULL) {
                SZrRawObject *rawObj = constant->value.object;
                if (rawObj->type == ZR_RAW_OBJECT_TYPE_STRING) {
                    str = ZR_CAST_STRING(state, rawObj);
                }
            }
            write_string_with_length(state, file, str);
            break;
        }
        case ZR_VALUE_TYPE_FUNCTION:
        case ZR_VALUE_TYPE_CLOSURE: {
            TZrBool hasFunctionValue = ZR_FALSE;
            SZrFunction *functionValue = ZR_NULL;
            TZrUInt64 helperId = ZR_IO_NATIVE_HELPER_NONE;
            if (constant->value.object != ZR_NULL) {
                SZrRawObject *rawObj = constant->value.object;
                if (rawObj->type == ZR_RAW_OBJECT_TYPE_FUNCTION) {
                    functionValue = ZR_CAST(SZrFunction *, rawObj);
                    hasFunctionValue = ZR_TRUE;
                } else if (rawObj->type == ZR_RAW_OBJECT_TYPE_CLOSURE) {
                    if (constant->isNative) {
                        SZrClosureNative *nativeClosure = ZR_CAST_NATIVE_CLOSURE(state, rawObj);
                        if (nativeClosure != ZR_NULL) {
                            helperId = writer_get_serializable_native_helper_id(nativeClosure->nativeFunction);
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
                write_io_function(state, file, functionValue, ZR_NULL);
            }
            startLineConst = helperId;
            break;
        }
        case ZR_VALUE_TYPE_NATIVE_POINTER: {
            FZrNativeFunction nativeFunction = ZR_CAST_PTR(constant->value.nativeObject.nativePointer);
            startLineConst = writer_get_serializable_native_helper_id(nativeFunction);
            break;
        }
        default:
            break;
    }

    fwrite(&startLineConst, sizeof(TZrUInt64), 1, file);
    fwrite(&endLineConst, sizeof(TZrUInt64), 1, file);
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
        write_function_typed_type_ref(file, state, &symbol->valueType);
        fwrite(&parameterCount, sizeof(TZrUInt64), 1, file);
        for (TZrUInt64 paramIndex = 0; paramIndex < parameterCount; paramIndex++) {
            write_function_typed_type_ref(file, state, &symbol->parameterTypes[paramIndex]);
        }
    }
}

static void write_function_prototypes(SZrState *state, FILE *file, SZrFunction *function) {
    TZrUInt64 prototypesLength = 0;
    TZrUInt64 classCount = 0;
    TZrUInt64 structCount = 0;

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
            TZrSize currentPrototypeSize = sizeof(SZrCompiledPrototypeInfo) +
                                           inheritsCount * sizeof(TZrUInt32) +
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
                TZrSize currentPrototypeSize = sizeof(SZrCompiledPrototypeInfo) +
                                               inheritsCount * sizeof(TZrUInt32) +
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
                TZrSize currentPrototypeSize = sizeof(SZrCompiledPrototypeInfo) +
                                               inheritsCount * sizeof(TZrUInt32) +
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
    }
}

static TZrBool write_io_function(SZrState *state, FILE *file, SZrFunction *function, const TZrChar *defaultName) {
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

    {
        TZrUInt64 constantsLength = function->constantValueLength;
        fwrite(&constantsLength, sizeof(TZrUInt64), 1, file);
        for (TZrUInt64 i = 0; i < constantsLength; i++) {
            write_function_constant(file, state, &function->constantValueList[i]);
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
        }
    }

    write_function_typed_local_bindings(file, state, function);
    write_function_typed_export_symbols(file, state, function);

    write_function_prototypes(state, file, function);

    {
        TZrUInt64 closuresLength = function->childFunctionLength;
        fwrite(&closuresLength, sizeof(TZrUInt64), 1, file);
        for (TZrUInt64 i = 0; i < closuresLength; i++) {
            write_io_function(state, file, &function->childFunctionList[i], ZR_NULL);
        }
    }

    {
        TZrUInt64 debugInfoLength = 0;
        fwrite(&debugInfoLength, sizeof(TZrUInt64), 1, file);
    }

    return ZR_TRUE;
}

// 写入二进制文件 (.zro)
ZR_PARSER_API TZrBool ZrParser_Writer_WriteBinaryFile(SZrState *state, SZrFunction *function, const TZrChar *filename) {
    if (state == ZR_NULL || function == ZR_NULL || filename == ZR_NULL) {
        return ZR_FALSE;
    }
    
    FILE *file = fopen(filename, "wb");
    if (file == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 写入文件头（.SOURCE格式）
    // SIGNATURE (4 bytes)
    fwrite(ZR_IO_SOURCE_SIGNATURE, 1, 4, file);
    
    // VERSION_MAJOR (4 bytes)
    TZrUInt32 versionMajor = ZR_VM_MAJOR_VERSION;
    fwrite(&versionMajor, sizeof(TZrUInt32), 1, file);
    
    // VERSION_MINOR (4 bytes)
    TZrUInt32 versionMinor = ZR_VM_MINOR_VERSION;
    fwrite(&versionMinor, sizeof(TZrUInt32), 1, file);
    
    // VERSION_PATCH (4 bytes)
    TZrUInt32 versionPatch = ZR_VM_PATCH_VERSION;
    fwrite(&versionPatch, sizeof(TZrUInt32), 1, file);
    
    // FORMAT (8 bytes)
    TZrUInt64 format = ((TZrUInt64)versionMajor << ZR_IO_VERSION_FORMAT_SHIFT_BITS) | versionMinor;
    fwrite(&format, sizeof(TZrUInt64), 1, file);
    
    // NATIVE_INT_SIZE (1 byte)
    TZrUInt8 nativeIntSize = ZR_IO_NATIVE_INT_SIZE;
    fwrite(&nativeIntSize, sizeof(TZrUInt8), 1, file);
    
    // SIZE_T_SIZE (1 byte)
    TZrUInt8 sizeTypeSize = ZR_IO_SIZE_T_SIZE;
    fwrite(&sizeTypeSize, sizeof(TZrUInt8), 1, file);
    
    // INSTRUCTION_SIZE (1 byte)
    TZrUInt8 instructionSize = ZR_IO_INSTRUCTION_SIZE;
    fwrite(&instructionSize, sizeof(TZrUInt8), 1, file);
    
    // ENDIAN (1 byte)
    TZrUInt8 endian = ZR_IO_IS_LITTLE_ENDIAN ? ZR_TRUE : ZR_FALSE;
    fwrite(&endian, sizeof(TZrUInt8), 1, file);
    
    // DEBUG (1 byte)
    TZrUInt8 debug = ZR_FALSE;
    fwrite(&debug, sizeof(TZrUInt8), 1, file);
    
    // OPT (3 bytes)
    TZrUInt8 opt[3] = {ZR_FALSE, ZR_FALSE, ZR_FALSE};
    fwrite(opt, sizeof(TZrUInt8), 3, file);
    
    // MODULES_LENGTH (8 bytes)
    TZrUInt64 modulesLength = 1;
    fwrite(&modulesLength, sizeof(TZrUInt64), 1, file);
    
    // MODULE: NAME [string]
    SZrString *moduleName = ZrCore_String_Create(state, "simple", 6);
    TZrSize nameLength = (moduleName->shortStringLength < ZR_VM_LONG_STRING_FLAG) ? 
                         (TZrSize)moduleName->shortStringLength : 
                         moduleName->longStringLength;
    fwrite(&nameLength, sizeof(TZrSize), 1, file);
    TZrNativeString nameStr = ZrCore_String_GetNativeString(moduleName);
    fwrite(nameStr, sizeof(TZrChar), nameLength, file);
    
    // MODULE: MD5 [string] (空字符串)
    TZrSize md5Length = 0;
    fwrite(&md5Length, sizeof(TZrSize), 1, file);
    
    // MODULE: IMPORTS_LENGTH (8 bytes)
    TZrUInt64 importsLength = 0;
    fwrite(&importsLength, sizeof(TZrUInt64), 1, file);
    
    // MODULE: DECLARES_LENGTH (8 bytes)
    TZrUInt64 declaresLength = 0;
    fwrite(&declaresLength, sizeof(TZrUInt64), 1, file);
    
    // MODULE: ENTRY [.FUNCTION]
    write_io_function(state, file, function, "__entry");
    
    fclose(file);
    return ZR_TRUE;
}

// 写入明文中间文件 (.zri)
