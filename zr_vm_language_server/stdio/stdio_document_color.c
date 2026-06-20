#include "zr_vm_language_server_stdio_internal.h"

typedef struct SZrStdioColorLiteral {
    SZrLspRange range;
    double red;
    double green;
    double blue;
    double alpha;
} SZrStdioColorLiteral;

static int color_hex_value(char ch) {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }
    return -1;
}

static int color_parse_byte(const char *text, unsigned int *outValue) {
    int high;
    int low;

    if (text == NULL || outValue == NULL) {
        return 0;
    }

    high = color_hex_value(text[0]);
    low = color_hex_value(text[1]);
    if (high < 0 || low < 0) {
        return 0;
    }

    *outValue = (unsigned int)((high << 4) | low);
    return 1;
}

static int color_try_parse_literal(const char *content,
                                   size_t contentLength,
                                   size_t offset,
                                   TZrInt32 line,
                                   TZrInt32 character,
                                   SZrStdioColorLiteral *outLiteral,
                                   size_t *outLength) {
    unsigned int red;
    unsigned int green;
    unsigned int blue;
    unsigned int alpha = 255;
    size_t digitCount = 0;

    if (content == NULL || outLiteral == NULL || outLength == NULL ||
        offset >= contentLength || content[offset] != '#') {
        return 0;
    }

    while (offset + 1 + digitCount < contentLength &&
           color_hex_value(content[offset + 1 + digitCount]) >= 0 &&
           digitCount < 8) {
        digitCount++;
    }
    if ((digitCount != 6 && digitCount != 8) ||
        (offset + 1 + digitCount < contentLength &&
         color_hex_value(content[offset + 1 + digitCount]) >= 0)) {
        return 0;
    }

    if (!color_parse_byte(content + offset + 1, &red) ||
        !color_parse_byte(content + offset + 3, &green) ||
        !color_parse_byte(content + offset + 5, &blue)) {
        return 0;
    }
    if (digitCount == 8 && !color_parse_byte(content + offset + 7, &alpha)) {
        return 0;
    }

    outLiteral->range.start.line = line;
    outLiteral->range.start.character = character;
    outLiteral->range.end.line = line;
    outLiteral->range.end.character = character + (TZrInt32)(digitCount + 1);
    outLiteral->red = (double)red / 255.0;
    outLiteral->green = (double)green / 255.0;
    outLiteral->blue = (double)blue / 255.0;
    outLiteral->alpha = (double)alpha / 255.0;
    *outLength = digitCount + 1;
    return 1;
}

static cJSON *color_serialize(const SZrStdioColorLiteral *literal) {
    cJSON *entry;
    cJSON *color;

    if (literal == NULL) {
        return NULL;
    }

    entry = cJSON_CreateObject();
    color = cJSON_CreateObject();
    if (entry == NULL || color == NULL) {
        cJSON_Delete(entry);
        cJSON_Delete(color);
        return NULL;
    }

    cJSON_AddItemToObject(entry, ZR_LSP_FIELD_RANGE, serialize_range(literal->range));
    cJSON_AddNumberToObject(color, ZR_LSP_FIELD_RED, literal->red);
    cJSON_AddNumberToObject(color, ZR_LSP_FIELD_GREEN, literal->green);
    cJSON_AddNumberToObject(color, ZR_LSP_FIELD_BLUE, literal->blue);
    cJSON_AddNumberToObject(color, ZR_LSP_FIELD_ALPHA, literal->alpha);
    cJSON_AddItemToObject(entry, ZR_LSP_FIELD_COLOR, color);
    return entry;
}

static unsigned int color_component_to_byte(const cJSON *value) {
    double number;

    if (!cJSON_IsNumber((cJSON *)value)) {
        return 0;
    }

    number = cJSON_GetNumberValue((cJSON *)value);
    if (number < 0.0) {
        number = 0.0;
    } else if (number > 1.0) {
        number = 1.0;
    }
    return (unsigned int)(number * 255.0 + 0.5);
}

