//
// Created by HeJiahui on 2025/7/27.
//

#ifndef ZR_VM_LIBRARY_PROJECT_H
#define ZR_VM_LIBRARY_PROJECT_H

#include "zr_vm_common/zr_aot_abi.h"
#include "zr_vm_library/conf.h"
#include "zr_vm_library/zrm.h"


#define ZR_LIBRARY_BINARY_FILE_EXT ZR_VM_BINARY_MODULE_FILE_EXTENSION
/* 解析器和运行器借此辨认 global 上的 project 指针，避免把其他 userData 当项目。 */
#define ZR_LIBRARY_PROJECT_SIGNATURE 0x5A525F50524F4A54ULL

/** @brief 已规范化的模块归属域；相同文本在不同域中仍是不同模块身份。 */
typedef enum EZrLibrary_ModuleDomain {
    ZR_LIBRARY_MODULE_DOMAIN_INVALID = 0,
    ZR_LIBRARY_MODULE_DOMAIN_OFFICIAL_NATIVE = 1,
    ZR_LIBRARY_MODULE_DOMAIN_REGISTERED_NATIVE = 2,
    ZR_LIBRARY_MODULE_DOMAIN_WORKSPACE = 3,
    ZR_LIBRARY_MODULE_DOMAIN_PACKAGE = 4
} EZrLibrary_ModuleDomain;

/** @brief import 字面量的语法类别；解析后才映射到模块身份或文件定位符。 */
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

