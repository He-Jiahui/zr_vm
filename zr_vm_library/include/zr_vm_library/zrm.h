#ifndef ZR_VM_LIBRARY_ZRM_H
#define ZR_VM_LIBRARY_ZRM_H

#include "zr_vm_library/conf.h"

/** ZRM v1 的磁盘布局契约；写入器和读取器必须使用同一组条目名与格式版本。 */
#define ZR_LIBRARY_ZRM_FILE_EXTENSION ".zrm"
#define ZR_LIBRARY_ZRM_FORMAT "zr.zrm/v1"
#define ZR_LIBRARY_ZRM_MANIFEST_ENTRY "META-INF/zrm.json"
#define ZR_LIBRARY_ZRM_MODULE_ENTRY_PREFIX "modules/"
#define ZR_LIBRARY_ZRM_RESOURCE_ENTRY_PREFIX "resources/"
#define ZR_LIBRARY_ZRM_COMPILE_TOOL_ENTRY_PREFIX "compile-tools/"
#define ZR_LIBRARY_ZRM_COMPILE_TOOL_EXECUTABLE_SCHEMA \
    "zr.compile-tool-executable/v1"
#define ZR_LIBRARY_ZRM_COMPILE_TOOL_EXECUTABLE_FORMAT "zr.source/utf8-v1"
#define ZR_LIBRARY_ZRM_ERROR_BUFFER_LENGTH 512U

/** ZRM v1 约定的条目压缩方式；模块与编译工具源码由写入器固定为 STORE。 */
typedef enum EZrLibrary_ZrmCompression {
    ZR_LIBRARY_ZRM_COMPRESSION_STORE = 0,
    ZR_LIBRARY_ZRM_COMPRESSION_DEFLATE = 1
} EZrLibrary_ZrmCompression;

/** 区分运行时、测试和编译期提供者，供导入解析与执行边界检查使用。 */
typedef enum EZrLibrary_ProviderPhase {
    ZR_LIBRARY_PROVIDER_PHASE_RUNTIME = 0,
    ZR_LIBRARY_PROVIDER_PHASE_TEST = 1,
    ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL = 2
} EZrLibrary_ProviderPhase;

/** 写入请求中的程序集身份；字符串由调用方持有，须在 WriteArchive 返回前保持有效。 */
typedef struct SZrLibrary_ZrmAssemblyInfo {
    const TZrChar *name;
    const TZrChar *version;
    const TZrChar *culture;
    const TZrChar *publicKeyToken;
    const TZrChar *kind;
    const TZrChar *entryModule;
    EZrLibrary_ProviderPhase providerPhase;
    const TZrChar *publicContractHash;
} SZrLibrary_ZrmAssemblyInfo;

/** 一个待打包模块；CompileTool 阶段还必须提供对应的可执行源码路径和 hash。 */
typedef struct SZrLibrary_ZrmPackModule {
    const TZrChar *moduleKey;
    const TZrChar *sourcePath;
    const TZrChar *hash;
    const TZrChar *compileToolExecutableSourcePath;
    const TZrChar *compileToolExecutableHash;
} SZrLibrary_ZrmPackModule;

/** 资源的逻辑名用于归档定位，sourcePath 仅用于读取本机输入文件。 */
typedef struct SZrLibrary_ZrmPackResource {
    const TZrChar *logicalName;
    const TZrChar *sourcePath;
    const TZrChar *hash;
    TZrBool compress;
} SZrLibrary_ZrmPackResource;

/** 一次同步打包请求；非零计数要求相应数组及其字符串在调用期间有效。 */
typedef struct SZrLibrary_ZrmPackRequest {
    const TZrChar *outputPath;
    SZrLibrary_ZrmAssemblyInfo assembly;
    const SZrLibrary_ZrmPackModule *modules;
    TZrSize moduleCount;
    const SZrLibrary_ZrmPackResource *resources;
    TZrSize resourceCount;
} SZrLibrary_ZrmPackRequest;