static cJSON *color_create_presentation(const char *label, SZrLspRange range) {
    cJSON *presentation;
    cJSON *textEdit;

    presentation = cJSON_CreateObject();
    textEdit = cJSON_CreateObject();
    if (presentation == NULL || textEdit == NULL || label == NULL) {
        cJSON_Delete(presentation);
        cJSON_Delete(textEdit);
        return NULL;
    }

    cJSON_AddStringToObject(presentation, ZR_LSP_FIELD_LABEL, label);
    cJSON_AddItemToObject(textEdit, ZR_LSP_FIELD_RANGE, serialize_range(range));
    cJSON_AddStringToObject(textEdit, ZR_LSP_FIELD_NEW_TEXT, label);
    cJSON_AddItemToObject(presentation, ZR_LSP_FIELD_TEXT_EDIT, textEdit);
    return presentation;
}

static int color_range_equals(SZrLspRange left, SZrLspRange right) {
    return left.start.line == right.start.line &&
           left.start.character == right.start.character &&
           left.end.line == right.end.line &&
           left.end.character == right.end.character;
}

static int color_document_contains_literal_range(const char *content,
                                                 size_t contentLength,
                                                 SZrLspRange range) {
    TZrInt32 line = 0;
    TZrInt32 character = 0;
    int inLineComment = 0;
    int inBlockComment = 0;
    char quote = '\0';
    int escaped = 0;

    if (content == NULL) {
        return 0;
    }

    for (size_t offset = 0; offset < contentLength; offset++) {
        SZrStdioColorLiteral literal;
        size_t literalLength;
        char current = content[offset];
        char next = offset + 1 < contentLength ? content[offset + 1] : '\0';

        if (inLineComment) {
            if (current == '\n') {
                inLineComment = 0;
                line++;
                character = 0;
            } else {
                character++;
            }
            continue;
        }

        if (inBlockComment) {
            if (current == '*' && next == '/') {
                inBlockComment = 0;
                offset++;
                character += 2;
                continue;
            }
            if (current == '\n') {
                line++;
                character = 0;
            } else {
                character++;
            }
            continue;
        }

        if (color_try_parse_literal(content, contentLength, offset, line, character, &literal, &literalLength)) {
            if (color_range_equals(literal.range, range)) {
                return 1;
            }
            offset += literalLength - 1;
            character += (TZrInt32)literalLength;
            continue;
        }

        if (quote != '\0') {
            if (escaped) {
                escaped = 0;
            } else if (current == '\\') {
                escaped = 1;
            } else if (current == quote) {
                quote = '\0';
            }
        } else if (current == '/' && next == '/') {
            inLineComment = 1;
        } else if (current == '/' && next == '*') {
            inBlockComment = 1;
        } else if (current == '"' || current == '\'' || current == '`') {
            quote = current;
        }

        if (current == '\n') {
            line++;
            character = 0;
        } else {
            character++;
        }
    }

    return 0;
}

