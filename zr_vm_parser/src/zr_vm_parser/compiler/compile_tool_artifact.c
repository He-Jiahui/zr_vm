#include "zr_vm_parser/compile_tool.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compile_tool_content_hash.h"
#include "zr_vm_core/string.h"

#define ZR_COMPILE_TOOL_ARTIFACT_SIGNATURE UINT64_C(0x5A525F4354415254)

static void compile_tool_artifact_set_error(
        TZrChar *errorBuffer,
        TZrSize errorBufferSize,
        const TZrChar *format,
        ...) {
    va_list arguments;

    if (errorBuffer == ZR_NULL || errorBufferSize == 0U) {
        return;
    }
    errorBuffer[0] = '\0';
    if (format == ZR_NULL) {
        return;
    }
    va_start(arguments, format);
    vsnprintf(errorBuffer, errorBufferSize, format, arguments);
    va_end(arguments);
}

static TZrBool compile_tool_artifact_copy_text(
        TZrChar *destination,
        TZrSize destinationSize,
        const TZrChar *source) {
    TZrSize length;

    if (destination == ZR_NULL || destinationSize == 0U || source == ZR_NULL) {
        return ZR_FALSE;
    }
    length = strlen(source);
    if (length + 1U > destinationSize) {
        return ZR_FALSE;
    }
    memcpy(destination, source, length + 1U);
    return ZR_TRUE;
}

static TZrBool compile_tool_artifact_read_file(
        const TZrChar *path,
        TZrByte **outBytes,
        TZrSize *outByteCount) {
    FILE *file;
    long fileSize;
    TZrByte *bytes;
    size_t readSize;

    if (outBytes != ZR_NULL) {
        *outBytes = ZR_NULL;
    }
    if (outByteCount != ZR_NULL) {
        *outByteCount = 0U;
    }
    if (path == ZR_NULL || outBytes == ZR_NULL || outByteCount == ZR_NULL) {
        return ZR_FALSE;
    }
    file = fopen(path, "rb");
    if (file == ZR_NULL) {
        return ZR_FALSE;
    }
    if (fseek(file, 0, SEEK_END) != 0 ||
        (fileSize = ftell(file)) <= 0 ||
        fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return ZR_FALSE;
    }
    bytes = (TZrByte *)malloc((size_t)fileSize);
    if (bytes == ZR_NULL) {
        fclose(file);
        return ZR_FALSE;
    }
    readSize = fread(bytes, 1U, (size_t)fileSize, file);
    fclose(file);
    if (readSize != (size_t)fileSize) {
        free(bytes);
        return ZR_FALSE;
    }
    *outBytes = bytes;
    *outByteCount = (TZrSize)fileSize;
    return ZR_TRUE;
}

static TZrBool compile_tool_artifact_same_package(
        const SZrLibrary_ModuleIdentity *lhs,
        const SZrLibrary_ModuleIdentity *rhs) {
    return (TZrBool)(lhs != ZR_NULL && rhs != ZR_NULL &&
                     lhs->domain == ZR_LIBRARY_MODULE_DOMAIN_PACKAGE &&
                     rhs->domain == ZR_LIBRARY_MODULE_DOMAIN_PACKAGE &&
                     strcmp(lhs->packageName, rhs->packageName) == 0);
}

static TZrBool compile_tool_artifact_parse_semver3(
        const TZrChar *text,
        TZrUInt64 outParts[3]) {
    const TZrChar *cursor = text;

    if (text == ZR_NULL || outParts == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrSize partIndex = 0U; partIndex < 3U; partIndex++) {
        TZrUInt64 value = 0U;
        TZrSize digitCount = 0U;
        TZrChar firstDigit = *cursor;

        while (*cursor >= '0' && *cursor <= '9') {
            TZrUInt64 digit = (TZrUInt64)(*cursor - '0');
            if (value > (UINT64_MAX - digit) / UINT64_C(10)) {
                return ZR_FALSE;
            }
            value = value * UINT64_C(10) + digit;
            cursor++;
            digitCount++;
        }
        if (digitCount == 0U || (digitCount > 1U && firstDigit == '0')) {
            return ZR_FALSE;
        }
        outParts[partIndex] = value;
        if (partIndex < 2U) {
            if (*cursor != '.') {
                return ZR_FALSE;
            }
            cursor++;
        }
    }
    return (TZrBool)(*cursor == '\0');
}

