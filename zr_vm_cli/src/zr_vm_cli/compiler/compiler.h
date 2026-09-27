#ifndef ZR_VM_CLI_COMPILER_H
#define ZR_VM_CLI_COMPILER_H

#include "command/command.h"
#include "project/project.h"
#include "zr_vm_parser/writer.h"

/** @brief 一次项目编译的观测结果；计数只反映本次扫描的模块集合。 */
typedef struct SZrCliCompileSummary {
    TZrSize compiledCount;
    TZrSize skippedCount;
    TZrSize removedCount;
    TZrUInt64 comptimeCacheHitCount;
    TZrUInt64 comptimeCacheMissCount;
    TZrUInt64 comptimeCacheRejectedCount;
    TZrBool packedAssembly;
    TZrChar zrmPath[ZR_LIBRARY_MAX_PATH_LENGTH];
} SZrCliCompileSummary;

/** @brief app 与测试使用的普通项目编译入口。 */
TZrBool ZrCli_Compiler_CompileProjectWithSummary(const SZrCliCommand *command, SZrCliCompileSummary *summary);
/** @brief 为每个模块的编译隔离环境额外注册宿主提供者；回调不得保留 global。 */
TZrBool ZrCli_Compiler_CompileProjectWithSummaryAndBootstrap(const SZrCliCommand *command,
                                                             SZrCliCompileSummary *summary,
                                                             FZrCliProjectGlobalBootstrap bootstrap,
                                                             TZrPtr userData);
/** @brief 把项目 AOT 模式投影到 writer，保证 full AOT 与符号裁剪一致。 */
TZrBool ZrCli_Compiler_ApplyProjectAotWriterOptions(const SZrCliProjectContext *project,
                                                    SZrAotWriterOptions *options);
/** @brief 将编译成败转换为进程退出码的 app 入口。 */
int ZrCli_Compiler_CompileProject(const SZrCliCommand *command);

#endif
