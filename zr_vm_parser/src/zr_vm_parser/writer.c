//
// Created by Auto on 2025/01/XX.
//

#include "zr_vm_parser/writer.h"

#include "zr_vm_core/function.h"
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
        TNativeString strStr = ZrStringGetNativeString(str);
        if (strStr != ZR_NULL) {
            fwrite(strStr, sizeof(TChar), strLength, file);
        }
    }
}

// 辅助函数：从常量池索引获取字符串
static SZrString *get_string_from_constant(SZrState *state, SZrFunction *function, TUInt32 index) {
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
static void write_io_reference(SZrState *state, FILE *file, TUInt32 stringIndex, SZrFunction *function) {
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
    TUInt32 memberType = memberInfo->memberType;
    TUInt32 nameStringIndex = memberInfo->nameStringIndex;
    
    // 确定EZrIoMemberDeclareType
    TUInt32 ioMemberType;
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
            // TODO: 目前只写入名称，函数引用需要从childFunctionList获取
            SZrString *methodName = get_string_from_constant(state, function, nameStringIndex);
            write_string_with_length(state, file, methodName);
            
            // FUNCTIONS_LENGTH [8] (目前设为0，函数引用暂不支持)
            TZrSize functionsLength = 0;
            fwrite(&functionsLength, sizeof(TZrSize), 1, file);
            break;
        }
        case ZR_IO_MEMBER_DECLARE_TYPE_PROPERTY: {
            // .PROPERTY: NAME [string], PROPERTY_TYPE [4], GETTER_FUNCTION [.FUNCTION], SETTER_FUNCTION [.FUNCTION]
            SZrString *propertyName = get_string_from_constant(state, function, nameStringIndex);
            write_string_with_length(state, file, propertyName);
            
            // PROPERTY_TYPE [4] (目前设为0)
            TUInt32 propertyType = 0;
            fwrite(&propertyType, sizeof(TUInt32), 1, file);
            
            // TODO: GETTER和SETTER函数引用需要从childFunctionList获取
            // 目前写入空的函数占位符
            // GETTER_FUNCTION [.FUNCTION] - 跳过（需要完整的函数序列化）
            // SETTER_FUNCTION [.FUNCTION] - 跳过（需要完整的函数序列化）
            break;
        }
        case ZR_IO_MEMBER_DECLARE_TYPE_META: {
            // .META: META_TYPE [4], FUNCTIONS_LENGTH [8], FUNCTIONS [.FUNCTION]
            TUInt32 metaType = memberInfo->metaType;
            fwrite(&metaType, sizeof(TUInt32), 1, file);
            
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
static void write_prototype_class(SZrState *state, FILE *file, const SZrCompiledPrototypeInfo *protoInfo, const TByte *data, SZrFunction *function) {
    // .CLASS: NAME [string]
    SZrString *className = get_string_from_constant(state, function, protoInfo->nameStringIndex);
    write_string_with_length(state, file, className);
    
    // SUPER_CLASS_LENGTH [8]
    TZrSize superClassLength = protoInfo->inheritsCount;
    fwrite(&superClassLength, sizeof(TZrSize), 1, file);
    
    // SUPER_CLASSES [.REFERENCE]
    if (superClassLength > 0) {
        const TUInt32 *inheritIndices = (const TUInt32 *)(data + sizeof(SZrCompiledPrototypeInfo));
        for (TUInt32 i = 0; i < superClassLength; i++) {
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
        TZrSize inheritArraySize = superClassLength * sizeof(TUInt32);
        const SZrCompiledMemberInfo *members = (const SZrCompiledMemberInfo *)(data + sizeof(SZrCompiledPrototypeInfo) + inheritArraySize);
        for (TUInt32 i = 0; i < declaresLength; i++) {
            write_member_declare(state, file, &members[i], function);
        }
    }
}

// 辅助函数：写入STRUCT prototype的结构化数据
static void write_prototype_struct(SZrState *state, FILE *file, const SZrCompiledPrototypeInfo *protoInfo, const TByte *data, SZrFunction *function) {
    // .STRUCT: NAME [string]
    SZrString *structName = get_string_from_constant(state, function, protoInfo->nameStringIndex);
    write_string_with_length(state, file, structName);
    
    // SUPER_STRUCT_LENGTH [8]
    TZrSize superStructLength = protoInfo->inheritsCount;
    fwrite(&superStructLength, sizeof(TZrSize), 1, file);
    
    // SUPER_STRUCTS [.REFERENCE]
    if (superStructLength > 0) {
        const TUInt32 *inheritIndices = (const TUInt32 *)(data + sizeof(SZrCompiledPrototypeInfo));
        for (TUInt32 i = 0; i < superStructLength; i++) {
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
        TZrSize inheritArraySize = superStructLength * sizeof(TUInt32);
        const SZrCompiledMemberInfo *members = (const SZrCompiledMemberInfo *)(data + sizeof(SZrCompiledPrototypeInfo) + inheritArraySize);
        for (TUInt32 i = 0; i < declaresLength; i++) {
            write_member_declare(state, file, &members[i], function);
        }
    }
}

// 写入二进制文件 (.zro)
ZR_PARSER_API TBool ZrWriterWriteBinaryFile(SZrState *state, SZrFunction *function, const TChar *filename) {
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
    TUInt32 versionMajor = ZR_VM_MAJOR_VERSION;
    fwrite(&versionMajor, sizeof(TUInt32), 1, file);
    
    // VERSION_MINOR (4 bytes)
    TUInt32 versionMinor = ZR_VM_MINOR_VERSION;
    fwrite(&versionMinor, sizeof(TUInt32), 1, file);
    
    // VERSION_PATCH (4 bytes)
    TUInt32 versionPatch = ZR_VM_PATCH_VERSION;
    fwrite(&versionPatch, sizeof(TUInt32), 1, file);
    
    // FORMAT (8 bytes)
    TUInt64 format = ((TUInt64)versionMajor << ZR_IO_VERSION_FORMAT_SHIFT_BITS) | versionMinor;
    fwrite(&format, sizeof(TUInt64), 1, file);
    
    // NATIVE_INT_SIZE (1 byte)
    TUInt8 nativeIntSize = ZR_IO_NATIVE_INT_SIZE;
    fwrite(&nativeIntSize, sizeof(TUInt8), 1, file);
    
    // SIZE_T_SIZE (1 byte)
    TUInt8 sizeTypeSize = ZR_IO_SIZE_T_SIZE;
    fwrite(&sizeTypeSize, sizeof(TUInt8), 1, file);
    
    // INSTRUCTION_SIZE (1 byte)
    TUInt8 instructionSize = ZR_IO_INSTRUCTION_SIZE;
    fwrite(&instructionSize, sizeof(TUInt8), 1, file);
    
    // ENDIAN (1 byte)
    TUInt8 endian = ZR_IO_IS_LITTLE_ENDIAN ? ZR_TRUE : ZR_FALSE;
    fwrite(&endian, sizeof(TUInt8), 1, file);
    
    // DEBUG (1 byte)
    TUInt8 debug = ZR_FALSE;
    fwrite(&debug, sizeof(TUInt8), 1, file);
    
    // OPT (3 bytes)
    TUInt8 opt[3] = {ZR_FALSE, ZR_FALSE, ZR_FALSE};
    fwrite(opt, sizeof(TUInt8), 3, file);
    
    // MODULES_LENGTH (8 bytes)
    TUInt64 modulesLength = 1;
    fwrite(&modulesLength, sizeof(TUInt64), 1, file);
    
    // MODULE: NAME [string]
    SZrString *moduleName = ZrStringCreate(state, "simple", 6);
    TZrSize nameLength = (moduleName->shortStringLength < ZR_VM_LONG_STRING_FLAG) ? 
                         (TZrSize)moduleName->shortStringLength : 
                         moduleName->longStringLength;
    fwrite(&nameLength, sizeof(TZrSize), 1, file);
    TNativeString nameStr = ZrStringGetNativeString(moduleName);
    fwrite(nameStr, sizeof(TChar), nameLength, file);
    
    // MODULE: MD5 [string] (空字符串)
    TZrSize md5Length = 0;
    fwrite(&md5Length, sizeof(TZrSize), 1, file);
    
    // MODULE: IMPORTS_LENGTH (8 bytes)
    TUInt64 importsLength = 0;
    fwrite(&importsLength, sizeof(TUInt64), 1, file);
    
    // MODULE: DECLARES_LENGTH (8 bytes)
    TUInt64 declaresLength = 0;
    fwrite(&declaresLength, sizeof(TUInt64), 1, file);
    
    // MODULE: ENTRY [.FUNCTION]
    // FUNCTION: NAME [string]
    // 使用函数对象中的 functionName，如果不存在则使用默认名称
    SZrString *funcName = function->functionName;
    if (funcName == ZR_NULL) {
        funcName = ZrStringCreate(state, "__entry", 7);
    }
    TZrSize funcNameLength = (funcName->shortStringLength < ZR_VM_LONG_STRING_FLAG) ? 
                              (TZrSize)funcName->shortStringLength : 
                              funcName->longStringLength;
    fwrite(&funcNameLength, sizeof(TZrSize), 1, file);
    TNativeString funcNameStr = ZrStringGetNativeString(funcName);
    fwrite(funcNameStr, sizeof(TChar), funcNameLength, file);
    
    // FUNCTION: START_LINE (8 bytes)
    TUInt64 startLine = function->lineInSourceStart;
    fwrite(&startLine, sizeof(TUInt64), 1, file);
    
    // FUNCTION: END_LINE (8 bytes)
    TUInt64 endLine = function->lineInSourceEnd;
    fwrite(&endLine, sizeof(TUInt64), 1, file);
    
    // FUNCTION: PARAMETERS_LENGTH (8 bytes)
    TUInt64 parametersLength = function->parameterCount;
    fwrite(&parametersLength, sizeof(TUInt64), 1, file);
    
    // FUNCTION: HAS_VAR_ARGS (8 bytes)
    TUInt64 hasVarArgs = function->hasVariableArguments ? ZR_TRUE : ZR_FALSE;
    fwrite(&hasVarArgs, sizeof(TUInt64), 1, file);
    
    // FUNCTION: INSTRUCTIONS_LENGTH (8 bytes)
    TUInt64 instructionsLength = function->instructionsLength;
    fwrite(&instructionsLength, sizeof(TUInt64), 1, file);
    
    // FUNCTION: INSTRUCTIONS [.INSTRUCTION]
    for (TUInt64 i = 0; i < instructionsLength; i++) {
        TZrInstruction *inst = &function->instructionsList[i];
        TUInt64 rawValue = inst->value;
        fwrite(&rawValue, sizeof(TUInt64), 1, file);
    }
    
    // FUNCTION: LOCAL_LENGTH (8 bytes)
    TUInt64 localLength = function->localVariableLength;
    fwrite(&localLength, sizeof(TUInt64), 1, file);
    
    // FUNCTION: LOCALS [.LOCAL]
    for (TUInt64 i = 0; i < localLength; i++) {
        SZrFunctionLocalVariable *local = &function->localVariableList[i];
        TUInt64 instructionStart = local->offsetActivate;
        TUInt64 instructionEnd = local->offsetDead;
        TUInt64 startLineLocal = 0;  // TODO: 从调试信息获取
        TUInt64 endLineLocal = 0;    // TODO: 从调试信息获取
        fwrite(&instructionStart, sizeof(TUInt64), 1, file);
        fwrite(&instructionEnd, sizeof(TUInt64), 1, file);
        fwrite(&startLineLocal, sizeof(TUInt64), 1, file);
        fwrite(&endLineLocal, sizeof(TUInt64), 1, file);
    }
    
    // FUNCTION: CONSTANTS_LENGTH (8 bytes)
    TUInt64 constantsLength = function->constantValueLength;
    fwrite(&constantsLength, sizeof(TUInt64), 1, file);
    
    // FUNCTION: CONSTANTS [.CONSTANT]
    for (TUInt64 i = 0; i < constantsLength; i++) {
        SZrTypeValue *constant = &function->constantValueList[i];
        TUInt32 type = (TUInt32)constant->type;
        fwrite(&type, sizeof(TUInt32), 1, file);
        
        // 写入值（根据类型）
        switch (constant->type) {
            case ZR_VALUE_TYPE_NULL:
                // 无数据
                break;
            case ZR_VALUE_TYPE_BOOL:
            case ZR_VALUE_TYPE_INT8:
            case ZR_VALUE_TYPE_INT16:
            case ZR_VALUE_TYPE_INT32:
            case ZR_VALUE_TYPE_INT64:
                fwrite(&constant->value, sizeof(TInt64), 1, file);
                break;
            case ZR_VALUE_TYPE_FLOAT:
            case ZR_VALUE_TYPE_DOUBLE:
                fwrite(&constant->value, sizeof(TDouble), 1, file);
                break;
            case ZR_VALUE_TYPE_STRING: {
                // 检查 object 是否为 NULL 或类型不匹配
                if (constant->value.object == ZR_NULL) {
                    // 空字符串
                    TZrSize strLength = 0;
                    fwrite(&strLength, sizeof(TZrSize), 1, file);
                } else {
                    // 验证对象类型
                    SZrRawObject *rawObj = constant->value.object;
                    if (rawObj->type == ZR_VALUE_TYPE_STRING) {
                        SZrString *str = ZR_CAST_STRING(state, rawObj);
                        TZrSize strLength = (str->shortStringLength < ZR_VM_LONG_STRING_FLAG) ?
                                             (TZrSize)str->shortStringLength : 
                                             str->longStringLength;
                        fwrite(&strLength, sizeof(TZrSize), 1, file);
                        TNativeString strStr = ZrStringGetNativeString(str);
                        fwrite(strStr, sizeof(TChar), strLength, file);
                    } else {
                        // 类型不匹配，写入空字符串
                        TZrSize strLength = 0;
                        fwrite(&strLength, sizeof(TZrSize), 1, file);
                    }
                }
                break;
            }
            default:
                // 其他类型暂不支持
                break;
        }
        
        TUInt64 startLineConst = 0;  // TODO: 从调试信息获取
        TUInt64 endLineConst = 0;    // TODO: 从调试信息获取
        fwrite(&startLineConst, sizeof(TUInt64), 1, file);
        fwrite(&endLineConst, sizeof(TUInt64), 1, file);
    }
    
    // FUNCTION: PROTOTYPES_LENGTH (8 bytes) - prototype 数量
    TUInt64 prototypesLength = 0;
    TUInt64 classCount = 0;
    TUInt64 structCount = 0;
    
    if (function->prototypeData != ZR_NULL && function->prototypeCount > 0) {
        prototypesLength = function->prototypeCount;
        
        // 统计CLASS和STRUCT的数量
        const TByte *prototypeData = function->prototypeData + sizeof(TUInt32);
        TZrSize remainingDataSize = function->prototypeDataLength - sizeof(TUInt32);
        const TByte *currentPos = prototypeData;
        
        for (TUInt32 i = 0; i < prototypesLength; i++) {
            if (remainingDataSize < sizeof(SZrCompiledPrototypeInfo)) {
                break;
            }
            
            const SZrCompiledPrototypeInfo *protoInfo = (const SZrCompiledPrototypeInfo *)currentPos;
            TUInt32 type = protoInfo->type;
            TUInt32 inheritsCount = protoInfo->inheritsCount;
            TUInt32 membersCount = protoInfo->membersCount;
            
            TZrSize inheritArraySize = inheritsCount * sizeof(TUInt32);
            TZrSize membersArraySize = membersCount * sizeof(SZrCompiledMemberInfo);
            TZrSize currentPrototypeSize = sizeof(SZrCompiledPrototypeInfo) + inheritArraySize + membersArraySize;
            
            if (remainingDataSize < currentPrototypeSize) {
                break;
            }
            
            if (type == ZR_OBJECT_PROTOTYPE_TYPE_CLASS) {
                classCount++;
            } else if (type == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT) {
                structCount++;
            }
            
            currentPos += currentPrototypeSize;
            remainingDataSize -= currentPrototypeSize;
        }
    }
    
    fwrite(&prototypesLength, sizeof(TUInt64), 1, file);
    
    // FUNCTION: PROTOTYPES - 写入结构化格式
    // 先写入CLASS数组，再写入STRUCT数组
    if (prototypesLength > 0 && function->prototypeData != ZR_NULL && function->prototypeDataLength > 0) {
        const TByte *prototypeData = function->prototypeData + sizeof(TUInt32);
        TZrSize remainingDataSize = function->prototypeDataLength - sizeof(TUInt32);
        const TByte *currentPos = prototypeData;
        
        // 写入CLASS数组
        fwrite(&classCount, sizeof(TUInt64), 1, file);
        if (classCount > 0) {
            for (TUInt32 i = 0; i < prototypesLength; i++) {
                if (remainingDataSize < sizeof(SZrCompiledPrototypeInfo)) {
                    break;
                }
                
                const SZrCompiledPrototypeInfo *protoInfo = (const SZrCompiledPrototypeInfo *)currentPos;
                TUInt32 type = protoInfo->type;
                TUInt32 inheritsCount = protoInfo->inheritsCount;
                TUInt32 membersCount = protoInfo->membersCount;
                
                TZrSize inheritArraySize = inheritsCount * sizeof(TUInt32);
                TZrSize membersArraySize = membersCount * sizeof(SZrCompiledMemberInfo);
                TZrSize currentPrototypeSize = sizeof(SZrCompiledPrototypeInfo) + inheritArraySize + membersArraySize;
                
                if (remainingDataSize < currentPrototypeSize) {
                    break;
                }
                
                if (type == ZR_OBJECT_PROTOTYPE_TYPE_CLASS) {
                    write_prototype_class(state, file, protoInfo, currentPos, function);
                }
                
                currentPos += currentPrototypeSize;
                remainingDataSize -= currentPrototypeSize;
            }
        }
        
        // 写入STRUCT数组
        fwrite(&structCount, sizeof(TUInt64), 1, file);
        if (structCount > 0) {
            // 重新遍历以写入STRUCT
            currentPos = prototypeData;
            remainingDataSize = function->prototypeDataLength - sizeof(TUInt32);
            
            for (TUInt32 i = 0; i < prototypesLength; i++) {
                if (remainingDataSize < sizeof(SZrCompiledPrototypeInfo)) {
                    break;
                }
                
                const SZrCompiledPrototypeInfo *protoInfo = (const SZrCompiledPrototypeInfo *)currentPos;
                TUInt32 type = protoInfo->type;
                TUInt32 inheritsCount = protoInfo->inheritsCount;
                TUInt32 membersCount = protoInfo->membersCount;
                
                TZrSize inheritArraySize = inheritsCount * sizeof(TUInt32);
                TZrSize membersArraySize = membersCount * sizeof(SZrCompiledMemberInfo);
                TZrSize currentPrototypeSize = sizeof(SZrCompiledPrototypeInfo) + inheritArraySize + membersArraySize;
                
                if (remainingDataSize < currentPrototypeSize) {
                    break;
                }
                
                if (type == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT) {
                    write_prototype_struct(state, file, protoInfo, currentPos, function);
                }
                
                currentPos += currentPrototypeSize;
                remainingDataSize -= currentPrototypeSize;
            }
        }
    } else {
        // 没有prototype数据
        TUInt64 zero = 0;
        fwrite(&zero, sizeof(TUInt64), 1, file); // classCount = 0
        fwrite(&zero, sizeof(TUInt64), 1, file); // structCount = 0
    }
    
    // FUNCTION: CLOSURES_LENGTH (8 bytes)
    TUInt64 closuresLength = function->childFunctionLength;
    fwrite(&closuresLength, sizeof(TUInt64), 1, file);
    
    // FUNCTION: CLOSURES [.CLOSURE] (TODO: 递归写入子函数)
    
    // FUNCTION: DEBUG_INFO_LENGTH (8 bytes)
    TUInt64 debugInfoLength = 0;
    fwrite(&debugInfoLength, sizeof(TUInt64), 1, file);
    
    fclose(file);
    return ZR_TRUE;
}

// 写入明文中间文件 (.zri)
ZR_PARSER_API TBool ZrWriterWriteIntermediateFile(SZrState *state, SZrFunction *function, const TChar *filename) {
    if (state == ZR_NULL || function == ZR_NULL || filename == ZR_NULL) {
        return ZR_FALSE;
    }
    
    FILE *file = fopen(filename, "w");
    if (file == ZR_NULL) {
        return ZR_FALSE;
    }
    
    fprintf(file, "// ZR Intermediate File (.zri)\n");
    fprintf(file, "// Generated from compiled function\n\n");
    
    // 输出函数名（如果存在，否则使用 "__entry"）
    if (function->functionName != ZR_NULL) {
        TNativeString funcNameStr = ZrStringGetNativeString(function->functionName);
        if (funcNameStr != ZR_NULL) {
            TZrSize nameLen;
            if (function->functionName->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
                nameLen = function->functionName->shortStringLength;
            } else {
                nameLen = function->functionName->longStringLength;
            }
            fprintf(file, "FUNCTION: %.*s\n", (int)nameLen, funcNameStr);
        } else {
            fprintf(file, "FUNCTION: <unnamed>\n");
        }
    } else {
        fprintf(file, "FUNCTION: <anonymous>\n");
    }
    fprintf(file, "  START_LINE: %u\n", function->lineInSourceStart);
    fprintf(file, "  END_LINE: %u\n", function->lineInSourceEnd);
    fprintf(file, "  PARAMETERS: %u\n", function->parameterCount);
    fprintf(file, "  HAS_VAR_ARGS: %s\n", function->hasVariableArguments ? "true" : "false");
    fprintf(file, "  STACK_SIZE: %u\n", function->stackSize);
    fprintf(file, "\n");
    
    // 常量列表
    fprintf(file, "CONSTANTS (%u):\n", function->constantValueLength);
    for (TUInt32 i = 0; i < function->constantValueLength; i++) {
        SZrTypeValue *constant = &function->constantValueList[i];
        fprintf(file, "  [%u] ", i);
        switch (constant->type) {
            case ZR_VALUE_TYPE_NULL:
                fprintf(file, "null\n");
                break;
            case ZR_VALUE_TYPE_BOOL:
                fprintf(file, "bool: %s\n", constant->value.nativeObject.nativeBool ? "true" : "false");
                break;
            case ZR_VALUE_TYPE_INT8:
            case ZR_VALUE_TYPE_INT16:
            case ZR_VALUE_TYPE_INT32:
            case ZR_VALUE_TYPE_INT64:
                fprintf(file, "int: %lld\n", (long long)constant->value.nativeObject.nativeInt64);
                break;
            case ZR_VALUE_TYPE_FLOAT:
            case ZR_VALUE_TYPE_DOUBLE:
                fprintf(file, "float: %f\n", constant->value.nativeObject.nativeDouble);
                break;
            case ZR_VALUE_TYPE_STRING: {
                // 检查 object 是否为 NULL 或类型不匹配
                if (constant->value.object == ZR_NULL) {
                    fprintf(file, "string: \"\"\n");
                } else {
                    // 验证对象类型
                    SZrRawObject *rawObj = constant->value.object;
                    if (rawObj->type == ZR_VALUE_TYPE_STRING) {
                        SZrString *str = ZR_CAST_STRING(state, rawObj);
                        TNativeString strStr = ZrStringGetNativeString(str);
                        fprintf(file, "string: \"%s\"\n", strStr);
                    } else {
                        // 类型不匹配，输出空字符串
                        fprintf(file, "string: \"\"\n");
                    }
                }
                break;
            }
            default:
                fprintf(file, "unknown type: %u\n", (TUInt32)constant->type);
                break;
        }
    }
    fprintf(file, "\n");
    
    // 局部变量列表
    fprintf(file, "LOCAL_VARIABLES (%u):\n", function->localVariableLength);
    for (TUInt32 i = 0; i < function->localVariableLength; i++) {
        SZrFunctionLocalVariable *local = &function->localVariableList[i];
        TNativeString nameStr = local->name ? ZrStringGetNativeString(local->name) : "<unnamed>";
        fprintf(file, "  [%u] %s: offset_activate=%u, offset_dead=%u\n", 
                i, nameStr, (TUInt32)local->offsetActivate, (TUInt32)local->offsetDead);
    }
    fprintf(file, "\n");
    
    // 闭包变量列表
    fprintf(file, "CLOSURE_VARIABLES (%u):\n", function->closureValueLength);
    for (TUInt32 i = 0; i < function->closureValueLength; i++) {
        SZrFunctionClosureVariable *closure = &function->closureValueList[i];
        TNativeString nameStr = closure->name ? ZrStringGetNativeString(closure->name) : "<unnamed>";
        
        // 输出类型名称
        const TChar *typeName = "UNKNOWN";
        switch (closure->valueType) {
            case ZR_VALUE_TYPE_NULL: typeName = "NULL"; break;
            case ZR_VALUE_TYPE_BOOL: typeName = "BOOL"; break;
            case ZR_VALUE_TYPE_INT8: typeName = "INT8"; break;
            case ZR_VALUE_TYPE_INT16: typeName = "INT16"; break;
            case ZR_VALUE_TYPE_INT32: typeName = "INT32"; break;
            case ZR_VALUE_TYPE_INT64: typeName = "INT64"; break;
            case ZR_VALUE_TYPE_UINT8: typeName = "UINT8"; break;
            case ZR_VALUE_TYPE_UINT16: typeName = "UINT16"; break;
            case ZR_VALUE_TYPE_UINT32: typeName = "UINT32"; break;
            case ZR_VALUE_TYPE_UINT64: typeName = "UINT64"; break;
            case ZR_VALUE_TYPE_FLOAT: typeName = "FLOAT"; break;
            case ZR_VALUE_TYPE_DOUBLE: typeName = "DOUBLE"; break;
            case ZR_VALUE_TYPE_STRING: typeName = "STRING"; break;
            case ZR_VALUE_TYPE_FUNCTION: typeName = "FUNCTION"; break;
            case ZR_VALUE_TYPE_CLOSURE: typeName = "CLOSURE"; break;
            case ZR_VALUE_TYPE_OBJECT: typeName = "OBJECT"; break;
            case ZR_VALUE_TYPE_ARRAY: typeName = "ARRAY"; break;
            default: typeName = "UNKNOWN"; break;
        }
        
        fprintf(file, "  [%u] %s: in_stack=%s, index=%u, type=%s\n", 
                i, nameStr, closure->inStack ? "true" : "false", 
                closure->index, typeName);
    }
    fprintf(file, "\n");
    
    // Prototype 数据列表
    if (function->prototypeData != ZR_NULL && function->prototypeCount > 0) {
        fprintf(file, "PROTOTYPES (%u):\n", function->prototypeCount);
        ZrDebugPrintPrototypesFromData(state, function, file);
        fprintf(file, "\n");
    }
    
    // 指令列表
    fprintf(file, "INSTRUCTIONS (%u):\n", function->instructionsLength);
    for (TUInt32 i = 0; i < function->instructionsLength; i++) {
        TZrInstruction *inst = &function->instructionsList[i];
        EZrInstructionCode opcode = (EZrInstructionCode)inst->instruction.operationCode;
        TUInt16 operandExtra = inst->instruction.operandExtra;
        
        fprintf(file, "  [%u] ", i);
        
        // 输出操作码名称
        switch (opcode) {
            case ZR_INSTRUCTION_ENUM(GET_STACK):
                fprintf(file, "GET_STACK");
                break;
            case ZR_INSTRUCTION_ENUM(SET_STACK):
                fprintf(file, "SET_STACK");
                break;
            case ZR_INSTRUCTION_ENUM(GET_CONSTANT):
                fprintf(file, "GET_CONSTANT");
                break;
            case ZR_INSTRUCTION_ENUM(SET_CONSTANT):
                fprintf(file, "SET_CONSTANT");
                break;
            case ZR_INSTRUCTION_ENUM(GET_CLOSURE):
                fprintf(file, "GET_CLOSURE");
                break;
            case ZR_INSTRUCTION_ENUM(SET_CLOSURE):
                fprintf(file, "SET_CLOSURE");
                break;
            case ZR_INSTRUCTION_ENUM(GETUPVAL):
                fprintf(file, "GETUPVAL");
                break;
            case ZR_INSTRUCTION_ENUM(SETUPVAL):
                fprintf(file, "SETUPVAL");
                break;
            case ZR_INSTRUCTION_ENUM(GETTABLE):
                fprintf(file, "GETTABLE");
                break;
            case ZR_INSTRUCTION_ENUM(SETTABLE):
                fprintf(file, "SETTABLE");
                break;
            case ZR_INSTRUCTION_ENUM(TO_BOOL):
                fprintf(file, "TO_BOOL");
                break;
            case ZR_INSTRUCTION_ENUM(TO_INT):
                fprintf(file, "TO_INT");
                break;
            case ZR_INSTRUCTION_ENUM(TO_UINT):
                fprintf(file, "TO_UINT");
                break;
            case ZR_INSTRUCTION_ENUM(TO_FLOAT):
                fprintf(file, "TO_FLOAT");
                break;
            case ZR_INSTRUCTION_ENUM(TO_STRING):
                fprintf(file, "TO_STRING");
                break;
            case ZR_INSTRUCTION_ENUM(ADD):
                fprintf(file, "ADD");
                break;
            case ZR_INSTRUCTION_ENUM(ADD_INT):
                fprintf(file, "ADD_INT");
                break;
            case ZR_INSTRUCTION_ENUM(ADD_FLOAT):
                fprintf(file, "ADD_FLOAT");
                break;
            case ZR_INSTRUCTION_ENUM(ADD_STRING):
                fprintf(file, "ADD_STRING");
                break;
            case ZR_INSTRUCTION_ENUM(SUB):
                fprintf(file, "SUB");
                break;
            case ZR_INSTRUCTION_ENUM(SUB_INT):
                fprintf(file, "SUB_INT");
                break;
            case ZR_INSTRUCTION_ENUM(SUB_FLOAT):
                fprintf(file, "SUB_FLOAT");
                break;
            case ZR_INSTRUCTION_ENUM(MUL):
                fprintf(file, "MUL");
                break;
            case ZR_INSTRUCTION_ENUM(MUL_SIGNED):
                fprintf(file, "MUL_SIGNED");
                break;
            case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED):
                fprintf(file, "MUL_UNSIGNED");
                break;
            case ZR_INSTRUCTION_ENUM(MUL_FLOAT):
                fprintf(file, "MUL_FLOAT");
                break;
            case ZR_INSTRUCTION_ENUM(NEG):
                fprintf(file, "NEG");
                break;
            case ZR_INSTRUCTION_ENUM(DIV):
                fprintf(file, "DIV");
                break;
            case ZR_INSTRUCTION_ENUM(DIV_SIGNED):
                fprintf(file, "DIV_SIGNED");
                break;
            case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED):
                fprintf(file, "DIV_UNSIGNED");
                break;
            case ZR_INSTRUCTION_ENUM(DIV_FLOAT):
                fprintf(file, "DIV_FLOAT");
                break;
            case ZR_INSTRUCTION_ENUM(MOD):
                fprintf(file, "MOD");
                break;
            case ZR_INSTRUCTION_ENUM(MOD_SIGNED):
                fprintf(file, "MOD_SIGNED");
                break;
            case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED):
                fprintf(file, "MOD_UNSIGNED");
                break;
            case ZR_INSTRUCTION_ENUM(MOD_FLOAT):
                fprintf(file, "MOD_FLOAT");
                break;
            case ZR_INSTRUCTION_ENUM(POW):
                fprintf(file, "POW");
                break;
            case ZR_INSTRUCTION_ENUM(POW_SIGNED):
                fprintf(file, "POW_SIGNED");
                break;
            case ZR_INSTRUCTION_ENUM(POW_UNSIGNED):
                fprintf(file, "POW_UNSIGNED");
                break;
            case ZR_INSTRUCTION_ENUM(POW_FLOAT):
                fprintf(file, "POW_FLOAT");
                break;
            case ZR_INSTRUCTION_ENUM(SHIFT_LEFT):
                fprintf(file, "SHIFT_LEFT");
                break;
            case ZR_INSTRUCTION_ENUM(SHIFT_LEFT_INT):
                fprintf(file, "SHIFT_LEFT_INT");
                break;
            case ZR_INSTRUCTION_ENUM(SHIFT_RIGHT):
                fprintf(file, "SHIFT_RIGHT");
                break;
            case ZR_INSTRUCTION_ENUM(SHIFT_RIGHT_INT):
                fprintf(file, "SHIFT_RIGHT_INT");
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_NOT):
                fprintf(file, "LOGICAL_NOT");
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_AND):
                fprintf(file, "LOGICAL_AND");
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_OR):
                fprintf(file, "LOGICAL_OR");
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_SIGNED):
                fprintf(file, "LOGICAL_GREATER_SIGNED");
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_UNSIGNED):
                fprintf(file, "LOGICAL_GREATER_UNSIGNED");
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_FLOAT):
                fprintf(file, "LOGICAL_GREATER_FLOAT");
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_SIGNED):
                fprintf(file, "LOGICAL_LESS_SIGNED");
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_UNSIGNED):
                fprintf(file, "LOGICAL_LESS_UNSIGNED");
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_FLOAT):
                fprintf(file, "LOGICAL_LESS_FLOAT");
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL):
                fprintf(file, "LOGICAL_EQUAL");
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL):
                fprintf(file, "LOGICAL_NOT_EQUAL");
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_SIGNED):
                fprintf(file, "LOGICAL_GREATER_EQUAL_SIGNED");
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_UNSIGNED):
                fprintf(file, "LOGICAL_GREATER_EQUAL_UNSIGNED");
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_FLOAT):
                fprintf(file, "LOGICAL_GREATER_EQUAL_FLOAT");
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_SIGNED):
                fprintf(file, "LOGICAL_LESS_EQUAL_SIGNED");
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_UNSIGNED):
                fprintf(file, "LOGICAL_LESS_EQUAL_UNSIGNED");
                break;
            case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_FLOAT):
                fprintf(file, "LOGICAL_LESS_EQUAL_FLOAT");
                break;
            case ZR_INSTRUCTION_ENUM(BITWISE_NOT):
                fprintf(file, "BITWISE_NOT");
                break;
            case ZR_INSTRUCTION_ENUM(BITWISE_AND):
                fprintf(file, "BITWISE_AND");
                break;
            case ZR_INSTRUCTION_ENUM(BITWISE_OR):
                fprintf(file, "BITWISE_OR");
                break;
            case ZR_INSTRUCTION_ENUM(BITWISE_XOR):
                fprintf(file, "BITWISE_XOR");
                break;
            case ZR_INSTRUCTION_ENUM(BITWISE_SHIFT_LEFT):
                fprintf(file, "BITWISE_SHIFT_LEFT");
                break;
            case ZR_INSTRUCTION_ENUM(BITWISE_SHIFT_RIGHT):
                fprintf(file, "BITWISE_SHIFT_RIGHT");
                break;
            case ZR_INSTRUCTION_ENUM(FUNCTION_CALL):
                fprintf(file, "FUNCTION_CALL");
                break;
            case ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL):
                fprintf(file, "FUNCTION_TAIL_CALL");
                break;
            case ZR_INSTRUCTION_ENUM(FUNCTION_RETURN):
                fprintf(file, "FUNCTION_RETURN");
                break;
            case ZR_INSTRUCTION_ENUM(GET_SUB_FUNCTION):
                fprintf(file, "GET_SUB_FUNCTION");
                break;
            case ZR_INSTRUCTION_ENUM(JUMP):
                fprintf(file, "JUMP");
                break;
            case ZR_INSTRUCTION_ENUM(JUMP_IF):
                fprintf(file, "JUMP_IF");
                break;
            case ZR_INSTRUCTION_ENUM(CREATE_CLOSURE):
                fprintf(file, "CREATE_CLOSURE");
                break;
            case ZR_INSTRUCTION_ENUM(CREATE_OBJECT):
                fprintf(file, "CREATE_OBJECT");
                break;
            case ZR_INSTRUCTION_ENUM(CREATE_ARRAY):
                fprintf(file, "CREATE_ARRAY");
                break;
            case ZR_INSTRUCTION_ENUM(TRY):
                fprintf(file, "TRY");
                break;
            case ZR_INSTRUCTION_ENUM(THROW):
                fprintf(file, "THROW");
                break;
            case ZR_INSTRUCTION_ENUM(CATCH):
                fprintf(file, "CATCH");
                break;
            default:
                fprintf(file, "OPCODE_%u", (TUInt32)opcode);
                break;
        }
        
        // 输出操作数（根据指令类型决定格式）
        // GET_CONSTANT, GET_STACK, SET_STACK 等使用 operandExtra + operand2[0]
        // ADD_INT, SUB_INT 等使用 operandExtra + operand1[0] + operand1[1]
        // FUNCTION_RETURN 使用 operandExtra + operand1[0] + operand1[1]
        fprintf(file, " (extra=%u", operandExtra);
        
        // 检查指令类型，决定使用哪种操作数格式
        switch (opcode) {
            // 使用 operand2[0] (TInt32) 的指令
            case ZR_INSTRUCTION_ENUM(GET_CONSTANT):
            case ZR_INSTRUCTION_ENUM(SET_CONSTANT):
            case ZR_INSTRUCTION_ENUM(GET_STACK):
            case ZR_INSTRUCTION_ENUM(SET_STACK):
            case ZR_INSTRUCTION_ENUM(GET_CLOSURE):
            case ZR_INSTRUCTION_ENUM(SET_CLOSURE):
            case ZR_INSTRUCTION_ENUM(GETUPVAL):
            case ZR_INSTRUCTION_ENUM(SETUPVAL):
            case ZR_INSTRUCTION_ENUM(GET_SUB_FUNCTION):
            case ZR_INSTRUCTION_ENUM(JUMP):
            case ZR_INSTRUCTION_ENUM(JUMP_IF):
            case ZR_INSTRUCTION_ENUM(THROW):
                fprintf(file, ", operand=%d", inst->instruction.operand.operand2[0]);
                break;
                
            // 使用 operand1[0] (TUInt16) 的指令（单操作数）
            case ZR_INSTRUCTION_ENUM(TO_BOOL):
            case ZR_INSTRUCTION_ENUM(TO_INT):
            case ZR_INSTRUCTION_ENUM(TO_UINT):
            case ZR_INSTRUCTION_ENUM(TO_FLOAT):
            case ZR_INSTRUCTION_ENUM(TO_STRING):
            case ZR_INSTRUCTION_ENUM(NEG):
            case ZR_INSTRUCTION_ENUM(LOGICAL_NOT):
            case ZR_INSTRUCTION_ENUM(BITWISE_NOT):
                fprintf(file, ", operand1=%u", inst->instruction.operand.operand1[0]);
                break;
                
            // 使用 operand1[0] + operand1[1] (TUInt16) 的指令（双操作数）
            case ZR_INSTRUCTION_ENUM(ADD):
            case ZR_INSTRUCTION_ENUM(ADD_INT):
            case ZR_INSTRUCTION_ENUM(ADD_FLOAT):
            case ZR_INSTRUCTION_ENUM(ADD_STRING):
            case ZR_INSTRUCTION_ENUM(SUB):
            case ZR_INSTRUCTION_ENUM(SUB_INT):
            case ZR_INSTRUCTION_ENUM(SUB_FLOAT):
            case ZR_INSTRUCTION_ENUM(MUL):
            case ZR_INSTRUCTION_ENUM(MUL_SIGNED):
            case ZR_INSTRUCTION_ENUM(MUL_UNSIGNED):
            case ZR_INSTRUCTION_ENUM(MUL_FLOAT):
            case ZR_INSTRUCTION_ENUM(DIV):
            case ZR_INSTRUCTION_ENUM(DIV_SIGNED):
            case ZR_INSTRUCTION_ENUM(DIV_UNSIGNED):
            case ZR_INSTRUCTION_ENUM(DIV_FLOAT):
            case ZR_INSTRUCTION_ENUM(MOD):
            case ZR_INSTRUCTION_ENUM(MOD_SIGNED):
            case ZR_INSTRUCTION_ENUM(MOD_UNSIGNED):
            case ZR_INSTRUCTION_ENUM(MOD_FLOAT):
            case ZR_INSTRUCTION_ENUM(POW):
            case ZR_INSTRUCTION_ENUM(POW_SIGNED):
            case ZR_INSTRUCTION_ENUM(POW_UNSIGNED):
            case ZR_INSTRUCTION_ENUM(POW_FLOAT):
            case ZR_INSTRUCTION_ENUM(SHIFT_LEFT):
            case ZR_INSTRUCTION_ENUM(SHIFT_LEFT_INT):
            case ZR_INSTRUCTION_ENUM(SHIFT_RIGHT):
            case ZR_INSTRUCTION_ENUM(SHIFT_RIGHT_INT):
            case ZR_INSTRUCTION_ENUM(LOGICAL_AND):
            case ZR_INSTRUCTION_ENUM(LOGICAL_OR):
            case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_SIGNED):
            case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_UNSIGNED):
            case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_FLOAT):
            case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_SIGNED):
            case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_UNSIGNED):
            case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_FLOAT):
            case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL):
            case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL):
            case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_SIGNED):
            case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_UNSIGNED):
            case ZR_INSTRUCTION_ENUM(LOGICAL_GREATER_EQUAL_FLOAT):
            case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_SIGNED):
            case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_UNSIGNED):
            case ZR_INSTRUCTION_ENUM(LOGICAL_LESS_EQUAL_FLOAT):
            case ZR_INSTRUCTION_ENUM(BITWISE_AND):
            case ZR_INSTRUCTION_ENUM(BITWISE_OR):
            case ZR_INSTRUCTION_ENUM(BITWISE_XOR):
            case ZR_INSTRUCTION_ENUM(BITWISE_SHIFT_LEFT):
            case ZR_INSTRUCTION_ENUM(BITWISE_SHIFT_RIGHT):
            case ZR_INSTRUCTION_ENUM(GETTABLE):
            case ZR_INSTRUCTION_ENUM(SETTABLE):
            case ZR_INSTRUCTION_ENUM(FUNCTION_CALL):
            case ZR_INSTRUCTION_ENUM(FUNCTION_TAIL_CALL):
            case ZR_INSTRUCTION_ENUM(FUNCTION_RETURN):
                fprintf(file, ", operand1=%u, operand2=%u", 
                        inst->instruction.operand.operand1[0], 
                        inst->instruction.operand.operand1[1]);
                break;
                
            // 只使用 operandExtra，不需要其他操作数
            case ZR_INSTRUCTION_ENUM(CREATE_OBJECT):
            case ZR_INSTRUCTION_ENUM(CREATE_ARRAY):
            case ZR_INSTRUCTION_ENUM(CREATE_CLOSURE):
            case ZR_INSTRUCTION_ENUM(TRY):
            case ZR_INSTRUCTION_ENUM(CATCH):
                break;
                
            default:
                // 对于未知指令，尝试输出所有可能的操作数格式
                if (inst->instruction.operand.operand2[0] != 0) {
                    fprintf(file, ", operand2=%d", inst->instruction.operand.operand2[0]);
                }
                if (inst->instruction.operand.operand1[0] != 0 || inst->instruction.operand.operand1[1] != 0) {
                    fprintf(file, ", operand1=%u, operand1_1=%u", 
                            inst->instruction.operand.operand1[0], 
                            inst->instruction.operand.operand1[1]);
                }
                break;
        }
        fprintf(file, ")\n");
    }
    fprintf(file, "\n");
    
    // 子函数列表（递归输出）
    if (function->childFunctionLength > 0) {
        fprintf(file, "CHILD_FUNCTIONS (%u):\n", function->childFunctionLength);
        for (TUInt32 i = 0; i < function->childFunctionLength; i++) {
            SZrFunction *childFunc = &function->childFunctionList[i];
            fprintf(file, "  [%u] FUNCTION:\n", i);
            // 输出函数名（如果存在）
            if (childFunc->functionName != ZR_NULL) {
                TNativeString nameStr = ZrStringGetNativeString(childFunc->functionName);
                TZrSize nameLen;
                if (childFunc->functionName->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
                    nameLen = childFunc->functionName->shortStringLength;
                } else {
                    nameLen = childFunc->functionName->longStringLength;
                }
                if (nameStr != ZR_NULL && nameLen > 0) {
                    fprintf(file, "    NAME: %.*s\n", (int)nameLen, nameStr);
                } else {
                    fprintf(file, "    NAME: <empty>\n");
                }
            } else {
                fprintf(file, "    NAME: <anonymous>\n");
            }
            fprintf(file, "    START_LINE: %u\n", childFunc->lineInSourceStart);
            fprintf(file, "    END_LINE: %u\n", childFunc->lineInSourceEnd);
            fprintf(file, "    PARAMETERS: %u\n", childFunc->parameterCount);
            fprintf(file, "    HAS_VAR_ARGS: %s\n", childFunc->hasVariableArguments ? "true" : "false");
            fprintf(file, "    STACK_SIZE: %u\n", childFunc->stackSize);
            fprintf(file, "    CONSTANTS (%u):\n", childFunc->constantValueLength);
            for (TUInt32 j = 0; j < childFunc->constantValueLength; j++) {
                SZrTypeValue *constant = &childFunc->constantValueList[j];
                fprintf(file, "      [%u] ", j);
                switch (constant->type) {
                    case ZR_VALUE_TYPE_NULL:
                        fprintf(file, "null\n");
                        break;
                    case ZR_VALUE_TYPE_BOOL:
                        fprintf(file, "bool: %s\n", constant->value.nativeObject.nativeBool ? "true" : "false");
                        break;
                    case ZR_VALUE_TYPE_INT8:
                    case ZR_VALUE_TYPE_INT16:
                    case ZR_VALUE_TYPE_INT32:
                    case ZR_VALUE_TYPE_INT64:
                        fprintf(file, "int: %lld\n", (long long)constant->value.nativeObject.nativeInt64);
                        break;
                    case ZR_VALUE_TYPE_FLOAT:
                    case ZR_VALUE_TYPE_DOUBLE:
                        fprintf(file, "float: %f\n", constant->value.nativeObject.nativeDouble);
                        break;
                    case ZR_VALUE_TYPE_STRING: {
                        if (constant->value.object == ZR_NULL) {
                            fprintf(file, "string: \"\"\n");
                        } else {
                            SZrRawObject *rawObj = constant->value.object;
                            if (rawObj->type == ZR_VALUE_TYPE_STRING) {
                                SZrString *str = ZR_CAST_STRING(state, rawObj);
                                TNativeString strStr = ZrStringGetNativeString(str);
                                fprintf(file, "string: \"%s\"\n", strStr);
                            } else {
                                fprintf(file, "string: \"\"\n");
                            }
                        }
                        break;
                    }
                    default:
                        fprintf(file, "unknown type: %u\n", (TUInt32)constant->type);
                        break;
                }
            }
            fprintf(file, "    INSTRUCTIONS (%u):\n", childFunc->instructionsLength);
            for (TUInt32 j = 0; j < childFunc->instructionsLength; j++) {
                TZrInstruction *inst = &childFunc->instructionsList[j];
                EZrInstructionCode opcode = (EZrInstructionCode)inst->instruction.operationCode;
                TUInt16 operandExtra = inst->instruction.operandExtra;
                
                fprintf(file, "      [%u] ", j);
                
                // 输出操作码名称
                switch (opcode) {
                    case ZR_INSTRUCTION_ENUM(GET_CONSTANT):
                        fprintf(file, "GET_CONSTANT");
                        break;
                    case ZR_INSTRUCTION_ENUM(SET_STACK):
                        fprintf(file, "SET_STACK");
                        break;
                    case ZR_INSTRUCTION_ENUM(GET_STACK):
                        fprintf(file, "GET_STACK");
                        break;
                    case ZR_INSTRUCTION_ENUM(ADD_INT):
                        fprintf(file, "ADD_INT");
                        break;
                    case ZR_INSTRUCTION_ENUM(SUB_INT):
                        fprintf(file, "SUB_INT");
                        break;
                    case ZR_INSTRUCTION_ENUM(MUL_SIGNED):
                        fprintf(file, "MUL_SIGNED");
                        break;
                    case ZR_INSTRUCTION_ENUM(DIV_SIGNED):
                        fprintf(file, "DIV_SIGNED");
                        break;
                    case ZR_INSTRUCTION_ENUM(FUNCTION_RETURN):
                        fprintf(file, "FUNCTION_RETURN");
                        break;
                    case ZR_INSTRUCTION_ENUM(CREATE_OBJECT):
                        fprintf(file, "CREATE_OBJECT");
                        break;
                    case ZR_INSTRUCTION_ENUM(CREATE_ARRAY):
                        fprintf(file, "CREATE_ARRAY");
                        break;
                    default:
                        fprintf(file, "OPCODE_%u", (TUInt32)opcode);
                        break;
                }
                
                fprintf(file, " (extra=%u", operandExtra);
                
                // 输出操作数
                switch (opcode) {
                    case ZR_INSTRUCTION_ENUM(GET_CONSTANT):
                    case ZR_INSTRUCTION_ENUM(SET_CONSTANT):
                    case ZR_INSTRUCTION_ENUM(GET_STACK):
                    case ZR_INSTRUCTION_ENUM(SET_STACK):
                    case ZR_INSTRUCTION_ENUM(GET_CLOSURE):
                    case ZR_INSTRUCTION_ENUM(SET_CLOSURE):
                    case ZR_INSTRUCTION_ENUM(GET_SUB_FUNCTION):
                    case ZR_INSTRUCTION_ENUM(JUMP):
                    case ZR_INSTRUCTION_ENUM(JUMP_IF):
                    case ZR_INSTRUCTION_ENUM(THROW):
                        fprintf(file, ", operand=%d", inst->instruction.operand.operand2[0]);
                        break;
                        
                    case ZR_INSTRUCTION_ENUM(ADD_INT):
                    case ZR_INSTRUCTION_ENUM(SUB_INT):
                    case ZR_INSTRUCTION_ENUM(MUL_SIGNED):
                    case ZR_INSTRUCTION_ENUM(DIV_SIGNED):
                    case ZR_INSTRUCTION_ENUM(MOD_SIGNED):
                    case ZR_INSTRUCTION_ENUM(LOGICAL_EQUAL):
                    case ZR_INSTRUCTION_ENUM(LOGICAL_NOT_EQUAL):
                    case ZR_INSTRUCTION_ENUM(LOGICAL_AND):
                    case ZR_INSTRUCTION_ENUM(LOGICAL_OR):
                    case ZR_INSTRUCTION_ENUM(BITWISE_AND):
                    case ZR_INSTRUCTION_ENUM(BITWISE_OR):
                    case ZR_INSTRUCTION_ENUM(BITWISE_XOR):
                    case ZR_INSTRUCTION_ENUM(GETTABLE):
                    case ZR_INSTRUCTION_ENUM(SETTABLE):
                    case ZR_INSTRUCTION_ENUM(FUNCTION_CALL):
                    case ZR_INSTRUCTION_ENUM(FUNCTION_RETURN):
                        fprintf(file, ", operand1=%u, operand2=%u", 
                                inst->instruction.operand.operand1[0], 
                                inst->instruction.operand.operand1[1]);
                        break;
                        
                    case ZR_INSTRUCTION_ENUM(CREATE_OBJECT):
                    case ZR_INSTRUCTION_ENUM(CREATE_ARRAY):
                    case ZR_INSTRUCTION_ENUM(CREATE_CLOSURE):
                    case ZR_INSTRUCTION_ENUM(TRY):
                    case ZR_INSTRUCTION_ENUM(CATCH):
                        break;
                        
                    default:
                        if (inst->instruction.operand.operand2[0] != 0) {
                            fprintf(file, ", operand2=%d", inst->instruction.operand.operand2[0]);
                        }
                        if (inst->instruction.operand.operand1[0] != 0 || inst->instruction.operand.operand1[1] != 0) {
                            fprintf(file, ", operand1=%u, operand1_1=%u", 
                                    inst->instruction.operand.operand1[0], 
                                    inst->instruction.operand.operand1[1]);
                        }
                        break;
                }
                fprintf(file, ")\n");
            }
            fprintf(file, "\n");
        }
        fprintf(file, "\n");
    }
    
    fclose(file);
    return ZR_TRUE;
}

