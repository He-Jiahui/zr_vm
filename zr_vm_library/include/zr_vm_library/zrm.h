#ifndef ZR_VM_LIBRARY_ZRM_H
#define ZR_VM_LIBRARY_ZRM_H

#include "zr_vm_library/conf.h"

#define ZR_LIBRARY_ZRM_FILE_EXTENSION ".zrm"
#define ZR_LIBRARY_ZRM_FORMAT "zr.zrm/v1"
#define ZR_LIBRARY_ZRM_MANIFEST_ENTRY "META-INF/zrm.json"
#define ZR_LIBRARY_ZRM_MODULE_ENTRY_PREFIX "modules/"
#define ZR_LIBRARY_ZRM_RESOURCE_ENTRY_PREFIX "resources/"
#define ZR_LIBRARY_ZRM_ERROR_BUFFER_LENGTH 512U

typedef enum EZrLibrary_ZrmCompression {
    ZR_LIBRARY_ZRM_COMPRESSION_STORE = 0,
    ZR_LIBRARY_ZRM_COMPRESSION_DEFLATE = 1
} EZrLibrary_ZrmCompression;

typedef struct SZrLibrary_ZrmAssemblyInfo {
    const TZrChar *name;
    const TZrChar *version;
    const TZrChar *culture;
    const TZrChar *publicKeyToken;
    const TZrChar *kind;
    const TZrChar *entryModule;
} SZrLibrary_ZrmAssemblyInfo;

typedef struct SZrLibrary_ZrmPackModule {
    const TZrChar *moduleKey;
    const TZrChar *sourcePath;
    const TZrChar *hash;
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
    SZrLibrary_ZrmEntryInfo *modules;
    TZrSize moduleCount;
    SZrLibrary_ZrmEntryInfo *resources;
    TZrSize resourceCount;
    TZrPtr zipHandle;
} SZrLibrary_ZrmArchive;

ZR_LIBRARY_API TZrBool ZrLibrary_Zrm_ValidateLogicalName(const TZrChar *name);

ZR_LIBRARY_API TZrBool ZrLibrary_Zrm_BuildModuleEntryName(const TZrChar *moduleKey,
                                                          TZrChar *buffer,
                                                          TZrSize bufferSize);

ZR_LIBRARY_API TZrBool ZrLibrary_Zrm_BuildResourceEntryName(const TZrChar *logicalName,
                                                            TZrChar *buffer,
                                                            TZrSize bufferSize);

ZR_LIBRARY_API TZrBool ZrLibrary_Zrm_WriteArchive(const SZrLibrary_ZrmPackRequest *request,
                                                  TZrChar *errorBuffer,
                                                  TZrSize errorBufferSize);

ZR_LIBRARY_API TZrBool ZrLibrary_Zrm_Open(const TZrChar *path,
                                          SZrLibrary_ZrmArchive *archive,
                                          TZrChar *errorBuffer,
                                          TZrSize errorBufferSize);

ZR_LIBRARY_API void ZrLibrary_Zrm_Close(SZrLibrary_ZrmArchive *archive);

ZR_LIBRARY_API const SZrLibrary_ZrmEntryInfo *ZrLibrary_Zrm_FindModule(const SZrLibrary_ZrmArchive *archive,
                                                                       const TZrChar *moduleKey);

ZR_LIBRARY_API const SZrLibrary_ZrmEntryInfo *ZrLibrary_Zrm_FindResource(const SZrLibrary_ZrmArchive *archive,
                                                                         const TZrChar *logicalName);

ZR_LIBRARY_API TZrBool ZrLibrary_Zrm_ReadEntry(const SZrLibrary_ZrmArchive *archive,
                                               const TZrChar *entryName,
                                               TZrByte **outBytes,
                                               TZrSize *outByteCount,
                                               TZrChar *errorBuffer,
                                               TZrSize errorBufferSize);

ZR_LIBRARY_API void ZrLibrary_Zrm_FreeBytes(TZrByte *bytes);

#endif // ZR_VM_LIBRARY_ZRM_H
