#include "stdio_frame_reader.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static TZrBool frame_reader_ascii_equals(const char *left, const char *right) {
    while (*left != '\0' && *right != '\0') {
        if (tolower((unsigned char)*left) != tolower((unsigned char)*right)) {
            return ZR_FALSE;
        }
        left++;
        right++;
    }
    return *left == '\0' && *right == '\0';
}

static char *frame_reader_skip_spaces(char *text) {
    while (*text == ' ' || *text == '\t') {
        text++;
    }
    return text;
}

static void frame_reader_trim_spaces(char *text) {
    size_t length = strlen(text);

    while (length > 0 && (text[length - 1] == ' ' || text[length - 1] == '\t')) {
        text[--length] = '\0';
    }
}

static TZrBool frame_reader_content_type_is_utf8(char *value) {
    char *parameter = strchr(value, ';');

    while (parameter != ZR_NULL) {
        char *part;
        char *next;
        char *separator;

        parameter++;
        part = frame_reader_skip_spaces(parameter);
        next = strchr(part, ';');
        if (next != ZR_NULL) {
            *next = '\0';
        }
        frame_reader_trim_spaces(part);
        separator = strchr(part, '=');
        if (separator != ZR_NULL) {
            char *name = part;
            char *charset;

            *separator++ = '\0';
            frame_reader_trim_spaces(name);
            if (frame_reader_ascii_equals(name, "charset")) {
                charset = frame_reader_skip_spaces(separator);
                frame_reader_trim_spaces(charset);
                if (charset[0] == '"') {
                    size_t length = strlen(charset);
                    if (length < 2 || charset[length - 1] != '"') {
                        return ZR_FALSE;
                    }
                    charset[length - 1] = '\0';
                    charset++;
                }
                if (!frame_reader_ascii_equals(charset, "utf-8") &&
                    !frame_reader_ascii_equals(charset, "utf8")) {
                    return ZR_FALSE;
                }
            }
        } else if (frame_reader_ascii_equals(part, "charset")) {
            return ZR_FALSE;
        }
        if (next == ZR_NULL) {
            break;
        }
        parameter = next;
    }

    return ZR_TRUE;
}

static EZrStdioFrameReadStatus frame_reader_parse_content_length(const char *value,
                                                                   TZrSize maxMessageBytes,
                                                                   TZrSize *outLength) {
    char *end;
    unsigned long long parsed;

    if (value == ZR_NULL || outLength == ZR_NULL || !isdigit((unsigned char)*value)) {
        return ZR_STDIO_FRAME_READ_MALFORMED_HEADER;
    }

    errno = 0;
    parsed = strtoull(value, &end, 10);
    if (errno == ERANGE || parsed > (unsigned long long)(SIZE_MAX - 1U) ||
        parsed > (unsigned long long)maxMessageBytes) {
        return ZR_STDIO_FRAME_READ_TOO_LARGE;
    }
    end = frame_reader_skip_spaces(end);
    if (*end != '\0') {
        return ZR_STDIO_FRAME_READ_MALFORMED_HEADER;
    }

    *outLength = (TZrSize)parsed;
    return ZR_STDIO_FRAME_READ_OK;
}

static SZrStdioFrameReaderLimits frame_reader_normalize_limits(
        const SZrStdioFrameReaderLimits *limits) {
    SZrStdioFrameReaderLimits normalized;

    ZrLanguageServer_StdioFrameReader_DefaultLimits(&normalized);
    if (limits == ZR_NULL) {
        return normalized;
    }

    if (limits->maxHeaderBytes != 0 && limits->maxHeaderBytes < normalized.maxHeaderBytes) {
        normalized.maxHeaderBytes = limits->maxHeaderBytes;
    }
    if (limits->maxHeaderCount != 0 && limits->maxHeaderCount < normalized.maxHeaderCount) {
        normalized.maxHeaderCount = limits->maxHeaderCount;
    }
    if (limits->maxMessageBytes != 0 && limits->maxMessageBytes < normalized.maxMessageBytes) {
        normalized.maxMessageBytes = limits->maxMessageBytes;
    }
    return normalized;
}

void ZrLanguageServer_StdioFrameReader_DefaultLimits(SZrStdioFrameReaderLimits *outLimits) {
    if (outLimits == ZR_NULL) {
        return;
    }

    outLimits->maxHeaderBytes = ZR_LSP_MAX_HEADER_BYTES;
    outLimits->maxHeaderCount = ZR_LSP_MAX_HEADER_COUNT;
    outLimits->maxMessageBytes = ZR_LSP_MAX_MESSAGE_BYTES;
}

