//
// Canonical URI codec used at the LSP/native-I/O boundary.
//

#include <ctype.h>
#include <string.h>

#include "zr_vm_language_server/lsp_uri.h"

static const TZrChar *lsp_uri_string_text(SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_NULL;
    }

    return value->shortStringLength < ZR_VM_LONG_STRING_FLAG
               ? ZrCore_String_GetNativeStringShort(value)
               : ZrCore_String_GetNativeString(value);
}

static TZrBool lsp_uri_ascii_equals(const TZrChar *text,
                                    TZrSize textLength,
                                    const TZrChar *expected) {
    TZrSize index;
    TZrSize expectedLength;

    if (text == ZR_NULL || expected == ZR_NULL) {
        return ZR_FALSE;
    }

    expectedLength = strlen(expected);
    if (textLength != expectedLength) {
        return ZR_FALSE;
    }

    for (index = 0; index < textLength; index++) {
        if (tolower((unsigned char)text[index]) != tolower((unsigned char)expected[index])) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrInt32 lsp_uri_hex_digit(TZrChar value) {
    if (value >= '0' && value <= '9') {
        return value - '0';
    }
    value = (TZrChar)tolower((unsigned char)value);
    return value >= 'a' && value <= 'f' ? value - 'a' + 10 : -1;
}

static TZrBool lsp_uri_append_byte(TZrChar *buffer,
                                   TZrSize bufferSize,
                                   TZrSize *ioLength,
                                   TZrChar value) {
    if (buffer == ZR_NULL || ioLength == ZR_NULL || *ioLength + 1 >= bufferSize) {
        return ZR_FALSE;
    }
    buffer[(*ioLength)++] = value;
    buffer[*ioLength] = '\0';
    return ZR_TRUE;
}

static TZrBool lsp_uri_append_text(TZrChar *buffer,
                                   TZrSize bufferSize,
                                   TZrSize *ioLength,
                                   const TZrChar *text,
                                   TZrSize textLength) {
    TZrSize index;

    for (index = 0; index < textLength; index++) {
        if (!lsp_uri_append_byte(buffer, bufferSize, ioLength, text[index])) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool lsp_uri_is_local_authority(const TZrChar *authority, TZrSize authorityLength) {
    return authorityLength == 0 || lsp_uri_ascii_equals(authority, authorityLength, "localhost");
}

static TZrBool lsp_uri_decode_path(const TZrChar *text,
                                   TZrSize start,
                                   TZrSize length,
                                   TZrChar *buffer,
                                   TZrSize bufferSize,
                                   TZrSize *ioLength) {
    TZrSize index;

    if (text == ZR_NULL || buffer == ZR_NULL || ioLength == ZR_NULL || start >= length ||
        text[start] != '/') {
        return ZR_FALSE;
    }

    for (index = start; index < length;) {
        TZrChar value = text[index++];

        if ((unsigned char)value < 0x20 || value == 0x7F || value == '?' || value == '#' || value == '\\') {
            return ZR_FALSE;
        }
        if (value == '%') {
            TZrInt32 high;
            TZrInt32 low;

            if (index + 1 >= length) {
                return ZR_FALSE;
            }
            high = lsp_uri_hex_digit(text[index]);
            low = lsp_uri_hex_digit(text[index + 1]);
            if (high < 0 || low < 0) {
                return ZR_FALSE;
            }
            value = (TZrChar)((high << 4) | low);
            index += 2;
            if (value == '\0' || value == '/' || value == '\\') {
                return ZR_FALSE;
            }
        }

#ifdef ZR_VM_PLATFORM_IS_WIN
        if (value == '/') {
            value = '\\';
        }
#endif
        if (!lsp_uri_append_byte(buffer, bufferSize, ioLength, value)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool lsp_uri_native_path_is_absolute(const TZrChar *path) {
    if (path == ZR_NULL || path[0] == '\0') {
        return ZR_FALSE;
    }
#ifdef ZR_VM_PLATFORM_IS_WIN
    return (path[1] != '\0' && path[2] != '\0' && isalpha((unsigned char)path[0]) && path[1] == ':' &&
            (path[2] == '/' || path[2] == '\\')) ||
           (path[0] == '\\' && path[1] == '\\');
#else
    return path[0] == '/';
#endif
}

static TZrBool lsp_uri_native_path_is_separator(TZrChar value) {
#ifdef ZR_VM_PLATFORM_IS_WIN
    return value == '/' || value == '\\';
#else
    return value == '/';
#endif
}

static TZrBool lsp_uri_native_path_equivalent(const TZrChar *left, const TZrChar *right) {
    TZrChar normalizedLeft[ZR_VM_PATH_LENGTH_MAX];
    TZrChar normalizedRight[ZR_VM_PATH_LENGTH_MAX];
    TZrSize leftLength = 0;
    TZrSize rightLength = 0;
    TZrSize index;

    if (left == ZR_NULL || right == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; left[index] != '\0';) {
        TZrSize segmentStart;
        TZrSize segmentLength;

        if (lsp_uri_native_path_is_separator(left[index])) {
            if (leftLength == 0 && !lsp_uri_append_byte(normalizedLeft, sizeof(normalizedLeft), &leftLength, '/')) {
                return ZR_FALSE;
            }
            index++;
            continue;
        }
        segmentStart = index;
        while (left[index] != '\0' && !lsp_uri_native_path_is_separator(left[index])) {
            index++;
        }
        segmentLength = index - segmentStart;
        if (segmentLength == 1 && left[segmentStart] == '.') {
            continue;
        }
        if (segmentLength == 2 && left[segmentStart] == '.' && left[segmentStart + 1] == '.') {
            while (leftLength > 1 && lsp_uri_native_path_is_separator(normalizedLeft[leftLength - 1])) {
                leftLength--;
            }
            while (leftLength > 1 && !lsp_uri_native_path_is_separator(normalizedLeft[leftLength - 1])) {
                leftLength--;
            }
            normalizedLeft[leftLength] = '\0';
            continue;
        }
        if (leftLength > 0 && !lsp_uri_native_path_is_separator(normalizedLeft[leftLength - 1]) &&
            !lsp_uri_append_byte(normalizedLeft, sizeof(normalizedLeft), &leftLength, '/')) {
            return ZR_FALSE;
        }
        if (!lsp_uri_append_text(normalizedLeft, sizeof(normalizedLeft), &leftLength,
                                 left + segmentStart, segmentLength)) {
            return ZR_FALSE;
        }
    }

    for (index = 0; right[index] != '\0';) {
        TZrSize segmentStart;
        TZrSize segmentLength;

        if (lsp_uri_native_path_is_separator(right[index])) {
            if (rightLength == 0 && !lsp_uri_append_byte(normalizedRight, sizeof(normalizedRight), &rightLength, '/')) {
                return ZR_FALSE;
            }
            index++;
            continue;
        }
        segmentStart = index;
        while (right[index] != '\0' && !lsp_uri_native_path_is_separator(right[index])) {
            index++;
        }
        segmentLength = index - segmentStart;
        if (segmentLength == 1 && right[segmentStart] == '.') {
            continue;
        }
        if (segmentLength == 2 && right[segmentStart] == '.' && right[segmentStart + 1] == '.') {
            while (rightLength > 1 && lsp_uri_native_path_is_separator(normalizedRight[rightLength - 1])) {
                rightLength--;
            }
            while (rightLength > 1 && !lsp_uri_native_path_is_separator(normalizedRight[rightLength - 1])) {
                rightLength--;
            }
            normalizedRight[rightLength] = '\0';
            continue;
        }
        if (rightLength > 0 && !lsp_uri_native_path_is_separator(normalizedRight[rightLength - 1]) &&
            !lsp_uri_append_byte(normalizedRight, sizeof(normalizedRight), &rightLength, '/')) {
            return ZR_FALSE;
        }
        if (!lsp_uri_append_text(normalizedRight, sizeof(normalizedRight), &rightLength,
                                 right + segmentStart, segmentLength)) {
            return ZR_FALSE;
        }
    }

#ifdef ZR_VM_PLATFORM_IS_WIN
    for (index = 0; normalizedLeft[index] != '\0'; index++) {
        normalizedLeft[index] = (TZrChar)tolower((unsigned char)normalizedLeft[index]);
    }
    for (index = 0; normalizedRight[index] != '\0'; index++) {
        normalizedRight[index] = (TZrChar)tolower((unsigned char)normalizedRight[index]);
    }
#endif
    return strcmp(normalizedLeft, normalizedRight) == 0;
}

TZrBool ZrLanguageServer_LspUri_FileToNativePath(SZrString *uri,
                                                  TZrChar *buffer,
                                                  TZrSize bufferSize) {
    const TZrChar *text = lsp_uri_string_text(uri);
    TZrSize length;
    TZrSize index;
    TZrSize authorityStart;
    TZrSize authorityLength;
    TZrSize writeLength = 0;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }
    buffer[0] = '\0';
    if (text == ZR_NULL) {
        return ZR_FALSE;
    }

    length = strlen(text);
    if (length < 6 || !lsp_uri_ascii_equals(text, 4, "file") || text[4] != ':') {
        return ZR_FALSE;
    }

    index = 5;
    authorityStart = index;
    authorityLength = 0;
    if (index + 1 < length && text[index] == '/' && text[index + 1] == '/') {
        index += 2;
        authorityStart = index;
        while (index < length && text[index] != '/') {
            if (text[index] == '?' || text[index] == '#' || text[index] == '\\') {
                return ZR_FALSE;
            }
            index++;
        }
        authorityLength = index - authorityStart;
    }
    if (index >= length || text[index] != '/') {
        return ZR_FALSE;
    }

#ifdef ZR_VM_PLATFORM_IS_WIN
    if (!lsp_uri_is_local_authority(text + authorityStart, authorityLength)) {
        if (!lsp_uri_append_text(buffer, bufferSize, &writeLength, "\\\\", 2) ||
            !lsp_uri_append_text(buffer, bufferSize, &writeLength,
                                 text + authorityStart, authorityLength)) {
            buffer[0] = '\0';
            return ZR_FALSE;
        }
    }
#else
    if (!lsp_uri_is_local_authority(text + authorityStart, authorityLength)) {
        return ZR_FALSE;
    }
#endif

    if (!lsp_uri_decode_path(text, index, length, buffer, bufferSize, &writeLength)) {
        buffer[0] = '\0';
        return ZR_FALSE;
    }

#ifdef ZR_VM_PLATFORM_IS_WIN
    if (lsp_uri_is_local_authority(text + authorityStart, authorityLength) &&
        buffer[0] == '\\' && isalpha((unsigned char)buffer[1]) && buffer[2] == ':') {
        memmove(buffer, buffer + 1, writeLength);
    }
#endif
    return lsp_uri_native_path_is_absolute(buffer);
}

static TZrBool lsp_uri_is_unreserved(TZrChar value) {
    return isalnum((unsigned char)value) || value == '-' || value == '.' || value == '_' || value == '~';
}

static TZrBool lsp_uri_append_encoded_path(TZrChar *buffer,
                                            TZrSize bufferSize,
                                            TZrSize *ioLength,
                                            const TZrChar *path) {
    static const TZrChar hex[] = "0123456789ABCDEF";
    TZrSize index;

    for (index = 0; path[index] != '\0'; index++) {
        unsigned char value = (unsigned char)path[index];

#ifdef ZR_VM_PLATFORM_IS_WIN
        if (value == '\\') {
            value = '/';
        }
#endif
        if (lsp_uri_is_unreserved((TZrChar)value) || value == '/' || value == ':') {
            if (!lsp_uri_append_byte(buffer, bufferSize, ioLength, (TZrChar)value)) {
                return ZR_FALSE;
            }
        } else if (!lsp_uri_append_byte(buffer, bufferSize, ioLength, '%') ||
                   !lsp_uri_append_byte(buffer, bufferSize, ioLength, hex[(value >> 4) & 0xF]) ||
                   !lsp_uri_append_byte(buffer, bufferSize, ioLength, hex[value & 0xF])) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

SZrString *ZrLanguageServer_LspUri_FromNativePath(SZrState *state, const TZrChar *nativePath) {
    TZrChar buffer[ZR_VM_PATH_LENGTH_MAX * 3 + 16];
    TZrSize writeLength = 0;

    if (state == ZR_NULL || !lsp_uri_native_path_is_absolute(nativePath) ||
        !lsp_uri_append_text(buffer, sizeof(buffer), &writeLength, "file://", 7)) {
        return ZR_NULL;
    }

#ifdef ZR_VM_PLATFORM_IS_WIN
    if (nativePath[0] == '\\' && nativePath[1] == '\\') {
        const TZrChar *host = nativePath + 2;
        const TZrChar *separator = host;

        while (*separator != '\0' && *separator != '\\' && *separator != '/') {
            separator++;
        }
        if (separator == host || *separator == '\0' ||
            !lsp_uri_append_text(buffer, sizeof(buffer), &writeLength, host, (TZrSize)(separator - host))) {
            return ZR_NULL;
        }
        nativePath = separator;
    } else if (!lsp_uri_append_byte(buffer, sizeof(buffer), &writeLength, '/')) {
        return ZR_NULL;
    }
#endif

    if (!lsp_uri_append_encoded_path(buffer, sizeof(buffer), &writeLength, nativePath)) {
        return ZR_NULL;
    }
    return ZrCore_String_Create(state, buffer, writeLength);
}

TZrBool ZrLanguageServer_LspUri_Equivalent(SZrString *left, SZrString *right) {
    const TZrChar *leftText = lsp_uri_string_text(left);
    const TZrChar *rightText = lsp_uri_string_text(right);
    TZrChar leftPath[ZR_VM_PATH_LENGTH_MAX];
    TZrChar rightPath[ZR_VM_PATH_LENGTH_MAX];

    if (leftText == ZR_NULL || rightText == ZR_NULL) {
        return ZR_FALSE;
    }
    if (strcmp(leftText, rightText) == 0) {
        return ZR_TRUE;
    }
    return ZrLanguageServer_LspUri_FileToNativePath(left, leftPath, sizeof(leftPath)) &&
           ZrLanguageServer_LspUri_FileToNativePath(right, rightPath, sizeof(rightPath)) &&
           lsp_uri_native_path_equivalent(leftPath, rightPath);
}
