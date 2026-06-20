//
// Created by HeJiahui on 2025/7/27.
//

#ifndef ZR_VM_LIBRARY_PROJECT_H
#define ZR_VM_LIBRARY_PROJECT_H

#include "zr_vm_library/conf.h"
#include "zr_vm_library/zrm.h"


#define ZR_LIBRARY_BINARY_FILE_EXT ZR_VM_BINARY_MODULE_FILE_EXTENSION
#define ZR_LIBRARY_PROJECT_SIGNATURE 0x5A525F50524F4A54ULL

typedef struct SZrLibrary_ProjectPathAlias {
    SZrString *alias;
    SZrString *modulePrefix;
} SZrLibrary_ProjectPathAlias;

typedef struct SZrLibrary_ProjectDependencyReference {
    SZrString *name;
    SZrString *assemblyName;
    TZrSize packageIndex;
    SZrString *minVersionInclusive;
    SZrString *maxVersionExclusive;
    TZrBool useAliasForModuleKey;
} SZrLibrary_ProjectDependencyReference;

typedef enum EZrLibrary_ProjectDependencyPackageArtifactKind {
    ZR_LIBRARY_PROJECT_DEPENDENCY_PACKAGE_PROJECT = 0,
    ZR_LIBRARY_PROJECT_DEPENDENCY_PACKAGE_ZRM = 1
} EZrLibrary_ProjectDependencyPackageArtifactKind;

typedef struct SZrLibrary_ProjectDependencyPackage {
    EZrLibrary_ProjectDependencyPackageArtifactKind artifactKind;
    SZrString *name;
    SZrString *assemblyName;
    SZrString *version;
    SZrString *file;
    SZrString *directory;
    SZrString *culture;
    SZrString *publicKeyToken;
    SZrString *kind;
    SZrString *source;
    SZrString *binary;
    SZrString *entry;
    SZrLibrary_ProjectPathAlias *pathAliases;
    TZrSize pathAliasCount;
    SZrLibrary_ProjectDependencyReference *dependencyRefs;
    TZrSize dependencyRefCount;
    TZrSize dependencyRefCapacity;
    SZrLibrary_ZrmArchive zrmArchive;
    TZrBool zrmArchiveOpen;
} SZrLibrary_ProjectDependencyPackage;

typedef struct SZrLibrary_ProjectResource {
    SZrString *logicalName;
    SZrString *sourcePath;
    TZrBool compress;
} SZrLibrary_ProjectResource;

struct ZR_STRUCT_ALIGN SZrLibrary_Project {
    TZrUInt64 signature;
    SZrString *file;
    SZrString *directory;
    SZrString *name;
    SZrString *assemblyName;
    SZrString *version;
    SZrString *assemblyCulture;
    SZrString *assemblyPublicKeyToken;
    SZrString *assemblyKind;
    SZrString *assemblyOutput;
    SZrString *description;
    SZrString *author;
    SZrString *email;
    SZrString *url;
    SZrString *license;
    SZrString *copyright;
    SZrString *binary;
    SZrString *source;
    SZrString *entry;
    SZrString *dependency;
    SZrString *local;
    TZrPtr aotRuntime;
    SZrLibrary_ProjectPathAlias *pathAliases;
    TZrSize pathAliasCount;
    SZrLibrary_ProjectResource *resources;
    TZrSize resourceCount;
    TZrSize resourceCapacity;
    SZrLibrary_ProjectDependencyPackage *dependencyPackages;
    TZrSize dependencyPackageCount;
    TZrSize dependencyPackageCapacity;
    SZrLibrary_ProjectDependencyReference *dependencyRefs;
    TZrSize dependencyRefCount;
    TZrSize dependencyRefCapacity;
    TZrBool supportMultithread;
    TZrBool autoCoroutine;
};

typedef struct SZrLibrary_Project SZrLibrary_Project;
ZR_LIBRARY_API SZrLibrary_Project *ZrLibrary_Project_New(SZrState *state, TZrNativeString raw, TZrNativeString file);

ZR_LIBRARY_API void ZrLibrary_Project_Free(SZrState *state, SZrLibrary_Project *project);

ZR_LIBRARY_API const SZrLibrary_Project *ZrLibrary_Project_GetFromGlobal(const SZrGlobalState *global);

ZR_LIBRARY_API TZrBool ZrLibrary_Project_NormalizeModuleKey(const TZrChar *modulePath,
                                                            TZrChar *buffer,
                                                            TZrSize bufferSize);

ZR_LIBRARY_API TZrBool ZrLibrary_Project_DeriveCurrentModuleKey(const SZrLibrary_Project *project,
                                                                const TZrChar *sourceName,
                                                                const TZrChar *explicitModuleKey,
                                                                TZrChar *buffer,
                                                                TZrSize bufferSize,
                                                                TZrChar *errorBuffer,
                                                                TZrSize errorBufferSize);

ZR_LIBRARY_API TZrBool ZrLibrary_Project_ResolveImportModuleKey(const SZrLibrary_Project *project,
                                                                const TZrChar *currentModuleKey,
                                                                const TZrChar *rawSpecifier,
                                                                TZrChar *buffer,
                                                                TZrSize bufferSize,
                                                                TZrChar *errorBuffer,
                                                                TZrSize errorBufferSize);

ZR_LIBRARY_API TZrBool ZrLibrary_Project_GetDependencyImportVersionRange(
        const SZrLibrary_Project *project,
        const TZrChar *currentModuleKey,
        const TZrChar *resolvedModuleKey,
        SZrString **outAssemblyName,
        SZrString **outRequestedVersion,
        SZrString **outMinVersionInclusive,
        SZrString **outMaxVersionExclusive);

ZR_LIBRARY_API TZrBool ZrLibrary_Project_ResolveSourcePath(const SZrLibrary_Project *project,
                                                           const TZrChar *moduleName,
                                                           TZrChar *buffer,
                                                           TZrSize bufferSize);

ZR_LIBRARY_API TZrBool ZrLibrary_Project_ResolveBinaryPath(const SZrLibrary_Project *project,
                                                           const TZrChar *moduleName,
                                                           TZrChar *buffer,
                                                           TZrSize bufferSize);

ZR_LIBRARY_API TZrBool ZrLibrary_Project_ResolveIntermediatePath(const SZrLibrary_Project *project,
                                                                 const TZrChar *moduleName,
                                                                 TZrChar *buffer,
                                                                 TZrSize bufferSize);

ZR_LIBRARY_API TZrBool ZrLibrary_Project_ResolveAssemblyOutputPath(const SZrLibrary_Project *project,
                                                                   TZrChar *buffer,
                                                                   TZrSize bufferSize);

ZR_LIBRARY_API TZrBool ZrLibrary_Project_ResolveZrmModuleEntry(
        const SZrLibrary_Project *project,
        const TZrChar *moduleName,
        const SZrLibrary_ZrmArchive **outArchive,
        const SZrLibrary_ZrmEntryInfo **outEntry);

ZR_LIBRARY_API EZrThreadStatus ZrLibrary_Project_Run(SZrState *state, SZrTypeValue *result);

ZR_LIBRARY_API void ZrLibrary_Project_Do(SZrState *state);

ZR_LIBRARY_API TZrBool ZrLibrary_Project_SourceLoadImplementation(SZrState *state, TZrNativeString path, TZrNativeString md5,
                                                                SZrIo *io);

#endif // ZR_VM_LIBRARY_PROJECT_H
