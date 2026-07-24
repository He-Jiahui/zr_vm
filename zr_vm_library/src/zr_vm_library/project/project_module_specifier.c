#include "zr_vm_library/project.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static void module_specifier_set_error(TZrChar *errorBuffer,
                                       TZrSize errorBufferSize,
                                       const TZrChar *format,
                                       ...) {
    va_list arguments;

    if (errorBuffer == ZR_NULL || errorBufferSize == 0u) {
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

static TZrBool module_specifier_copy_text(const TZrChar *text,
                                          TZrChar *buffer,
                                          TZrSize bufferSize) {
    TZrSize length;

    if (text == ZR_NULL || buffer == ZR_NULL || bufferSize == 0u) {
        return ZR_FALSE;
    }

    length = strlen(text);
    if (length + 1u > bufferSize) {
        return ZR_FALSE;
    }

    memcpy(buffer, text, length + 1u);
    return ZR_TRUE;
}

static TZrBool module_specifier_is_identifier_start(TZrChar value) {
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') || value == '_';
}

static TZrBool module_specifier_is_identifier_continue(TZrChar value) {
    return module_specifier_is_identifier_start(value) || (value >= '0' && value <= '9');
}

static TZrBool module_specifier_is_segment_separator(TZrChar value) {
    return value == '.' || value == '/';
}

static TZrBool module_specifier_copy_segments(const TZrChar *text,
                                               TZrChar *buffer,
                                               TZrSize bufferSize) {
    TZrSize readIndex = 0u;
    TZrSize writeIndex = 0u;
    TZrBool needsSegmentStart = ZR_TRUE;

    if (text == ZR_NULL || buffer == ZR_NULL || bufferSize == 0u || text[0] == '\0') {
        return ZR_FALSE;
    }

    while (text[readIndex] != '\0') {
        TZrChar value = text[readIndex++];

        if (module_specifier_is_segment_separator(value)) {
            if (needsSegmentStart || writeIndex + 1u >= bufferSize) {
                return ZR_FALSE;
            }
            buffer[writeIndex++] = '.';
            needsSegmentStart = ZR_TRUE;
            continue;
        }

        if ((needsSegmentStart && !module_specifier_is_identifier_start(value)) ||
            (!needsSegmentStart && !module_specifier_is_identifier_continue(value)) ||
            writeIndex + 1u >= bufferSize) {
            return ZR_FALSE;
        }

        buffer[writeIndex++] = value;
        needsSegmentStart = ZR_FALSE;
    }

    if (needsSegmentStart) {
        return ZR_FALSE;
    }

    buffer[writeIndex] = '\0';
    return ZR_TRUE;
}

static TZrBool module_specifier_split_root(const TZrChar *segments,
                                           TZrChar *root,
                                           TZrSize rootSize,
                                           TZrChar *suffix,
                                           TZrSize suffixSize) {
    const TZrChar *separator;
    TZrSize rootLength;

    if (segments == ZR_NULL || root == ZR_NULL || suffix == ZR_NULL || rootSize == 0u || suffixSize == 0u) {
        return ZR_FALSE;
    }

    separator = strchr(segments, '.');
    rootLength = separator == ZR_NULL ? strlen(segments) : (TZrSize)(separator - segments);
    if (rootLength == 0u || rootLength + 1u > rootSize) {
        return ZR_FALSE;
    }

    memcpy(root, segments, rootLength);
    root[rootLength] = '\0';
    return module_specifier_copy_text(separator == ZR_NULL ? "" : separator + 1, suffix, suffixSize);
}

static TZrBool module_specifier_is_official_native_literal(const TZrChar *literal) {
    return strcmp(literal, "zr") == 0 || strncmp(literal, "zr.", 3u) == 0 || strncmp(literal, "zr/", 3u) == 0;
}

static TZrBool module_specifier_is_file_locator(const TZrChar *literal) {
    const TZrChar *locator;
    const TZrChar *authorityEnd;

    if (literal == ZR_NULL || strncmp(literal, "file:", 5u) != 0 || strchr(literal, '\\') != ZR_NULL) {
        return ZR_FALSE;
    }

    locator = literal + 5u;
    if (strncmp(locator, "///", 3u) == 0) {
        return locator[3] != '\0' && locator[3] != '/';
    }
    if (strncmp(locator, "//", 2u) != 0 || locator[2] == '\0' || locator[2] == '/') {
        return ZR_FALSE;
    }

    authorityEnd = strchr(locator + 2u, '/');
    return authorityEnd != ZR_NULL && authorityEnd != locator + 2u && authorityEnd[1] != '\0' &&
           authorityEnd[1] != '/';
}

static TZrBool module_specifier_identity_is_valid(const SZrLibrary_ModuleIdentity *identity) {
    if (identity == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (identity->domain) {
    case ZR_LIBRARY_MODULE_DOMAIN_PACKAGE:
        return identity->packageName[0] != '\0';
    case ZR_LIBRARY_MODULE_DOMAIN_OFFICIAL_NATIVE:
    case ZR_LIBRARY_MODULE_DOMAIN_REGISTERED_NATIVE:
    case ZR_LIBRARY_MODULE_DOMAIN_WORKSPACE:
        return identity->segments[0] != '\0' && identity->packageName[0] == '\0';
    default:
        return ZR_FALSE;
    }
}

static TZrBool module_specifier_parse_relative(const TZrChar *literal,
                                               SZrLibrary_ModuleSpecifier *outSpecifier) {
    const TZrChar *segments = literal;
    TZrSize dotCount = 0u;

    outSpecifier->kind = ZR_LIBRARY_MODULE_SPECIFIER_KIND_RELATIVE;
    while (segments[0] == '.' && segments[1] == '.' && segments[2] == '/') {
        outSpecifier->relativeParentLevels++;
        segments += 3u;
    }

    if (outSpecifier->relativeParentLevels == 0u) {
        while (segments[dotCount] == '.') {
            dotCount++;
        }
        if (dotCount == 0u) {
            return ZR_FALSE;
        }
        outSpecifier->relativeParentLevels = dotCount - 1u;
        segments += dotCount;
        if (segments[0] == '/') {
            segments++;
        }
    } else if (segments[0] == '.') {
        return ZR_FALSE;
    }

    return module_specifier_copy_segments(segments, outSpecifier->identity.segments,
                                          sizeof(outSpecifier->identity.segments));
}

static TZrBool module_specifier_parse_alias(const TZrChar *literal,
                                            SZrLibrary_ModuleSpecifier *outSpecifier) {
    TZrChar segments[ZR_LIBRARY_MAX_PATH_LENGTH];

    if (!module_specifier_copy_segments(literal + 1u, segments, sizeof(segments)) ||
        !module_specifier_split_root(segments,
                                     outSpecifier->aliasRoot,
                                     sizeof(outSpecifier->aliasRoot),
                                     outSpecifier->identity.segments,
                                     sizeof(outSpecifier->identity.segments))) {
        return ZR_FALSE;
    }

    outSpecifier->kind = ZR_LIBRARY_MODULE_SPECIFIER_KIND_ALIAS;
    return ZR_TRUE;
}

static TZrBool module_specifier_parse_package(const TZrChar *literal,
                                              SZrLibrary_ModuleSpecifier *outSpecifier) {
    TZrChar segments[ZR_LIBRARY_MAX_PATH_LENGTH];

    if (!module_specifier_copy_segments(literal + 1u, segments, sizeof(segments)) ||
        !module_specifier_split_root(segments,
                                     outSpecifier->identity.packageName,
                                     sizeof(outSpecifier->identity.packageName),
                                     outSpecifier->identity.segments,
                                     sizeof(outSpecifier->identity.segments))) {
        return ZR_FALSE;
    }

    outSpecifier->kind = ZR_LIBRARY_MODULE_SPECIFIER_KIND_PACKAGE;
    outSpecifier->identity.domain = ZR_LIBRARY_MODULE_DOMAIN_PACKAGE;
    return ZR_TRUE;
}

ZR_LIBRARY_API TZrBool ZrLibrary_ModuleSpecifier_Parse(const TZrChar *literal,
                                                        SZrLibrary_ModuleSpecifier *outSpecifier,
                                                        TZrChar *errorBuffer,
                                                        TZrSize errorBufferSize) {
    const TZrChar *segments;

    if (outSpecifier == ZR_NULL) {
        module_specifier_set_error(errorBuffer, errorBufferSize, "module specifier output is required");
        return ZR_FALSE;
    }

    memset(outSpecifier, 0, sizeof(*outSpecifier));
    if (literal == ZR_NULL || literal[0] == '\0') {
        module_specifier_set_error(errorBuffer, errorBufferSize, "module specifier cannot be empty");
        return ZR_FALSE;
    }

    if (strncmp(literal, "file:", 5u) == 0) {
        if (!module_specifier_is_file_locator(literal) ||
            !module_specifier_copy_text(literal, outSpecifier->locator, sizeof(outSpecifier->locator))) {
            module_specifier_set_error(errorBuffer, errorBufferSize, "invalid canonical file locator '%s'", literal);
            return ZR_FALSE;
        }
        outSpecifier->kind = ZR_LIBRARY_MODULE_SPECIFIER_KIND_FILE;
        return ZR_TRUE;
    }

    if (strncmp(literal, "native:", 7u) == 0) {
        segments = literal + 7u;
        if (!module_specifier_copy_segments(segments, outSpecifier->identity.segments,
                                            sizeof(outSpecifier->identity.segments)) ||
            module_specifier_is_official_native_literal(segments)) {
            module_specifier_set_error(errorBuffer, errorBufferSize, "invalid registered native specifier '%s'", literal);
            return ZR_FALSE;
        }
        outSpecifier->kind = ZR_LIBRARY_MODULE_SPECIFIER_KIND_REGISTERED_NATIVE;
        outSpecifier->identity.domain = ZR_LIBRARY_MODULE_DOMAIN_REGISTERED_NATIVE;
        return ZR_TRUE;
    }

    if (literal[0] == '.') {
        if (!module_specifier_parse_relative(literal, outSpecifier)) {
            module_specifier_set_error(errorBuffer, errorBufferSize, "invalid relative module specifier '%s'", literal);
            return ZR_FALSE;
        }
        return ZR_TRUE;
    }

    if (literal[0] == '#') {
        if (!module_specifier_parse_alias(literal, outSpecifier)) {
            module_specifier_set_error(errorBuffer, errorBufferSize, "invalid module alias '%s'", literal);
            return ZR_FALSE;
        }
        return ZR_TRUE;
    }

    if (literal[0] == '@') {
        if (!module_specifier_parse_package(literal, outSpecifier)) {
            module_specifier_set_error(errorBuffer, errorBufferSize, "invalid package module specifier '%s'", literal);
            return ZR_FALSE;
        }
        return ZR_TRUE;
    }

    if (!module_specifier_copy_segments(literal, outSpecifier->identity.segments,
                                        sizeof(outSpecifier->identity.segments))) {
        module_specifier_set_error(errorBuffer, errorBufferSize, "invalid workspace module specifier '%s'", literal);
        return ZR_FALSE;
    }

    if (module_specifier_is_official_native_literal(literal)) {
        outSpecifier->kind = ZR_LIBRARY_MODULE_SPECIFIER_KIND_OFFICIAL_NATIVE;
        outSpecifier->identity.domain = ZR_LIBRARY_MODULE_DOMAIN_OFFICIAL_NATIVE;
    } else {
        outSpecifier->kind = ZR_LIBRARY_MODULE_SPECIFIER_KIND_WORKSPACE;
        outSpecifier->identity.domain = ZR_LIBRARY_MODULE_DOMAIN_WORKSPACE;
    }
    return ZR_TRUE;
}

ZR_LIBRARY_API TZrBool ZrLibrary_ModuleIdentity_Equals(const SZrLibrary_ModuleIdentity *lhs,
                                                        const SZrLibrary_ModuleIdentity *rhs) {
    return module_specifier_identity_is_valid(lhs) && module_specifier_identity_is_valid(rhs) &&
           lhs->domain == rhs->domain && strcmp(lhs->segments, rhs->segments) == 0 &&
           strcmp(lhs->packageName, rhs->packageName) == 0;
}

ZR_LIBRARY_API TZrBool ZrLibrary_ModuleSpecifier_ResolveRelative(
        const SZrLibrary_ModuleIdentity *currentIdentity,
        const SZrLibrary_ModuleSpecifier *relativeSpecifier,
        SZrLibrary_ModuleIdentity *outIdentity,
        TZrChar *errorBuffer,
        TZrSize errorBufferSize) {
    TZrChar parentSegments[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar currentPackageName[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar relativeSegments[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar *separator;
    EZrLibrary_ModuleDomain currentDomain;
    TZrSize parentLevel;
    int written;

    if (!module_specifier_identity_is_valid(currentIdentity) || relativeSpecifier == ZR_NULL || outIdentity == ZR_NULL ||
        (currentIdentity->domain != ZR_LIBRARY_MODULE_DOMAIN_WORKSPACE &&
         currentIdentity->domain != ZR_LIBRARY_MODULE_DOMAIN_PACKAGE) ||
        currentIdentity->segments[0] == '\0' ||
        relativeSpecifier->kind != ZR_LIBRARY_MODULE_SPECIFIER_KIND_RELATIVE ||
        relativeSpecifier->identity.segments[0] == '\0') {
        module_specifier_set_error(errorBuffer, errorBufferSize, "relative module identity inputs are invalid");
        return ZR_FALSE;
    }

    if (!module_specifier_copy_text(currentIdentity->segments, parentSegments, sizeof(parentSegments))) {
        module_specifier_set_error(errorBuffer, errorBufferSize, "current module identity is too long");
        return ZR_FALSE;
    }
    if (!module_specifier_copy_text(currentIdentity->packageName,
                                    currentPackageName,
                                    sizeof(currentPackageName)) ||
        !module_specifier_copy_text(relativeSpecifier->identity.segments,
                                    relativeSegments,
                                    sizeof(relativeSegments))) {
        module_specifier_set_error(errorBuffer, errorBufferSize, "relative module identity inputs are too long");
        return ZR_FALSE;
    }
    currentDomain = currentIdentity->domain;

    separator = strrchr(parentSegments, '.');
    if (separator == ZR_NULL) {
        parentSegments[0] = '\0';
    } else {
        *separator = '\0';
    }

    for (parentLevel = 0u; parentLevel < relativeSpecifier->relativeParentLevels; parentLevel++) {
        separator = strrchr(parentSegments, '.');
        if (separator == ZR_NULL) {
            module_specifier_set_error(errorBuffer, errorBufferSize, "relative module specifier escapes its module root");
            return ZR_FALSE;
        }
        *separator = '\0';
    }

    memset(outIdentity, 0, sizeof(*outIdentity));
    outIdentity->domain = currentDomain;
    if (!module_specifier_copy_text(currentPackageName,
                                    outIdentity->packageName,
                                    sizeof(outIdentity->packageName))) {
        module_specifier_set_error(errorBuffer, errorBufferSize, "package identity is too long");
        return ZR_FALSE;
    }

    written = snprintf(outIdentity->segments,
                       sizeof(outIdentity->segments),
                       "%s%s%s",
                       parentSegments,
                       parentSegments[0] == '\0' ? "" : ".",
                       relativeSegments);
    if (written < 0 || (TZrSize)written >= sizeof(outIdentity->segments)) {
        memset(outIdentity, 0, sizeof(*outIdentity));
        module_specifier_set_error(errorBuffer, errorBufferSize, "resolved module identity is too long");
        return ZR_FALSE;
    }

    return ZR_TRUE;
}
