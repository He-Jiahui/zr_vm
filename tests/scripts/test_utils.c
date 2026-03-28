//
// Created by Auto on 2025/01/XX.
//

#include "test_utils.h"
#include "zr_vm_core/conversion.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_common/zr_instruction_conf.h"
#include "test_support.h"
#include <sys/stat.h>

#ifndef ZR_VM_SCRIPTS_SOURCE_DIR
#define ZR_VM_SCRIPTS_SOURCE_DIR "tests/scripts"
#endif

#ifndef ZR_VM_SCRIPTS_BINARY_DIR
#define ZR_VM_SCRIPTS_BINARY_DIR "tests/scripts"
#endif

// 包含cJSON库
#ifdef __cplusplus
extern "C" {
#endif
// 使用相对路径包含cJSON
#include "../../../third_party/zr_c_json/cJSON/cJSON.h"
#ifdef __cplusplus
}
#endif

#ifdef _MSC_VER
    #include <direct.h>
    #define mkdir(path, mode) _mkdir(path)
#else
    #include <unistd.h>
#endif

static void ensure_directory(const TZrChar* path) {
    if (path == ZR_NULL || path[0] == '\0') {
        return;
    }
    mkdir(path, 0755);
}

static void build_output_path(const TZrChar* rootDir, const TZrChar* baseName, const TZrChar* subDir, const TZrChar* extension,
                              TZrChar* outPath, TZrSize maxLen) {
    if (rootDir == ZR_NULL || baseName == ZR_NULL || subDir == ZR_NULL || extension == ZR_NULL || outPath == ZR_NULL ||
        maxLen == 0) {
        return;
    }

    TZrChar outputDir[1024];
    TZrChar targetDir[1024];
    snprintf(outputDir, sizeof(outputDir), "%s/output", rootDir);
    snprintf(targetDir, sizeof(targetDir), "%s/%s", outputDir, subDir);
    ensure_directory(outputDir);
    ensure_directory(targetDir);

    snprintf(outPath, maxLen, "%s/%s%s", targetDir, baseName, extension);
}

// 创建测试用的VM状态
SZrState* create_test_state(void) {
    return ZrTests_State_Create(ZR_NULL);
}

// 销毁测试状态
void destroy_test_state(SZrState* state) {
    ZrTests_State_Destroy(state);
}