static int compile_tool_artifact_compare_semver3(
        const TZrUInt64 lhs[3],
        const TZrUInt64 rhs[3]) {
    for (TZrSize index = 0U; index < 3U; index++) {
        if (lhs[index] < rhs[index]) {
            return -1;
        }
        if (lhs[index] > rhs[index]) {
            return 1;
        }
    }
    return 0;
}

static TZrBool compile_tool_artifact_version_satisfies_requirement(
        const TZrChar *resolvedVersion,
        const SZrString *versionRequirement) {
    const TZrChar *requirement;
    TZrUInt64 resolved[3];
    TZrUInt64 minimum[3];
    TZrUInt64 maximum[3];

    if (resolvedVersion == ZR_NULL || versionRequirement == ZR_NULL) {
        return ZR_FALSE;
    }
    requirement = ZrCore_String_GetNativeString((SZrString *)versionRequirement);
    if (requirement == ZR_NULL || requirement[0] == '\0') {
        return ZR_FALSE;
    }
    if (requirement[0] != '^') {
        return (TZrBool)(compile_tool_artifact_parse_semver3(
                                requirement, minimum) &&
                         compile_tool_artifact_parse_semver3(
                                resolvedVersion, resolved) &&
                         compile_tool_artifact_compare_semver3(
                                resolved, minimum) == 0);
    }
    if (!compile_tool_artifact_parse_semver3(requirement + 1, minimum) ||
        !compile_tool_artifact_parse_semver3(resolvedVersion, resolved)) {
        return ZR_FALSE;
    }
    memcpy(maximum, minimum, sizeof(maximum));
    if (minimum[0] != 0U) {
        if (maximum[0] == UINT64_MAX) {
            return ZR_FALSE;
        }
        maximum[0]++;
        maximum[1] = 0U;
        maximum[2] = 0U;
    } else if (minimum[1] != 0U) {
        if (maximum[1] == UINT64_MAX) {
            return ZR_FALSE;
        }
        maximum[1]++;
        maximum[2] = 0U;
    } else {
        if (maximum[2] == UINT64_MAX) {
            return ZR_FALSE;
        }
        maximum[2]++;
    }
    return (TZrBool)(compile_tool_artifact_compare_semver3(
                             resolved, minimum) >= 0 &&
                     compile_tool_artifact_compare_semver3(
                             resolved, maximum) < 0);
}

static TZrBool compile_tool_artifact_is_content_hash(const TZrChar *text) {
    if (text == ZR_NULL || strlen(text) != 50U ||
        strncmp(text, "sha256:", 7U) != 0) {
        return ZR_FALSE;
    }
    for (TZrSize index = 7U; index < 50U; index++) {
        TZrChar character = text[index];

        if (!((character >= 'A' && character <= 'Z') ||
              (character >= 'a' && character <= 'z') ||
              (character >= '0' && character <= '9') ||
              character == '-' || character == '_')) {
            return ZR_FALSE;
        }
    }
    return (TZrBool)(strchr("AEIMQUYcgkosw048", text[49]) != ZR_NULL);
}

static TZrBool compile_tool_artifact_hash_u64(
        SZrParserSha256Context *context,
        TZrUInt64 value) {
    TZrByte bytes[8];

    for (TZrSize index = 0U; index < sizeof(bytes); index++) {
        bytes[sizeof(bytes) - 1U - index] =
                (TZrByte)(value >> (index * 8U));
    }
    return ZrParser_Sha256_Update(context, bytes, sizeof(bytes));
}

static TZrBool compile_tool_artifact_hash_text(
        SZrParserSha256Context *context,
        const TZrChar *text) {
    TZrSize length;

    if (context == ZR_NULL || text == ZR_NULL) {
        return ZR_FALSE;
    }
    length = strlen(text);
    return (TZrBool)(compile_tool_artifact_hash_u64(
                             context, (TZrUInt64)length) &&
                     ZrParser_Sha256_Update(
                             context, (const TZrByte *)text, length));
}

