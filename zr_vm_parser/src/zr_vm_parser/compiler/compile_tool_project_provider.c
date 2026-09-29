#include "compile_tool_project_provider.h"

#include "compile_tool_binding.h"
#include "compile_time_import.h"
#include "zr_vm_library/project.h"
#include "zr_vm_parser/compile_tool.h"

#include <stdio.h>
#include <string.h>
/** @brief 只让完整加载的 provider 可绑定；LOADING 用于识别递归重入。 */
typedef enum EZrCompileToolProjectProviderState {
    ZR_COMPILE_TOOL_PROJECT_PROVIDER_LOADING = 1, /**< 正在解析所持 artifact 的编译期源码。 */
    ZR_COMPILE_TOOL_PROJECT_PROVIDER_READY = 2   /**< 模块已装载，可建立 alias 与契约绑定。 */
} EZrCompileToolProjectProviderState;
/** @brief 限制诊断遍历的 compiler-state 父链，避免循环报告使用无界临时栈。 */
enum { ZR_COMPILE_TOOL_PROJECT_PROVIDER_MAX_ANCESTRY = 64U };
/** @brief 拥有已解析 artifact；descriptor 借用本对象字段，module 由 compiler 的导入模块表管理。 */
struct SZrCompileToolProjectProvider {
    SZrParserCompileToolResolvedArtifact artifact;
    SZrParserCompileToolModuleDescriptor descriptor;
    SZrImportedCompileTimeModule *module;
    TZrChar moduleName[ZR_LIBRARY_MAX_PATH_LENGTH]; /**< descriptor.moduleName 的稳定存储，随 provider 释放。 */
    EZrCompileToolProjectProviderState state; /**< READY 前不能发布 alias；回滚按所属列表逆序释放。 */
};
/** @brief 借出 VM 字符串的 native 文本供本次同步解析，不转移字符串所有权。 */
static const TZrChar *compile_tool_project_provider_text(const SZrString *value) {
    return value != ZR_NULL ? ZrCore_String_GetNativeString(value) : ZR_NULL;
}
/** @brief 将包模块说明映射到当前项目的 buildDependencies；版本与 lock 校验留给 artifact opener。 */
static const SZrLibrary_ProjectManifestDependency *
compile_tool_project_provider_find_dependency(
        const SZrLibrary_Project *project,
        const TZrChar *rawSpecifier) {
    SZrLibrary_ModuleSpecifier specifier;

    if (project == ZR_NULL || rawSpecifier == ZR_NULL ||
        !ZrLibrary_ModuleSpecifier_Parse(rawSpecifier, &specifier, ZR_NULL, 0U) ||
        specifier.kind != ZR_LIBRARY_MODULE_SPECIFIER_KIND_PACKAGE) {
        return ZR_NULL;
    }
    for (TZrSize index = 0U;
         index < project->manifestBuildDependencyCount;
         index++) {
        const SZrLibrary_ProjectManifestDependency *dependency =
                &project->manifestBuildDependencies[index];
        if (dependency->packageIdentity.domain ==
                    ZR_LIBRARY_MODULE_DOMAIN_PACKAGE &&
            specifier.identity.domain == ZR_LIBRARY_MODULE_DOMAIN_PACKAGE &&
            strcmp(
                    dependency->packageIdentity.packageName,
                    specifier.identity.packageName) == 0) {
            return dependency;
        }
    }
    return ZR_NULL;
}
/** @brief 判断 source 是否跳过项目目录拼接，当前按分隔符或首字符后的冒号识别。 */
static TZrBool compile_tool_project_provider_is_absolute_path(
        const TZrChar *path) {
    return path != ZR_NULL &&
           (path[0] == '/' || path[0] == '\\' ||
            (path[0] != '\0' && path[1] == ':')); // TODO: 核查是否必须限定盘符字母，避免误判 POSIX 的 x:relative 文件名。
}
/** @brief 将 path source 转成 artifact opener 使用的路径。 */
static TZrBool compile_tool_project_provider_archive_path(
        const SZrLibrary_Project *project,
        const SZrLibrary_ProjectManifestDependency *dependency,
        TZrChar *outPath,
        TZrSize outPathSize) {
    const TZrChar *source;
    const TZrChar *directory;
    int written;

    if (project == ZR_NULL || dependency == ZR_NULL || outPath == ZR_NULL ||
        outPathSize == 0U ||
        dependency->sourceKind !=
                ZR_LIBRARY_PROJECT_MANIFEST_DEPENDENCY_SOURCE_PATH) {
        return ZR_FALSE;
    }
    source = compile_tool_project_provider_text(dependency->source);
    directory = compile_tool_project_provider_text(project->directory);
    if (source == ZR_NULL || source[0] == '\0') {
        return ZR_FALSE;
    }
    if (compile_tool_project_provider_is_absolute_path(source)) {
        written = snprintf(outPath, outPathSize, "%s", source);
    } else if (directory != ZR_NULL && directory[0] != '\0') {
        written = snprintf(outPath, outPathSize, "%s/%s", directory, source);
    } else {
        written = snprintf(outPath, outPathSize, "%s", source);
    }
    return written >= 0 && (TZrSize)written < outPathSize;
}
/** @brief 按原始模块说明在当前与父 compiler state 中复用 provider，返回借用指针。 */
static SZrCompileToolProjectProvider *compile_tool_project_provider_find(
        const SZrCompilerState *cs,
        const TZrChar *rawSpecifier) {
    if (cs == ZR_NULL || rawSpecifier == ZR_NULL) {
        return ZR_NULL;
    }
    for (const SZrCompilerState *cursor = cs;
         cursor != ZR_NULL;
         cursor = cursor->compileToolProviderParent) {
        for (TZrSize index = 0U;
             index < cursor->ownedCompileToolProviders.length;
             index++) {
            SZrCompileToolProjectProvider **provider =
                    (SZrCompileToolProjectProvider **)ZrCore_Array_Get(
                            (SZrArray *)&cursor->ownedCompileToolProviders,
                            index);
            if (provider != ZR_NULL && *provider != ZR_NULL &&
                strcmp((*provider)->moduleName, rawSpecifier) == 0) {
                return *provider;
            }
        }
    }
    return ZR_NULL;
}
/** @brief 按已打开 artifact 的规范模块身份跨父链去重，返回仍由所属 state 持有的对象。 */
static SZrCompileToolProjectProvider *compile_tool_project_provider_find_identity(
        const SZrCompilerState *cs,
        const SZrLibrary_ModuleIdentity *identity) {
    if (cs == ZR_NULL || identity == ZR_NULL) {
        return ZR_NULL;
    }
    for (const SZrCompilerState *cursor = cs;
         cursor != ZR_NULL;
         cursor = cursor->compileToolProviderParent) {
        for (TZrSize index = 0U;
             index < cursor->ownedCompileToolProviders.length;
             index++) {
            SZrCompileToolProjectProvider **provider =
                    (SZrCompileToolProjectProvider **)ZrCore_Array_Get(
                            (SZrArray *)&cursor->ownedCompileToolProviders,
                            index);
            if (provider != ZR_NULL && *provider != ZR_NULL &&
                ZrParser_CompileToolArtifact_IsOpen(&(*provider)->artifact) &&
                ZrLibrary_ModuleIdentity_Equals(
                        &(*provider)->artifact.moduleIdentity,
                        identity)) {
                return *provider;
            }
        }
    }
    return ZR_NULL;
}
/** @brief 先登记 resolved provider，再登记限定 alias；任一失败都恢复两张可见表的长度。 */
static TZrBool compile_tool_project_provider_bind(
        SZrCompilerState *cs,
        SZrString *aliasName,
        SZrCompileToolProjectProvider *provider,
        SZrFileRange location) {
    TZrSize bindingMark;
    TZrSize aliasMark;

    if (cs == ZR_NULL || aliasName == ZR_NULL || provider == ZR_NULL ||
        provider->state != ZR_COMPILE_TOOL_PROJECT_PROVIDER_READY ||
        provider->module == ZR_NULL) {
        return ZR_FALSE;
    }
    bindingMark = ZrParser_CompileToolBinding_Mark(cs);
    aliasMark = cs->importedCompileTimeModuleAliases.length;
    if (ZrParser_CompileToolBinding_DeclareResolvedProvider(
                cs,
                aliasName,
                &provider->descriptor,
                &provider->artifact) &&
        ZrParser_CompileTimeImport_RegisterModuleAlias(
                cs,
                aliasName,
                provider->module,
                location,
                ZR_FALSE)) {
        return ZR_TRUE;
    }
    ZrParser_CompileToolBinding_Restore(cs, bindingMark);
    cs->importedCompileTimeModuleAliases.length = aliasMark;
    return ZR_FALSE;
}
/** @brief 按父链顺序列出 LOADING provider 与重入说明，并同步复制诊断文本到 compiler state。 */
static void compile_tool_project_provider_report_cycle(
        SZrCompilerState *cs,
        const TZrChar *rawSpecifier,
        SZrFileRange location) {
    const SZrCompilerState
            *ancestry[ZR_COMPILE_TOOL_PROJECT_PROVIDER_MAX_ANCESTRY];
    TZrSize ancestryLength = 0U;
    TZrChar message[ZR_PARSER_ERROR_BUFFER_LENGTH];
    TZrSize offset = 0U;
    int written;

    written = snprintf(
            message,
            sizeof(message),
            "comptime.phase_cycle: compile-tool provider graph ");
    if (written > 0 && (TZrSize)written < sizeof(message)) {
        offset = (TZrSize)written;
    }
    for (const SZrCompilerState *cursor = cs;
         cursor != ZR_NULL &&
         ancestryLength < ZR_COMPILE_TOOL_PROJECT_PROVIDER_MAX_ANCESTRY;
         cursor = cursor->compileToolProviderParent) {
        ancestry[ancestryLength++] = cursor;
    }
    while (ancestryLength > 0U && offset < sizeof(message)) {
        const SZrCompilerState *cursor = ancestry[--ancestryLength];

        for (TZrSize index = 0U;
             index < cursor->ownedCompileToolProviders.length;
             index++) {
            SZrCompileToolProjectProvider **provider =
                    (SZrCompileToolProjectProvider **)ZrCore_Array_Get(
                            (SZrArray *)&cursor->ownedCompileToolProviders,
                            index);
            if (provider == ZR_NULL || *provider == ZR_NULL ||
                (*provider)->state !=
                        ZR_COMPILE_TOOL_PROJECT_PROVIDER_LOADING) {
                continue;
            }
            written = snprintf(
                    message + offset,
                    sizeof(message) - offset,
                    "%s -> ",
                    (*provider)->moduleName);
            if (written < 0 ||
                (TZrSize)written >= sizeof(message) - offset) {
                offset = sizeof(message) - 1U;
                break;
            }
            offset += (TZrSize)written;
        }
    }
    if (offset < sizeof(message)) {
        snprintf(
                message + offset,
                sizeof(message) - offset,
                "%s",
                rawSpecifier != ZR_NULL ? rawSpecifier : "<unknown>");
    }
    ZrParser_Compiler_Error(cs, message, location);
}
/** @brief 关闭 provider 独占的 artifact 并释放记录；编译期模块仍由 importedCompileTimeModules 持有。 */
static void compile_tool_project_provider_release(
        SZrCompilerState *cs,
        SZrCompileToolProjectProvider *provider) {
    if (cs == ZR_NULL || cs->state == ZR_NULL || provider == ZR_NULL) {
        return;
    }
    ZrParser_CompileToolArtifact_Close(&provider->artifact);
    ZrCore_Memory_RawFreeWithType(
            cs->state->global,
            provider,
            sizeof(*provider),
            ZR_MEMORY_NATIVE_TYPE_ARRAY);
}
/** @brief 将 provider 列表回退到 mark，按后进先出关闭 artifact 并收缩逻辑长度。 */
static void compile_tool_project_provider_restore(
        SZrCompilerState *cs,
        TZrSize mark) {
    if (cs == ZR_NULL || mark > cs->ownedCompileToolProviders.length) {
        return;
    }
    while (cs->ownedCompileToolProviders.length > mark) {
        TZrSize index = cs->ownedCompileToolProviders.length - 1U;
        SZrCompileToolProjectProvider **provider =
                (SZrCompileToolProjectProvider **)ZrCore_Array_Get(
                        &cs->ownedCompileToolProviders,
                        index);

        if (provider != ZR_NULL) {
            compile_tool_project_provider_release(cs, *provider);
        }
        cs->ownedCompileToolProviders.length = index;
    }
}
/** @brief 从已锁定的项目 path build dependency 装载编译期模块并把 alias 绑定到其 artifact 契约。 */
TZrBool ZrParser_CompileToolProjectProvider_Declare(
        SZrCompilerState *cs,
        SZrString *aliasName,
        const TZrChar *rawSpecifier,
        SZrFileRange location) {
    const SZrLibrary_Project *project;
    const SZrLibrary_ProjectManifestDependency *dependency;
    SZrCompileToolProjectProvider *provider;
    SZrCompileToolProjectProvider *identityProvider;
    SZrString *moduleName;
    TZrSize providerMark;
    TZrSize moduleMark;
    TZrChar archivePath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar error[ZR_LIBRARY_ZRM_ERROR_BUFFER_LENGTH];

    if (cs == ZR_NULL || cs->state == ZR_NULL ||
        cs->state->global == ZR_NULL || aliasName == ZR_NULL ||
        rawSpecifier == ZR_NULL) {
        return ZR_FALSE;
    }
    provider = compile_tool_project_provider_find(cs, rawSpecifier); // 同一说明复用既有对象；LOADING 表示递归导入，不能发布半成品 alias。
    if (provider != ZR_NULL) {
        if (provider->state == ZR_COMPILE_TOOL_PROJECT_PROVIDER_LOADING) {
            compile_tool_project_provider_report_cycle(
                    cs, rawSpecifier, location);
            return ZR_FALSE;
        }
        return compile_tool_project_provider_bind(
                cs, aliasName, provider, location);
    }
    // 只接受当前项目已声明且可物化为本地路径的 build provider。
    project = ZrLibrary_Project_GetFromGlobal(cs->state->global);
    dependency = compile_tool_project_provider_find_dependency(
            project, rawSpecifier);
    if (dependency == ZR_NULL ||
        !compile_tool_project_provider_archive_path(
                project, dependency, archivePath, sizeof(archivePath))) {
        ZrParser_Compiler_Error(
                cs,
                "compiletool.artifact.source: build dependency requires a materialized path provider",
                location);
        return ZR_FALSE;
    }
    // 模块与 provider 使用各自 mark，任何后续失败都按相反依赖顺序回滚。
    providerMark = cs->ownedCompileToolProviders.length;
    moduleMark = cs->importedCompileTimeModules.length;
    provider = (SZrCompileToolProjectProvider *)
            ZrCore_Memory_RawMallocWithType(
                    cs->state->global,
                    sizeof(*provider),
                    ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (provider == ZR_NULL) {
        ZrParser_Compiler_Error(
                cs, "compiletool.artifact.allocation: provider allocation failed", location);
        return ZR_FALSE;
    }
    ZrCore_Memory_RawSet(provider, 0, sizeof(*provider)); // 清零临时记录，使 Open 失败路径能安全释放；成功后 artifact 归此记录所有。
    if (!ZrParser_CompileToolArtifact_OpenProjectBuildDependency(
                project,
                rawSpecifier,
                archivePath,
                &provider->artifact,
                error,
                sizeof(error))) {
        ZrCore_Memory_RawFreeWithType(
                cs->state->global,
                provider,
                sizeof(*provider),
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
        ZrParser_Compiler_Error(
                cs,
                error[0] != '\0'
                        ? error
                        : "compiletool.artifact.open: failed to open provider artifact",
                location);
        return ZR_FALSE;
    }
    identityProvider = compile_tool_project_provider_find_identity(
            cs, &provider->artifact.moduleIdentity); // 跨说明按规范身份去重；READY 可复用，LOADING 表示环。
    if (identityProvider != ZR_NULL) {
        TZrBool ready = (TZrBool)(identityProvider->state ==
                                  ZR_COMPILE_TOOL_PROJECT_PROVIDER_READY);

        if (!ready) {
            compile_tool_project_provider_report_cycle(
                    cs, rawSpecifier, location);
        }
        compile_tool_project_provider_release(cs, provider);
        return ready
                       ? compile_tool_project_provider_bind(
                                 cs,
                                 aliasName,
                                 identityProvider,
                                 location)
                       : ZR_FALSE;
    }
    if (snprintf(
                provider->moduleName,
                sizeof(provider->moduleName),
                "%s",
                rawSpecifier) < 0 ||
        strlen(rawSpecifier) >= sizeof(provider->moduleName)) {
        ZrParser_CompileToolArtifact_Close(&provider->artifact);
        ZrCore_Memory_RawFreeWithType(
                cs->state->global,
                provider,
                sizeof(*provider),
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
        ZrParser_Compiler_Error(
                cs, "compiletool.artifact.identity: module name is too long", location);
        return ZR_FALSE;
    }
    provider->descriptor.moduleName = provider->moduleName; // descriptor 借用 provider 地址内的名字与 artifact hash。
    provider->descriptor.providerPhase = ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL;
    provider->descriptor.publicContractHash = provider->artifact.publicContractHash;
    provider->state = ZR_COMPILE_TOOL_PROJECT_PROVIDER_LOADING; // 先登记 LOADING，让递归 build-facts 能发现该节点。
    ZrCore_Array_Push(cs->state, &cs->ownedCompileToolProviders, &provider); // BUG: compiler_state.c:225 的 void Array_Init OOM 仍标有效；compile_time_executor.c:114 可达此 Push，array.h:74 断言空 head，array.h:84/88 在扩容 OOM 后 RawCopy 到空 head。
    moduleName = ZrCore_String_CreateFromNative(
            cs->state, provider->moduleName);
    provider->module = ZrParser_CompileTimeImport_LoadSourceModule(
            cs,
            moduleName,
            provider->artifact.artifactBytes,
            provider->artifact.artifactByteCount,
            ZR_FALSE); // source bytes 仅在本次调用读取；模块借用 moduleName 至恢复/释放（长字符串 GC 根仍待核查），返回模块归 importedCompileTimeModules 持有。
    if (moduleName == ZR_NULL || provider->module == ZR_NULL) {
        ZrParser_CompileTimeImport_RestoreModules(cs, moduleMark); // 解析失败先撤销本次模块导入，避免模块仍借用待释放 provider。
        compile_tool_project_provider_restore(cs, providerMark); // 再关闭事务中新建的 artifact 与 provider 记录。
        if (cs->errorMessage == ZR_NULL) {
            ZrParser_Compiler_Error(
                    cs,
                    "compiletool.artifact.executable_invalid: provider module is not valid compiler-owned source",
                    location);
        }
        return ZR_FALSE;
    }
    provider->state = ZR_COMPILE_TOOL_PROJECT_PROVIDER_READY; // 只在模块完整建立后开放绑定。
    if (!compile_tool_project_provider_bind(
                cs, aliasName, provider, location)) {
        ZrParser_CompileTimeImport_RestoreModules(cs, moduleMark); // alias/契约发布失败时先撤销新模块。
        compile_tool_project_provider_restore(cs, providerMark); // 随后释放 provider，保持借用关系有效到回滚完成。
        ZrParser_Compiler_Error(
                cs, "compiletool.binding: resolved provider contract mismatch", location);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}
/** @brief compiler state teardown 时关闭并释放其拥有的全部项目 provider。 */
void ZrParser_CompileToolProjectProvider_FreeAll(SZrCompilerState *cs) {
    if (cs == ZR_NULL || cs->state == ZR_NULL ||
        !cs->ownedCompileToolProviders.isValid) {
        return;
    }
    compile_tool_project_provider_restore(cs, 0U);
    ZrCore_Array_Free(cs->state, &cs->ownedCompileToolProviders);
}
