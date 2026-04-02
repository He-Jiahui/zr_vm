//
// Created by Auto on 2025/01/XX.
//

#include "zr_vm_parser/writer.h"

#include "zr_vm_core/closure.h"
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

static const TZrChar *writer_intermediate_primitive_type_name(EZrValueType baseType) {
    switch (baseType) {
        case ZR_VALUE_TYPE_NULL:
            return "null";
        case ZR_VALUE_TYPE_BOOL:
            return "bool";
        case ZR_VALUE_TYPE_INT8:
            return "i8";
        case ZR_VALUE_TYPE_INT16:
            return "i16";
        case ZR_VALUE_TYPE_INT32:
            return "i32";
        case ZR_VALUE_TYPE_INT64:
            return "int";
        case ZR_VALUE_TYPE_UINT8:
            return "u8";
        case ZR_VALUE_TYPE_UINT16:
            return "u16";
        case ZR_VALUE_TYPE_UINT32:
            return "u32";
        case ZR_VALUE_TYPE_UINT64:
            return "uint";
        case ZR_VALUE_TYPE_FLOAT:
        case ZR_VALUE_TYPE_DOUBLE:
            return "float";
        case ZR_VALUE_TYPE_STRING:
            return "string";
        case ZR_VALUE_TYPE_FUNCTION:
        case ZR_VALUE_TYPE_CLOSURE:
            return "function";
        case ZR_VALUE_TYPE_ARRAY:
            return "array";
        default:
            return "object";
    }
}

static void writer_intermediate_format_type_ref(const SZrFunctionTypedTypeRef *typeRef,
                                                TZrChar *buffer,
                                                TZrSize bufferSize) {
    const TZrChar *baseName;
    TZrNativeString userTypeName;
    TZrNativeString elementTypeName;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }

    buffer[0] = '\0';
    if (typeRef == ZR_NULL) {
        snprintf(buffer, bufferSize, "object");
        return;
    }

    if (typeRef->isArray) {
        if (typeRef->elementTypeName != ZR_NULL) {
            elementTypeName = ZrCore_String_GetNativeString(typeRef->elementTypeName);
            snprintf(buffer, bufferSize, "%s[]", elementTypeName != ZR_NULL ? elementTypeName : "object");
        } else {
            snprintf(buffer, bufferSize, "%s[]", writer_intermediate_primitive_type_name(typeRef->elementBaseType));
        }
        return;
    }

    userTypeName = typeRef->typeName != ZR_NULL ? ZrCore_String_GetNativeString(typeRef->typeName) : ZR_NULL;
    if (userTypeName != ZR_NULL) {
        snprintf(buffer, bufferSize, "%s", userTypeName);
        return;
    }

    baseName = writer_intermediate_primitive_type_name(typeRef->baseType);
    snprintf(buffer, bufferSize, "%s", baseName);
}

static void writer_intermediate_write_metadata_parameters(FILE *file,
                                                          const SZrFunctionMetadataParameter *parameters,
                                                          TZrUInt32 parameterCount) {
    if (file == ZR_NULL) {
        return;
    }

    for (TZrUInt32 index = 0; index < parameterCount; index++) {
        const SZrFunctionMetadataParameter *parameter = &parameters[index];
        TZrNativeString name = parameter->name != ZR_NULL ? ZrCore_String_GetNativeString(parameter->name) : ZR_NULL;
        TZrChar typeBuffer[128];

        if (index > 0) {
            fprintf(file, ", ");
        }

        writer_intermediate_format_type_ref(&parameter->type, typeBuffer, sizeof(typeBuffer));
        if (name != ZR_NULL && name[0] != '\0') {
            fprintf(file, "%s: %s", name, typeBuffer);
        } else {
            fprintf(file, "%s", typeBuffer);
        }
    }
}