static int compile_tool_artifact_compare_lock_pointer(
        const void *lhsPointer,
        const void *rhsPointer) {
    const SZrLibrary_ProjectManifestDependencyLockEntry *lhs =
            *(const SZrLibrary_ProjectManifestDependencyLockEntry *const *)
                    lhsPointer;
    const SZrLibrary_ProjectManifestDependencyLockEntry *rhs =
            *(const SZrLibrary_ProjectManifestDependencyLockEntry *const *)
                    rhsPointer;
    int comparison;

    if (lhs->packageIdentity.domain != rhs->packageIdentity.domain) {
        return lhs->packageIdentity.domain < rhs->packageIdentity.domain
                       ? -1
                       : 1;
    }
    comparison = strcmp(
            lhs->packageIdentity.packageName,
            rhs->packageIdentity.packageName);
    if (comparison != 0) {
        return comparison;
    }
    comparison = strcmp(
            lhs->packageIdentity.segments,
            rhs->packageIdentity.segments);
    if (comparison != 0) {
        return comparison;
    }
    if (lhs->providerPhase == rhs->providerPhase) {
        return 0;
    }
    return lhs->providerPhase < rhs->providerPhase ? -1 : 1;
}

static TZrBool compile_tool_artifact_lock_graph_hash(
        const SZrLibrary_ProjectManifestDependencyLockEntry *entries,
        TZrSize entryCount,
        TZrChar *outHash,
        TZrSize outHashSize,
        TZrChar *errorBuffer,
        TZrSize errorBufferSize) {
    const SZrLibrary_ProjectManifestDependencyLockEntry **sortedEntries;
    SZrParserSha256Context context;
    TZrByte digest[ZR_PARSER_SHA256_DIGEST_BYTE_COUNT];
    TZrSize compileToolCount = 0U;
    TZrBool ok = ZR_FALSE;

    if (entries == ZR_NULL || entryCount == 0U || outHash == ZR_NULL ||
        entryCount > SIZE_MAX / sizeof(*sortedEntries)) {
        return ZR_FALSE;
    }
    sortedEntries = (const SZrLibrary_ProjectManifestDependencyLockEntry **)
            calloc((size_t)entryCount, sizeof(*sortedEntries));
    if (sortedEntries == ZR_NULL) {
        compile_tool_artifact_set_error(
                errorBuffer, errorBufferSize,
                "compiletool.artifact.lock_graph allocation failed");
        return ZR_FALSE;
    }
    for (TZrSize index = 0U; index < entryCount; index++) {
        const SZrLibrary_ProjectManifestDependencyLockEntry *entry =
                &entries[index];
        TZrUInt64 versionParts[3];

        if (entry->providerPhase != ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL) {
            continue;
        }
        if (entry->packageIdentity.domain !=
                    ZR_LIBRARY_MODULE_DOMAIN_PACKAGE ||
            entry->packageIdentity.packageName[0] == '\0' ||
            entry->packageIdentity.segments[0] != '\0' ||
            entry->resolvedVersion == ZR_NULL ||
            !compile_tool_artifact_parse_semver3(
                    entry->resolvedVersion, versionParts) ||
            !compile_tool_artifact_is_content_hash(entry->contentHash) ||
            !compile_tool_artifact_is_content_hash(
                    entry->transitiveIdentity) ||
            (entry->providerSourceKind !=
                     ZR_LIBRARY_PROJECT_MANIFEST_DEPENDENCY_SOURCE_PATH &&
             entry->providerSourceKind !=
                     ZR_LIBRARY_PROJECT_MANIFEST_DEPENDENCY_SOURCE_REGISTRY &&
             entry->providerSourceKind !=
                     ZR_LIBRARY_PROJECT_MANIFEST_DEPENDENCY_SOURCE_GIT)) {
            compile_tool_artifact_set_error(
                    errorBuffer, errorBufferSize,
                    "compiletool.artifact.lock_graph contains a non-canonical CompileTool entry");
            goto cleanup;
        }
        sortedEntries[compileToolCount++] = entry;
    }
    if (compileToolCount == 0U) {
        compile_tool_artifact_set_error(
                errorBuffer, errorBufferSize,
                "compiletool.artifact.lock_graph has no CompileTool entries");
        goto cleanup;
    }
    qsort(
            sortedEntries,
            (size_t)compileToolCount,
            sizeof(*sortedEntries),
            compile_tool_artifact_compare_lock_pointer);
    for (TZrSize index = 1U; index < compileToolCount; index++) {
        if (compile_tool_artifact_compare_lock_pointer(
                    &sortedEntries[index - 1U],
                    &sortedEntries[index]) == 0) {
            compile_tool_artifact_set_error(
                    errorBuffer, errorBufferSize,
                    "compiletool.artifact.lock_graph contains duplicate CompileTool package identities");
            goto cleanup;
        }
    }

    ZrParser_Sha256_Init(&context);
    if (!compile_tool_artifact_hash_text(
                &context, "zr.compiletool.lock-graph/v1") ||
        !compile_tool_artifact_hash_u64(
                &context, (TZrUInt64)compileToolCount)) {
        goto cleanup;
    }
    for (TZrSize index = 0U; index < compileToolCount; index++) {
        const SZrLibrary_ProjectManifestDependencyLockEntry *entry =
                sortedEntries[index];

        if (!compile_tool_artifact_hash_u64(
                    &context, (TZrUInt64)entry->packageIdentity.domain) ||
            !compile_tool_artifact_hash_text(
                    &context, entry->packageIdentity.packageName) ||
            !compile_tool_artifact_hash_text(
                    &context, entry->packageIdentity.segments) ||
            !compile_tool_artifact_hash_u64(
                    &context, (TZrUInt64)entry->providerPhase) ||
            !compile_tool_artifact_hash_u64(
                    &context, (TZrUInt64)entry->providerSourceKind) ||
            !compile_tool_artifact_hash_text(
                    &context, entry->resolvedVersion) ||
            !compile_tool_artifact_hash_text(
                    &context, entry->contentHash) ||
            !compile_tool_artifact_hash_text(
                    &context, entry->transitiveIdentity)) {
            goto cleanup;
        }
    }
    ZrParser_Sha256_Final(&context, digest);
    ok = ZrParser_Sha256_FormatDigest(digest, outHash, outHashSize);

cleanup:
    free(sortedEntries);
    return ok;
}

