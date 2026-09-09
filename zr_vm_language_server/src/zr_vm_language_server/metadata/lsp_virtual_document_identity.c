#include "metadata/lsp_virtual_document_identity.h"
#include "lsp_virtual_documents.h"
#include "zr_vm_core/string_builder.h"
#include "zr_vm_library/native_registry.h"
#include "cJSON/cJSON.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ZR_LSP_SCOPED_VIRTUAL_URI_PREFIX "zr-decompiled:/"
#define ZR_LSP_SCOPED_VIRTUAL_URI_INITIAL_CAPACITY 256U
#define ZR_LSP_UINT64_DECIMAL_CAPACITY 21U

static TZrBool identity_append_encoded(SZrStringBuilder *builder, const TZrChar *text) {
    static const TZrChar hex[] = "0123456789ABCDEF";
    for (const unsigned char *cursor = (const unsigned char *)text; *cursor != 0U; cursor++) {
        TZrChar encoded[3];
        TZrBool unreserved = (*cursor >= 'a' && *cursor <= 'z') ||
                (*cursor >= 'A' && *cursor <= 'Z') || (*cursor >= '0' && *cursor <= '9') ||
                *cursor == '-' || *cursor == '.' || *cursor == '_' || *cursor == '~';
        encoded[0] = unreserved ? (TZrChar)*cursor : '%';
        encoded[1] = hex[*cursor >> 4U];
        encoded[2] = hex[*cursor & 15U];
        if (!ZrCore_StringBuilder_AppendNative(builder, encoded, unreserved ? 1U : 3U)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static int identity_hex_digit(TZrChar value) {
    if (value >= '0' && value <= '9') {
        return value - '0';
    }
    if (value >= 'A' && value <= 'F') {
        return value - 'A' + 10;
    }
    if (value >= 'a' && value <= 'f') {
        return value - 'a' + 10;
    }
    return -1;
}

static SZrString *identity_decode(SZrState *state, const TZrChar *text, TZrSize length) {
    SZrStringBuilder builder;
    SZrString *result = ZR_NULL;
    if (!ZrCore_StringBuilder_Init(state, &builder, length + 1U)) {
        return ZR_NULL;
    }
    for (TZrSize index = 0U; index < length; index++) {
        TZrChar value = text[index];
        if (value == '%') {
            int high;
            int low;
            if (length - index < 3U || (high = identity_hex_digit(text[index + 1U])) < 0 ||
                (low = identity_hex_digit(text[index + 2U])) < 0) {
                goto cleanup;
            }
            value = (TZrChar)((high << 4) | low);
            index += 2U;
        } else if (value == '#') {
            goto cleanup;
        }
        if (value == '\0' || !ZrCore_StringBuilder_AppendNative(&builder, &value, 1U)) {
            goto cleanup;
        }
    }
    result = ZrCore_StringBuilder_Freeze(&builder);
cleanup:
    ZrCore_StringBuilder_Dispose(&builder);
    return result;
}

TZrBool ZrLanguageServer_LspVirtualDocumentIdentity_IsScoped(SZrString *uri) {
    return ZrLanguageServer_LspVirtualDocuments_IsDeclarationUri(uri) &&
           strchr(ZrCore_String_GetNativeString(uri), '?') != ZR_NULL;
}

SZrString *ZrLanguageServer_LspVirtualDocumentIdentity_Create(
        SZrState *state, const SZrLspVirtualDocumentIdentity *identity) {
    SZrStringBuilder builder;
    SZrString *result = ZR_NULL;
    cJSON *json = ZR_NULL;
    TZrChar *jsonText = ZR_NULL;
    TZrChar generation[ZR_LSP_UINT64_DECIMAL_CAPACITY];

    if (state == ZR_NULL || identity == ZR_NULL || identity->moduleName == ZR_NULL ||
        identity->projectUri == ZR_NULL || identity->originUri == ZR_NULL ||
        identity->providerGeneration == 0U ||
        ZrCore_String_GetNativeString(identity->moduleName)[0] == '\0' ||
        ZrCore_String_GetNativeString(identity->projectUri)[0] == '\0' ||
        ZrCore_String_GetNativeString(identity->originUri)[0] == '\0' ||
        !ZrCore_StringBuilder_Init(state, &builder, ZR_LSP_SCOPED_VIRTUAL_URI_INITIAL_CAPACITY)) {
        return ZR_NULL;
    }
    snprintf(generation, sizeof(generation), "%llu", (unsigned long long)identity->providerGeneration);
    json = cJSON_CreateObject();
    if (json == ZR_NULL ||
        cJSON_AddStringToObject(json, "project", ZrCore_String_GetNativeString(identity->projectUri)) == ZR_NULL ||
        cJSON_AddStringToObject(json, "origin", ZrCore_String_GetNativeString(identity->originUri)) == ZR_NULL ||
        cJSON_AddStringToObject(json, "generation", generation) == ZR_NULL ||
        (jsonText = cJSON_PrintUnformatted(json)) == ZR_NULL ||
        !ZrCore_StringBuilder_AppendNative(&builder, ZR_LSP_SCOPED_VIRTUAL_URI_PREFIX,
                sizeof(ZR_LSP_SCOPED_VIRTUAL_URI_PREFIX) - 1U) ||
        !identity_append_encoded(&builder, ZrCore_String_GetNativeString(identity->moduleName)) ||
        !ZrCore_StringBuilder_AppendNative(&builder, ".zr?", 4U) ||
        !identity_append_encoded(&builder, jsonText)) {
        goto cleanup;
    }
    result = ZrCore_StringBuilder_Freeze(&builder);
cleanup:
    cJSON_free(jsonText);
    cJSON_Delete(json);
    ZrCore_StringBuilder_Dispose(&builder);
    return result;
}

TZrBool ZrLanguageServer_LspVirtualDocumentIdentity_Parse(
        SZrState *state, SZrString *uri, SZrLspVirtualDocumentIdentity *outIdentity) {
    const TZrChar *moduleText;
    const TZrChar *query;
    SZrString *decoded;
    SZrString *moduleName;
    cJSON *json = ZR_NULL;
    const cJSON *project;
    const cJSON *origin;
    const cJSON *generation;
    TZrChar *canonical = ZR_NULL;
    TZrChar *end = ZR_NULL;
    unsigned long long value;
    TZrBool parsed = ZR_FALSE;

    if (outIdentity != ZR_NULL) {
        memset(outIdentity, 0, sizeof(*outIdentity));
    }
    if (state == ZR_NULL || outIdentity == ZR_NULL ||
        !ZrLanguageServer_LspVirtualDocumentIdentity_IsScoped(uri)) {
        return ZR_FALSE;
    }
    moduleText = ZrCore_String_GetNativeString(uri) + sizeof(ZR_LSP_SCOPED_VIRTUAL_URI_PREFIX) - 1U;
    query = strchr(moduleText, '?');
    if (query - moduleText <= 3 || memcmp(query - 3, ".zr", 3U) != 0 ||
        (moduleName = identity_decode(state, moduleText, (TZrSize)(query - moduleText) - 3U)) == ZR_NULL ||
        (decoded = identity_decode(state, query + 1, strlen(query + 1))) == ZR_NULL) {
        return ZR_FALSE;
    }
    json = cJSON_ParseWithOpts(ZrCore_String_GetNativeString(decoded), ZR_NULL, 1);
    project = cJSON_GetObjectItemCaseSensitive(json, "project");
    origin = cJSON_GetObjectItemCaseSensitive(json, "origin");
    generation = cJSON_GetObjectItemCaseSensitive(json, "generation");
    if (!cJSON_IsObject(json) || cJSON_GetArraySize(json) != 3 ||
        !cJSON_IsString(project) || project->valuestring[0] == '\0' ||
        !cJSON_IsString(origin) || origin->valuestring[0] == '\0' ||
        !cJSON_IsString(generation) || generation->valuestring[0] < '1' ||
        generation->valuestring[0] > '9') {
        goto cleanup;
    }
    for (const TZrChar *digit = generation->valuestring; *digit != '\0'; digit++) {
        if (*digit < '0' || *digit > '9') {
            goto cleanup;
        }
    }
    errno = 0;
    value = strtoull(generation->valuestring, &end, 10);
    if (errno == ERANGE || end == ZR_NULL || *end != '\0' || value == 0U) {
        goto cleanup;
    }
    /* Canonical JSON also rejects embedded NUL and lossy string decoding. */
    canonical = cJSON_PrintUnformatted(json);
    if (canonical == ZR_NULL || strcmp(canonical, ZrCore_String_GetNativeString(decoded)) != 0) {
        goto cleanup;
    }
    outIdentity->moduleName = moduleName;
    outIdentity->projectUri = ZrCore_String_CreateFromNative(state, project->valuestring);
    outIdentity->originUri = ZrCore_String_CreateFromNative(state, origin->valuestring);
    outIdentity->providerGeneration = (TZrUInt64)value;
    parsed = outIdentity->projectUri != ZR_NULL && outIdentity->originUri != ZR_NULL;
cleanup:
    cJSON_free(canonical);
    cJSON_Delete(json);
    if (!parsed) {
        memset(outIdentity, 0, sizeof(*outIdentity));
    }
    return parsed;
}

static TZrBool identity_ensure_project_provider(
        SZrState *state, SZrLspProjectIndex *projectIndex, SZrString *moduleName) {
    return state != ZR_NULL && projectIndex != ZR_NULL && projectIndex->project != ZR_NULL &&
           projectIndex->project->directory != ZR_NULL && moduleName != ZR_NULL &&
           ZrLibrary_NativeRegistry_EnsureProjectDescriptorPlugin(state,
                   ZrCore_String_GetNativeString(projectIndex->project->directory),
                   ZrCore_String_GetNativeString(moduleName));
}

TZrBool ZrLanguageServer_LspVirtualDocumentIdentity_ResolveNativeUri(
        SZrState *state, SZrLspContext *context, SZrLspProjectIndex *projectIndex,
        SZrString *moduleName, SZrString **outUri) {
    SZrString *origin = ZR_NULL;
    SZrLspVirtualDocumentIdentity identity = {0};
    if (outUri != ZR_NULL) {
        *outUri = ZR_NULL;
    }
    if (outUri == ZR_NULL || !ZrLanguageServer_LspModuleMetadata_ResolveNativeModuleUri(
                state, projectIndex, moduleName, &origin)) {
        return ZR_FALSE;
    }
    if (ZrLanguageServer_LspVirtualDocuments_IsDeclarationUri(origin)) {
        EZrLspImportedModuleSourceKind sourceKind = ZR_LSP_IMPORTED_MODULE_SOURCE_UNRESOLVED;
        if (ZrLanguageServer_LspModuleMetadata_ResolveNativeModuleDescriptor(
                    state, ZrCore_String_GetNativeString(moduleName), &sourceKind) == ZR_NULL ||
            sourceKind != ZR_LSP_IMPORTED_MODULE_SOURCE_NATIVE_BUILTIN) {
            return ZR_FALSE;
        }
        *outUri = origin;
        return ZR_TRUE;
    }
    if (context == ZR_NULL || !identity_ensure_project_provider(state, projectIndex, moduleName)) {
        return ZR_FALSE;
    }
    identity.moduleName = moduleName;
    identity.projectUri = projectIndex->projectFileUri;
    identity.originUri = origin;
    identity.providerGeneration = context->semanticSnapshotProviderGeneration;
    *outUri = ZrLanguageServer_LspVirtualDocumentIdentity_Create(state, &identity);
    return *outUri != ZR_NULL;
}

SZrLspProjectIndex *ZrLanguageServer_LspVirtualDocumentIdentity_FindProject(
        SZrLspContext *context, SZrString *uri) {
    SZrLspVirtualDocumentIdentity identity;
    if (context == ZR_NULL ||
        !ZrLanguageServer_LspVirtualDocumentIdentity_Parse(context->state, uri, &identity) ||
        identity.providerGeneration != context->semanticSnapshotProviderGeneration) {
        return ZR_NULL;
    }
    return ZrLanguageServer_LspProject_FindProjectByProjectUri(context, identity.projectUri, ZR_NULL);
}

TZrBool ZrLanguageServer_LspVirtualDocumentIdentity_ResolveNativeDescriptor(
        SZrState *state, SZrLspContext *context, SZrString *uri,
        SZrLspVirtualDocumentIdentity *outIdentity, SZrLspProjectIndex **outProject,
        const ZrLibModuleDescriptor **outDescriptor) {
    SZrLspVirtualDocumentIdentity identity;
    SZrLspProjectIndex *project;
    SZrLspResolvedImportedModule resolved;
    SZrString *origin = ZR_NULL;
    if (outIdentity != ZR_NULL) {
        memset(outIdentity, 0, sizeof(*outIdentity));
    }
    if (outProject != ZR_NULL) {
        *outProject = ZR_NULL;
    }
    if (outDescriptor != ZR_NULL) {
        *outDescriptor = ZR_NULL;
    }
    if (context == ZR_NULL || outDescriptor == ZR_NULL ||
        !ZrLanguageServer_LspVirtualDocumentIdentity_Parse(state, uri, &identity) ||
        identity.providerGeneration != context->semanticSnapshotProviderGeneration ||
        (project = ZrLanguageServer_LspProject_FindProjectByProjectUri(context, identity.projectUri, ZR_NULL)) == ZR_NULL ||
        !identity_ensure_project_provider(state, project, identity.moduleName) ||
        !ZrLanguageServer_LspModuleMetadata_ResolveImportedModule(
                state, ZR_NULL, project, identity.moduleName, &resolved) ||
        resolved.sourceKind != ZR_LSP_IMPORTED_MODULE_SOURCE_NATIVE_DESCRIPTOR_PLUGIN ||
        resolved.nativeDescriptor == ZR_NULL ||
        !ZrLanguageServer_LspModuleMetadata_ResolveNativeModuleUri(state, project, identity.moduleName, &origin) ||
        origin == ZR_NULL || !ZrCore_String_Equal(origin, identity.originUri)) {
        return ZR_FALSE;
    }
    if (outIdentity != ZR_NULL) {
        *outIdentity = identity;
    }
    if (outProject != ZR_NULL) {
        *outProject = project;
    }
    *outDescriptor = resolved.nativeDescriptor;
    return ZR_TRUE;
}