static void writer_intermediate_write_type_metadata(FILE *file, SZrState *state, SZrFunction *function) {
    ZR_UNUSED_PARAMETER(state);
    fprintf(file, "TYPE_METADATA:\n");

    fprintf(file, "  LOCAL_BINDINGS (%u):\n", function->typedLocalBindingLength);
    for (TZrUInt32 index = 0; index < function->typedLocalBindingLength; index++) {
        SZrFunctionTypedLocalBinding *binding = &function->typedLocalBindings[index];
        TZrNativeString name = binding->name != ZR_NULL ? ZrCore_String_GetNativeString(binding->name) : "<unnamed>";
        TZrChar typeBuffer[128];

        writer_intermediate_format_type_ref(&binding->type, typeBuffer, sizeof(typeBuffer));
        fprintf(file, "    [%u] %s: %s\n", binding->stackSlot, name != ZR_NULL ? name : "<unnamed>", typeBuffer);
    }

    fprintf(file, "  EXPORTED_SYMBOLS (%u):\n", function->typedExportedSymbolLength);
    for (TZrUInt32 index = 0; index < function->typedExportedSymbolLength; index++) {
        SZrFunctionTypedExportSymbol *symbol = &function->typedExportedSymbols[index];
        TZrNativeString name = symbol->name != ZR_NULL ? ZrCore_String_GetNativeString(symbol->name) : "<unnamed>";
        TZrChar valueTypeBuffer[128];

        writer_intermediate_format_type_ref(&symbol->valueType, valueTypeBuffer, sizeof(valueTypeBuffer));
        if (symbol->symbolKind == ZR_FUNCTION_TYPED_SYMBOL_FUNCTION) {
            fprintf(file, "    fn %s(", name != ZR_NULL ? name : "<unnamed>");
            for (TZrUInt32 paramIndex = 0; paramIndex < symbol->parameterCount; paramIndex++) {
                TZrChar paramBuffer[128];
                if (paramIndex > 0) {
                    fprintf(file, ", ");
                }
                writer_intermediate_format_type_ref(&symbol->parameterTypes[paramIndex],
                                                    paramBuffer,
                                                    sizeof(paramBuffer));
                fprintf(file, "%s", paramBuffer);
            }
            fprintf(file, "): %s\n", valueTypeBuffer);
        } else {
            fprintf(file, "    var %s: %s\n", name != ZR_NULL ? name : "<unnamed>", valueTypeBuffer);
        }
    }

    fprintf(file, "  COMPILE_TIME_VARIABLES (%u):\n", function->compileTimeVariableInfoLength);
    for (TZrUInt32 index = 0; index < function->compileTimeVariableInfoLength; index++) {
        SZrFunctionCompileTimeVariableInfo *info = &function->compileTimeVariableInfos[index];
        TZrNativeString name = info->name != ZR_NULL ? ZrCore_String_GetNativeString(info->name) : "<unnamed>";
        TZrChar typeBuffer[128];

        writer_intermediate_format_type_ref(&info->type, typeBuffer, sizeof(typeBuffer));
        fprintf(file, "    var %s: %s\n", name != ZR_NULL ? name : "<unnamed>", typeBuffer);
    }

    fprintf(file, "  COMPILE_TIME_FUNCTIONS (%u):\n", function->compileTimeFunctionInfoLength);
    for (TZrUInt32 index = 0; index < function->compileTimeFunctionInfoLength; index++) {
        SZrFunctionCompileTimeFunctionInfo *info = &function->compileTimeFunctionInfos[index];
        TZrNativeString name = info->name != ZR_NULL ? ZrCore_String_GetNativeString(info->name) : "<unnamed>";
        TZrChar returnTypeBuffer[128];

        writer_intermediate_format_type_ref(&info->returnType, returnTypeBuffer, sizeof(returnTypeBuffer));
        fprintf(file, "    fn %s(", name != ZR_NULL ? name : "<unnamed>");
        writer_intermediate_write_metadata_parameters(file, info->parameters, info->parameterCount);
        fprintf(file, "): %s\n", returnTypeBuffer);
    }

    fprintf(file, "  TESTS (%u):\n", function->testInfoLength);
    for (TZrUInt32 index = 0; index < function->testInfoLength; index++) {
        SZrFunctionTestInfo *info = &function->testInfos[index];
        TZrNativeString name = info->name != ZR_NULL ? ZrCore_String_GetNativeString(info->name) : "<unnamed>";

        fprintf(file, "    test %s(", name != ZR_NULL ? name : "<unnamed>");
        writer_intermediate_write_metadata_parameters(file, info->parameters, info->parameterCount);
        if (info->hasVariableArguments) {
            if (info->parameterCount > 0) {
                fprintf(file, ", ");
            }
            fprintf(file, "...");
        }
        fprintf(file, ")\n");
    }

    fprintf(file, "\n");
}

static void writer_intermediate_write_constant(FILE *file, SZrState *state, const SZrTypeValue *constant) {
    if (file == ZR_NULL || constant == ZR_NULL) {
        return;
    }

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
            fprintf(file, "int: %lld\n", (long long) constant->value.nativeObject.nativeInt64);
            break;

        case ZR_VALUE_TYPE_FLOAT:
        case ZR_VALUE_TYPE_DOUBLE:
            fprintf(file, "float: %f\n", constant->value.nativeObject.nativeDouble);
            break;

        case ZR_VALUE_TYPE_STRING:
            if (constant->value.object == ZR_NULL) {
                fprintf(file, "string: \"\"\n");
            } else {
                SZrRawObject *rawObj = constant->value.object;
                if (rawObj->type == ZR_RAW_OBJECT_TYPE_STRING) {
                    SZrString *str = ZR_CAST_STRING(state, rawObj);
                    TZrNativeString strStr = ZrCore_String_GetNativeString(str);
                    fprintf(file, "string: \"%s\"\n", strStr != ZR_NULL ? strStr : "");
                } else {
                    fprintf(file, "string: \"\"\n");
                }
            }
            break;

        case ZR_VALUE_TYPE_FUNCTION:
            fprintf(file, "function\n");
            break;

        case ZR_VALUE_TYPE_CLOSURE:
            if (constant->value.object != ZR_NULL) {
                SZrRawObject *rawObj = constant->value.object;
                fprintf(file, "%sclosure\n", rawObj->isNative ? "native " : "");
            } else {
                fprintf(file, "closure\n");
            }
            break;

        case ZR_VALUE_TYPE_NATIVE_POINTER:
            fprintf(file, "native pointer\n");
            break;

        default:
            fprintf(file, "unknown type: %u\n", (TZrUInt32) constant->type);
            break;
    }
}