/** 已打开归档的条目索引；指针属于 Archive，Close 后失效。 */
typedef struct SZrLibrary_ZrmEntryInfo {
    TZrChar logicalName[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar entryName[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar hash[64];
    TZrUInt64 uncompressedSize;
    TZrUInt64 compressedSize;
    TZrUInt32 crc32;
    EZrLibrary_ZrmCompression compression;
} SZrLibrary_ZrmEntryInfo;

/** Open/OpenBytes 建立的读取会话；索引和 ZIP 句柄由 Close 统一释放。 */
typedef struct SZrLibrary_ZrmArchive {
    TZrChar path[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar assemblyName[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar assemblyVersion[64];
    TZrChar assemblyCulture[64];
    TZrChar assemblyPublicKeyToken[128];
    TZrChar assemblyKind[64];
    TZrChar entryModule[ZR_LIBRARY_MAX_PATH_LENGTH];
    EZrLibrary_ProviderPhase providerPhase;
    TZrChar publicContractHash[128];
    SZrLibrary_ZrmEntryInfo *modules;
    TZrSize moduleCount;
    SZrLibrary_ZrmEntryInfo *resources;
    TZrSize resourceCount;
    SZrLibrary_ZrmEntryInfo *compileToolExecutables;
    TZrSize compileToolExecutableCount;
    TZrPtr zipHandle;
} SZrLibrary_ZrmArchive;

/** @brief 检查模块或资源逻辑名能否作为归档内的相对路径。
 * 供写入、读取清单和资源调用入口共用；仅接受 `/` 分隔的非空安全路径段。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_Zrm_ValidateLogicalName(const TZrChar *name);

/** @brief 将模块键映射为 `modules/<key>.zro`；失败时清空有效输出缓冲区。 */
ZR_LIBRARY_API TZrBool ZrLibrary_Zrm_BuildModuleEntryName(const TZrChar *moduleKey,
                                                          TZrChar *buffer,
                                                          TZrSize bufferSize);

/** @brief 将资源逻辑名映射为 `resources/<name>`；失败时清空有效输出缓冲区。 */
ZR_LIBRARY_API TZrBool ZrLibrary_Zrm_BuildResourceEntryName(const TZrChar *logicalName,
                                                            TZrChar *buffer,
                                                            TZrSize bufferSize);

/** @brief 将 CompileTool 模块键映射为 `compile-tools/<key>.zrs`。 */
ZR_LIBRARY_API TZrBool ZrLibrary_Zrm_BuildCompileToolExecutableEntryName(
        const TZrChar *moduleKey,
        TZrChar *buffer,
        TZrSize bufferSize);

/** @brief 同步打包模块、资源和程序集清单。
 * 输入文件与请求字符串只在调用期间借用；成功后输出路径可交给 Open/项目导入器。
 * 失败时 errorBuffer 可选；写入失败可能留下不完整输出文件，调用方不得发布它。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_Zrm_WriteArchive(const SZrLibrary_ZrmPackRequest *request,
                                                  TZrChar *errorBuffer,
                                                  TZrSize errorBufferSize);

/** @brief 从文件建立归档读取会话。
 * archive 应为未打开的对象；成功后必须 Close，Find* 返回的索引随 Close 失效。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_Zrm_Open(const TZrChar *path,
                                          SZrLibrary_ZrmArchive *archive,
                                          TZrChar *errorBuffer,
                                          TZrSize errorBufferSize);

/** @brief 从调用方持有的包字节建立归档读取会话，内部仍校验 ZIP 与清单。
 * CompileTool 当前调用链先验内容 hash；bytes 在 Close 前必须有效且不可变，sourceName 只作来源标识。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_Zrm_OpenBytes(const TZrByte *bytes,
                                               TZrSize byteCount,
                                               const TZrChar *sourceName,
                                               SZrLibrary_ZrmArchive *archive,
                                               TZrChar *errorBuffer,
                                               TZrSize errorBufferSize);

/** @brief 释放 Open/OpenBytes 的索引及 ZIP 句柄；允许对零初始化或已关闭对象重复调用。 */
ZR_LIBRARY_API void ZrLibrary_Zrm_Close(SZrLibrary_ZrmArchive *archive);

/** @brief 按模块键查找只读条目索引；返回值借用自 archive，Close 后失效。 */
ZR_LIBRARY_API const SZrLibrary_ZrmEntryInfo *ZrLibrary_Zrm_FindModule(const SZrLibrary_ZrmArchive *archive,
                                                                       const TZrChar *moduleKey);

/** @brief 按资源逻辑名查找只读条目索引；调用方再用 ReadEntry 取得独立字节副本。 */
ZR_LIBRARY_API const SZrLibrary_ZrmEntryInfo *ZrLibrary_Zrm_FindResource(const SZrLibrary_ZrmArchive *archive,
                                                                         const TZrChar *logicalName);

/** @brief 查找 CompileTool 模块的可执行源码条目；仅对相应 provider phase 有意义。 */
ZR_LIBRARY_API const SZrLibrary_ZrmEntryInfo *
ZrLibrary_Zrm_FindCompileToolExecutable(
        const SZrLibrary_ZrmArchive *archive,
        const TZrChar *moduleKey);

/** @brief 从已打开归档提取条目副本，通常传入 Find* 返回的 entryName。
 * 输出指针和长度在调用开始时归零；成功获得的字节由 FreeBytes 释放，不能用 free。
 */
ZR_LIBRARY_API TZrBool ZrLibrary_Zrm_ReadEntry(const SZrLibrary_ZrmArchive *archive,
                                               const TZrChar *entryName,
                                               TZrByte **outBytes,
                                               TZrSize *outByteCount,
                                               TZrChar *errorBuffer,
                                               TZrSize errorBufferSize);

/** @brief 释放 ReadEntry 返回的 miniz 字节副本；允许传入空指针。 */
ZR_LIBRARY_API void ZrLibrary_Zrm_FreeBytes(TZrByte *bytes);

#endif // ZR_VM_LIBRARY_ZRM_H
