/* 原生数组的分配器故障与边界回归；不启动 GC 或执行脚本。 */
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zr_vm_core/array.h"
#include "zr_vm_core/exception.h"

static SZrGlobalState g_global;
static SZrState g_state;
static SZrArray g_array;
static jmp_buf g_jump;
static void *g_block;
static size_t g_blockSize;
static size_t g_syntheticOldSize;
static size_t g_calls;
static size_t g_lastRequest;
static int g_fail;
static int g_allocatorError;
static int g_operation;
static size_t g_elementSize;
static size_t g_count;
static TZrBool g_result;
static int g_values[] = {11, 22, 33, 44};

/* 替换异常传播边界以观察 header 的失败原子性；真实 TryRun 另由运行时套件覆盖。 */
void ZrCore_Exception_Throw(SZrState *state, EZrThreadStatus status) {
    if (state != &g_state || status != ZR_THREAD_STATUS_MEMORY_ERROR) {
        abort();
    }
    longjmp(g_jump, (int) status);
}

/* 分配失败保留旧块；成功时强制搬移，验证真实副本、释放大小与类别。 */
static TZrPtr allocator(TZrPtr data, TZrPtr old, TZrSize oldSize,
                        TZrSize newSize, TZrInt64 flag) {
    void *next;
    (void) data;
    g_calls++;
    g_lastRequest = newSize;
    if (flag != ZR_MEMORY_NATIVE_TYPE_ARRAY || old != g_block ||
        oldSize != (g_syntheticOldSize ? g_syntheticOldSize : g_blockSize)) {
        g_allocatorError = 1;
        return NULL;
    }
    if (newSize == 0) {
        free(g_block);
        g_block = NULL;
        g_blockSize = 0;
        return NULL;
    }
    if (g_fail || newSize > 4096u) {
        return NULL;
    }
    next = malloc(newSize);
    if (next == NULL) {
        abort();
    }
    if (old != NULL) {
        memcpy(next, old, oldSize < newSize ? oldSize : newSize);
        free(old);
    }
    g_block = next;
    g_blockSize = newSize;
    return next;
}

/* 测试状态放在静态区，避免 longjmp 后读取被修改的自动对象。 */
static int run_operation(void) {
    int status = setjmp(g_jump);
    if (status == 0) {
        if (g_operation == 0) {
            ZrCore_Array_Init(&g_state, &g_array, g_elementSize, g_count);
        } else if (g_operation == 1) {
            ZrCore_Array_Push(&g_state, &g_array, g_values);
        } else {
            ZrCore_Array_Append(&g_state, &g_array, g_values, g_count);
        }
    }
    return status;
}

static int run_try_operation(void) {
    int status = setjmp(g_jump);
    if (status == 0) {
        if (g_operation == 0) {
            g_result = ZrCore_Array_TryInit(&g_state, &g_array, g_elementSize, g_count);
        } else if (g_operation == 1) {
            g_result = ZrCore_Array_TryPush(&g_state, &g_array, g_values);
        } else if (g_operation == 2) {
            g_result = ZrCore_Array_TryAppend(&g_state, &g_array, g_values, g_count);
        } else {
            g_result = ZrCore_Array_TryReserve(&g_state, &g_array, g_count);
        }
    }
    return status;
}

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "line %d: %s\n", __LINE__, #condition); \
        return 1; \
    } \
} while (0)

static int is_empty(void) {
    return g_array.head == NULL && g_array.capacity == 0 && g_array.length == 0 &&
           g_array.elementSize == 0 && !g_array.isValid;
}

static void init_one(void) {
    g_elementSize = sizeof(int);
    g_count = 1;
    g_operation = 0;
    if (run_operation() != 0) {
        abort();
    }
    ZrCore_Array_Push(&g_state, &g_array, g_values);
}

static int normal(void) {
    g_elementSize = sizeof(int);
    g_count = 0;
    CHECK(run_operation() == 0);
    CHECK(g_array.isValid && g_array.capacity == 1 && g_array.length == 0);
    ZrCore_Array_Push(&g_state, &g_array, g_values);
    ZrCore_Array_Append(&g_state, &g_array, g_values + 1, 3);
    CHECK(g_array.length == 4 && g_array.capacity >= 4);
    CHECK(memcmp(g_array.head, g_values, sizeof(g_values)) == 0);
    ZrCore_Array_Set(&g_array, 0, g_values + 3);
    CHECK(*(int *) ZrCore_Array_Get(&g_array, 0) == 44);
    CHECK(*(int *) ZrCore_Array_Pop(&g_array) == 44);
    ZrCore_Array_Empty(&g_array);
    CHECK(g_array.length == 0);
    ZrCore_Array_Free(&g_state, &g_array);
    CHECK(is_empty() && g_block == NULL && !g_allocatorError);
    ZrCore_Array_Free(&g_state, &g_array);
    return 0;
}

