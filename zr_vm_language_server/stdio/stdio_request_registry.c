#include "stdio_request_registry.h"

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

typedef enum EZrStdioRequestIdKind {
    ZR_STDIO_REQUEST_ID_NULL = 0,
    ZR_STDIO_REQUEST_ID_NUMBER,
    ZR_STDIO_REQUEST_ID_STRING,
} EZrStdioRequestIdKind;

typedef struct SZrStdioRequestRegistryEntry {
    EZrStdioRequestIdKind kind;
    double numberValue;
    char *stringValue;
    TZrBool cancelled;
    struct SZrStdioRequestRegistryEntry *next;
} SZrStdioRequestRegistryEntry;

struct SZrStdioRequestRegistry {
#ifdef _WIN32
    CRITICAL_SECTION lock;
#else
    pthread_mutex_t lock;
#endif
    SZrStdioRequestRegistryEntry *entries;
};

static void request_registry_lock(SZrStdioRequestRegistry *registry) {
#ifdef _WIN32
    EnterCriticalSection(&registry->lock);
#else
    pthread_mutex_lock(&registry->lock);
#endif
}

static void request_registry_unlock(SZrStdioRequestRegistry *registry) {
#ifdef _WIN32
    LeaveCriticalSection(&registry->lock);
#else
    pthread_mutex_unlock(&registry->lock);
#endif
}

static TZrBool request_registry_get_id_kind(const cJSON *id, EZrStdioRequestIdKind *outKind) {
    if (id == ZR_NULL || outKind == ZR_NULL) {
        return ZR_FALSE;
    }
    if (cJSON_IsNull(id)) {
        *outKind = ZR_STDIO_REQUEST_ID_NULL;
        return ZR_TRUE;
    }
    if (cJSON_IsNumber(id)) {
        *outKind = ZR_STDIO_REQUEST_ID_NUMBER;
        return ZR_TRUE;
    }
    if (cJSON_IsString(id) && cJSON_GetStringValue((cJSON *)id) != ZR_NULL) {
        *outKind = ZR_STDIO_REQUEST_ID_STRING;
        return ZR_TRUE;
    }
    return ZR_FALSE;
}

static TZrBool request_registry_id_equals(const SZrStdioRequestRegistryEntry *entry,
                                          EZrStdioRequestIdKind kind,
                                          const cJSON *id) {
    if (entry == ZR_NULL || entry->kind != kind) {
        return ZR_FALSE;
    }
    if (kind == ZR_STDIO_REQUEST_ID_NULL) {
        return ZR_TRUE;
    }
    if (kind == ZR_STDIO_REQUEST_ID_NUMBER) {
        return entry->numberValue == id->valuedouble;
    }
    return strcmp(entry->stringValue, cJSON_GetStringValue((cJSON *)id)) == 0;
}

static SZrStdioRequestRegistryEntry *request_registry_find_locked(
        SZrStdioRequestRegistry *registry,
        EZrStdioRequestIdKind kind,
        const cJSON *id) {
    SZrStdioRequestRegistryEntry *entry = registry->entries;

    while (entry != ZR_NULL) {
        if (request_registry_id_equals(entry, kind, id)) {
            return entry;
        }
        entry = entry->next;
    }
    return ZR_NULL;
}

static char *request_registry_duplicate_string(const char *text) {
    size_t length;
    char *copy;

    if (text == ZR_NULL) {
        return ZR_NULL;
    }
    length = strlen(text);
    copy = (char *)malloc(length + 1U);
    if (copy != ZR_NULL) {
        memcpy(copy, text, length + 1U);
    }
    return copy;
}

SZrStdioRequestRegistry *ZrLanguageServer_StdioRequestRegistry_New(void) {
    SZrStdioRequestRegistry *registry =
            (SZrStdioRequestRegistry *)calloc(1, sizeof(SZrStdioRequestRegistry));

    if (registry == ZR_NULL) {
        return ZR_NULL;
    }
#ifdef _WIN32
    InitializeCriticalSection(&registry->lock);
#else
    if (pthread_mutex_init(&registry->lock, ZR_NULL) != 0) {
        free(registry);
        return ZR_NULL;
    }
#endif
    return registry;
}

