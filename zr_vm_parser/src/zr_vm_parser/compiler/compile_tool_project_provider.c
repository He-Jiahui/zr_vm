#include "compile_tool_project_provider.h"

#include "compile_tool_binding.h"
#include "compile_time_import.h"
#include "zr_vm_library/project.h"
#include "zr_vm_parser/compile_tool.h"

#include <stdio.h>
#include <string.h>

struct SZrCompileToolProjectProvider {
    SZrParserCompileToolResolvedArtifact artifact;
    SZrParserCompileToolModuleDescriptor descriptor;
    SZrImportedCompileTimeModule *module;
    TZrChar moduleName[ZR_LIBRARY_MAX_PATH_LENGTH];
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
    for (TZrSize index = 0U;
         index < cs->ownedCompileToolProviders.length;
         index++) {
        SZrCompileToolProjectProvider **provider =
                (SZrCompileToolProjectProvider **)ZrCore_Array_Get(
                        (SZrArray *)&cs->ownedCompileToolProviders, index);
        if (provider != ZR_NULL && *provider != ZR_NULL &&
            strcmp((*provider)->moduleName, rawSpecifier) == 0) {
            return *provider;
        }
    }
    return ZR_NULL;
}

TZrBool ZrParser_CompileToolProjectProvider_Declare(
        SZrCompilerState *cs,
        SZrString *aliasName,
        const TZrChar *rawSpecifier,
        SZrFileRange location) {
    const SZrLibrary_Project *project;
    const SZrLibrary_ProjectManifestDependency *dependency;
    SZrCompileToolProjectProvider *provider;
    SZrString *moduleName;
    TZrSize bindingMark;
    TZrChar archivePath[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar error[ZR_LIBRARY_ZRM_ERROR_BUFFER_LENGTH];

    if (cs == ZR_NULL || cs->state == ZR_NULL ||
        cs->state->global == ZR_NULL || aliasName == ZR_NULL ||
        rawSpecifier == ZR_NULL) {
        return ZR_FALSE;
    }
    provider = compile_tool_project_provider_find(cs, rawSpecifier);
    if (provider != ZR_NULL) {
        bindingMark = ZrParser_CompileToolBinding_Mark(cs);
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
        return ZR_FALSE;
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
    moduleName = ZrCore_String_CreateFromNative(
            cs->state, provider->moduleName);
    provider->module = ZrParser_CompileTimeImport_LoadSourceModule(
            cs,
            moduleName,
            provider->artifact.artifactBytes,
            provider->artifact.artifactByteCount,
            ZR_FALSE);
    if (moduleName == ZR_NULL || provider->module == ZR_NULL) {
        ZrParser_CompileToolArtifact_Close(&provider->artifact);
        ZrCore_Memory_RawFreeWithType(
                cs->state->global,
                provider,
                sizeof(*provider),
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
        ZrParser_Compiler_Error(
                cs,
                "compiletool.artifact.executable_invalid: provider module is not valid compiler-owned source",
                location);
        return ZR_FALSE;
    }
    ZrCore_Array_Push(cs->state, &cs->ownedCompileToolProviders, &provider);
    bindingMark = ZrParser_CompileToolBinding_Mark(cs);
    if (!ZrParser_CompileToolBinding_DeclareResolvedProvider(
                cs, aliasName, &provider->descriptor, &provider->artifact) ||
        !ZrParser_CompileTimeImport_RegisterModuleAlias(
                cs,
                aliasName,
                provider->module,
                location,
                ZR_FALSE)) {
        ZrParser_CompileToolBinding_Restore(cs, bindingMark);
        cs->ownedCompileToolProviders.length--;
        ZrParser_CompileToolArtifact_Close(&provider->artifact);
        ZrCore_Memory_RawFreeWithType(
                cs->state->global,
                provider,
                sizeof(*provider),
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
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
    for (TZrSize index = 0U;
         index < cs->ownedCompileToolProviders.length;
         index++) {
        SZrCompileToolProjectProvider **provider =
                (SZrCompileToolProjectProvider **)ZrCore_Array_Get(
                        &cs->ownedCompileToolProviders, index);
        if (provider == ZR_NULL || *provider == ZR_NULL) {
            continue;
        }
        ZrParser_CompileToolArtifact_Close(&(*provider)->artifact);
        ZrCore_Memory_RawFreeWithType(
                cs->state->global,
                *provider,
                sizeof(**provider),
                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    ZrCore_Array_Free(cs->state, &cs->ownedCompileToolProviders);
}
