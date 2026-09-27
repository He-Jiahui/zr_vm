#ifndef ZR_VM_CLI_COMPILER_AOT_EXPORTS_H
#define ZR_VM_CLI_COMPILER_AOT_EXPORTS_H

#include "compiler/compiler_aot.h"

/** @brief 初始化共享根容器中的导出声明子数组。 */
void ZrCli_Compiler_AotExportDeclarations_Init(SZrCliAotPreserveRoots *roots);
/** @brief 释放导出声明数组，不释放从项目配置借用的 target 字符串。 */
void ZrCli_Compiler_AotExportDeclarations_Free(SZrCliAotPreserveRoots *roots);

/** @brief 发布项目导出声明，并在可找到元数据时关联 TypeDef/MemberDef token。
 * @pre roots 已初始化；options 仅借用 roots 中的数组，writer 返回后方可释放。
 */
TZrBool ZrCli_Compiler_ApplyProjectAotExportDeclarations(const SZrCliProjectContext *project,
                                                         const SZrFunction *function,
                                                         SZrAotWriterOptions *options,
                                                         SZrCliAotPreserveRoots *roots);

#endif
