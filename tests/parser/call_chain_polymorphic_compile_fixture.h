#ifndef ZR_VM_TESTS_PARSER_CALL_CHAIN_POLYMORPHIC_COMPILE_FIXTURE_H
#define ZR_VM_TESTS_PARSER_CALL_CHAIN_POLYMORPHIC_COMPILE_FIXTURE_H

#include "path_support.h"

typedef struct SZrGlobalState SZrGlobalState;
typedef struct SZrState SZrState;
typedef struct SZrFunction SZrFunction;

/** 多态调用链基准的独立工程副本及其编译结果；state 由 global 拥有。 */
typedef struct ZrCallChainPolymorphicCompileFixture {
    char projectPath[ZR_TESTS_PATH_MAX];
    char sourcePath[ZR_TESTS_PATH_MAX];
    char sourceRootPath[ZR_TESTS_PATH_MAX];
    char *source;
    SZrGlobalState *global;
    SZrState *state;
    SZrFunction *function;
} ZrCallChainPolymorphicCompileFixture;

/**
 * @brief 复制调用链基准工程并编译 main.zr，供编译器回归用例检查字节码。
 * @pre fixture 可写且未持有上一轮资源；artifactName 非空，用于隔离输出目录。
 * @return 成功时由调用方用 Free 释放内存资源；失败时可用 Free 清理部分状态。
 * @note 已创建的测试工程文件不会由 Free 删除。
 */
TZrBool ZrTests_PrepareCallChainPolymorphicCompileFixture(ZrCallChainPolymorphicCompileFixture *fixture,
                                                          const TZrChar *artifactName);
/** @brief 释放编译函数、源码缓冲和 VM 全局状态；接受空指针或已清零的 fixture。 */
void ZrTests_FreeCallChainPolymorphicCompileFixture(ZrCallChainPolymorphicCompileFixture *fixture);

#endif