static const SZrLibrary_ProjectManifestDependency *
compile_tool_artifact_find_build_dependency(
        const SZrLibrary_Project *project,
        const SZrLibrary_ModuleIdentity *packageIdentity) {
    const SZrLibrary_ProjectManifestDependency *match = ZR_NULL;

    for (TZrSize index = 0U;
         project != ZR_NULL && index < project->manifestBuildDependencyCount;
         index++) {
        const SZrLibrary_ProjectManifestDependency *candidate =
                &project->manifestBuildDependencies[index];
        if (!compile_tool_artifact_same_package(
                    &candidate->packageIdentity, packageIdentity)) {
            continue;
        }
        if (match != ZR_NULL) {
            return ZR_NULL;
        }
        match = candidate;
    }
    return match;
}

static const SZrLibrary_ProjectManifestDependencyLockEntry *
compile_tool_artifact_find_lock_entry(
        const SZrLibrary_ProjectManifestDependencyLockEntry *entries,
        TZrSize entryCount,
        const SZrLibrary_ModuleIdentity *packageIdentity,
        TZrBool *outHasWrongPhase,
        TZrBool *outDuplicate) {
    const SZrLibrary_ProjectManifestDependencyLockEntry *match = ZR_NULL;

    *outHasWrongPhase = ZR_FALSE;
    *outDuplicate = ZR_FALSE;
    for (TZrSize index = 0U; entries != ZR_NULL && index < entryCount; index++) {
        const SZrLibrary_ProjectManifestDependencyLockEntry *candidate =
                &entries[index];
        if (!compile_tool_artifact_same_package(
                    &candidate->packageIdentity, packageIdentity)) {
            continue;
        }
        if (candidate->providerPhase != ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL) {
            *outHasWrongPhase = ZR_TRUE;
            continue;
        }
        if (match != ZR_NULL) {
            *outDuplicate = ZR_TRUE;
            return ZR_NULL;
        }
        match = candidate;
    }
    return match;
}