// 获取 AST 节点类型名称
static const TChar *get_ast_node_type_name(EZrAstNodeType type) {
    switch (type) {
        case ZR_AST_SCRIPT: return "SCRIPT";
        case ZR_AST_MODULE_DECLARATION: return "MODULE_DECLARATION";
        case ZR_AST_VARIABLE_DECLARATION: return "VARIABLE_DECLARATION";
        case ZR_AST_FUNCTION_DECLARATION: return "FUNCTION_DECLARATION";
        case ZR_AST_EXPRESSION_STATEMENT: return "EXPRESSION_STATEMENT";
        case ZR_AST_ASSIGNMENT_EXPRESSION: return "ASSIGNMENT_EXPRESSION";
        case ZR_AST_BINARY_EXPRESSION: return "BINARY_EXPRESSION";
        case ZR_AST_UNARY_EXPRESSION: return "UNARY_EXPRESSION";
        case ZR_AST_CONDITIONAL_EXPRESSION: return "CONDITIONAL_EXPRESSION";
        case ZR_AST_LOGICAL_EXPRESSION: return "LOGICAL_EXPRESSION";
        case ZR_AST_FUNCTION_CALL: return "FUNCTION_CALL";
        case ZR_AST_MEMBER_EXPRESSION: return "MEMBER_EXPRESSION";
        case ZR_AST_PRIMARY_EXPRESSION: return "PRIMARY_EXPRESSION";
        case ZR_AST_IDENTIFIER_LITERAL: return "IDENTIFIER";
        case ZR_AST_BOOLEAN_LITERAL: return "BOOLEAN_LITERAL";
        case ZR_AST_INTEGER_LITERAL: return "INTEGER_LITERAL";
        case ZR_AST_FLOAT_LITERAL: return "FLOAT_LITERAL";
        case ZR_AST_STRING_LITERAL: return "STRING_LITERAL";
        case ZR_AST_CHAR_LITERAL: return "CHAR_LITERAL";
        case ZR_AST_NULL_LITERAL: return "NULL_LITERAL";
        case ZR_AST_ARRAY_LITERAL: return "ARRAY_LITERAL";
        case ZR_AST_OBJECT_LITERAL: return "OBJECT_LITERAL";
        case ZR_AST_BLOCK: return "BLOCK";
        case ZR_AST_RETURN_STATEMENT: return "RETURN_STATEMENT";
        case ZR_AST_IF_EXPRESSION: return "IF_EXPRESSION";
        case ZR_AST_WHILE_LOOP: return "WHILE_LOOP";
        case ZR_AST_FOR_LOOP: return "FOR_LOOP";
        case ZR_AST_FOREACH_LOOP: return "FOREACH_LOOP";
        case ZR_AST_LAMBDA_EXPRESSION: return "LAMBDA_EXPRESSION";
        case ZR_AST_TEST_DECLARATION: return "TEST_DECLARATION";
        case ZR_AST_SWITCH_EXPRESSION: return "SWITCH_EXPRESSION";
        case ZR_AST_SWITCH_CASE: return "SWITCH_CASE";
        case ZR_AST_SWITCH_DEFAULT: return "SWITCH_DEFAULT";
        case ZR_AST_BREAK_CONTINUE_STATEMENT: return "BREAK_CONTINUE_STATEMENT";
        case ZR_AST_THROW_STATEMENT: return "THROW_STATEMENT";
        case ZR_AST_OUT_STATEMENT: return "OUT_STATEMENT";
        case ZR_AST_TRY_CATCH_FINALLY_STATEMENT: return "TRY_CATCH_FINALLY_STATEMENT";
        case ZR_AST_KEY_VALUE_PAIR: return "KEY_VALUE_PAIR";
        case ZR_AST_UNPACK_LITERAL: return "UNPACK_LITERAL";
        case ZR_AST_GENERATOR_EXPRESSION: return "GENERATOR_EXPRESSION";
        case ZR_AST_DECORATOR_EXPRESSION: return "DECORATOR_EXPRESSION";
        case ZR_AST_DESTRUCTURING_OBJECT: return "DESTRUCTURING_OBJECT";
        case ZR_AST_DESTRUCTURING_ARRAY: return "DESTRUCTURING_ARRAY";
        case ZR_AST_PARAMETER: return "PARAMETER";
        case ZR_AST_PARAMETER_LIST: return "PARAMETER_LIST";
        case ZR_AST_TYPE: return "TYPE";
        case ZR_AST_GENERIC_TYPE: return "GENERIC_TYPE";
        case ZR_AST_TUPLE_TYPE: return "TUPLE_TYPE";
        case ZR_AST_GENERIC_DECLARATION: return "GENERIC_DECLARATION";
        case ZR_AST_STRUCT_DECLARATION: return "STRUCT_DECLARATION";
        case ZR_AST_CLASS_DECLARATION: return "CLASS_DECLARATION";
        case ZR_AST_INTERFACE_DECLARATION: return "INTERFACE_DECLARATION";
        case ZR_AST_ENUM_DECLARATION: return "ENUM_DECLARATION";
        case ZR_AST_INTERMEDIATE_STATEMENT: return "INTERMEDIATE_STATEMENT";
        case ZR_AST_INTERMEDIATE_DECLARATION: return "INTERMEDIATE_DECLARATION";
        case ZR_AST_INTERMEDIATE_CONSTANT: return "INTERMEDIATE_CONSTANT";
        case ZR_AST_INTERMEDIATE_INSTRUCTION: return "INTERMEDIATE_INSTRUCTION";
        case ZR_AST_INTERMEDIATE_INSTRUCTION_PARAMETER: return "INTERMEDIATE_INSTRUCTION_PARAMETER";
        case ZR_AST_STRUCT_FIELD: return "STRUCT_FIELD";
        case ZR_AST_STRUCT_METHOD: return "STRUCT_METHOD";
        case ZR_AST_STRUCT_META_FUNCTION: return "STRUCT_META_FUNCTION";
        case ZR_AST_CLASS_FIELD: return "CLASS_FIELD";
        case ZR_AST_CLASS_METHOD: return "CLASS_METHOD";
        case ZR_AST_CLASS_PROPERTY: return "CLASS_PROPERTY";
        case ZR_AST_CLASS_META_FUNCTION: return "CLASS_META_FUNCTION";
        case ZR_AST_INTERFACE_FIELD_DECLARATION: return "INTERFACE_FIELD_DECLARATION";
        case ZR_AST_INTERFACE_METHOD_SIGNATURE: return "INTERFACE_METHOD_SIGNATURE";
        case ZR_AST_INTERFACE_PROPERTY_SIGNATURE: return "INTERFACE_PROPERTY_SIGNATURE";
        case ZR_AST_INTERFACE_META_SIGNATURE: return "INTERFACE_META_SIGNATURE";
        case ZR_AST_ENUM_MEMBER: return "ENUM_MEMBER";
        case ZR_AST_META_IDENTIFIER: return "META_IDENTIFIER";
        case ZR_AST_ACCESS_MODIFIER: return "ACCESS_MODIFIER";
        case ZR_AST_PROPERTY_GET: return "PROPERTY_GET";
        case ZR_AST_PROPERTY_SET: return "PROPERTY_SET";
        default: return "UNKNOWN";
    }
}

