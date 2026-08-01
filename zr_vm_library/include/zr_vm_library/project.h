//
// Created by HeJiahui on 2025/7/27.
//

#ifndef ZR_VM_LIBRARY_PROJECT_H
#define ZR_VM_LIBRARY_PROJECT_H

#include "zr_vm_common/zr_aot_abi.h"
#include "zr_vm_library/conf.h"
#include "zr_vm_library/zrm.h"


#define ZR_LIBRARY_BINARY_FILE_EXT ZR_VM_BINARY_MODULE_FILE_EXTENSION
#define ZR_LIBRARY_PROJECT_SIGNATURE 0x5A525F50524F4A54ULL

typedef enum EZrLibrary_ModuleDomain {
    ZR_LIBRARY_MODULE_DOMAIN_INVALID = 0,
    ZR_LIBRARY_MODULE_DOMAIN_OFFICIAL_NATIVE = 1,
    ZR_LIBRARY_MODULE_DOMAIN_REGISTERED_NATIVE = 2,
    ZR_LIBRARY_MODULE_DOMAIN_WORKSPACE = 3,
    ZR_LIBRARY_MODULE_DOMAIN_PACKAGE = 4
} EZrLibrary_ModuleDomain;

typedef enum EZrLibrary_ModuleSpecifierKind {
    ZR_LIBRARY_MODULE_SPECIFIER_KIND_INVALID = 0,
    ZR_LIBRARY_MODULE_SPECIFIER_KIND_OFFICIAL_NATIVE = 1,
    ZR_LIBRARY_MODULE_SPECIFIER_KIND_REGISTERED_NATIVE = 2,
    ZR_LIBRARY_MODULE_SPECIFIER_KIND_WORKSPACE = 3,
    ZR_LIBRARY_MODULE_SPECIFIER_KIND_RELATIVE = 4,
    ZR_LIBRARY_MODULE_SPECIFIER_KIND_ALIAS = 5,
    ZR_LIBRARY_MODULE_SPECIFIER_KIND_PACKAGE = 6,
    ZR_LIBRARY_MODULE_SPECIFIER_KIND_FILE = 7
} EZrLibrary_ModuleSpecifierKind;