static TZrBool compile_tool_artifact_module_key(
        const SZrLibrary_ModuleSpecifier *specifier,
        const SZrLibrary_ZrmArchive *archive,
        TZrChar *outModuleKey,
        TZrSize outModuleKeySize) {
    const TZrChar *segments;
    TZrSize length;

    if (specifier == ZR_NULL || archive == ZR_NULL || outModuleKey == ZR_NULL ||
        outModuleKeySize == 0U) {
        return ZR_FALSE;
    }
    segments = specifier->identity.segments;
    if (segments[0] == '\0') {
        return compile_tool_artifact_copy_text(
                outModuleKey, outModuleKeySize, archive->entryModule);
    }
    length = strlen(segments);
    if (length + 1U > outModuleKeySize) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0U; index <= length; index++) {
        outModuleKey[index] = segments[index] == '.' ? '/' : segments[index];
    }
    return ZR_TRUE;
}

TZrBool ZrParser_CompileToolArtifact_IsOpen(
        const SZrParserCompileToolResolvedArtifact *artifact) {
    return (TZrBool)(artifact != ZR_NULL &&
                     artifact->signature == ZR_COMPILE_TOOL_ARTIFACT_SIGNATURE &&
                     artifact->archive.zipHandle != ZR_NULL &&
                     artifact->archiveBytes != ZR_NULL &&
                     artifact->archiveByteCount != 0U &&
                     artifact->entry != ZR_NULL &&
                     artifact->artifactBytes != ZR_NULL &&
                     artifact->artifactContentHash[0] != '\0');
}

void ZrParser_CompileToolArtifact_Close(
        SZrParserCompileToolResolvedArtifact *artifact) {
    if (artifact == ZR_NULL) {
        return;
    }
    if (artifact->signature == ZR_COMPILE_TOOL_ARTIFACT_SIGNATURE) {
        ZrLibrary_Zrm_FreeBytes(artifact->artifactBytes);
        ZrLibrary_Zrm_Close(&artifact->archive);
        free(artifact->archiveBytes);
    }
    memset(artifact, 0, sizeof(*artifact));
}