/** @brief 解析器、依赖解析器和 AOT 共享的规范模块身份，保留域与包根以防同名冲突。 */
typedef struct SZrLibrary_ModuleIdentity {
    EZrLibrary_ModuleDomain domain;
    TZrChar segments[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar packageName[ZR_LIBRARY_MAX_PATH_LENGTH];
} SZrLibrary_ModuleIdentity;

/** @brief import 请求的中间表示；相对层级和 alias/file 定位符在解析前不能当规范键使用。 */
typedef struct SZrLibrary_ModuleSpecifier {
    EZrLibrary_ModuleSpecifierKind kind;
    SZrLibrary_ModuleIdentity identity;
    TZrChar aliasRoot[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar locator[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrSize relativeParentLevels;
} SZrLibrary_ModuleSpecifier;

/** @brief 旧版项目路径别名，供 import 解析时将简写前缀映射到模块前缀。 */
typedef struct SZrLibrary_ProjectPathAlias {
    SZrString *alias;
    SZrString *modulePrefix;
} SZrLibrary_ProjectPathAlias;

/** @brief v2 manifest 的别名映射；target 保留已解析的目标类别以限制后续重写。 */
typedef struct SZrLibrary_ProjectManifestAlias {
    SZrString *root;
    SZrLibrary_ModuleSpecifier target;
} SZrLibrary_ProjectManifestAlias;

/** @brief 包的显式导出表；ResolvePackageExport 只对已声明的键返回目标。 */
typedef struct SZrLibrary_ProjectPackageExport {
    SZrString *key;
    SZrLibrary_ModuleSpecifier target;
} SZrLibrary_ProjectPackageExport;

/** @brief v2 依赖来源的分类，用于锁文件验证与发布可移植性判断。 */
typedef enum EZrLibrary_ProjectManifestDependencySourceKind {
    ZR_LIBRARY_PROJECT_MANIFEST_DEPENDENCY_SOURCE_PATH = 1,
    ZR_LIBRARY_PROJECT_MANIFEST_DEPENDENCY_SOURCE_REGISTRY = 2,
    ZR_LIBRARY_PROJECT_MANIFEST_DEPENDENCY_SOURCE_GIT = 3
} EZrLibrary_ProjectManifestDependencySourceKind;

/** @brief manifest 中尚未锁定的依赖声明；版本约束与来源须在锁文件匹配时一起核对。 */
typedef struct SZrLibrary_ProjectManifestDependency {
    SZrLibrary_ModuleIdentity packageIdentity;
    SZrString *versionRequirement;
    EZrLibrary_ProjectManifestDependencySourceKind sourceKind;
    SZrString *source;
} SZrLibrary_ProjectManifestDependency;

/** @brief 锁文件中已解析依赖的视图；写入时借用调用者字符串，读入后由 project 持有其存储。 */
typedef struct SZrLibrary_ProjectManifestDependencyLockEntry {
    SZrLibrary_ModuleIdentity packageIdentity;
    const TZrChar *resolvedVersion;
    const TZrChar *contentHash;
    const TZrChar *transitiveIdentity;
    EZrLibrary_ProjectManifestDependencySourceKind providerSourceKind;
    EZrLibrary_ProviderPhase providerPhase;
} SZrLibrary_ProjectManifestDependencyLockEntry;

/** @brief 旧版依赖引用的名字、程序集与版本约束；packageIndex 指向同一项目的 package 数组。 */
typedef struct SZrLibrary_ProjectDependencyReference {
    SZrString *name;
    SZrString *assemblyName;
    TZrSize packageIndex;
    SZrString *minVersionInclusive;
    SZrString *maxVersionExclusive;
    TZrBool useAliasForModuleKey;
} SZrLibrary_ProjectDependencyReference;

/** @brief 依赖交付形式决定模块从项目文件还是 ZRM 归档加载。 */
typedef enum EZrLibrary_ProjectDependencyPackageArtifactKind {
    ZR_LIBRARY_PROJECT_DEPENDENCY_PACKAGE_PROJECT = 0,
    ZR_LIBRARY_PROJECT_DEPENDENCY_PACKAGE_ZRM = 1
} EZrLibrary_ProjectDependencyPackageArtifactKind;

/** @brief 已解析依赖包；原始数组和已打开的 ZRM 由 Project_Free 统一回收。 */
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

/** @brief import provider 定位结果；归档、条目和 SZrString 指针均借用 project，释放项目后失效。 */
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

/** @brief AOT 加载请求把 provider 位置与后端库路径绑定，供 AOT runtime 构造加载描述符。 */
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

/** @brief 项目资源的逻辑名与源路径，供后续资源打包阶段使用。 */
typedef struct SZrLibrary_ProjectResource {
    SZrString *logicalName;
    SZrString *sourcePath;
    TZrBool compress;
} SZrLibrary_ProjectResource;

/** @brief manifest feature 布尔值，供属性绑定、编译期求值和 AOT 共享。 */
typedef struct SZrLibrary_ProjectFeatureSwitch {
    SZrString *name;
    TZrBool value;
} SZrLibrary_ProjectFeatureSwitch;

/** @brief CLI 编译的 AOT 策略；缺省 hybrid，full-aot 要求完整静态覆盖。 */
typedef enum EZrLibrary_ProjectAotMode {
    ZR_LIBRARY_PROJECT_AOT_MODE_HYBRID = 0,
    ZR_LIBRARY_PROJECT_AOT_MODE_FULL_AOT = 1
} EZrLibrary_ProjectAotMode;

/** @brief AOT 保留根的目标类别。 */
typedef enum EZrLibrary_ProjectPreserveRuleKind {
    ZR_LIBRARY_PROJECT_PRESERVE_RULE_TYPE = 1,
    ZR_LIBRARY_PROJECT_PRESERVE_RULE_METHOD = 2,
    ZR_LIBRARY_PROJECT_PRESERVE_RULE_GENERIC = 3
} EZrLibrary_ProjectPreserveRuleKind;

/** @brief 类型保留根的成员扩展策略。 */
typedef enum EZrLibrary_ProjectPreserveMembers {
    ZR_LIBRARY_PROJECT_PRESERVE_MEMBERS_DEFAULT = 0,
    ZR_LIBRARY_PROJECT_PRESERVE_MEMBERS_ALL = 1,
    ZR_LIBRARY_PROJECT_PRESERVE_MEMBERS_METHODS = 2
} EZrLibrary_ProjectPreserveMembers;

/** @brief manifest 保留声明；genericArguments 和 feature 条件供 CLI AOT 根收集器消费。 */
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

/** @brief AOT 公共导出声明的目标类别。 */
typedef enum EZrLibrary_ProjectExportDeclarationKind {
    ZR_LIBRARY_PROJECT_EXPORT_DECLARATION_TYPE = 1,
    ZR_LIBRARY_PROJECT_EXPORT_DECLARATION_METHOD = 2,
    ZR_LIBRARY_PROJECT_EXPORT_DECLARATION_FIELD = 3
} EZrLibrary_ProjectExportDeclarationKind;

/** @brief CLI AOT 将该声明转换成产物公开导出清单。 */
typedef struct SZrLibrary_ProjectExportDeclaration {
    EZrLibrary_ProjectExportDeclarationKind kind;
    SZrString *target;
} SZrLibrary_ProjectExportDeclaration;

/** @brief 一个项目 manifest 的运行期视图；GC 字符串归 VM 管理，原始数组和归档由 Free 回收。 */
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
    SZrLibrary_ProjectManifestDependencyLockEntry *manifestDependencyLockEntries;
    TZrSize manifestDependencyLockEntryCount;
    TZrSize manifestDependencyLockStorageSize;
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
/** @brief 解析项目 manifest，并建立 CLI、解析器及运行器共用的项目状态。
 * @pre state、state->global、raw 与 file 为有效输入。
 * @return 失败时返回空指针；成功结果必须由 Project_Free 释放原始数组和归档。 */
ZR_LIBRARY_API SZrLibrary_Project *ZrLibrary_Project_New(SZrState *state, TZrNativeString raw, TZrNativeString file);

/** @brief 释放 New 建立的项目资源；调用后不得再使用项目中借出的归档和字符串视图。
 * @pre project 非空时 state 与 state->global 必须仍然有效。 */
ZR_LIBRARY_API void ZrLibrary_Project_Free(SZrState *state, SZrLibrary_Project *project);

/** @brief 从全局状态取当前有效项目；解析器和运行时在使用前据签名确认 userData 类型。 */
ZR_LIBRARY_API const SZrLibrary_Project *ZrLibrary_Project_GetFromGlobal(const SZrGlobalState *global);

/** @brief 将 import 字面量解析成显式类别与身份；失败时 errorBuffer 可接收诊断。 */
ZR_LIBRARY_API TZrBool ZrLibrary_ModuleSpecifier_Parse(const TZrChar *literal,
                                                        SZrLibrary_ModuleSpecifier *outSpecifier,
                                                        TZrChar *errorBuffer,
                                                        TZrSize errorBufferSize);

/** @brief 比较规范身份而非原始 import 字面量，避免路径分隔写法影响去重。 */
ZR_LIBRARY_API TZrBool ZrLibrary_ModuleIdentity_Equals(const SZrLibrary_ModuleIdentity *lhs,
                                                        const SZrLibrary_ModuleIdentity *rhs);

/** @brief 以当前模块身份为基准解析相对 import；越过其归属域根时失败。 */
ZR_LIBRARY_API TZrBool ZrLibrary_ModuleSpecifier_ResolveRelative(
        const SZrLibrary_ModuleIdentity *currentIdentity,
        const SZrLibrary_ModuleSpecifier *relativeSpecifier,
        SZrLibrary_ModuleIdentity *outIdentity,
        TZrChar *errorBuffer,
        TZrSize errorBufferSize);

/** @brief 将 v2 alias 请求替换为 manifest 声明的目标，同时保留请求后缀。 */
ZR_LIBRARY_API TZrBool ZrLibrary_Project_ResolveManifestAlias(
        const SZrLibrary_Project *project,
        const SZrLibrary_ModuleSpecifier *aliasSpecifier,
        SZrLibrary_ModuleSpecifier *outTargetSpecifier);

/** @brief 只把包中显式导出的键解析为目标；未导出键须保持不可访问。 */
ZR_LIBRARY_API TZrBool ZrLibrary_Project_ResolvePackageExport(
        const SZrLibrary_Project *project,
        const SZrLibrary_ModuleSpecifier *packageSpecifier,
        SZrLibrary_ModuleSpecifier *outTargetSpecifier);

/** @brief 将项目中可发布的 v2 声明写成规范 manifest 投影；调用者提供足够大的输出缓冲。 */
ZR_LIBRARY_API TZrBool ZrLibrary_ProjectManifestV2_Write(const SZrLibrary_Project *project,
                                                          TZrChar *outManifest,
                                                          TZrSize outManifestSize);

/** @brief 输出经解析的依赖锁，供可重复构建和依赖验证使用。 */
ZR_LIBRARY_API TZrBool ZrLibrary_ProjectManifestV2_WriteDependencyLock(
        const SZrLibrary_Project *project,
        const SZrLibrary_ProjectManifestDependencyLockEntry *entries,
        TZrSize entryCount,
        TZrChar *outLock,
        TZrSize outLockSize);

/** @brief 将锁文件绑定到已有项目声明；失败时不应将部分结果视为有效锁状态。 */
ZR_LIBRARY_API TZrBool ZrLibrary_ProjectManifestV2_ReadDependencyLock(
        SZrState *state,
        SZrLibrary_Project *project,
        const TZrChar *rawLock,
        TZrChar *errorBuffer,
        TZrSize errorBufferSize);

/** @brief 把模块路径收敛为统一的 import 键，供缓存、解析和文件定位共用。 */
ZR_LIBRARY_API TZrBool ZrLibrary_Project_NormalizeModuleKey(const TZrChar *modulePath,
                                                            TZrChar *buffer,
                                                            TZrSize bufferSize);

/** @brief 从显式键或源文件位置推导当前模块键，为相对 import 提供可信基点。 */
ZR_LIBRARY_API TZrBool ZrLibrary_Project_DeriveCurrentModuleKey(const SZrLibrary_Project *project,
                                                                const TZrChar *sourceName,
                                                                const TZrChar *explicitModuleKey,
                                                                TZrChar *buffer,
                                                                TZrSize bufferSize,
                                                                TZrChar *errorBuffer,
                                                                TZrSize errorBufferSize);

/** @brief 把 import 请求解析成唯一模块键，统一工作区、别名和依赖包的边界。 */
ZR_LIBRARY_API TZrBool ZrLibrary_Project_ResolveImportModuleKey(const SZrLibrary_Project *project,
                                                                const TZrChar *currentModuleKey,
                                                                const TZrChar *rawSpecifier,
                                                                TZrChar *buffer,
                                                                TZrSize bufferSize,
                                                                TZrChar *errorBuffer,
                                                                TZrSize errorBufferSize);

/** @brief 查询跨依赖 import 的程序集与版本限制，供编译和元数据装载校验。 */
ZR_LIBRARY_API TZrBool ZrLibrary_Project_GetDependencyImportVersionRange(
        const SZrLibrary_Project *project,
        const TZrChar *currentModuleKey,
        const TZrChar *resolvedModuleKey,
        SZrString **outAssemblyName,
        SZrString **outRequestedVersion,
        SZrString **outMinVersionInclusive,
        SZrString **outMaxVersionExclusive);

/** @brief 定位 import 的 provider，返回模块键与源、二进制或归档入口。
 * @note outLocation 中的指针借用 project，项目释放后失效。 */
ZR_LIBRARY_API TZrBool ZrLibrary_Project_ResolveImportProviderLocation(
        const SZrLibrary_Project *project,
        const TZrChar *currentModuleKey,
        const TZrChar *rawSpecifier,
        TZrChar *resolvedModuleKey,
        TZrSize resolvedModuleKeySize,
        SZrLibrary_ProjectImportProviderLocation *outLocation,
        TZrChar *errorBuffer,
        TZrSize errorBufferSize);

/** @brief 在 provider 定位结果上加入 AOT 后端与动态库路径，供运行时加载。 */
ZR_LIBRARY_API TZrBool ZrLibrary_Project_ResolveImportProviderAotLoadRequest(
        const SZrLibrary_Project *project,
        const TZrChar *currentModuleKey,
        const TZrChar *rawSpecifier,
        EZrAotBackendKind backendKind,
        SZrLibrary_ProjectImportProviderAotLoadRequest *outRequest,
        TZrChar *errorBuffer,
        TZrSize errorBufferSize);

/** @brief 解析规范模块键对应的源文件位置；依赖包会使用各自的源根。 */
ZR_LIBRARY_API TZrBool ZrLibrary_Project_ResolveSourcePath(const SZrLibrary_Project *project,
                                                           const TZrChar *moduleName,
                                                           TZrChar *buffer,
                                                           TZrSize bufferSize);

/** @brief 解析规范模块键对应的编译后二进制位置。 */
ZR_LIBRARY_API TZrBool ZrLibrary_Project_ResolveBinaryPath(const SZrLibrary_Project *project,
                                                           const TZrChar *moduleName,
                                                           TZrChar *buffer,
                                                           TZrSize bufferSize);

/** @brief 解析 AOT/编译中间产物位置，保持与源、二进制模块键一致。 */
ZR_LIBRARY_API TZrBool ZrLibrary_Project_ResolveIntermediatePath(const SZrLibrary_Project *project,
                                                                 const TZrChar *moduleName,
                                                                 TZrChar *buffer,
                                                                 TZrSize bufferSize);

/** @brief 解析项目程序集最终产物位置，供 CLI 编译输出使用。 */
ZR_LIBRARY_API TZrBool ZrLibrary_Project_ResolveAssemblyOutputPath(const SZrLibrary_Project *project,
                                                                   TZrChar *buffer,
                                                                   TZrSize bufferSize);

/** @brief 在已打开依赖归档中查找模块条目；返回指针借用项目持有的 ZRM。 */
ZR_LIBRARY_API TZrBool ZrLibrary_Project_ResolveZrmModuleEntry(
        const SZrLibrary_Project *project,
        const TZrChar *moduleName,
        const SZrLibrary_ZrmArchive **outArchive,
        const SZrLibrary_ZrmEntryInfo **outEntry);

/** @brief 运行当前项目入口，并把执行状态及结果交还 CLI 或测试调用者。 */
ZR_LIBRARY_API EZrThreadStatus ZrLibrary_Project_Run(SZrState *state, SZrTypeValue *result);

/** @brief 无结果包装入口，只委托 Run 并丢弃结果；需要诊断的调用者应使用 Run。 */
ZR_LIBRARY_API void ZrLibrary_Project_Do(SZrState *state);

/** @brief VM 源加载回调；根据项目归属解析模块键并装载源、二进制或 ZRM 条目。 */
ZR_LIBRARY_API TZrBool ZrLibrary_Project_SourceLoadImplementation(SZrState *state, TZrNativeString path, TZrNativeString md5,
                                                                SZrIo *io);

#endif // ZR_VM_LIBRARY_PROJECT_H