static int init_overflow(void) {
    g_elementSize = 4;
    g_count = SIZE_MAX / 4 + 2;
    CHECK(run_operation() == ZR_THREAD_STATUS_MEMORY_ERROR);
    CHECK(g_calls == 0 && is_empty());
    return 0;
}

static int init_oom(void) {
    g_elementSize = sizeof(int);
    g_count = 2;
    g_fail = 1;
    CHECK(run_operation() == ZR_THREAD_STATUS_MEMORY_ERROR);
    CHECK(g_calls == 1 && is_empty());
    g_fail = 0;
    CHECK(run_operation() == 0);
    ZrCore_Array_Free(&g_state, &g_array);
    CHECK(is_empty() && !g_allocatorError);
    return 0;
}

static int growth_oom(int operation, size_t count) {
    SZrArray before;
    init_one();
    before = g_array;
    g_operation = operation;
    g_count = count;
    g_fail = 1;
    CHECK(run_operation() == ZR_THREAD_STATUS_MEMORY_ERROR);
    CHECK(g_array.head == before.head && g_array.capacity == before.capacity &&
          g_array.length == before.length && g_array.isValid);
    CHECK(*(int *)g_array.head == 11);
    g_fail = 0;
    CHECK(run_operation() == 0);
    CHECK(g_array.length == before.length + (g_operation == 1 ? 1 : g_count));
    ZrCore_Array_Free(&g_state, &g_array);
    CHECK(is_empty() && g_block == NULL && !g_allocatorError);
    return 0;
}

static int push_oom(void) {
    return growth_oom(1, 1);
}

static int append_oom(void) {
    init_one();
    g_operation = 2;
    g_count = 3;
    /* 复用相同故障路径，初始元素不得因失败改变。 */
    {
        SZrArray before = g_array;
        g_fail = 1;
        CHECK(run_operation() == ZR_THREAD_STATUS_MEMORY_ERROR);
        CHECK(g_array.head == before.head && g_array.capacity == before.capacity &&
              g_array.length == before.length && *(int *)g_array.head == 11);
        g_fail = 0;
        CHECK(run_operation() == 0 && g_array.length == 4);
    }
    ZrCore_Array_Free(&g_state, &g_array);
    CHECK(is_empty() && !g_allocatorError);
    return 0;
}

static int length_overflow(void) {
    init_one();
    g_operation = 2;
    g_count = SIZE_MAX;
    {
        SZrArray before = g_array;
        size_t calls = g_calls;
        CHECK(run_operation() == ZR_THREAD_STATUS_MEMORY_ERROR);
        CHECK(g_calls == calls && g_array.head == before.head &&
              g_array.length == before.length && *(int *)g_array.head == 11);
    }
    ZrCore_Array_Free(&g_state, &g_array);
    CHECK(!g_allocatorError);
    return 0;
}

static int capacity_limit(void) {
    init_one();
    {
        SZrArray before = g_array;
        size_t calls = g_calls;
        g_array.capacity = SIZE_MAX / sizeof(int);
        g_array.length = g_array.capacity;
        g_operation = 1;
        CHECK(run_operation() == ZR_THREAD_STATUS_MEMORY_ERROR);
        CHECK(g_calls == calls && g_array.head == before.head &&
              g_array.capacity == SIZE_MAX / sizeof(int) &&
              g_array.length == g_array.capacity && *(int *)g_array.head == 11);
        g_array = before;
    }
    ZrCore_Array_Free(&g_state, &g_array);
    CHECK(!g_allocatorError);
    return 0;
}

static int free_null_head(void) {
    g_array.isValid = ZR_TRUE;
    g_array.capacity = 4;
    g_array.elementSize = sizeof(int);
    ZrCore_Array_Free(&g_state, &g_array);
    CHECK(is_empty() && g_calls == 0);
    return 0;
}

