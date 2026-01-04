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

// 测试时间测量结构
typedef struct {
    clock_t startTime;
    clock_t endTime;
} SZrTestTimer;

// 测试结果结构
typedef struct {
    TBool success;
    const TChar *errorMessage;
    SZrState *state;
    SZrAstNode *ast;
    SZrFunction *function;
} SZrTestResult;

// 创建测试用的VM状态
SZrState* create_test_state(void);

// 销毁测试状态
void destroy_test_state(SZrState* state);

// 加载zr文件内容
TChar* load_zr_file(const TChar* filepath, TZrSize* outLength);

// 解析并编译zr代码
SZrTestResult* parse_and_compile(SZrState* state, const TChar* source, TZrSize sourceLength, const TChar* sourceName);

// 执行函数
TBool execute_function(SZrState* state, SZrFunction* function, SZrTypeValue* result);

// 输出AST到文件（文本和JSON）
TBool dump_ast_to_file(SZrState* state, SZrAstNode* ast, const TChar* basePath);

// 输出中间码到文件（文本和JSON）
TBool dump_intermediate_to_file(SZrState* state, SZrFunction* function, const TChar* basePath);

// 输出运行状态到文件（文本和JSON）
TBool dump_runtime_state(SZrState* state, const TChar* basePath);

// 比较两个值是否相等
TBool compare_values(SZrState* state, SZrTypeValue* a, SZrTypeValue* b);

// 释放测试结果
void free_test_result(SZrTestResult* result);

// 获取输出目录路径
void get_output_path(const TChar* baseName, const TChar* subDir, const TChar* extension, TChar* outPath, TZrSize maxLen);

#endif //ZR_VM_SCRIPTS_TEST_UTILS_H