typedef struct SZrLibrary_ModuleIdentity {
    EZrLibrary_ModuleDomain domain;
    TZrChar segments[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar packageName[ZR_LIBRARY_MAX_PATH_LENGTH];
} SZrLibrary_ModuleIdentity;

typedef struct SZrLibrary_ModuleSpecifier {
    EZrLibrary_ModuleSpecifierKind kind;
    SZrLibrary_ModuleIdentity identity;
    TZrChar aliasRoot[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar locator[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrSize relativeParentLevels;
} SZrLibrary_ModuleSpecifier;

typedef struct SZrLibrary_ProjectPathAlias {
    SZrString *alias;
    SZrString *modulePrefix;
} SZrLibrary_ProjectPathAlias;

typedef struct SZrLibrary_ProjectManifestAlias {
    SZrString *root;
    SZrLibrary_ModuleSpecifier target;
} SZrLibrary_ProjectManifestAlias;

typedef struct SZrLibrary_ProjectPackageExport {
    SZrString *key;
    SZrLibrary_ModuleSpecifier target;
} SZrLibrary_ProjectPackageExport;

typedef enum EZrLibrary_ProjectManifestDependencySourceKind {
    ZR_LIBRARY_PROJECT_MANIFEST_DEPENDENCY_SOURCE_PATH = 1,
    ZR_LIBRARY_PROJECT_MANIFEST_DEPENDENCY_SOURCE_REGISTRY = 2,
    ZR_LIBRARY_PROJECT_MANIFEST_DEPENDENCY_SOURCE_GIT = 3
} EZrLibrary_ProjectManifestDependencySourceKind;

typedef struct SZrLibrary_ProjectManifestDependency {
    SZrLibrary_ModuleIdentity packageIdentity;
    SZrString *versionRequirement;
    EZrLibrary_ProjectManifestDependencySourceKind sourceKind;
    SZrString *source;
} SZrLibrary_ProjectManifestDependency;

typedef struct SZrLibrary_ProjectManifestDependencyLockEntry {
    SZrLibrary_ModuleIdentity packageIdentity;
    const TZrChar *resolvedVersion;
    const TZrChar *contentHash;
    const TZrChar *transitiveIdentity;
    EZrLibrary_ProjectManifestDependencySourceKind providerSourceKind;
    EZrLibrary_ProviderPhase providerPhase;
} SZrLibrary_ProjectManifestDependencyLockEntry;

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

typedef struct SZrLibrary_ProjectImportProviderLocation {
    EZrLibrary_ProjectDependencyPackageArtifactKind artifactKind;
    EZrLibrary_ProviderPhase providerPhase;
    SZrString *assemblyName;
    SZrString *requestedVersion;
    SZrString *minVersionInclusive;
    SZrString *maxVersionExclusive;
    const SZrLibrary_ZrmArchive *archive;
    const SZrLibrary_ZrmEntryInfo *entry;
    TZrChar artifactEntry[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar publicContractHash[128];
    TZrChar sourcePath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar binaryPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar intermediatePath[ZR_LIBRARY_MAX_PATH_LENGTH];
} SZrLibrary_ProjectImportProviderLocation;

typedef struct SZrLibrary_ProjectImportProviderAotLoadRequest {
    EZrAotBackendKind backendKind;
    EZrLibrary_ProjectDependencyPackageArtifactKind artifactKind;
    EZrLibrary_ProviderPhase providerPhase;
    SZrString *assemblyName;
    SZrString *requestedVersion;
    SZrString *minVersionInclusive;
    SZrString *maxVersionExclusive;
    const SZrLibrary_ZrmArchive *archive;
    const SZrLibrary_ZrmEntryInfo *entry;
    TZrChar artifactEntry[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar publicContractHash[128];
    TZrChar resolvedModuleKey[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar descriptorModuleName[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar sourcePath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar binaryPath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar intermediatePath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar libraryPath[ZR_LIBRARY_MAX_PATH_LENGTH];
} SZrLibrary_ProjectImportProviderAotLoadRequest;

typedef struct SZrLibrary_ProjectResource {
    SZrString *logicalName;
    SZrString *sourcePath;
    TZrBool compress;
} SZrLibrary_ProjectResource;

typedef struct SZrLibrary_ProjectFeatureSwitch {
    SZrString *name;
    TZrBool value;
} SZrLibrary_ProjectFeatureSwitch;

typedef enum EZrLibrary_ProjectAotMode {
    ZR_LIBRARY_PROJECT_AOT_MODE_HYBRID = 0,
    ZR_LIBRARY_PROJECT_AOT_MODE_FULL_AOT = 1
} EZrLibrary_ProjectAotMode;

typedef enum EZrLibrary_ProjectPreserveRuleKind {
    ZR_LIBRARY_PROJECT_PRESERVE_RULE_TYPE = 1,
    ZR_LIBRARY_PROJECT_PRESERVE_RULE_METHOD = 2,
    ZR_LIBRARY_PROJECT_PRESERVE_RULE_GENERIC = 3
} EZrLibrary_ProjectPreserveRuleKind;

typedef enum EZrLibrary_ProjectPreserveMembers {
    ZR_LIBRARY_PROJECT_PRESERVE_MEMBERS_DEFAULT = 0,
    ZR_LIBRARY_PROJECT_PRESERVE_MEMBERS_ALL = 1,
    ZR_LIBRARY_PROJECT_PRESERVE_MEMBERS_METHODS = 2
} EZrLibrary_ProjectPreserveMembers;

typedef struct SZrLibrary_ProjectPreserveRule {
    EZrLibrary_ProjectPreserveRuleKind kind;
    SZrString *target;
    EZrLibrary_ProjectPreserveMembers members;
    SZrString **genericArguments;
    TZrSize genericArgumentCount;
    TZrSize genericArgumentCapacity;
    SZrString *feature;
    TZrBool hasFeatureValue;
    TZrBool featureValue;
} SZrLibrary_ProjectPreserveRule;

typedef enum EZrLibrary_ProjectExportDeclarationKind {
    ZR_LIBRARY_PROJECT_EXPORT_DECLARATION_TYPE = 1,
    ZR_LIBRARY_PROJECT_EXPORT_DECLARATION_METHOD = 2,
    ZR_LIBRARY_PROJECT_EXPORT_DECLARATION_FIELD = 3
} EZrLibrary_ProjectExportDeclarationKind;

typedef struct SZrLibrary_ProjectExportDeclaration {
    EZrLibrary_ProjectExportDeclarationKind kind;
    SZrString *target;
} SZrLibrary_ProjectExportDeclaration;

struct ZR_STRUCT_ALIGN SZrLibrary_Project {
    TZrUInt64 signature;
    TZrUInt32 manifestVersion;
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
    EZrLibrary_ProjectAotMode aotMode;
    SZrLibrary_ProjectPathAlias *pathAliases;
    TZrSize pathAliasCount;
    SZrLibrary_ProjectManifestAlias *manifestAliases;
    TZrSize manifestAliasCount;
    TZrSize manifestAliasCapacity;
    SZrLibrary_ModuleIdentity packageIdentity;
    SZrLibrary_ProjectPackageExport *packageExports;
    TZrSize packageExportCount;
    TZrSize packageExportCapacity;
    SZrLibrary_ProjectManifestDependency *manifestDependencies;
    TZrSize manifestDependencyCount;
    TZrSize manifestDependencyCapacity;
    SZrLibrary_ProjectManifestDependency *manifestBuildDependencies;
    TZrSize manifestBuildDependencyCount;
    TZrSize manifestBuildDependencyCapacity;
    SZrLibrary_ProjectResource *resources;
    TZrSize resourceCount;
    TZrSize resourceCapacity;
    SZrLibrary_ProjectFeatureSwitch *featureSwitches;
    TZrSize featureSwitchCount;
    TZrSize featureSwitchCapacity;
    SZrLibrary_ProjectPreserveRule *preserveRules;
    TZrSize preserveRuleCount;
    TZrSize preserveRuleCapacity;
    SZrLibrary_ProjectExportDeclaration *exportDeclarations;
    TZrSize exportDeclarationCount;
    TZrSize exportDeclarationCapacity;
    SZrLibrary_ProjectDependencyPackage *dependencyPackages;
    TZrSize dependencyPackageCount;
    TZrSize dependencyPackageCapacity;
    SZrLibrary_ProjectDependencyReference *dependencyRefs;
    TZrSize dependencyRefCount;
    TZrSize dependencyRefCapacity;
    TZrBool supportMultithread;
};

typedef struct SZrLibrary_Project SZrLibrary_Project;
ZR_LIBRARY_API SZrLibrary_Project *ZrLibrary_Project_New(SZrState *state, TZrNativeString raw, TZrNativeString file);

ZR_LIBRARY_API void ZrLibrary_Project_Free(SZrState *state, SZrLibrary_Project *project);

ZR_LIBRARY_API const SZrLibrary_Project *ZrLibrary_Project_GetFromGlobal(const SZrGlobalState *global);

ZR_LIBRARY_API TZrBool ZrLibrary_ModuleSpecifier_Parse(const TZrChar *literal,
                                                        SZrLibrary_ModuleSpecifier *outSpecifier,
                                                        TZrChar *errorBuffer,
                                                        TZrSize errorBufferSize);

ZR_LIBRARY_API TZrBool ZrLibrary_ModuleIdentity_Equals(const SZrLibrary_ModuleIdentity *lhs,
                                                        const SZrLibrary_ModuleIdentity *rhs);

ZR_LIBRARY_API TZrBool ZrLibrary_ModuleSpecifier_ResolveRelative(
        const SZrLibrary_ModuleIdentity *currentIdentity,
        const SZrLibrary_ModuleSpecifier *relativeSpecifier,
        SZrLibrary_ModuleIdentity *outIdentity,
        TZrChar *errorBuffer,
        TZrSize errorBufferSize);

ZR_LIBRARY_API TZrBool ZrLibrary_Project_ResolveManifestAlias(
        const SZrLibrary_Project *project,
        const SZrLibrary_ModuleSpecifier *aliasSpecifier,
        SZrLibrary_ModuleSpecifier *outTargetSpecifier);

ZR_LIBRARY_API TZrBool ZrLibrary_Project_ResolvePackageExport(
        const SZrLibrary_Project *project,
        const SZrLibrary_ModuleSpecifier *packageSpecifier,
        SZrLibrary_ModuleSpecifier *outTargetSpecifier);

ZR_LIBRARY_API TZrBool ZrLibrary_ProjectManifestV2_Write(const SZrLibrary_Project *project,
                                                          TZrChar *outManifest,
                                                          TZrSize outManifestSize);

ZR_LIBRARY_API TZrBool ZrLibrary_ProjectManifestV2_WriteDependencyLock(
        const SZrLibrary_Project *project,
        const SZrLibrary_ProjectManifestDependencyLockEntry *entries,
        TZrSize entryCount,
        TZrChar *outLock,
        TZrSize outLockSize);

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

ZR_LIBRARY_API TZrBool ZrLibrary_Project_ResolveImportProviderLocation(
        const SZrLibrary_Project *project,
        const TZrChar *currentModuleKey,
        const TZrChar *rawSpecifier,
        TZrChar *resolvedModuleKey,
        TZrSize resolvedModuleKeySize,
        SZrLibrary_ProjectImportProviderLocation *outLocation,
        TZrChar *errorBuffer,
        TZrSize errorBufferSize);

ZR_LIBRARY_API TZrBool ZrLibrary_Project_ResolveImportProviderAotLoadRequest(
        const SZrLibrary_Project *project,
        const TZrChar *currentModuleKey,
        const TZrChar *rawSpecifier,
        EZrAotBackendKind backendKind,
        SZrLibrary_ProjectImportProviderAotLoadRequest *outRequest,
        TZrChar *errorBuffer,
        TZrSize errorBufferSize);

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