/* 只合成元数据，不访问巨型地址；检查 reserve 饱和请求，再按真实元数据释放。 */
static int growth_saturation(void) {
    SZrArray before;
    size_t maximum = SIZE_MAX / sizeof(int);
    init_one();
    before = g_array;
    g_array.capacity = maximum - 1;
    g_array.length = g_array.capacity;
    g_syntheticOldSize = g_array.capacity * sizeof(int);
    g_operation = 1;
    CHECK(run_operation() == ZR_THREAD_STATUS_MEMORY_ERROR);
    CHECK(g_lastRequest == maximum * sizeof(int) && !g_allocatorError);
    CHECK(g_array.head == before.head && g_array.capacity == maximum - 1 &&
          g_array.length == maximum - 1 && *(int *)g_array.head == 11);
    g_array = before;
    g_syntheticOldSize = 0;
    ZrCore_Array_Free(&g_state, &g_array);
    CHECK(is_empty() && !g_allocatorError);
    return 0;
}

static int append_empty(void) {
    init_one();
    {
        size_t calls = g_calls;
        ZrCore_Array_Append(&g_state, &g_array, NULL, 0);
        CHECK(g_calls == calls && g_array.length == 1 && *(int *)g_array.head == 11);
    }
    ZrCore_Array_Free(&g_state, &g_array);
    CHECK(!g_allocatorError);
    return 0;
}

static int try_init_failure(void) {
    g_elementSize = 4;
    g_count = SIZE_MAX / 4 + 2;
    CHECK(run_try_operation() == 0 && !g_result && g_calls == 0 && is_empty());
    g_count = 1;
    g_fail = 1;
    CHECK(run_try_operation() == 0 && !g_result && g_calls == 1 && is_empty());
    g_fail = 0;
    CHECK(run_try_operation() == 0 && g_result && g_array.isValid);
    ZrCore_Array_Free(&g_state, &g_array);
    CHECK(is_empty() && !g_allocatorError);
    return 0;
}

static int try_growth_failure(int operation) {
    SZrArray before;
    init_one();
    before = g_array;
    g_operation = operation;
    g_count = 3;
    g_fail = 1;
    CHECK(run_try_operation() == 0 && !g_result);
    CHECK(g_array.head == before.head && g_array.length == before.length &&
          g_array.capacity == before.capacity && *(int *)g_array.head == 11);
    g_fail = 0;
    CHECK(run_try_operation() == 0 && g_result);
    CHECK(g_array.length == (operation == 1 ? 2 : 4));
    ZrCore_Array_Free(&g_state, &g_array);
    CHECK(is_empty() && !g_allocatorError);
    return 0;
}

static int try_push_failure(void) { return try_growth_failure(1); }
static int try_append_failure(void) { return try_growth_failure(2); }

static int try_reserve(void) {
    SZrArray before;
    size_t calls;
    init_one();
    before = g_array;
    g_operation = 3;
    g_count = 4;
    g_fail = 1;
    CHECK(run_try_operation() == 0 && !g_result);
    CHECK(g_array.head == before.head && g_array.capacity == before.capacity &&
          g_array.length == 1 && *(int *)g_array.head == 11);
    g_fail = 0;
    CHECK(run_try_operation() == 0 && g_result);
    CHECK(g_array.capacity >= 4 && g_array.length == 1 && *(int *)g_array.head == 11);
    before = g_array;
    calls = g_calls;
    g_count = SIZE_MAX / sizeof(int) + 1;
    CHECK(run_try_operation() == 0 && !g_result);
    CHECK(g_calls == calls && g_array.head == before.head &&
          g_array.capacity == before.capacity && g_array.length == 1);
    g_count = 0;
    CHECK(run_try_operation() == 0 && g_result && g_calls == calls);
    ZrCore_Array_Free(&g_state, &g_array);
    CHECK(is_empty() && !g_allocatorError);
    return 0;
}

int main(int argc, char **argv) {
    static const struct { const char *name; int (*run)(void); } cases[] = {
        {"normal", normal}, {"init_overflow", init_overflow}, {"init_oom", init_oom},
        {"push_oom", push_oom}, {"append_oom", append_oom},
        {"length_overflow", length_overflow}, {"capacity_limit", capacity_limit},
        {"free_null_head", free_null_head}, {"append_empty", append_empty},
        {"growth_saturation", growth_saturation},
        {"try_init_failure", try_init_failure}, {"try_push_failure", try_push_failure},
        {"try_append_failure", try_append_failure}, {"try_reserve", try_reserve}
    };
    size_t i;
    int result = 1;
    g_global.allocator = allocator;
    g_state.global = &g_global;
    ZrCore_Array_Construct(&g_array);
    if (argc != 2) return 2;
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        if (strcmp(argv[1], cases[i].name) == 0) {
            result = cases[i].run();
            break;
        }
    }
    free(g_block);
    if (result == 0) printf("PASS %s\n", argv[1]);
    return result;
}
