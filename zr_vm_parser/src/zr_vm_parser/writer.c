//
// Created by Auto on 2025/01/XX.
//

#include "zr_vm_parser/writer.h"

#include "zr_vm_core/function.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/location.h"
#include "zr_vm_common/zr_io_conf.h"
#include "zr_vm_common/zr_instruction_conf.h"

#include <stdio.h>
#include <string.h>

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
    TUInt32 versionMajor = 0;
    fwrite(&versionMajor, sizeof(TUInt32), 1, file);
    
    // VERSION_MINOR (4 bytes)
    TUInt32 versionMinor = 0;
    fwrite(&versionMinor, sizeof(TUInt32), 1, file);
    
    // VERSION_PATCH (4 bytes)
    TUInt32 versionPatch = 1;
    fwrite(&versionPatch, sizeof(TUInt32), 1, file);
    
    // FORMAT (8 bytes)
    TUInt64 format = ((TUInt64)versionMajor << 32) | versionMinor;
    fwrite(&format, sizeof(TUInt64), 1, file);
    
    // NATIVE_INT_SIZE (1 byte)
    TUInt8 nativeIntSize = 8;
    fwrite(&nativeIntSize, sizeof(TUInt8), 1, file);
    
    // SIZE_T_SIZE (1 byte)
    TUInt8 sizeTypeSize = 8;
    fwrite(&sizeTypeSize, sizeof(TUInt8), 1, file);
    
    // INSTRUCTION_SIZE (1 byte)
    TUInt8 instructionSize = 8;
    fwrite(&instructionSize, sizeof(TUInt8), 1, file);
    
    // ENDIAN (1 byte)
    TUInt8 endian = ZR_IO_IS_LITTLE_ENDIAN ? 1 : 0;
    fwrite(&endian, sizeof(TUInt8), 1, file);
    
    // DEBUG (1 byte)
    TUInt8 debug = 0;
    fwrite(&debug, sizeof(TUInt8), 1, file);
    
    // OPT (3 bytes)
    TUInt8 opt[3] = {0, 0, 0};
    fwrite(opt, sizeof(TUInt8), 3, file);
    
    // MODULES_LENGTH (8 bytes)
    TUInt64 modulesLength = 1;
    fwrite(&modulesLength, sizeof(TUInt64), 1, file);
    
    // MODULE: NAME [string]
    SZrString *moduleName = ZrStringCreate(state, "simple", 6);
    TZrSize nameLength = (moduleName->shortStringLength < 0xFF) ? 
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
    SZrString *funcName = ZrStringCreate(state, "__entry", 7);
    TZrSize funcNameLength = (funcName->shortStringLength < 0xFF) ? 
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
    TUInt64 hasVarArgs = function->hasVariableArguments ? 1 : 0;
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
                        TZrSize strLength = (str->shortStringLength < 0xFF) ? 
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
    
    fprintf(file, "FUNCTION: __entry\n");
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
    
    // 指令列表
    fprintf(file, "INSTRUCTIONS (%u):\n", function->instructionsLength);
    for (TUInt32 i = 0; i < function->instructionsLength; i++) {
        TZrInstruction *inst = &function->instructionsList[i];
        EZrInstructionCode opcode = (EZrInstructionCode)inst->instruction.operationCode;
        TUInt16 operandExtra = inst->instruction.operandExtra;
        
        fprintf(file, "  [%u] ", i);
        
        // 输出操作码名称（简化处理）
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
        
        // 输出操作数（根据指令类型决定格式）
        // GET_CONSTANT, GET_STACK, SET_STACK 等使用 operandExtra + operand2[0]
        // ADD_INT, SUB_INT 等使用 operandExtra + operand1[0] + operand1[1]
        // FUNCTION_RETURN 使用 operandExtra + operand1[0] + operand1[1]
        fprintf(file, " (extra=%u", operandExtra);
        
        // 检查指令类型，决定使用哪种操作数格式
        switch (opcode) {
            case ZR_INSTRUCTION_ENUM(GET_CONSTANT):
            case ZR_INSTRUCTION_ENUM(SET_CONSTANT):
            case ZR_INSTRUCTION_ENUM(GET_STACK):
            case ZR_INSTRUCTION_ENUM(SET_STACK):
            case ZR_INSTRUCTION_ENUM(GET_CLOSURE):
            case ZR_INSTRUCTION_ENUM(SET_CLOSURE):
            case ZR_INSTRUCTION_ENUM(GET_VALUE):
            case ZR_INSTRUCTION_ENUM(SET_VALUE):
            case ZR_INSTRUCTION_ENUM(JUMP):
            case ZR_INSTRUCTION_ENUM(JUMP_IF):
            case ZR_INSTRUCTION_ENUM(THROW):
                // 使用 operand2[0] (TInt32)
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
                // 使用 operand1[0] + operand1[1] (TUInt16)
                fprintf(file, ", operand1=%u, operand2=%u", 
                        inst->instruction.operand.operand1[0], 
                        inst->instruction.operand.operand1[1]);
                break;
                
            case ZR_INSTRUCTION_ENUM(CREATE_OBJECT):
            case ZR_INSTRUCTION_ENUM(CREATE_ARRAY):
            case ZR_INSTRUCTION_ENUM(CREATE_CLOSURE):
            case ZR_INSTRUCTION_ENUM(TRY):
            case ZR_INSTRUCTION_ENUM(CATCH):
                // 只使用 operandExtra，不需要其他操作数
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
    
    // 子函数列表
    if (function->childFunctionLength > 0) {
        fprintf(file, "CHILD_FUNCTIONS (%u):\n", function->childFunctionLength);
        for (TUInt32 i = 0; i < function->childFunctionLength; i++) {
            fprintf(file, "  [%u] <function>\n", i);
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
                // 不打印字符串内容，避免段错误（字符串对象可能已被释放）
                fprintf(file, "  name: <string>\n");
            }
            break;
        }
        case ZR_AST_STRING_LITERAL: {
            SZrStringLiteral *str = &node->data.stringLiteral;
            if (str->value != ZR_NULL) {
                // 不打印字符串内容，避免段错误（字符串对象可能已被释放）
                fprintf(file, "  value: <string>\n");
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

