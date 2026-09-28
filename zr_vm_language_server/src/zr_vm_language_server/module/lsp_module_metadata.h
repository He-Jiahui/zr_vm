#ifndef ZR_VM_LANGUAGE_SERVER_LSP_MODULE_METADATA_H
#define ZR_VM_LANGUAGE_SERVER_LSP_MODULE_METADATA_H

#include "project/lsp_project_internal.h"

#include "zr_vm_core/io.h"
#include "zr_vm_library/native_binding.h"
#include "zr_vm_parser/compiler.h"

/** @brief 汇合项目源码、编译器原型、二进制元数据与原生描述符的导入判定。
 *  字段均借用各自所有者；调用方只在相应项目索引、分析快照和注册表有效期内读取。
 *  sourceKind 表示导航的优先来源，nativeDescriptor 可能同时存在。 */
typedef struct SZrLspResolvedImportedModule {
    SZrString *moduleName;
    SZrLspProjectIndex *projectIndex;
    SZrLspProjectFileRecord *sourceRecord;
    const ZrLibModuleDescriptor *nativeDescriptor;
    const SZrTypePrototypeInfo *modulePrototype;
    EZrLspImportedModuleSourceKind sourceKind;
} SZrLspResolvedImportedModule;

/** @brief 按编译器记录的精确类型名读取当前分析快照中的原型；结果为借用指针。 */
const SZrTypePrototypeInfo *ZrLanguageServer_LspModuleMetadata_FindTypePrototype(SZrSemanticAnalyzer *analyzer,
                                                                                 const TZrChar *typeName);
/** @brief 将导入模块名作为类型名查询编译器原型；仅在分析快照存活时使用。 */
const SZrTypePrototypeInfo *ZrLanguageServer_LspModuleMetadata_FindModulePrototype(SZrSemanticAnalyzer *analyzer,
                                                                                   SZrString *moduleName);
/** @brief 检查项目 binary 目录中的模块元数据文件，可选返回本机路径。
 *  @pre buffer 非空时 bufferSize 应足以容纳完整路径；小缓冲区可能只获得截断路径。
 *  @return 仅表示目标文件存在，不表示元数据可解析。 */
TZrBool ZrLanguageServer_LspModuleMetadata_ProjectHasBinaryModule(SZrLspProjectIndex *projectIndex,
                                                                  const TZrChar *moduleName,
                                                                  TZrChar *buffer,
                                                                  TZrSize bufferSize);
/** @brief 按项目源码、二进制文件和原生注册表汇总导入来源，供导航与悬停复用。
 *  @pre outResolved 非空；调用方须持有 projectIndex、analyzer 及 state 的有效期。
 *  @return 至少发现一种来源或编译器原型时返回真；输出中的指针均为借用。 */
TZrBool ZrLanguageServer_LspModuleMetadata_ResolveImportedModule(SZrState *state,
                                                                 SZrSemanticAnalyzer *analyzer,
                                                                 SZrLspProjectIndex *projectIndex,
                                                                 SZrString *moduleName,
                                                                 SZrLspResolvedImportedModule *outResolved);
/** @brief 优先查 CompileTool 静态投影，再查已注册原生模块及链接子模块。
 *  @note 此入口不带项目索引；项目本地插件需通过 ResolveImportedModule 加载。 */
const ZrLibModuleDescriptor *ZrLanguageServer_LspModuleMetadata_ResolveNativeModuleDescriptor(SZrState *state,
                                                                                               const TZrChar *moduleName,
                                                                                               EZrLspImportedModuleSourceKind *outSourceKind);
/** @brief 从限定模块或注册表中寻找原生类型；结果与 outModule 借用静态投影或注册表存储。
 *  @note 无项目索引时，重名插件的项目优先级由上层导入解析处理。 */
const ZrLibTypeDescriptor *ZrLanguageServer_LspModuleMetadata_FindNativeTypeDescriptor(SZrState *state,
                                                                                       const TZrChar *typeName,
                                                                                       const ZrLibModuleDescriptor **outModule);
/** @brief 为一次 LSP 查询从项目 .zro 读取临时 SZrIoSource。
 *  @pre outSource 非空；成功后调用方必须用 FreeBinaryModuleSource 释放结果。
 *  @return 文件不存在、读取或解析失败时返回假并清空输出。 */
TZrBool ZrLanguageServer_LspModuleMetadata_LoadBinaryModuleSource(SZrState *state,
                                                                  SZrLspProjectIndex *projectIndex,
                                                                  SZrString *moduleName,
                                                                  SZrIoSource **outSource);
/** @brief 释放 LoadBinaryModuleSource 成功返回的临时元数据树。 */
void ZrLanguageServer_LspModuleMetadata_FreeBinaryModuleSource(SZrGlobalState *global, SZrIoSource *source);
/** @brief 将项目二进制元数据文件定位为导航所需的物理 file URI。 */
TZrBool ZrLanguageServer_LspModuleMetadata_ResolveBinaryModuleUri(SZrState *state,
                                                                  SZrLspProjectIndex *projectIndex,
                                                                  SZrString *moduleName,
                                                                  SZrString **outUri);
/** @brief 从 .zro 的 typed export 表取得成员定义 URI 与源码坐标。
 *  @note 找不到有效坐标时调用方可退回模块入口范围；临时源树在此函数内释放。 */
TZrBool ZrLanguageServer_LspModuleMetadata_ResolveBinaryExportDeclaration(SZrState *state,
                                                                          SZrLspProjectIndex *projectIndex,
                                                                          SZrString *moduleName,
                                                                          SZrString *memberName,
                                                                          SZrString **outUri,
                                                                          SZrFileRange *outRange);
/** @brief 优先为原生插件返回物理 URI；来源不可用及内建模块退回虚拟声明 URI。
 *  @note CompileTool 虚拟 URI 由独立投影路径处理；项目本地插件依赖 projectIndex。 */
TZrBool ZrLanguageServer_LspModuleMetadata_ResolveNativeModuleUri(SZrState *state,
                                                                  SZrLspProjectIndex *projectIndex,
                                                                  SZrString *moduleName,
                                                                  SZrString **outUri);
/** @brief 将内部来源类别转换为悬停/项目摘要的稳定显示标签。 */
const TZrChar *ZrLanguageServer_LspModuleMetadata_SourceKindLabel(EZrLspImportedModuleSourceKind sourceKind);

#endif