cJSON *handle_document_color_request(SZrStdioServer *server, const cJSON *params) {
    const char *uriText;
    SZrString *uri;
    SZrFileVersion *fileVersion;
    SZrFileVersionContentSnapshot snapshot = {0};
    const char *content;
    size_t contentLength;
    TZrInt32 line = 0;
    TZrInt32 character = 0;
    int inLineComment = 0;
    int inBlockComment = 0;
    char quote = '\0';
    int escaped = 0;
    cJSON *result;

    if (!get_uri_from_text_document(server, params, &uriText, &uri)) {
        return cJSON_CreateArray();
    }
    ZR_UNUSED_PARAMETER(uriText);

    fileVersion = get_file_version_for_uri(server, uri);
    if (!ZrLanguageServer_FileVersionContentSnapshot_Acquire(server->state, fileVersion, &snapshot)) {
        return cJSON_CreateArray();
    }

    result = cJSON_CreateArray();
    if (result == NULL) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(server->state, &snapshot);
        return NULL;
    }

    content = snapshot.content;
    contentLength = (size_t)snapshot.contentLength;
    for (size_t offset = 0; offset < contentLength; offset++) {
        SZrStdioColorLiteral literal;
        size_t literalLength;
        cJSON *entry;
        char current = content[offset];
        char next = offset + 1 < contentLength ? content[offset + 1] : '\0';

        if (inLineComment) {
            if (current == '\n') {
                inLineComment = 0;
                line++;
                character = 0;
            } else {
                character++;
            }
            continue;
        }

        if (inBlockComment) {
            if (current == '*' && next == '/') {
                inBlockComment = 0;
                offset++;
                character += 2;
                continue;
            }
            if (current == '\n') {
                line++;
                character = 0;
            } else {
                character++;
            }
            continue;
        }

        if (color_try_parse_literal(content, contentLength, offset, line, character, &literal, &literalLength)) {
            entry = color_serialize(&literal);
            if (entry != NULL) {
                cJSON_AddItemToArray(result, entry);
            }
            offset += literalLength - 1;
            character += (TZrInt32)literalLength;
            continue;
        }

        if (quote != '\0') {
            if (escaped) {
                escaped = 0;
            } else if (current == '\\') {
                escaped = 1;
            } else if (current == quote) {
                quote = '\0';
            }
        } else if (current == '/' && next == '/') {
            inLineComment = 1;
        } else if (current == '/' && next == '*') {
            inBlockComment = 1;
        } else if (current == '"' || current == '\'' || current == '`') {
            quote = current;
        }

        if (current == '\n') {
            line++;
            character = 0;
        } else {
            character++;
        }
    }

    ZrLanguageServer_FileVersionContentSnapshot_Free(server->state, &snapshot);
    return result;
}

cJSON *handle_color_presentation_request(SZrStdioServer *server, const cJSON *params) {
    const cJSON *color;
    const char *uriText;
    SZrString *uri;
    SZrFileVersion *fileVersion;
    SZrFileVersionContentSnapshot snapshot = {0};
    SZrLspRange range;
    char label[10];
    unsigned int red;
    unsigned int green;
    unsigned int blue;
    unsigned int alpha;
    cJSON *result;
    cJSON *presentation;

    if (!get_uri_from_text_document(server, params, &uriText, &uri)) {
        return cJSON_CreateArray();
    }
    if (params == NULL || !parse_range_for_uri(server, uri, get_object_item(params, ZR_LSP_FIELD_RANGE), &range)) {
        return cJSON_CreateArray();
    }
    ZR_UNUSED_PARAMETER(uriText);

    fileVersion = get_file_version_for_uri(server, uri);
    if (!ZrLanguageServer_FileVersionContentSnapshot_Acquire(server->state, fileVersion, &snapshot)) {
        return cJSON_CreateArray();
    }
    if (!color_document_contains_literal_range(snapshot.content, (size_t)snapshot.contentLength, range)) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(server->state, &snapshot);
        return cJSON_CreateArray();
    }
    ZrLanguageServer_FileVersionContentSnapshot_Free(server->state, &snapshot);

    color = get_object_item(params, ZR_LSP_FIELD_COLOR);
    red = color_component_to_byte(get_object_item(color, ZR_LSP_FIELD_RED));
    green = color_component_to_byte(get_object_item(color, ZR_LSP_FIELD_GREEN));
    blue = color_component_to_byte(get_object_item(color, ZR_LSP_FIELD_BLUE));
    alpha = color_component_to_byte(get_object_item(color, ZR_LSP_FIELD_ALPHA));

    if (alpha < 255) {
        snprintf(label, sizeof(label), "#%02X%02X%02X%02X", red, green, blue, alpha);
    } else {
        snprintf(label, sizeof(label), "#%02X%02X%02X", red, green, blue);
    }

    result = cJSON_CreateArray();
    presentation = color_create_presentation(label, range);
    if (result == NULL || presentation == NULL) {
        cJSON_Delete(result);
        cJSON_Delete(presentation);
        return cJSON_CreateArray();
    }
    cJSON_AddItemToArray(result, presentation);
    return result;
}