ZR_PARSER_API TZrBool ZrParser_Writer_WriteIntermediateFile(SZrState *state, SZrFunction *function, const TZrChar *filename) {
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
        TZrNativeString funcNameStr = ZrCore_String_GetNativeString(function->functionName);
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
    for (TZrUInt32 i = 0; i < function->constantValueLength; i++) {
        SZrTypeValue *constant = &function->constantValueList[i];
        fprintf(file, "  [%u] ", i);
        writer_intermediate_write_constant(file, state, constant);
    }
    fprintf(file, "\n");
    
    // 局部变量列表
    fprintf(file, "LOCAL_VARIABLES (%u):\n", function->localVariableLength);
    for (TZrUInt32 i = 0; i < function->localVariableLength; i++) {
        SZrFunctionLocalVariable *local = &function->localVariableList[i];
        TZrNativeString nameStr = local->name ? ZrCore_String_GetNativeString(local->name) : "<unnamed>";
        fprintf(file, "  [%u] %s: offset_activate=%u, offset_dead=%u\n", 
                i, nameStr, (TZrUInt32)local->offsetActivate, (TZrUInt32)local->offsetDead);
    }
    fprintf(file, "\n");
    
    // 闭包变量列表
    fprintf(file, "CLOSURE_VARIABLES (%u):\n", function->closureValueLength);
    for (TZrUInt32 i = 0; i < function->closureValueLength; i++) {
        SZrFunctionClosureVariable *closure = &function->closureValueList[i];
        TZrNativeString nameStr = closure->name ? ZrCore_String_GetNativeString(closure->name) : "<unnamed>";
        
        // 输出类型名称
        const TZrChar *typeName = "UNKNOWN";
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

    writer_intermediate_write_type_metadata(file, state, function);
    
    // Prototype 数据列表
    if (function->prototypeData != ZR_NULL && function->prototypeCount > 0) {
        fprintf(file, "PROTOTYPES (%u):\n", function->prototypeCount);
        ZrCore_Debug_PrintPrototypesFromData(state, function, file);
        fprintf(file, "\n");
    }
    
    // 指令列表
    fprintf(file, "INSTRUCTIONS (%u):\n", function->instructionsLength);
    for (TZrUInt32 i = 0; i < function->instructionsLength; i++) {
        TZrInstruction *inst = &function->instructionsList[i];
        EZrInstructionCode opcode = (EZrInstructionCode)inst->instruction.operationCode;
        TZrUInt16 operandExtra = inst->instruction.operandExtra;
        
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
                fprintf(file, "OPCODE_%u", (TZrUInt32)opcode);
                break;
        }
        
        // 输出操作数（根据指令类型决定格式）
        // GET_CONSTANT, GET_STACK, SET_STACK 等使用 operandExtra + operand2[0]
        // ADD_INT, SUB_INT 等使用 operandExtra + operand1[0] + operand1[1]
        // FUNCTION_RETURN 使用 operandExtra + operand1[0] + operand1[1]
        fprintf(file, " (extra=%u", operandExtra);
        
        // 检查指令类型，决定使用哪种操作数格式
        switch (opcode) {
            // 使用 operand2[0] (TZrInt32) 的指令
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
                
            // 使用 operand1[0] (TZrUInt16) 的指令（单操作数）
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
                
            // 使用 operand1[0] + operand1[1] (TZrUInt16) 的指令（双操作数）
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
        for (TZrUInt32 i = 0; i < function->childFunctionLength; i++) {
            SZrFunction *childFunc = &function->childFunctionList[i];
            fprintf(file, "  [%u] FUNCTION:\n", i);
            // 输出函数名（如果存在）
            if (childFunc->functionName != ZR_NULL) {
                TZrNativeString nameStr = ZrCore_String_GetNativeString(childFunc->functionName);
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
            for (TZrUInt32 j = 0; j < childFunc->constantValueLength; j++) {
                SZrTypeValue *constant = &childFunc->constantValueList[j];
                fprintf(file, "      [%u] ", j);
                writer_intermediate_write_constant(file, state, constant);
            }
            fprintf(file, "    INSTRUCTIONS (%u):\n", childFunc->instructionsLength);
            for (TZrUInt32 j = 0; j < childFunc->instructionsLength; j++) {
                TZrInstruction *inst = &childFunc->instructionsList[j];
                EZrInstructionCode opcode = (EZrInstructionCode)inst->instruction.operationCode;
                TZrUInt16 operandExtra = inst->instruction.operandExtra;
                
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
                        fprintf(file, "OPCODE_%u", (TZrUInt32)opcode);
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
