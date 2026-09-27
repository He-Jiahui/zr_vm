#ifndef ZR_VM_CLI_REPL_SESSION_H
#define ZR_VM_CLI_REPL_SESSION_H

#include "zr_vm_core/gc_domain.h"
#include "zr_vm_parser/compiler.h"

struct SZrClosure;
struct SZrGlobalState;
struct SZrState;

/** 单个交互会话的持久环境；GC 根持有 closure 和 source 名称，裸指针仅作重新解析后的缓存。
 *  成功提交推进 environment/cell generation，失败提交不得发布新 binding；reset 另推进 module generation。
 */
typedef struct ZrCliReplSession {
    struct SZrGlobalState *global;
    struct SZrState *state;
    SZrGcRootHandle environmentRoot;
    SZrGcRootHandle sourceNameRoot;
    struct SZrClosure *activeClosure;
    struct SZrString *sourceName;
    SZrParserSubmissionBinding *bindings;
    TZrSize bindingCount;
    SZrParserSubmissionCallableSignature *callableSignatures;
    TZrSize callableSignatureCount;
    TZrUInt64 moduleGeneration;
    TZrUInt64 environmentGeneration;
    TZrUInt64 nextCellGeneration;
} ZrCliReplSession;

/** @brief 建立裸 VM、标准模块及首个空 closure，供 REPL 控制器持续提交 cell。
 *  @pre session 指向可写存储；初始化失败后仍可调用 Free。
 */
int ZrCli_ReplSession_Init(ZrCliReplSession *session);
/** @brief 释放 binding、GC 根和 VM；对空会话以及重复调用安全。 */
void ZrCli_ReplSession_Free(ZrCliReplSession *session);
/** @brief 编译并执行一个 cell，成功后才发布 successor closure 与结构化 binding。
 *  @note 已执行的外部副作用不随发布失败回滚；返回 0 表示提交成功。
 */
int ZrCli_ReplSession_Submit(ZrCliReplSession *session, const TZrChar *code);
/** @brief 用当前代际的 binding 推断表达式并输出语义事实，不执行或发布 cell。 */
int ZrCli_ReplSession_TypeQuery(ZrCliReplSession *session, const TZrChar *expression);
/** @brief 清除已提交 binding 和当前环境，使后续 cell 从新 generation 开始。 */
int ZrCli_ReplSession_Reset(ZrCliReplSession *session);

#endif
