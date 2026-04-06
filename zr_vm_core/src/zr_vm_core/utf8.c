//
// Core UTF-8 helpers backed by utf8proc.
//

#include "zr_vm_core/utf8.h"

#include "utf8proc/utf8proc.h"

static TZrBool zr_utf8_decode_internal(TZrNativeString string,
                                       TZrSize length,
                                       utf8proc_int32_t *outCodePoint,
                                       TZrSize *outConsumedBytes) {
    utf8proc_int32_t codePoint = 0;
    utf8proc_ssize_t status;

    if ((string == ZR_NULL && length > 0) || outConsumedBytes == ZR_NULL) {
        return ZR_FALSE;
    }

    status = utf8proc_iterate((const utf8proc_uint8_t *)string,
                              (utf8proc_ssize_t)length,
                              &codePoint);
    if (status <= 0 || !utf8proc_codepoint_valid(codePoint)) {
        return ZR_FALSE;
    }

    if (outCodePoint != ZR_NULL) {
        *outCodePoint = codePoint;
    }
    *outConsumedBytes = (TZrSize)status;
    return ZR_TRUE;
}

TZrBool ZrCore_Utf8_IsValid(TZrNativeString string, TZrSize length) {
    TZrSize offset = 0;

    if (string == ZR_NULL) {
        return length == 0 ? ZR_TRUE : ZR_FALSE;
    }

    while (offset < length) {
        TZrSize consumedBytes = 0;
        if (!zr_utf8_decode_internal(string + offset,
                                     length - offset,
                                     ZR_NULL,
                                     &consumedBytes)) {
            return ZR_FALSE;
        }
        offset += consumedBytes;
    }

    return ZR_TRUE;
}

TZrBool ZrCore_Utf8_DecodeCodePoint(TZrNativeString string,
                                    TZrSize length,
                                    TZrUInt32 *outCodePoint,
                                    TZrSize *outConsumedBytes) {
    utf8proc_int32_t codePoint = 0;
    TZrSize consumedBytes = 0;

    if (!zr_utf8_decode_internal(string, length, &codePoint, &consumedBytes)) {
        return ZR_FALSE;
    }

    if (outCodePoint != ZR_NULL) {
        *outCodePoint = (TZrUInt32)codePoint;
    }
    if (outConsumedBytes != ZR_NULL) {
        *outConsumedBytes = consumedBytes;
    }
    return ZR_TRUE;
}

TZrBool ZrCore_Utf8_EncodeCodePoint(TZrUInt32 codePoint,
                                    TZrChar *buffer,
                                    TZrSize *outLength) {
    utf8proc_ssize_t status;

    if (buffer == ZR_NULL || outLength == ZR_NULL) {
        return ZR_FALSE;
    }

    status = utf8proc_encode_char((utf8proc_int32_t)codePoint, (utf8proc_uint8_t *)buffer);
    if (status <= 0) {
        return ZR_FALSE;
    }

    *outLength = (TZrSize)status;
    if (*outLength < ZR_STRING_UTF8_SIZE) {
        buffer[*outLength] = '\0';
    }
    return ZR_TRUE;
}

TZrBool ZrCore_Utf8_CountCodePoints(TZrNativeString string,
                                    TZrSize length,
                                    TZrSize *outCount) {
    TZrSize offset = 0;
    TZrSize count = 0;

    if (outCount == ZR_NULL) {
        return ZR_FALSE;
    }

    if (string == ZR_NULL) {
        if (length == 0) {
            *outCount = 0;
            return ZR_TRUE;
        }
        return ZR_FALSE;
    }

    while (offset < length) {
        TZrSize consumedBytes = 0;
        if (!zr_utf8_decode_internal(string + offset,
                                     length - offset,
                                     ZR_NULL,
                                     &consumedBytes)) {
            return ZR_FALSE;
        }
        offset += consumedBytes;
        count++;
    }

    *outCount = count;
    return ZR_TRUE;
}

TZrBool ZrCore_Utf8_CodePointCountToByteOffset(TZrNativeString string,
                                               TZrSize length,
                                               TZrSize codePointCount,
                                               TZrSize *outOffset) {
    TZrSize offset = 0;
    TZrSize count = 0;

    if (outOffset == ZR_NULL) {
        return ZR_FALSE;
    }

    if (string == ZR_NULL) {
        if (length == 0) {
            *outOffset = 0;
            return ZR_TRUE;
        }
        return ZR_FALSE;
    }

    while (offset < length && count < codePointCount) {
        TZrSize consumedBytes = 0;
        if (!zr_utf8_decode_internal(string + offset,
                                     length - offset,
                                     ZR_NULL,
                                     &consumedBytes)) {
            return ZR_FALSE;
        }
        offset += consumedBytes;
        count++;
    }

    *outOffset = offset;
    return ZR_TRUE;
}
