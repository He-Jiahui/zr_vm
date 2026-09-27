#include "stdio_request_registry.h"

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

/* JSON-RPC 的数值、字符串和显式 null ID 不得互相折叠；同类值才参与去重。 */
typedef enum EZrStdioRequestIdKind {
    ZR_STDIO_REQUEST_ID_NULL = 0,
    ZR_STDIO_REQUEST_ID_NUMBER,
    ZR_STDIO_REQUEST_ID_STRING,
} EZrStdioRequestIdKind;

/* 预留跨越消息入队和处理，字符串必须自有副本，取消位由注册表锁保护。 */
typedef struct SZrStdioRequestRegistryEntry {
    EZrStdioRequestIdKind kind;
    double numberValue;
    char *stringValue;
    TZrBool cancelled;
    struct SZrStdioRequestRegistryEntry *next;
} SZrStdioRequestRegistryEntry;

/* 读线程写入预留/取消，主线程查询/完成；链表和取消位共享一把锁。 */
struct SZrStdioRequestRegistry {
#ifdef _WIN32
    CRITICAL_SECTION lock;
#else
    pthread_mutex_t lock;
#endif
    SZrStdioRequestRegistryEntry *entries;
};

/* 平台锁只用于短暂访问注册表；不得持锁执行处理器或输出通知。 */
static void request_registry_lock(SZrStdioRequestRegistry *registry) {
#ifdef _WIN32
    EnterCriticalSection(&registry->lock);
#else
    pthread_mutex_lock(&registry->lock);
#endif
}

/* 与所有预留、取消、查询和完成路径配对，维持跨线程可见性。 */
static void request_registry_unlock(SZrStdioRequestRegistry *registry) {
#ifdef _WIN32
    LeaveCriticalSection(&registry->lock);
#else
    pthread_mutex_unlock(&registry->lock);
#endif
}

/* 统一入口的 ID 类型判定，避免通知、请求和完成路径各自解释 ID。 */
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

/* BUG: 合法字符串 ID 含 U+0000 时 cJSON 保留内嵌 NUL，strlen/strcmp 只比较前缀，可能误判重复或取消目标。 */
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

/* 返回借用的表项，仅可在持锁区间使用；Cancel 与 IsCancelled 都走同一匹配规则。 */
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

/* 读线程的消息树可能先于预留完成而释放，注册表因此独立持有字符串 ID。 */
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

/* 由 server 初始化拥有，锁初始化失败时不向读线程发布半成品。 */
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

/* server 关闭读线程后调用；此处不持锁，依赖已无并发访问的生命周期约束。 */
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

/* 在读线程中先于入队执行，使排队请求也能观察后来到达的取消通知。 */
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
    /* 发布节点前完成 ID 自有副本；失败路径不留下半个预留。 */
    entry->next = registry->entries;
    registry->entries = entry;
    request_registry_unlock(registry);
    return ZR_STDIO_REQUEST_RESERVATION_ACCEPTED;
}

/* 取消位仅记录意图，处理器通过 IsCancelled 决定是否中止正在执行的请求。 */
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

/* 锁内复制取消位，调用方不持有链表节点，以免 Complete 并发释放造成悬空引用。 */
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

/* 主线程回复后移除预留；迟到取消只会匹配之后重新预留的同值 ID。 */
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
