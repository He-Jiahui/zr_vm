#ifndef ZR_VM_CLI_COMPILER_AOT_H
#define ZR_VM_CLI_COMPILER_AOT_H

#include "project/project.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/state.h"
#include "zr_vm_parser/writer.h"

/** @brief AOT writer 规则输入的所有者：方法索引、泛型根及导出声明数组。
 * @note writer options 只借用这些数组；必须等同步写入完成后再 Free。
 */
typedef struct SZrCliAotPreserveRoots {
    TZrUInt32 *indices;
    TZrUInt32 count;
    TZrUInt32 capacity;
    SZrAotManifestGenericRoot *genericRoots;
    TZrUInt32 genericRootCount;
    TZrUInt32 genericRootCapacity;
    SZrAotManifestExportDeclaration *exportDeclarations;
    TZrUInt32 exportDeclarationCount;
    TZrUInt32 exportDeclarationCapacity;
} SZrCliAotPreserveRoots;

/** @brief 初始化新的根容器；重复初始化前须先释放旧数组。 */
void ZrCli_Compiler_AotPreserveRoots_Init(SZrCliAotPreserveRoots *roots);
/** @brief 释放所有根数组及泛型实参数组，不释放借用的项目字符串。 */
void ZrCli_Compiler_AotPreserveRoots_Free(SZrCliAotPreserveRoots *roots);

/** @brief 将项目保留/导出规则绑定到已编译函数的元数据和 writer 选项。
 * @pre roots 已初始化，project、state、function 存活到 writer 返回；选项借用 roots。
 */
TZrBool ZrCli_Compiler_ApplyProjectAotPreserveRules(const SZrCliProjectContext *project,
                                                    SZrState *state,
                                                    SZrFunction *function,
                                                    const TZrChar *moduleName,
                                                    SZrAotWriterOptions *options,
                                                    SZrCliAotPreserveRoots *roots);

/** @brief 从刚写出的 .zro 生成嵌入二进制的 AOT C 和可发布的元数据 sidecar。
 * @pre function、项目配置和输入路径在同步 writer 返回前有效。
 */
TZrBool ZrCli_Compiler_WriteAotCFileForModule(const SZrCliProjectContext *project,
                                              SZrState *state,
                                              SZrFunction *function,
                                              const TZrChar *moduleName,
                                              const TZrChar *sourceHash,
                                              const TZrChar *zroHash,
                                              const TZrChar *zroPath,
                                              const TZrChar *aotCPath);

#endif
