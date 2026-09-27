#ifndef ZR_VM_TESTS_PARSER_MATRIX_ADD_2D_COMPILE_FIXTURE_H
#define ZR_VM_TESTS_PARSER_MATRIX_ADD_2D_COMPILE_FIXTURE_H

#include "path_support.h"

typedef struct SZrGlobalState SZrGlobalState;
typedef struct SZrState SZrState;
typedef struct SZrFunction SZrFunction;

/** 二维矩阵基准的独立工程副本及其编译结果；state 由 global 拥有。 */
typedef struct ZrMatrixAdd2dCompileFixture {
    char projectPath[ZR_TESTS_PATH_MAX];
    char sourcePath[ZR_TESTS_PATH_MAX];
    char sourceRootPath[ZR_TESTS_PATH_MAX];
    char *source;
    SZrGlobalState *global;
    SZrState *state;
    SZrFunction *function;
} ZrMatrixAdd2dCompileFixture;

/**
 * @brief 复制矩阵基准工程并注册容器/系统模块，供编译器用例检查热路径。
 * @pre fixture 可写且未持有上一轮资源；artifactName 非空，用于隔离输出目录。
 * @return 成功时由调用方用 Free 释放内存资源；失败时可用 Free 清理部分状态。
 * @note 已创建的测试工程文件不会由 Free 删除。
 */
TZrBool ZrTests_PrepareMatrixAdd2dCompileFixture(ZrMatrixAdd2dCompileFixture *fixture, const TZrChar *artifactName);
/** @brief 释放编译函数、源码缓冲和 VM 全局状态；接受空指针或已清零的 fixture。 */
void ZrTests_FreeMatrixAdd2dCompileFixture(ZrMatrixAdd2dCompileFixture *fixture);

#endif