void ZrLanguageServer_StdioRequestRegistry_Free(SZrStdioRequestRegistry *registry) {
    SZrStdioRequestRegistryEntry *entry;

    if (registry == ZR_NULL) {
        return;
    }
    entry = registry->entries;
    while (entry != ZR_NULL) {
        SZrStdioRequestRegistryEntry *next = entry->next;
        free(entry->stringValue);
        free(entry);
        entry = next;
    }
#ifdef _WIN32
    DeleteCriticalSection(&registry->lock);
#else
    pthread_mutex_destroy(&registry->lock);
#endif
    free(registry);
}

EZrStdioRequestReservation ZrLanguageServer_StdioRequestRegistry_Reserve(
        SZrStdioRequestRegistry *registry,
        const cJSON *id) {
    EZrStdioRequestIdKind kind;
    SZrStdioRequestRegistryEntry *entry;

    if (registry == ZR_NULL || !request_registry_get_id_kind(id, &kind)) {
        return ZR_STDIO_REQUEST_RESERVATION_FAILED;
    }

    request_registry_lock(registry);
    if (request_registry_find_locked(registry, kind, id) != ZR_NULL) {
        request_registry_unlock(registry);
        return ZR_STDIO_REQUEST_RESERVATION_DUPLICATE;
    }

    entry = (SZrStdioRequestRegistryEntry *)calloc(1, sizeof(SZrStdioRequestRegistryEntry));
    if (entry == ZR_NULL) {
        request_registry_unlock(registry);
        return ZR_STDIO_REQUEST_RESERVATION_FAILED;
    }
    entry->kind = kind;
    if (kind == ZR_STDIO_REQUEST_ID_NUMBER) {
        entry->numberValue = id->valuedouble;
    } else if (kind == ZR_STDIO_REQUEST_ID_STRING) {
        entry->stringValue = request_registry_duplicate_string(cJSON_GetStringValue((cJSON *)id));
        if (entry->stringValue == ZR_NULL) {
            free(entry);
            request_registry_unlock(registry);
            return ZR_STDIO_REQUEST_RESERVATION_FAILED;
        }
    }
    entry->next = registry->entries;
    registry->entries = entry;
    request_registry_unlock(registry);
    return ZR_STDIO_REQUEST_RESERVATION_ACCEPTED;
}

TZrBool ZrLanguageServer_StdioRequestRegistry_Cancel(SZrStdioRequestRegistry *registry,
                                                      const cJSON *id) {
    EZrStdioRequestIdKind kind;
    SZrStdioRequestRegistryEntry *entry;

    if (registry == ZR_NULL || !request_registry_get_id_kind(id, &kind)) {
        return ZR_FALSE;
    }
    request_registry_lock(registry);
    entry = request_registry_find_locked(registry, kind, id);
    if (entry != ZR_NULL) {
        entry->cancelled = ZR_TRUE;
    }
    request_registry_unlock(registry);
    return entry != ZR_NULL;
}

TZrBool ZrLanguageServer_StdioRequestRegistry_IsCancelled(
        SZrStdioRequestRegistry *registry,
        const cJSON *id) {
    EZrStdioRequestIdKind kind;
    SZrStdioRequestRegistryEntry *entry;
    TZrBool cancelled = ZR_FALSE;

    if (registry == ZR_NULL || !request_registry_get_id_kind(id, &kind)) {
        return ZR_FALSE;
    }
    request_registry_lock(registry);
    entry = request_registry_find_locked(registry, kind, id);
    if (entry != ZR_NULL) {
        cancelled = entry->cancelled;
    }
    request_registry_unlock(registry);
    return cancelled;
}

void ZrLanguageServer_StdioRequestRegistry_Complete(SZrStdioRequestRegistry *registry,
                                                     const cJSON *id) {
    EZrStdioRequestIdKind kind;
    SZrStdioRequestRegistryEntry **slot;
    SZrStdioRequestRegistryEntry *entry;

    if (registry == ZR_NULL || !request_registry_get_id_kind(id, &kind)) {
        return;
    }
    request_registry_lock(registry);
    slot = &registry->entries;
    while (*slot != ZR_NULL && !request_registry_id_equals(*slot, kind, id)) {
        slot = &(*slot)->next;
    }
    if (*slot != ZR_NULL) {
        entry = *slot;
        *slot = entry->next;
        free(entry->stringValue);
        free(entry);
    }
    request_registry_unlock(registry);
}