EZrStdioFrameReadStatus ZrLanguageServer_StdioFrameReader_Read(
        FILE *input,
        const SZrStdioFrameReaderLimits *limits,
        char **outPayload,
        TZrSize *outLength) {
    SZrStdioFrameReaderLimits effectiveLimits = frame_reader_normalize_limits(limits);
    char line[ZR_LSP_MAX_HEADER_BYTES + 1];
    TZrSize headerBytes = 0;
    TZrSize headerCount = 0;
    TZrSize lineLength = 0;
    TZrSize contentLength = 0;
    TZrBool hasContentLength = ZR_FALSE;
    int character;

    if (outPayload != ZR_NULL) {
        *outPayload = ZR_NULL;
    }
    if (outLength != ZR_NULL) {
        *outLength = 0;
    }
    if (input == ZR_NULL || outPayload == ZR_NULL || outLength == ZR_NULL ||
        effectiveLimits.maxHeaderBytes == 0 || effectiveLimits.maxHeaderCount == 0) {
        return ZR_STDIO_FRAME_READ_IO_ERROR;
    }

    for (;;) {
        character = fgetc(input);
        if (character == EOF) {
            if (ferror(input)) {
                return ZR_STDIO_FRAME_READ_IO_ERROR;
            }
            return headerBytes == 0 ? ZR_STDIO_FRAME_READ_EOF
                                    : ZR_STDIO_FRAME_READ_MALFORMED_HEADER;
        }
        if (character == '\0') {
            return ZR_STDIO_FRAME_READ_MALFORMED_HEADER;
        }
        if (headerBytes >= effectiveLimits.maxHeaderBytes ||
            lineLength >= effectiveLimits.maxHeaderBytes) {
            return ZR_STDIO_FRAME_READ_TOO_LARGE;
        }

        headerBytes++;
        line[lineLength++] = (char)character;
        if (character != '\n') {
            continue;
        }
        if (lineLength < 2 || line[lineLength - 2] != '\r') {
            return ZR_STDIO_FRAME_READ_MALFORMED_HEADER;
        }

        line[lineLength - 2] = '\0';
        if (lineLength == 2) {
            break;
        }
        if (++headerCount > effectiveLimits.maxHeaderCount) {
            return ZR_STDIO_FRAME_READ_TOO_LARGE;
        }

        {
            char *separator = strchr(line, ':');
            char *value;

            if (separator == ZR_NULL || separator == line) {
                return ZR_STDIO_FRAME_READ_MALFORMED_HEADER;
            }
            *separator++ = '\0';
            value = frame_reader_skip_spaces(separator);
            frame_reader_trim_spaces(line);
    if (frame_reader_ascii_equals(line, "Content-Length")) {
                EZrStdioFrameReadStatus status;

                if (hasContentLength) {
                    return ZR_STDIO_FRAME_READ_MALFORMED_HEADER;
                }
                status = frame_reader_parse_content_length(value,
                                                           effectiveLimits.maxMessageBytes,
                                                           &contentLength);
                if (status != ZR_STDIO_FRAME_READ_OK) {
                    return status;
                }
                hasContentLength = ZR_TRUE;
            } else if (frame_reader_ascii_equals(line, "Content-Type") &&
                       !frame_reader_content_type_is_utf8(value)) {
                return ZR_STDIO_FRAME_READ_MALFORMED_HEADER;
            }
        }
        lineLength = 0;
    }

    if (!hasContentLength || contentLength > SIZE_MAX - 1U) {
        return ZR_STDIO_FRAME_READ_MALFORMED_HEADER;
    }

    {
        char *payload = (char *)malloc(contentLength + 1U);
        TZrSize totalRead = 0;

        if (payload == ZR_NULL) {
            return ZR_STDIO_FRAME_READ_IO_ERROR;
        }
        while (totalRead < contentLength) {
            TZrSize readNow = fread(payload + totalRead, 1, contentLength - totalRead, input);

            if (readNow == 0) {
                free(payload);
                return ferror(input) ? ZR_STDIO_FRAME_READ_IO_ERROR
                                     : ZR_STDIO_FRAME_READ_PAYLOAD_TRUNCATED;
            }
            totalRead += readNow;
        }

        payload[contentLength] = '\0';
        *outPayload = payload;
        *outLength = contentLength;
    }

    return ZR_STDIO_FRAME_READ_OK;
}

const char *ZrLanguageServer_StdioFrameReader_StatusName(EZrStdioFrameReadStatus status) {
    switch (status) {
        case ZR_STDIO_FRAME_READ_OK:
            return "OK";
        case ZR_STDIO_FRAME_READ_EOF:
            return "EOF";
        case ZR_STDIO_FRAME_READ_MALFORMED_HEADER:
            return "MALFORMED_HEADER";
        case ZR_STDIO_FRAME_READ_PAYLOAD_TRUNCATED:
            return "PAYLOAD_TRUNCATED";
        case ZR_STDIO_FRAME_READ_TOO_LARGE:
            return "TOO_LARGE";
        case ZR_STDIO_FRAME_READ_IO_ERROR:
            return "IO_ERROR";
        default:
            return "UNKNOWN";
    }
}
