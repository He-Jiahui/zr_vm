#include "zr_vm_language_server_stdio_internal.h"

#define ZR_LSP_SEMANTIC_RESULT_HASH_OFFSET 1469598103934665603ULL
#define ZR_LSP_SEMANTIC_RESULT_HASH_PRIME 1099511628211ULL

static unsigned long long semantic_tokens_hash(SZrArray *tokens) {
    unsigned long long hash = ZR_LSP_SEMANTIC_RESULT_HASH_OFFSET;

    for (TZrSize index = 0; tokens != ZR_NULL && index < tokens->length; index++) {
        TZrUInt32 value = semantic_tokens_value_at(tokens, index);

        for (TZrSize byteIndex = 0; byteIndex < sizeof(TZrUInt32); byteIndex++) {
            hash ^= (unsigned long long)((value >> (byteIndex * 8U)) & 0xffU);
            hash *= ZR_LSP_SEMANTIC_RESULT_HASH_PRIME;
        }
    }

    return hash;
}

void format_semantic_tokens_result_id(SZrArray *tokens, char *buffer, size_t bufferLength) {
    snprintf(buffer,
             bufferLength,
             "zr-semantic:%u:%llx",
             (unsigned int)(tokens != ZR_NULL ? tokens->length : 0),
             semantic_tokens_hash(tokens));
}

TZrUInt32 semantic_tokens_value_at(SZrArray *tokens, TZrSize index) {
    TZrUInt32 *valuePtr;

    if (tokens == ZR_NULL || index >= tokens->length) {
        return 0;
    }

    valuePtr = (TZrUInt32 *)ZrCore_Array_Get(tokens, index);
    return valuePtr != ZR_NULL ? *valuePtr : 0;
}

SZrSemanticTokenSnapshot *find_semantic_token_snapshot(SZrStdioServer *server, const char *uriText) {
    if (server == ZR_NULL || uriText == NULL) {
        return NULL;
    }

    for (size_t index = 0; index < server->semanticTokenCache.count; index++) {
        if (server->semanticTokenCache.items[index].uriText != NULL &&
            strcmp(server->semanticTokenCache.items[index].uriText, uriText) == 0) {
            return &server->semanticTokenCache.items[index];
        }
    }

    return NULL;
}

static void clear_semantic_token_snapshot(SZrSemanticTokenSnapshot *snapshot) {
    if (snapshot == NULL) {
        return;
    }

    free(snapshot->uriText);
    free(snapshot->data);
    memset(snapshot, 0, sizeof(*snapshot));
}

void remove_semantic_token_cache_for_uri(SZrStdioServer *server, const char *uriText) {
    SZrSemanticTokenSnapshot *snapshot;
    size_t index;

    if (server == ZR_NULL || uriText == NULL) {
        return;
    }

    snapshot = find_semantic_token_snapshot(server, uriText);
    if (snapshot == NULL) {
        return;
    }

    index = (size_t)(snapshot - server->semanticTokenCache.items);
    clear_semantic_token_snapshot(snapshot);
    if (index + 1 < server->semanticTokenCache.count) {
        memmove(&server->semanticTokenCache.items[index],
                &server->semanticTokenCache.items[index + 1],
                (server->semanticTokenCache.count - index - 1) * sizeof(SZrSemanticTokenSnapshot));
    }
    server->semanticTokenCache.count--;
    if (server->semanticTokenCache.count > 0) {
        memset(&server->semanticTokenCache.items[server->semanticTokenCache.count],
               0,
               sizeof(SZrSemanticTokenSnapshot));
    }
}

TZrBool upsert_semantic_token_snapshot(SZrStdioServer *server,
                                       const char *uriText,
                                       const char *resultId,
                                       SZrArray *tokens) {
    SZrSemanticTokenSnapshot *snapshot;
    TZrUInt32 *newData = NULL;
    TZrSize length = tokens != ZR_NULL ? tokens->length : 0;

    if (server == ZR_NULL || uriText == NULL || resultId == NULL) {
        return ZR_FALSE;
    }

    if (length > 0) {
        newData = (TZrUInt32 *)malloc((size_t)length * sizeof(TZrUInt32));
        if (newData == NULL) {
            return ZR_FALSE;
        }
        for (TZrSize index = 0; index < length; index++) {
            newData[index] = semantic_tokens_value_at(tokens, index);
        }
    }

    snapshot = find_semantic_token_snapshot(server, uriText);
    if (snapshot == NULL) {
        if (server->semanticTokenCache.count == server->semanticTokenCache.capacity) {
            size_t newCapacity = server->semanticTokenCache.capacity == 0
                                     ? ZR_LSP_ARRAY_INITIAL_CAPACITY
                                     : server->semanticTokenCache.capacity * ZR_LSP_DYNAMIC_CAPACITY_GROWTH_FACTOR;
            SZrSemanticTokenSnapshot *newItems =
                (SZrSemanticTokenSnapshot *)realloc(server->semanticTokenCache.items,
                                                    newCapacity * sizeof(SZrSemanticTokenSnapshot));
            if (newItems == NULL) {
                free(newData);
                return ZR_FALSE;
            }
            memset(&newItems[server->semanticTokenCache.capacity],
                   0,
                   (newCapacity - server->semanticTokenCache.capacity) * sizeof(SZrSemanticTokenSnapshot));
            server->semanticTokenCache.items = newItems;
            server->semanticTokenCache.capacity = newCapacity;
        }

        snapshot = &server->semanticTokenCache.items[server->semanticTokenCache.count];
        snapshot->uriText = duplicate_c_string(uriText);
        if (snapshot->uriText == NULL) {
            free(newData);
            return ZR_FALSE;
        }
        server->semanticTokenCache.count++;
    } else {
        free(snapshot->data);
    }

    snprintf(snapshot->resultId, sizeof(snapshot->resultId), "%s", resultId);
    snapshot->data = newData;
    snapshot->length = length;
    return ZR_TRUE;
}
