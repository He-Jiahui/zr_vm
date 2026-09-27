#ifndef ZR_VM_CLI_RUNTIME_H
#define ZR_VM_CLI_RUNTIME_H

#include "zr_vm_common.h"
#include "command/command.h"
#include "project/project.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/value.h"

/** 将项目运行结果和所属 VM 一起交给 CLI、测试或 Rust 调用方；result 仅在 global 存活期间有效。 */
typedef struct SZrCliRunCapture {
    SZrGlobalState *global;
    SZrTypeValue result;
    TZrChar executedVia[32];
} SZrCliRunCapture;

/** 预备阶段拥有项目 VM；执行成功时将所有权转移给 SZrCliRunCapture。 */
typedef struct SZrCliPreparedProjectRuntime {
    SZrGlobalState *global;
    SZrCliProjectContext project;
    TZrChar effectiveEntryModule[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar entryIdentifier[ZR_LIBRARY_MAX_PATH_LENGTH];
} SZrCliPreparedProjectRuntime;

/** @brief CLI 项目运行入口；负责打印返回值和诊断后释放 capture。 */
int ZrCli_Runtime_RunProject(const SZrCliCommand *command);
/** @brief 在独立裸 VM 中编译并运行 -e 代码；不复用项目上下文。 */
int ZrCli_Runtime_RunInline(const SZrCliCommand *command);
/** @brief 为一次项目执行建立 VM、加载项目配置并注入程序参数。 */
TZrBool ZrCli_Runtime_PrepareProjectExecution(const SZrCliCommand *command,
                                             SZrCliPreparedProjectRuntime *outPrepared);
/** @brief 同上，但允许 Rust 等宿主在标准模块注册期间安装额外 provider。
 *  @pre outPrepared 为尚未拥有 VM 的可写对象；成功后由 Free 或 RunPreparedProjectCapture 接管。
 */
TZrBool ZrCli_Runtime_PrepareProjectExecutionWithBootstrap(const SZrCliCommand *command,
                                                           SZrCliPreparedProjectRuntime *outPrepared,
                                                           FZrCliProjectGlobalBootstrap bootstrap,
                                                           TZrPtr userData);
/** @brief 释放尚未移交的项目 VM；允许在执行失败后重复调用。 */
void ZrCli_Runtime_PreparedProject_Free(SZrCliPreparedProjectRuntime *prepared);
/** @brief 在预备 VM 上执行入口，成功时将 VM 和结果移交到 outCapture。
 *  @note 进入执行后的失败路径释放 prepared；参数前置条件不满足时调用方仍负责清理。
 */
TZrBool ZrCli_Runtime_RunPreparedProjectCapture(SZrCliPreparedProjectRuntime *prepared,
                                                const SZrCliCommand *command,
                                                SZrCliRunCapture *outCapture);
/** @brief 一步式执行并返回拥有 VM 的结果，供 CLI 回显及集成测试观察。 */
TZrBool ZrCli_Runtime_RunProjectCapture(const SZrCliCommand *command, SZrCliRunCapture *outCapture);
/** @brief 释放 capture 及其结果所属 VM；释放后 result 中的对象引用失效。 */
void ZrCli_Runtime_RunCapture_Free(SZrCliRunCapture *capture);
/** @brief 在执行前设置 zr.system.process.arguments，首项为入口标识，后续项来自命令行。
 *  @pre 标准模块已注册且 state 属于本次运行的 VM。
 */
TZrBool ZrCli_Runtime_InjectProcessArguments(struct SZrState *state,
                                             const TZrChar *entryIdentifier,
                                             const TZrChar *const *programArgs,
                                             TZrSize programArgCount);

#endif
