#include "compile_tool_project_provider.h"

#include "compile_tool_binding.h"
#include "compile_time_import.h"
#include "zr_vm_library/project.h"
#include "zr_vm_parser/compile_tool.h"

#include <stdio.h>
#include <string.h>

typedef enum EZrCompileToolProjectProviderState {
    ZR_COMPILE_TOOL_PROJECT_PROVIDER_LOADING = 1,
    ZR_COMPILE_TOOL_PROJECT_PROVIDER_READY = 2
} EZrCompileToolProjectProviderState;

enum { ZR_COMPILE_TOOL_PROJECT_PROVIDER_MAX_ANCESTRY = 64U };

struct SZrCompileToolProjectProvider {
    SZrParserCompileToolResolvedArtifact artifact;
    SZrParserCompileToolModuleDescriptor descriptor;
    SZrImportedCompileTimeModule *module;
    TZrChar moduleName[ZR_LIBRARY_MAX_PATH_LENGTH];
    EZrCompileToolProjectProviderState state;
};

static const TZrChar *compile_tool_project_provider_text(const SZrString *value) {
    return value != ZR_NULL ? ZrCore_String_GetNativeString(value) : ZR_NULL;
}

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

static TZrBool compile_tool_project_provider_is_absolute_path(
        const TZrChar *path) {
    return path != ZR_NULL &&
           (path[0] == '/' || path[0] == '\\' ||
            (path[0] != '\0' && path[1] == ':'));
}

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
    provider = compile_tool_project_provider_find(cs, rawSpecifier);
    if (provider != ZR_NULL) {
        if (provider->state == ZR_COMPILE_TOOL_PROJECT_PROVIDER_LOADING) {
            compile_tool_project_provider_report_cycle(
                    cs, rawSpecifier, location);
            return ZR_FALSE;
        }
        return compile_tool_project_provider_bind(
                cs, aliasName, provider, location);
    }

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
    ZrCore_Memory_RawSet(provider, 0, sizeof(*provider));
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
            cs, &provider->artifact.moduleIdentity);
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
    provider->descriptor.moduleName = provider->moduleName;
    provider->descriptor.providerPhase = ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL;
    provider->descriptor.publicContractHash = provider->artifact.publicContractHash;
    provider->state = ZR_COMPILE_TOOL_PROJECT_PROVIDER_LOADING;
    ZrCore_Array_Push(cs->state, &cs->ownedCompileToolProviders, &provider);
    moduleName = ZrCore_String_CreateFromNative(
            cs->state, provider->moduleName);
    provider->module = ZrParser_CompileTimeImport_LoadSourceModule(
            cs,
            moduleName,
            provider->artifact.artifactBytes,
            provider->artifact.artifactByteCount,
            ZR_FALSE);
    if (moduleName == ZR_NULL || provider->module == ZR_NULL) {
        ZrParser_CompileTimeImport_RestoreModules(cs, moduleMark);
        compile_tool_project_provider_restore(cs, providerMark);
        if (cs->errorMessage == ZR_NULL) {
            ZrParser_Compiler_Error(
                    cs,
                    "compiletool.artifact.executable_invalid: provider module is not valid compiler-owned source",
                    location);
        }
        return ZR_FALSE;
    }
    provider->state = ZR_COMPILE_TOOL_PROJECT_PROVIDER_READY;
    if (!compile_tool_project_provider_bind(
                cs, aliasName, provider, location)) {
        ZrParser_CompileTimeImport_RestoreModules(cs, moduleMark);
        compile_tool_project_provider_restore(cs, providerMark);
        ZrParser_Compiler_Error(
                cs, "compiletool.binding: resolved provider contract mismatch", location);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

void ZrParser_CompileToolProjectProvider_FreeAll(SZrCompilerState *cs) {
    if (cs == ZR_NULL || cs->state == ZR_NULL ||
        !cs->ownedCompileToolProviders.isValid) {
        return;
    }
    compile_tool_project_provider_restore(cs, 0U);
    ZrCore_Array_Free(cs->state, &cs->ownedCompileToolProviders);
}