// 注意：由于字符串对象可能已被垃圾回收，我们不在 print_ast_node 中打印字符串内容
// 这可以避免段错误。如果需要打印字符串内容，需要确保字符串对象在打印期间有效

// 递归打印 AST 节点
static void print_ast_node(SZrState *state, FILE *file, SZrAstNode *node, TZrSize indent) {
    if (node == ZR_NULL) {
        for (TZrSize i = 0; i < indent; i++) fprintf(file, "  ");
        fprintf(file, "NULL\n");
        return;
    }
    
    // 打印缩进
    for (TZrSize i = 0; i < indent; i++) fprintf(file, "  ");
    
    // 打印节点类型和位置
    const TChar *typeName = get_ast_node_type_name(node->type);
    fprintf(file, "%s", typeName);
    
    // 打印位置信息
    // 注意：如果 source 指向的字符串对象已被释放，这里可能会崩溃
    // 为了安全，我们只打印行号和列号，不打印文件名
    if (node->location.start.line > 0) {
        fprintf(file, " [line:%d:col:%d]", 
                node->location.start.line, node->location.start.column);
    }
    fprintf(file, "\n");
    
    // 根据节点类型打印详细信息
    switch (node->type) {
        case ZR_AST_SCRIPT: {
            SZrScript *script = &node->data.script;
            if (script->moduleName != ZR_NULL) {
                for (TZrSize i = 0; i < indent + 1; i++) fprintf(file, "  ");
                fprintf(file, "module: ");
                print_ast_node(state, file, script->moduleName, indent + 1);
            }
            if (script->statements != ZR_NULL) {
                for (TZrSize i = 0; i < indent + 1; i++) fprintf(file, "  ");
                fprintf(file, "statements (%zu):\n", script->statements->count);
                for (TZrSize i = 0; i < script->statements->count; i++) {
                    print_ast_node(state, file, script->statements->nodes[i], indent + 2);
                }
            }
            break;
        }
        case ZR_AST_MODULE_DECLARATION: {
            SZrModuleDeclaration *module = &node->data.moduleDeclaration;
            if (module->name != ZR_NULL) {
                print_ast_node(state, file, module->name, indent + 1);
            }
            break;
        }
        case ZR_AST_VARIABLE_DECLARATION: {
            SZrVariableDeclaration *var = &node->data.variableDeclaration;
            for (TZrSize i = 0; i < indent + 1; i++) fprintf(file, "  ");
            fprintf(file, "pattern: ");
            print_ast_node(state, file, var->pattern, indent + 1);
            for (TZrSize i = 0; i < indent + 1; i++) fprintf(file, "  ");
            fprintf(file, "value: ");
            print_ast_node(state, file, var->value, indent + 1);
            break;
        }
        case ZR_AST_IDENTIFIER_LITERAL: {
            SZrIdentifier *ident = &node->data.identifier;
            if (ident->name != ZR_NULL) {
                TNativeString nameStr = ZrStringGetNativeString(ident->name);
                if (nameStr != ZR_NULL) {
                    fprintf(file, "  name: \"%s\"\n", nameStr);
                } else {
                    fprintf(file, "  name: <null>\n");
                }
            }
            break;
        }
        case ZR_AST_STRING_LITERAL: {
            SZrStringLiteral *str = &node->data.stringLiteral;
            if (str->value != ZR_NULL) {
                TNativeString valueStr = ZrStringGetNativeString(str->value);
                if (valueStr != ZR_NULL) {
                    fprintf(file, "  value: \"%s\"\n", valueStr);
                } else {
                    fprintf(file, "  value: <null>\n");
                }
            } else if (str->literal != ZR_NULL) {
                TNativeString literalStr = ZrStringGetNativeString(str->literal);
                if (literalStr != ZR_NULL) {
                    fprintf(file, "  literal: \"%s\"\n", literalStr);
                } else {
                    fprintf(file, "  literal: <null>\n");
                }
            } else {
                fprintf(file, "  value: <null>\n");
            }
            break;
        }
        case ZR_AST_INTEGER_LITERAL: {
            SZrIntegerLiteral *intLit = &node->data.integerLiteral;
            fprintf(file, "  value: %lld\n", (long long)intLit->value);
            break;
        }
        case ZR_AST_FLOAT_LITERAL: {
            SZrFloatLiteral *floatLit = &node->data.floatLiteral;
            // 打印数值，不打印字符串字面量（避免段错误）
            fprintf(file, "  value: %f\n", floatLit->value);
            if (floatLit->literal != ZR_NULL) {
                fprintf(file, "  literal: <string>\n");
            }
            break;
        }
        case ZR_AST_BOOLEAN_LITERAL: {
            SZrBooleanLiteral *boolLit = &node->data.booleanLiteral;
            fprintf(file, "  value: %s\n", boolLit->value ? "true" : "false");
            break;
        }
        case ZR_AST_NULL_LITERAL: {
            fprintf(file, "  value: null\n");
            break;
        }
        case ZR_AST_ASSIGNMENT_EXPRESSION: {
            SZrAssignmentExpression *assign = &node->data.assignmentExpression;
            for (TZrSize i = 0; i < indent + 1; i++) fprintf(file, "  ");
            fprintf(file, "left: ");
            print_ast_node(state, file, assign->left, indent + 1);
            for (TZrSize i = 0; i < indent + 1; i++) fprintf(file, "  ");
            fprintf(file, "op: %s\n", assign->op.op ? assign->op.op : "=");
            for (TZrSize i = 0; i < indent + 1; i++) fprintf(file, "  ");
            fprintf(file, "right: ");
            print_ast_node(state, file, assign->right, indent + 1);
            break;
        }
        case ZR_AST_BINARY_EXPRESSION: {
            SZrBinaryExpression *bin = &node->data.binaryExpression;
            for (TZrSize i = 0; i < indent + 1; i++) fprintf(file, "  ");
            fprintf(file, "left: ");
            print_ast_node(state, file, bin->left, indent + 1);
            for (TZrSize i = 0; i < indent + 1; i++) fprintf(file, "  ");
            fprintf(file, "op: %s\n", bin->op.op ? bin->op.op : "?");
            for (TZrSize i = 0; i < indent + 1; i++) fprintf(file, "  ");
            fprintf(file, "right: ");
            print_ast_node(state, file, bin->right, indent + 1);
            break;
        }
        case ZR_AST_FUNCTION_CALL: {
            SZrFunctionCall *call = &node->data.functionCall;
            if (call->args != ZR_NULL) {
                for (TZrSize i = 0; i < indent + 1; i++) fprintf(file, "  ");
                fprintf(file, "args (%zu):\n", call->args->count);
                for (TZrSize i = 0; i < call->args->count; i++) {
                    print_ast_node(state, file, call->args->nodes[i], indent + 2);
                }
            }
            break;
        }
        case ZR_AST_MEMBER_EXPRESSION: {
            SZrMemberExpression *member = &node->data.memberExpression;
            for (TZrSize i = 0; i < indent + 1; i++) fprintf(file, "  ");
            fprintf(file, "computed: %s\n", member->computed ? "true" : "false");
            for (TZrSize i = 0; i < indent + 1; i++) fprintf(file, "  ");
            fprintf(file, "property: ");
            print_ast_node(state, file, member->property, indent + 1);
            break;
        }
        case ZR_AST_PRIMARY_EXPRESSION: {
            SZrPrimaryExpression *primary = &node->data.primaryExpression;
            for (TZrSize i = 0; i < indent + 1; i++) fprintf(file, "  ");
            fprintf(file, "property: ");
            print_ast_node(state, file, primary->property, indent + 1);
            if (primary->members != ZR_NULL && primary->members->count > 0) {
                for (TZrSize i = 0; i < indent + 1; i++) fprintf(file, "  ");
                fprintf(file, "members (%zu):\n", primary->members->count);
                for (TZrSize i = 0; i < primary->members->count; i++) {
                    print_ast_node(state, file, primary->members->nodes[i], indent + 2);
                }
            }
            break;
        }
        case ZR_AST_EXPRESSION_STATEMENT: {
            SZrExpressionStatement *stmt = &node->data.expressionStatement;
            print_ast_node(state, file, stmt->expr, indent + 1);
            break;
        }
        case ZR_AST_LAMBDA_EXPRESSION: {
            SZrLambdaExpression *lambda = &node->data.lambdaExpression;
            // 打印参数列表
            if (lambda->params != ZR_NULL && lambda->params->count > 0) {
                for (TZrSize i = 0; i < indent + 1; i++) fprintf(file, "  ");
                fprintf(file, "params (%zu):\n", lambda->params->count);
                for (TZrSize i = 0; i < lambda->params->count; i++) {
                    print_ast_node(state, file, lambda->params->nodes[i], indent + 2);
                }
            } else {
                for (TZrSize i = 0; i < indent + 1; i++) fprintf(file, "  ");
                fprintf(file, "params: ()\n");
            }
            // 打印可变参数（如果有）
            if (lambda->args != ZR_NULL) {
                for (TZrSize i = 0; i < indent + 1; i++) fprintf(file, "  ");
                fprintf(file, "args: <parameter>\n");
            }
            // 打印函数体
            if (lambda->block != ZR_NULL) {
                for (TZrSize i = 0; i < indent + 1; i++) fprintf(file, "  ");
                fprintf(file, "body: ");
                print_ast_node(state, file, lambda->block, indent + 1);
            }
            break;
        }
        case ZR_AST_TEST_DECLARATION: {
            SZrTestDeclaration *test = &node->data.testDeclaration;
            // 打印测试名称
            if (test->name != ZR_NULL && test->name->name != ZR_NULL) {
                for (TZrSize i = 0; i < indent + 1; i++) fprintf(file, "  ");
                fprintf(file, "name: ");
                print_ast_node(state, file, (SZrAstNode *)test->name, indent + 1);
            }
            // 打印参数列表
            if (test->params != ZR_NULL && test->params->count > 0) {
                for (TZrSize i = 0; i < indent + 1; i++) fprintf(file, "  ");
                fprintf(file, "params (%zu):\n", test->params->count);
                for (TZrSize i = 0; i < test->params->count; i++) {
                    print_ast_node(state, file, test->params->nodes[i], indent + 2);
                }
            }
            // 打印可变参数（如果有）
            if (test->args != ZR_NULL) {
                for (TZrSize i = 0; i < indent + 1; i++) fprintf(file, "  ");
                fprintf(file, "args: <parameter>\n");
            }
            // 打印函数体
            if (test->body != ZR_NULL) {
                for (TZrSize i = 0; i < indent + 1; i++) fprintf(file, "  ");
                fprintf(file, "body: ");
                print_ast_node(state, file, test->body, indent + 1);
            }
            break;
        }
        default:
            // 对于其他类型，只打印类型名称
            break;
    }
}

// 写入语法树文件 (.zrs)
ZR_PARSER_API TBool ZrWriterWriteSyntaxTreeFile(SZrState *state, SZrAstNode *ast, const TChar *filename) {
    if (state == ZR_NULL || ast == ZR_NULL || filename == ZR_NULL) {
        return ZR_FALSE;
    }
    
    FILE *file = fopen(filename, "w");
    if (file == ZR_NULL) {
        return ZR_FALSE;
    }
    
    fprintf(file, "// ZR Syntax Tree File (.zrs)\n");
    fprintf(file, "// Generated from parsed AST\n\n");
    
    print_ast_node(state, file, ast, 0);
    
    fclose(file);
    return ZR_TRUE;
}

