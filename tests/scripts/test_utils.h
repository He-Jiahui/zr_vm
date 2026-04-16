//
// Created by Auto on 2025/01/XX.
//

#ifndef ZR_VM_SCRIPTS_TEST_UTILS_H
#define ZR_VM_SCRIPTS_TEST_UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "zr_vm_core/state.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/writer.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/execution.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_common/zr_common_conf.h"
#include "zr_test_log_macros.h"

// 测试结果结构
typedef struct {
    TZrBool success;
    const TZrChar *errorMessage;
    SZrState *state;
    SZrAstNode *ast;
    SZrFunction *function;
} SZrTestResult;

// 创建测试用的VM状态
SZrState* create_test_state(void);

// 销毁测试状态
void destroy_test_state(SZrState* state);

// 加载zr文件内容
TZrChar* load_zr_file(const TZrChar* filepath, TZrSize* outLength);

// 解析并编译zr代码
SZrTestResult* parse_and_compile(SZrState* state, const TZrChar* source, TZrSize sourceLength, const TZrChar* sourceName);

// 执行函数
TZrBool execute_function(SZrState* state, SZrFunction* function, SZrTypeValue* result);

// 输出AST到文件（文本和JSON）
TZrBool dump_ast_to_file(SZrState* state, SZrAstNode* ast, const TZrChar* basePath);

// 输出中间码到文件（文本和JSON）
TZrBool dump_intermediate_to_file(SZrState* state, SZrFunction* function, const TZrChar* basePath);

// 输出二进制文件（.zro）
TZrBool dump_binary_to_file(SZrState* state, SZrFunction* function, const TZrChar* basePath);

// 输出运行状态到文件（文本和JSON）
TZrBool dump_runtime_state(SZrState* state, const TZrChar* basePath);

// 比较两个值是否相等
TZrBool compare_values(SZrState* state, SZrTypeValue* a, SZrTypeValue* b);

// 释放测试结果
void free_test_result(SZrTestResult* result);

// 获取输出目录路径
void get_output_path(const TZrChar* baseName, const TZrChar* subDir, const TZrChar* extension, TZrChar* outPath, TZrSize maxLen);

// 获取源码仓库中的 golden 文件路径
void get_golden_output_path(const TZrChar* baseName, const TZrChar* subDir, const TZrChar* extension, TZrChar* outPath, TZrSize maxLen);

// 获取脚本测试用例路径
void get_test_case_path(const TZrChar* fileName, TZrChar* outPath, TZrSize maxLen);

#endif //ZR_VM_SCRIPTS_TEST_UTILS_H





