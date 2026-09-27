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

/** @brief 保存一次脚本编译的结果；state 由调用方持有，ast 由 free_test_result 回收，function 随 VM 状态存活。 */
typedef struct {
    TZrBool success;
    const TZrChar *errorMessage;
    SZrState *state;
    SZrAstNode *ast;
    SZrFunction *function;
} SZrTestResult;

/** @brief 为独立 golden 场景创建 VM 状态；调用方在所有派生对象使用完毕后调用 destroy_test_state。 */
SZrState* create_test_state(void);

/** @brief 结束场景并释放 VM 状态；不得再访问该状态派生的函数和常量。 */
void destroy_test_state(SZrState* state);

/** @brief 读取测试源文件；返回缓冲区由调用方 free，长度通过 outLength 返回。 */
TZrChar* load_zr_file(const TZrChar* filepath, TZrSize* outLength);

/** @brief 把同一份源码的 AST 与函数一起交给 golden 用例；失败也可能返回带 errorMessage 的结果，调用方均需 free_test_result。 */
SZrTestResult* parse_and_compile(SZrState* state, const TZrChar* source, TZrSize sourceLength, const TZrChar* sourceName);

/** @brief 旧脚本测试的函数执行辅助接口，当前没有调用者。TODO: 恢复测试时确认结果值及失败状态应由哪个断言消费。 */
TZrBool execute_function(SZrState* state, SZrFunction* function, SZrTypeValue* result);

/** @brief 为语法树生成文本及辅助 JSON 快照；返回值仅反映文本写入，比较用例须另查 JSON 是否存在。 */
TZrBool dump_ast_to_file(SZrState* state, SZrAstNode* ast, const TZrChar* basePath);

/** @brief 为同一编译结果生成可比较的中间码文本及辅助 JSON；返回值仅反映文本写入。 */
TZrBool dump_intermediate_to_file(SZrState* state, SZrFunction* function, const TZrChar* basePath);

/** @brief 写入 golden 比较所需的 .zro；AOT C 工件还会读取这个文件作为嵌入模块。 */
TZrBool dump_binary_to_file(SZrState* state, SZrFunction* function, const TZrChar* basePath);

/** @brief 把编译结果和 .zro 封装成 AOT C 工件供文本 golden 比较；缺失的 .zro 会先生成。 */
TZrBool dump_aot_c_to_file(SZrState* state, SZrFunction* function, const TZrChar* basePath);

/** @brief 生成 AOT LLVM 文本供 golden 比较；只验证写入，执行路径由其他测试覆盖。 */
TZrBool dump_aot_llvm_to_file(SZrState* state, SZrFunction* function, const TZrChar* basePath);

/** @brief 尝试输出诊断用运行状态；返回 TRUE 只证明文本文件成功打开，不保证写入成功，JSON 属于尽力生成。 */
TZrBool dump_runtime_state(SZrState* state, const TZrChar* basePath);

/** @brief 在同一 VM 状态内比较结果值，沿用运行时的相等语义而非字节比较。 */
TZrBool compare_values(SZrState* state, SZrTypeValue* a, SZrTypeValue* b);

/** @brief 释放结果包装及 AST；不释放借用的 state，也不单独释放 VM 管理的 function。 */
void free_test_result(SZrTestResult* result);

/** @brief 从测试输出根定位生成工件；失败时在非零容量缓冲区写空串。 */
void get_output_path(const TZrChar* baseName, const TZrChar* subDir, const TZrChar* extension, TZrChar* outPath, TZrSize maxLen);

/** @brief 定位源码树里的稳定 golden，供生成结果逐字节比较。 */
void get_golden_output_path(const TZrChar* baseName, const TZrChar* subDir, const TZrChar* extension, TZrChar* outPath, TZrSize maxLen);

/** @brief 按根测试目录下的 fixtures/scripts 定位输入；调用方须提供非空缓冲区及容量，当前归档样例路径另见实现处 BUG。 */
void get_test_case_path(const TZrChar* fileName, TZrChar* outPath, TZrSize maxLen);

#endif //ZR_VM_SCRIPTS_TEST_UTILS_H






