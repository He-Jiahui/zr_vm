#include "zr_vm_language_server_stdio_internal.h"

#include <ctype.h>

#define ZR_LSP_STDIO_HEX_DIGIT_INVALID ((TZrInt32)-1)

static void stdio_get_string_view(SZrString *value, TZrNativeString *text, TZrSize *length) {
    if (text == ZR_NULL || length == ZR_NULL) {
        return;
    }

    *text = ZR_NULL;
    *length = 0;
    if (value == ZR_NULL) {
        return;
    }

    if (value->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        *text = ZrCore_String_GetNativeStringShort(value);
        *length = value->shortStringLength;
    } else {
        *text = ZrCore_String_GetNativeString(value);
        *length = value->longStringLength;
    }
}

static TZrBool stdio_is_hex_digit_char(TZrChar value) {
    return (value >= '0' && value <= '9') ||
           (value >= 'a' && value <= 'f') ||
           (value >= 'A' && value <= 'F');
}

static TZrInt32 stdio_hex_digit_to_value(TZrChar value) {
    if (value >= '0' && value <= '9') {
        return value - '0';
    }

    if (value >= 'a' && value <= 'f') {
        return value - 'a' + 10;
    }

    if (value >= 'A' && value <= 'F') {
        return value - 'A' + 10;
    }

    return ZR_LSP_STDIO_HEX_DIGIT_INVALID;
}

static TZrBool stdio_uri_to_native_path(SZrString *uri, TZrChar *buffer, TZrSize bufferSize) {
    TZrNativeString uriText;
    TZrSize uriLength;
    TZrSize readIndex = 0;
    TZrSize writeIndex = 0;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    buffer[0] = '\0';
    stdio_get_string_view(uri, &uriText, &uriLength);
    if (uriText == ZR_NULL) {
        return ZR_FALSE;
    }

    if (uriLength >= 7 && memcmp(uriText, "file://", 7) == 0) {
        readIndex = 7;
    }

#ifdef ZR_VM_PLATFORM_IS_WIN
    if (readIndex < uriLength &&
        uriText[readIndex] == '/' &&
        readIndex + 2 < uriLength &&
        isalpha((unsigned char)uriText[readIndex + 1]) &&
        uriText[readIndex + 2] == ':') {
        readIndex++;
    }
#endif

    while (readIndex < uriLength && writeIndex + 1 < bufferSize) {
        TZrChar current = uriText[readIndex];
        if (current == '%' &&
            readIndex + 2 < uriLength &&
            stdio_is_hex_digit_char(uriText[readIndex + 1]) &&
            stdio_is_hex_digit_char(uriText[readIndex + 2])) {
            buffer[writeIndex++] = (TZrChar)((stdio_hex_digit_to_value(uriText[readIndex + 1]) << 4) |
                                             stdio_hex_digit_to_value(uriText[readIndex + 2]));
            readIndex += 3;
            continue;
        }

#ifdef ZR_VM_PLATFORM_IS_WIN
        buffer[writeIndex++] = current == '/' ? '\\' : current;
#else
        buffer[writeIndex++] = current;
#endif
        readIndex++;
    }

    buffer[writeIndex] = '\0';
    return writeIndex > 0;
}

static char *stdio_read_all_text(const TZrChar *nativePath, size_t *outLength) {
    FILE *file;
    long rawSize;
    size_t readLength;
    char *buffer;

    if (nativePath == ZR_NULL || outLength == NULL) {
        return NULL;
    }

    *outLength = 0;
    file = fopen(nativePath, "rb");
    if (file == NULL) {
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    rawSize = ftell(file);
    if (rawSize < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    buffer = (char *)malloc((size_t)rawSize + 1);
    if (buffer == NULL) {
        fclose(file);
        return NULL;
    }

    readLength = rawSize > 0 ? fread(buffer, 1, (size_t)rawSize, file) : 0;
    fclose(file);
    if (rawSize > 0 && readLength != (size_t)rawSize) {
        free(buffer);
        return NULL;
    }

    buffer[readLength] = '\0';
    *outLength = readLength;
    return buffer;
}

char *read_document_text_from_uri(SZrString *uri, size_t *outLength) {
    TZrChar nativePath[ZR_VM_PATH_LENGTH_MAX];

    if (!stdio_uri_to_native_path(uri, nativePath, sizeof(nativePath))) {
        if (outLength != NULL) {
            *outLength = 0;
        }
        return NULL;
    }

    return stdio_read_all_text(nativePath, outLength);
}