// 加载zr文件内容
TZrChar* load_zr_file(const TZrChar* filepath, TZrSize* outLength) {
    if (filepath == ZR_NULL || outLength == ZR_NULL) {
        return ZR_NULL;
    }
    
    FILE* file = fopen(filepath, "rb");
    if (file == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 获取文件大小
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (fileSize < 0) {
        fclose(file);
        return ZR_NULL;
    }
    
    // 分配内存
    TZrChar* buffer = (TZrChar*)malloc((TZrSize)fileSize + 1);
    if (buffer == ZR_NULL) {
        fclose(file);
        return ZR_NULL;
    }
    
    // 读取文件内容
    TZrSize readSize = (TZrSize)fread(buffer, 1, (TZrSize)fileSize, file);
    fclose(file);
    
    if (readSize != (TZrSize)fileSize) {
        free(buffer);
        return ZR_NULL;
    }
    
    buffer[readSize] = '\0';
    *outLength = readSize;
    return buffer;
}

// 解析并编译zr代码
SZrTestResult* parse_and_compile(SZrState* state, const TZrChar* source, TZrSize sourceLength, const TZrChar* sourceName) {
    if (state == ZR_NULL || source == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrTestResult* result = (SZrTestResult*)malloc(sizeof(SZrTestResult));
    if (result == ZR_NULL) {
        return ZR_NULL;
    }
    
    result->success = ZR_FALSE;
    result->errorMessage = ZR_NULL;
    result->state = state;
    result->ast = ZR_NULL;
    result->function = ZR_NULL;
    
    // 创建源文件名
    SZrString* sourceNameStr = ZR_NULL;
    if (sourceName != ZR_NULL) {
        sourceNameStr = ZrCore_String_Create(state, sourceName, strlen(sourceName));
    } else {
        sourceNameStr = ZrCore_String_Create(state, "test.zr", 7);
    }
    
    // 解析AST
    result->ast = ZrParser_Parse(state, source, sourceLength, sourceNameStr);
    if (result->ast == ZR_NULL) {
        result->errorMessage = "Failed to parse source code";
        return result;
    }
    
    // 编译为函数
    result->function = ZrParser_Compiler_Compile(state, result->ast);
    if (result->function == ZR_NULL) {
        result->errorMessage = "Failed to compile AST to function";
        return result;
    }
    
    result->success = ZR_TRUE;
    return result;
}

// 执行函数
TZrBool execute_function(SZrState* state, SZrFunction* function, SZrTypeValue* result) {
    return ZrTests_Function_Execute(state, function, result);
}

// 获取输出目录路径
void get_output_path(const TZrChar* baseName, const TZrChar* subDir, const TZrChar* extension, TZrChar* outPath, TZrSize maxLen) {
    build_output_path(ZR_VM_SCRIPTS_BINARY_DIR, baseName, subDir, extension, outPath, maxLen);
}

void get_golden_output_path(const TZrChar* baseName, const TZrChar* subDir, const TZrChar* extension, TZrChar* outPath, TZrSize maxLen) {
    build_output_path(ZR_VM_SCRIPTS_SOURCE_DIR, baseName, subDir, extension, outPath, maxLen);
}

void get_test_case_path(const TZrChar* fileName, TZrChar* outPath, TZrSize maxLen) {
    if (fileName == ZR_NULL || outPath == ZR_NULL || maxLen == 0) {
        return;
    }

    snprintf(outPath, maxLen, "%s/test_cases/%s", ZR_VM_SCRIPTS_SOURCE_DIR, fileName);
}

// 辅助函数：将AST节点序列化为JSON（简化版本）
static cJSON* ast_node_to_json(SZrState* state, SZrAstNode* node) {
    if (node == ZR_NULL) {
        return cJSON_CreateNull();
    }
    
    cJSON* json = cJSON_CreateObject();
    if (json == ZR_NULL) {
        return ZR_NULL;
    }
    
    // 添加节点类型
    const TZrChar* typeName = "UNKNOWN";
    switch (node->type) {
        case ZR_AST_SCRIPT: typeName = "SCRIPT"; break;
        case ZR_AST_MODULE_DECLARATION: typeName = "MODULE_DECLARATION"; break;
        case ZR_AST_VARIABLE_DECLARATION: typeName = "VARIABLE_DECLARATION"; break;
        case ZR_AST_FUNCTION_DECLARATION: typeName = "FUNCTION_DECLARATION"; break;
        case ZR_AST_INTEGER_LITERAL: typeName = "INTEGER_LITERAL"; break;
        case ZR_AST_FLOAT_LITERAL: typeName = "FLOAT_LITERAL"; break;
        case ZR_AST_STRING_LITERAL: typeName = "STRING_LITERAL"; break;
        case ZR_AST_BOOLEAN_LITERAL: typeName = "BOOLEAN_LITERAL"; break;
        case ZR_AST_NULL_LITERAL: typeName = "NULL_LITERAL"; break;
        case ZR_AST_IDENTIFIER_LITERAL: typeName = "IDENTIFIER_LITERAL"; break;
        case ZR_AST_BINARY_EXPRESSION: typeName = "BINARY_EXPRESSION"; break;
        case ZR_AST_UNARY_EXPRESSION: typeName = "UNARY_EXPRESSION"; break;
        case ZR_AST_IF_EXPRESSION: typeName = "IF_EXPRESSION"; break;
        case ZR_AST_WHILE_LOOP: typeName = "WHILE_LOOP"; break;
        case ZR_AST_FOR_LOOP: typeName = "FOR_LOOP"; break;
        case ZR_AST_FOREACH_LOOP: typeName = "FOREACH_LOOP"; break;
        case ZR_AST_RETURN_STATEMENT: typeName = "RETURN_STATEMENT"; break;
        case ZR_AST_LAMBDA_EXPRESSION: typeName = "LAMBDA_EXPRESSION"; break;
        case ZR_AST_CLASS_DECLARATION: typeName = "CLASS_DECLARATION"; break;
        case ZR_AST_ENUM_DECLARATION: typeName = "ENUM_DECLARATION"; break;
        case ZR_AST_STRUCT_DECLARATION: typeName = "STRUCT_DECLARATION"; break;
        default: typeName = "UNKNOWN"; break;
    }
    cJSON_AddStringToObject(json, "type", typeName);
    
    // 添加位置信息
    cJSON* location = cJSON_CreateObject();
    cJSON_AddNumberToObject(location, "startLine", node->location.start.line);
    cJSON_AddNumberToObject(location, "startColumn", node->location.start.column);
    cJSON_AddNumberToObject(location, "endLine", node->location.end.line);
    cJSON_AddNumberToObject(location, "endColumn", node->location.end.column);
    cJSON_AddItemToObject(json, "location", location);
    
    // 根据节点类型添加特定信息
    switch (node->type) {
        case ZR_AST_INTEGER_LITERAL: {
            cJSON_AddNumberToObject(json, "value", (double)node->data.integerLiteral.value);
            break;
        }
        case ZR_AST_FLOAT_LITERAL: {
            cJSON_AddNumberToObject(json, "value", node->data.floatLiteral.value);
            break;
        }
        case ZR_AST_BOOLEAN_LITERAL: {
            cJSON_AddBoolToObject(json, "value", node->data.booleanLiteral.value);
            break;
        }
        default:
            // 其他类型可以进一步扩展
            break;
    }
    
    return json;
}

// 输出AST到文件（文本和JSON）
TZrBool dump_ast_to_file(SZrState* state, SZrAstNode* ast, const TZrChar* basePath) {
    if (state == ZR_NULL || ast == ZR_NULL || basePath == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 输出文本格式
    TZrChar textPath[1024];
    get_output_path(basePath, "ast", ".zrs", textPath, sizeof(textPath));
    TZrBool textResult = ZrParser_Writer_WriteSyntaxTreeFile(state, ast, textPath);
    
    // 输出JSON格式
    TZrChar jsonPath[1024];
    get_output_path(basePath, "ast", ".zrs.json", jsonPath, sizeof(jsonPath));
    
    cJSON* json = ast_node_to_json(state, ast);
    if (json != ZR_NULL) {
        TZrChar* jsonString = cJSON_Print(json);
        if (jsonString != ZR_NULL) {
            FILE* jsonFile = fopen(jsonPath, "w");
            if (jsonFile != ZR_NULL) {
                fprintf(jsonFile, "%s", jsonString);
                fclose(jsonFile);
            }
            free(jsonString);
        }
        cJSON_Delete(json);
    }
    
    return textResult;
}

// 输出中间码到文件（文本和JSON）
TZrBool dump_intermediate_to_file(SZrState* state, SZrFunction* function, const TZrChar* basePath) {
    if (state == ZR_NULL || function == ZR_NULL || basePath == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 输出文本格式
    TZrChar textPath[1024];
    get_output_path(basePath, "intermediate", ".zri", textPath, sizeof(textPath));
    TZrBool textResult = ZrParser_Writer_WriteIntermediateFile(state, function, textPath);
    
    // 输出JSON格式
    TZrChar jsonPath[1024];
    get_output_path(basePath, "intermediate", ".zri.json", jsonPath, sizeof(jsonPath));
    
    cJSON* json = cJSON_CreateObject();
    if (json != ZR_NULL) {
        cJSON_AddNumberToObject(json, "startLine", function->lineInSourceStart);
        cJSON_AddNumberToObject(json, "endLine", function->lineInSourceEnd);
        cJSON_AddNumberToObject(json, "parameterCount", function->parameterCount);
        cJSON_AddBoolToObject(json, "hasVariableArguments", function->hasVariableArguments);
        cJSON_AddNumberToObject(json, "stackSize", function->stackSize);
        cJSON_AddNumberToObject(json, "instructionsLength", function->instructionsLength);
        cJSON_AddNumberToObject(json, "constantValueLength", function->constantValueLength);
        cJSON_AddNumberToObject(json, "localVariableLength", function->localVariableLength);
        
        // 常量列表
        cJSON* constants = cJSON_CreateArray();
        for (TZrUInt32 i = 0; i < function->constantValueLength; i++) {
            SZrTypeValue* constant = &function->constantValueList[i];
            cJSON* constItem = cJSON_CreateObject();
            cJSON_AddNumberToObject(constItem, "index", i);
            cJSON_AddNumberToObject(constItem, "type", constant->type);
            cJSON_AddItemToArray(constants, constItem);
        }
        cJSON_AddItemToObject(json, "constants", constants);
        
        // 指令列表
        cJSON* instructions = cJSON_CreateArray();
        for (TZrUInt32 i = 0; i < function->instructionsLength; i++) {
            TZrInstruction* inst = &function->instructionsList[i];
            cJSON* instItem = cJSON_CreateObject();
            cJSON_AddNumberToObject(instItem, "index", i);
            cJSON_AddNumberToObject(instItem, "opcode", inst->instruction.operationCode);
            cJSON_AddNumberToObject(instItem, "operandExtra", inst->instruction.operandExtra);
            cJSON_AddItemToArray(instructions, instItem);
        }
        cJSON_AddItemToObject(json, "instructions", instructions);
        
        TZrChar* jsonString = cJSON_Print(json);
        if (jsonString != ZR_NULL) {
            FILE* jsonFile = fopen(jsonPath, "w");
            if (jsonFile != ZR_NULL) {
                fprintf(jsonFile, "%s", jsonString);
                fclose(jsonFile);
            }
            free(jsonString);
        }
        cJSON_Delete(json);
    }
    
    return textResult;
}

TZrBool dump_binary_to_file(SZrState* state, SZrFunction* function, const TZrChar* basePath) {
    if (state == ZR_NULL || function == ZR_NULL || basePath == ZR_NULL) {
        return ZR_FALSE;
    }

    TZrChar binaryPath[1024];
    get_output_path(basePath, "binary", ".zro", binaryPath, sizeof(binaryPath));
    return ZrParser_Writer_WriteBinaryFile(state, function, binaryPath);
}

// 输出运行状态到文件（文本和JSON）
TZrBool dump_runtime_state(SZrState* state, const TZrChar* basePath) {
    if (state == ZR_NULL || basePath == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 输出文本格式
    TZrChar textPath[1024];
    get_output_path(basePath, "runtime", ".runtime.txt", textPath, sizeof(textPath));
    
    FILE* file = fopen(textPath, "w");
    if (file == ZR_NULL) {
        return ZR_FALSE;
    }
    
    fprintf(file, "// ZR Runtime State\n");
    fprintf(file, "// Generated from execution\n\n");
    
    fprintf(file, "THREAD_STATUS: %d\n", (int)state->threadStatus);
    fprintf(file, "STACK_BASE: %p\n", (void*)state->stackBase.valuePointer);
    fprintf(file, "STACK_TOP: %p\n", (void*)state->stackTop.valuePointer);
    fprintf(file, "STACK_TAIL: %p\n", (void*)state->stackTail.valuePointer);
    
    // 输出调用栈信息
    if (state->callInfoList != ZR_NULL) {
        fprintf(file, "CALL_INFO_LIST: %p\n", (void*)state->callInfoList);
    }
    
    fclose(file);
    
    // 输出JSON格式
    TZrChar jsonPath[1024];
    get_output_path(basePath, "runtime", ".runtime.json", jsonPath, sizeof(jsonPath));
    
    cJSON* json = cJSON_CreateObject();
    if (json != ZR_NULL) {
        cJSON_AddNumberToObject(json, "threadStatus", state->threadStatus);
        cJSON_AddStringToObject(json, "stackBase", "pointer");
        cJSON_AddStringToObject(json, "stackTop", "pointer");
        cJSON_AddStringToObject(json, "stackTail", "pointer");
        
        if (state->callInfoList != ZR_NULL) {
            cJSON_AddStringToObject(json, "callInfoList", "pointer");
        }
        
        TZrChar* jsonString = cJSON_Print(json);
        if (jsonString != ZR_NULL) {
            FILE* jsonFile = fopen(jsonPath, "w");
            if (jsonFile != ZR_NULL) {
                fprintf(jsonFile, "%s", jsonString);
                fclose(jsonFile);
            }
            free(jsonString);
        }
        cJSON_Delete(json);
    }
    
    return ZR_TRUE;
}

// 比较两个值是否相等
TZrBool compare_values(SZrState* state, SZrTypeValue* a, SZrTypeValue* b) {
    if (state == ZR_NULL || a == ZR_NULL || b == ZR_NULL) {
        return ZR_FALSE;
    }
    
    return ZrCore_Value_Equal(state, a, b);
}

// 释放测试结果
void free_test_result(SZrTestResult* result) {
    if (result == ZR_NULL) {
        return;
    }
    
    if (result->ast != ZR_NULL && result->state != ZR_NULL) {
        ZrParser_Ast_Free(result->state, result->ast);
    }
    
    free(result);
}
