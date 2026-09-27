#ifndef ZR_VM_CLI_PROJECT_H
#define ZR_VM_CLI_PROJECT_H

#include "zr_vm_core/global.h"
#include "zr_vm_core/io.h"
#include "zr_vm_cli/conf.h"
#include "zr_vm_library/conf.h"

/** @brief 从项目全局态提取的 CLI 路径快照。
 * @note libraryProject 借用 global->userData；编译或运行全程须保留该 global。
 */
typedef struct SZrCliProjectContext {
    TZrChar projectPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar projectRoot[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar sourceRoot[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar binaryRoot[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar entryModule[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar manifestPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar comptimeCachePath[ZR_LIBRARY_MAX_PATH_LENGTH];
    const struct SZrLibrary_Project *libraryProject;
} SZrCliProjectContext;

/** @brief 模块导入名的有序去重列表；items 及元素由列表持有。 */
typedef struct SZrCliStringList {
    TZrChar **items;
    TZrSize count;
    TZrSize capacity;
} SZrCliStringList;

/** @brief 单模块增量判定所需的输入、产物和依赖快照。 */
typedef struct SZrCliManifestEntry {
    TZrChar moduleName[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar sourceHash[ZR_CLI_SOURCE_HASH_HEX_LENGTH];
    TZrChar zroHash[ZR_CLI_SOURCE_HASH_HEX_LENGTH];
    TZrChar zroPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar zriPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar aotCPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    SZrCliStringList imports;
} SZrCliManifestEntry;

/** @brief 磁盘清单的内存所有者；entries 中的 imports 一并由其释放。 */
typedef struct SZrCliIncrementalManifest {
    TZrUInt32 version;
    SZrCliManifestEntry *entries;
    TZrSize count;
    TZrSize capacity;
} SZrCliIncrementalManifest;

/** @brief 在标准库注册之后补充宿主提供者的同步回调。 */
typedef TZrBool (*FZrCliProjectGlobalBootstrap)(SZrGlobalState *global, TZrPtr userData);

/** @brief 为无项目的内联运行、单文件测试或迁移建立独立全局态；调用方释放结果。 */
SZrGlobalState *ZrCli_Project_CreateBareGlobal(void);
/** @brief 按 .zrp 建立拥有项目配置的全局态；调用方释放结果。 */
SZrGlobalState *ZrCli_Project_CreateProjectGlobal(const TZrChar *projectPath);
/** @brief 注册 CLI 运行及编译共用的标准提供者。 */
TZrBool ZrCli_Project_RegisterStandardModules(SZrGlobalState *global);
/** @brief 注册标准提供者并允许 test/宿主在相同全局态补充模块。 */
TZrBool ZrCli_Project_RegisterStandardModulesWithBootstrap(SZrGlobalState *global,
                                                           FZrCliProjectGlobalBootstrap bootstrap,
                                                           TZrPtr userData);
/** @brief 解析项目路径、入口和缓存位置，供扫描、产物与运行链复用。
 * @pre global 来自项目全局态；使用 context.libraryProject 时须保持 global 存活。
 */
TZrBool ZrCli_ProjectContext_FromGlobal(SZrCliProjectContext *context,
                                        SZrGlobalState *global,
                                        const TZrChar *projectPath);

/** @brief 沿用项目库的模块键规范化规则，避免扫描与加载使用不同键。 */
TZrBool ZrCli_Project_NormalizeModuleName(const TZrChar *modulePath, TZrChar *buffer, TZrSize bufferSize);
/** @brief 定位项目源码；依赖选择器优先交给项目库解析。 */
TZrBool ZrCli_Project_ResolveSourcePath(const SZrCliProjectContext *context,
                                        const TZrChar *moduleName,
                                        TZrChar *buffer,
                                        TZrSize bufferSize);
/** @brief 定位模块二进制；结果同时作为 AOT C 路径推导的基准。 */
TZrBool ZrCli_Project_ResolveBinaryPath(const SZrCliProjectContext *context,
                                        const TZrChar *moduleName,
                                        TZrChar *buffer,
                                        TZrSize bufferSize);
/** @brief 定位可选中间表示产物，供编译和旧产物清理共用。 */
TZrBool ZrCli_Project_ResolveIntermediatePath(const SZrCliProjectContext *context,
                                              const TZrChar *moduleName,
                                              TZrChar *buffer,
                                              TZrSize bufferSize);
/** @brief 把模块二进制位置映射到项目 AOT 源码树。 */
TZrBool ZrCli_Project_ResolveAotCPath(const SZrCliProjectContext *context,
                                      const TZrChar *moduleName,
                                      TZrChar *buffer,
                                      TZrSize bufferSize);
/** @brief 从 AOT C 文件名推导紧凑元数据 sidecar 名称，供写入与过期清理共用。 */
TZrBool ZrCli_Project_ResolveAotCompactedMetadataPathFromAotCPath(const TZrChar *aotCPath,
                                                                  TZrChar *buffer,
                                                                  TZrSize bufferSize);
/** @brief 从模块键定位其 AOT 紧凑元数据 sidecar。 */
TZrBool ZrCli_Project_ResolveAotCompactedMetadataPath(const SZrCliProjectContext *context,
                                                      const TZrChar *moduleName,
                                                      TZrChar *buffer,
                                                      TZrSize bufferSize);
/** @brief 将项目文件读句柄包装为核心 IO；成功后调用方负责执行 io.close。 */
TZrBool ZrCli_Project_OpenFileIo(SZrState *state, const TZrChar *path, TZrBool isBinary, SZrIo *io);

/** @brief 为编译产物或清单创建父目录，不创建目标文件。 */
TZrBool ZrCli_Project_EnsureParentDirectory(const TZrChar *filePath);
/** @brief 清理不再请求的可选产物；文件不存在视为成功。 */
TZrBool ZrCli_Project_RemoveFileIfExists(const TZrChar *filePath);
/** @brief 读取完整文本并补终止符；成功后调用方以 free 释放 outBuffer。 */
TZrBool ZrCli_Project_ReadTextFile(const TZrChar *path, TZrChar **outBuffer, TZrSize *outLength);
/** @brief 计算增量清单使用的稳定内容哈希，不承担密码学完整性保证。 */
TZrUInt64 ZrCli_Project_StableHashBytes(const TZrByte *bytes, TZrSize length);
/** @brief 将稳定哈希写为清单中的十六进制文本；缓冲区须容纳完整格式。 */
void ZrCli_Project_HashToHex(TZrUInt64 hash, TZrChar *buffer, TZrSize bufferSize);

/** @brief 初始化新的导入名列表；已有元素须先 Free。 */
void ZrCli_Project_StringList_Init(SZrCliStringList *list);
/** @brief 释放列表持有的每个导入名和数组。 */
void ZrCli_Project_StringList_Free(SZrCliStringList *list);
/** @brief 保存唯一导入名的副本；失败时列表保持可释放。 */
TZrBool ZrCli_Project_StringList_AppendUnique(SZrCliStringList *list, const TZrChar *value);
/** @brief 比较有序导入列表，供增量判定识别依赖变化。 */
TZrBool ZrCli_Project_StringList_Equals(const SZrCliStringList *left, const SZrCliStringList *right);
/** @brief 为下一版清单深拷贝导入名；destination 必须是未持有资源的新对象。 */
TZrBool ZrCli_Project_StringList_Copy(SZrCliStringList *destination, const SZrCliStringList *source);

/** @brief 初始化新的内存清单；已有 entries 须先 Free。 */
void ZrCli_Project_Manifest_Init(SZrCliIncrementalManifest *manifest);
/** @brief 释放清单及各模块导入名。 */
void ZrCli_Project_Manifest_Free(SZrCliIncrementalManifest *manifest);
/** @brief 加载旧增量清单；文件不存在返回空清单，格式损坏返回失败。 */
TZrBool ZrCli_Project_LoadManifest(const SZrCliProjectContext *context, SZrCliIncrementalManifest *manifest);
/** @brief 持久化新的模块快照，供下次增量编译判断脏模块。 */
TZrBool ZrCli_Project_SaveManifest(const SZrCliProjectContext *context, const SZrCliIncrementalManifest *manifest);
/** @brief 返回清单内可修改条目；追加或释放清单后指针失效。 */
SZrCliManifestEntry *ZrCli_Project_FindManifestEntry(SZrCliIncrementalManifest *manifest, const TZrChar *moduleName);
/** @brief 返回清单内只读条目；清单释放后指针失效。 */
const SZrCliManifestEntry *ZrCli_Project_FindManifestEntryConst(const SZrCliIncrementalManifest *manifest,
                                                                const TZrChar *moduleName);

#endif
