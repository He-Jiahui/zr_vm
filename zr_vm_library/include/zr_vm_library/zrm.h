#ifndef ZR_VM_LIBRARY_ZRM_H
#define ZR_VM_LIBRARY_ZRM_H

#include "zr_vm_library/conf.h"

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

typedef enum EZrLibrary_ZrmCompression {
    ZR_LIBRARY_ZRM_COMPRESSION_STORE = 0,
    ZR_LIBRARY_ZRM_COMPRESSION_DEFLATE = 1
} EZrLibrary_ZrmCompression;

typedef enum EZrLibrary_ProviderPhase {
    ZR_LIBRARY_PROVIDER_PHASE_RUNTIME = 0,
    ZR_LIBRARY_PROVIDER_PHASE_TEST = 1,
    ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL = 2
} EZrLibrary_ProviderPhase;

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

typedef struct SZrLibrary_ZrmPackModule {
    const TZrChar *moduleKey;
    const TZrChar *sourcePath;
    const TZrChar *hash;
    const TZrChar *compileToolExecutableSourcePath;
    const TZrChar *compileToolExecutableHash;
} SZrLibrary_ZrmPackModule;

typedef struct SZrLibrary_ZrmPackResource {
    const TZrChar *logicalName;
    const TZrChar *sourcePath;
    const TZrChar *hash;
    TZrBool compress;
} SZrLibrary_ZrmPackResource;

typedef struct SZrLibrary_ZrmPackRequest {
    const TZrChar *outputPath;
    SZrLibrary_ZrmAssemblyInfo assembly;
    const SZrLibrary_ZrmPackModule *modules;
    TZrSize moduleCount;
    const SZrLibrary_ZrmPackResource *resources;
    TZrSize resourceCount;
} SZrLibrary_ZrmPackRequest;

typedef struct SZrLibrary_ZrmEntryInfo {
    TZrChar logicalName[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar entryName[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar hash[64];
    TZrUInt64 uncompressedSize;
    TZrUInt64 compressedSize;
    TZrUInt32 crc32;
    EZrLibrary_ZrmCompression compression;
} SZrLibrary_ZrmEntryInfo;

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

ZR_LIBRARY_API TZrBool ZrLibrary_Zrm_ValidateLogicalName(const TZrChar *name);

ZR_LIBRARY_API TZrBool ZrLibrary_Zrm_BuildModuleEntryName(const TZrChar *moduleKey,
                                                          TZrChar *buffer,
                                                          TZrSize bufferSize);

ZR_LIBRARY_API TZrBool ZrLibrary_Zrm_BuildResourceEntryName(const TZrChar *logicalName,
                                                            TZrChar *buffer,
                                                            TZrSize bufferSize);

ZR_LIBRARY_API TZrBool ZrLibrary_Zrm_BuildCompileToolExecutableEntryName(
        const TZrChar *moduleKey,
        TZrChar *buffer,
        TZrSize bufferSize);

ZR_LIBRARY_API TZrBool ZrLibrary_Zrm_WriteArchive(const SZrLibrary_ZrmPackRequest *request,
                                                  TZrChar *errorBuffer,
                                                  TZrSize errorBufferSize);

ZR_LIBRARY_API TZrBool ZrLibrary_Zrm_Open(const TZrChar *path,
                                          SZrLibrary_ZrmArchive *archive,
                                          TZrChar *errorBuffer,
                                          TZrSize errorBufferSize);

// bytes are borrowed and must remain immutable until ZrLibrary_Zrm_Close.
ZR_LIBRARY_API TZrBool ZrLibrary_Zrm_OpenBytes(const TZrByte *bytes,
                                               TZrSize byteCount,
                                               const TZrChar *sourceName,
                                               SZrLibrary_ZrmArchive *archive,
                                               TZrChar *errorBuffer,
                                               TZrSize errorBufferSize);

ZR_LIBRARY_API void ZrLibrary_Zrm_Close(SZrLibrary_ZrmArchive *archive);

ZR_LIBRARY_API const SZrLibrary_ZrmEntryInfo *ZrLibrary_Zrm_FindModule(const SZrLibrary_ZrmArchive *archive,
                                                                       const TZrChar *moduleKey);

ZR_LIBRARY_API const SZrLibrary_ZrmEntryInfo *ZrLibrary_Zrm_FindResource(const SZrLibrary_ZrmArchive *archive,
                                                                         const TZrChar *logicalName);

ZR_LIBRARY_API const SZrLibrary_ZrmEntryInfo *
ZrLibrary_Zrm_FindCompileToolExecutable(
        const SZrLibrary_ZrmArchive *archive,
        const TZrChar *moduleKey);

ZR_LIBRARY_API TZrBool ZrLibrary_Zrm_ReadEntry(const SZrLibrary_ZrmArchive *archive,
                                               const TZrChar *entryName,
                                               TZrByte **outBytes,
                                               TZrSize *outByteCount,
                                               TZrChar *errorBuffer,
                                               TZrSize errorBufferSize);

ZR_LIBRARY_API void ZrLibrary_Zrm_FreeBytes(TZrByte *bytes);

#endif // ZR_VM_LIBRARY_ZRM_H