TZrBool ZrParser_CompileToolArtifact_OpenBuildDependency(
        const SZrLibrary_Project *project,
        const TZrChar *rawSpecifier,
        const SZrLibrary_ProjectManifestDependencyLockEntry *lockEntries,
        TZrSize lockEntryCount,
        const TZrChar *archivePath,
        SZrParserCompileToolResolvedArtifact *outArtifact,
        TZrChar *errorBuffer,
        TZrSize errorBufferSize) {
    SZrParserCompileToolResolvedArtifact artifact;
    SZrLibrary_ModuleSpecifier specifier;
    const SZrLibrary_ProjectManifestDependency *dependency;
    const SZrLibrary_ProjectManifestDependencyLockEntry *lockEntry;
    const SZrLibrary_ZrmEntryInfo *entry;
    TZrChar moduleKey[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar actualPackageHash[ZR_PARSER_COMPILE_TOOL_CONTENT_HASH_BUFFER_LENGTH];
    TZrChar actualArtifactHash[ZR_PARSER_COMPILE_TOOL_CONTENT_HASH_BUFFER_LENGTH];
    TZrChar actualLockGraphHash[ZR_PARSER_COMPILE_TOOL_CONTENT_HASH_BUFFER_LENGTH];
    TZrBool hasWrongPhase;
    TZrBool duplicateLock;
    TZrBool ok = ZR_FALSE;

    memset(&artifact, 0, sizeof(artifact));
    compile_tool_artifact_set_error(errorBuffer, errorBufferSize, ZR_NULL);
    if (outArtifact != ZR_NULL) {
        memset(outArtifact, 0, sizeof(*outArtifact));
    }
    if (project == ZR_NULL || project->manifestVersion != 2U ||
        rawSpecifier == ZR_NULL || lockEntries == ZR_NULL ||
        lockEntryCount == 0U || archivePath == ZR_NULL ||
        outArtifact == ZR_NULL) {
        compile_tool_artifact_set_error(
                errorBuffer, errorBufferSize,
                "compiletool.artifact.invalid_argument: v2 project, lock graph, archive path, and output are required");
        return ZR_FALSE;
    }

    if (!ZrLibrary_ModuleSpecifier_Parse(
                rawSpecifier, &specifier, errorBuffer, errorBufferSize) ||
        specifier.kind != ZR_LIBRARY_MODULE_SPECIFIER_KIND_PACKAGE) {
        compile_tool_artifact_set_error(
                errorBuffer, errorBufferSize,
                "compiletool.artifact.specifier: build dependency import must use @package syntax");
        return ZR_FALSE;
    }
    dependency = compile_tool_artifact_find_build_dependency(
            project, &specifier.identity);
    if (dependency == ZR_NULL) {
        compile_tool_artifact_set_error(
                errorBuffer, errorBufferSize,
                "compiletool.artifact.build_dependency: package is not uniquely declared in buildDependencies");
        return ZR_FALSE;
    }
    lockEntry = compile_tool_artifact_find_lock_entry(
            lockEntries,
            lockEntryCount,
            &specifier.identity,
            &hasWrongPhase,
            &duplicateLock);
    if (duplicateLock) {
        compile_tool_artifact_set_error(
                errorBuffer, errorBufferSize,
                "compiletool.artifact.lock_duplicate: duplicate CompileTool lock entries");
        return ZR_FALSE;
    }
    if (lockEntry == ZR_NULL) {
        compile_tool_artifact_set_error(
                errorBuffer, errorBufferSize,
                hasWrongPhase
                        ? "compiletool.artifact.lock phase mismatch: build dependency requires CompileTool"
                        : "compiletool.artifact.lock_missing: build dependency has no CompileTool lock entry");
        return ZR_FALSE;
    }
    if (lockEntry->providerSourceKind != dependency->sourceKind ||
        lockEntry->resolvedVersion == ZR_NULL ||
        lockEntry->resolvedVersion[0] == '\0' ||
        lockEntry->contentHash == ZR_NULL ||
        lockEntry->contentHash[0] == '\0' ||
        lockEntry->transitiveIdentity == ZR_NULL ||
        lockEntry->transitiveIdentity[0] == '\0') {
        compile_tool_artifact_set_error(
                errorBuffer, errorBufferSize,
                "compiletool.artifact.lock_contract: source, version, content hash, and transitive identity are required");
        return ZR_FALSE;
    }
    if (!compile_tool_artifact_lock_graph_hash(
                lockEntries,
                lockEntryCount,
                actualLockGraphHash,
                sizeof(actualLockGraphHash),
                errorBuffer,
                errorBufferSize)) {
        return ZR_FALSE;
    }
    if (!compile_tool_artifact_version_satisfies_requirement(
                lockEntry->resolvedVersion, dependency->versionRequirement)) {
        compile_tool_artifact_set_error(
                errorBuffer, errorBufferSize,
                "compiletool.artifact.version requirement does not admit the locked version");
        return ZR_FALSE;
    }
    if (!compile_tool_artifact_read_file(
                archivePath,
                &artifact.archiveBytes,
                &artifact.archiveByteCount) ||
        !ZrParser_CompileToolContentHash_Bytes(
                artifact.archiveBytes,
                artifact.archiveByteCount,
                actualPackageHash,
                sizeof(actualPackageHash)) ||
        strcmp(actualPackageHash, lockEntry->contentHash) != 0) {
        compile_tool_artifact_set_error(
                errorBuffer, errorBufferSize,
                "compiletool.artifact.package content hash mismatch");
        goto cleanup;
    }
    if (!ZrLibrary_Zrm_OpenBytes(
                artifact.archiveBytes,
                artifact.archiveByteCount,
                archivePath,
                &artifact.archive,
                errorBuffer,
                errorBufferSize)) {
        goto cleanup;
    }
    if (artifact.archive.providerPhase != ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL) {
        compile_tool_artifact_set_error(
                errorBuffer, errorBufferSize,
                "compiletool.artifact.provider phase mismatch: archive is not CompileTool");
        goto cleanup;
    }
    if (strcmp(
                artifact.archive.assemblyName,
                specifier.identity.packageName) != 0) {
        compile_tool_artifact_set_error(
                errorBuffer, errorBufferSize,
                "compiletool.artifact.package identity mismatch between import and archive");
        goto cleanup;
    }
    if (strcmp(artifact.archive.assemblyVersion, lockEntry->resolvedVersion) != 0) {
        compile_tool_artifact_set_error(
                errorBuffer, errorBufferSize,
                "compiletool.artifact.version mismatch between lock and archive");
        goto cleanup;
    }
    if (artifact.archive.publicContractHash[0] == '\0') {
        compile_tool_artifact_set_error(
                errorBuffer, errorBufferSize,
                "compiletool.artifact.public contract hash is required");
        goto cleanup;
    }
    if (!compile_tool_artifact_module_key(
                &specifier, &artifact.archive, moduleKey, sizeof(moduleKey))) {
        compile_tool_artifact_set_error(
                errorBuffer, errorBufferSize,
                "compiletool.artifact.module identity is too long");
        goto cleanup;
    }
    entry = ZrLibrary_Zrm_FindModule(&artifact.archive, moduleKey);
    if (entry == ZR_NULL) {
        compile_tool_artifact_set_error(
                errorBuffer, errorBufferSize,
                "compiletool.artifact.module is not exported by the archive");
        goto cleanup;
    }
    if (!ZrLibrary_Zrm_ReadEntry(
                &artifact.archive,
                entry->entryName,
                &artifact.artifactBytes,
                &artifact.artifactByteCount,
                errorBuffer,
                errorBufferSize)) {
        goto cleanup;
    }
    if (artifact.artifactBytes == ZR_NULL || artifact.artifactByteCount == 0U) {
        compile_tool_artifact_set_error(
                errorBuffer, errorBufferSize,
                "compiletool.artifact.module entry is empty");
        goto cleanup;
    }
    if (!ZrParser_CompileToolContentHash_Bytes(
            artifact.artifactBytes,
            artifact.artifactByteCount,
            actualArtifactHash,
            sizeof(actualArtifactHash))) {
        compile_tool_artifact_set_error(
                errorBuffer, errorBufferSize,
                "compiletool.artifact.failed to hash module entry");
        goto cleanup;
    }
    if (entry->hash[0] == '\0' ||
        strcmp(entry->hash, actualArtifactHash) != 0) {
        compile_tool_artifact_set_error(
                errorBuffer, errorBufferSize,
                "compiletool.artifact.artifact content hash mismatch");
        goto cleanup;
    }
    artifact.signature = ZR_COMPILE_TOOL_ARTIFACT_SIGNATURE;
    artifact.moduleIdentity = specifier.identity;
    artifact.providerSourceKind = dependency->sourceKind;
    artifact.providerPhase = artifact.archive.providerPhase;
    artifact.entry = entry;
    if (!compile_tool_artifact_copy_text(
                artifact.resolvedVersion,
                sizeof(artifact.resolvedVersion),
                lockEntry->resolvedVersion) ||
        !compile_tool_artifact_copy_text(
                artifact.packageContentHash,
                sizeof(artifact.packageContentHash),
                actualPackageHash) ||
        !compile_tool_artifact_copy_text(
                artifact.lockGraphHash,
                sizeof(artifact.lockGraphHash),
                actualLockGraphHash) ||
        !compile_tool_artifact_copy_text(
                artifact.publicContractHash,
                sizeof(artifact.publicContractHash),
                artifact.archive.publicContractHash) ||
        !compile_tool_artifact_copy_text(
                artifact.artifactEntry,
                sizeof(artifact.artifactEntry),
                entry->entryName) ||
        !compile_tool_artifact_copy_text(
                artifact.artifactContentHash,
                sizeof(artifact.artifactContentHash),
                actualArtifactHash)) {
        compile_tool_artifact_set_error(
                errorBuffer, errorBufferSize,
                "compiletool.artifact.identity field exceeds its canonical bound");
        goto cleanup;
    }

    *outArtifact = artifact;
    ok = ZR_TRUE;

cleanup:
    if (!ok) {
        ZrLibrary_Zrm_FreeBytes(artifact.artifactBytes);
        ZrLibrary_Zrm_Close(&artifact.archive);
        free(artifact.archiveBytes);
        memset(outArtifact, 0, sizeof(*outArtifact));
    }
    return ok;
}

TZrBool ZrParser_CompileToolArtifact_OpenProjectBuildDependency(
        const SZrLibrary_Project *project,
        const TZrChar *rawSpecifier,
        const TZrChar *archivePath,
        SZrParserCompileToolResolvedArtifact *outArtifact,
        TZrChar *errorBuffer,
        TZrSize errorBufferSize) {
    return ZrParser_CompileToolArtifact_OpenBuildDependency(
            project,
            rawSpecifier,
            project != ZR_NULL
                    ? project->manifestDependencyLockEntries
                    : ZR_NULL,
            project != ZR_NULL
                    ? project->manifestDependencyLockEntryCount
                    : 0u,
            archivePath,
            outArtifact,
            errorBuffer,
            errorBufferSize);
}
