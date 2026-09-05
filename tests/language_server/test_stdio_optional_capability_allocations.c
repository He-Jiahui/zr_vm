#include "zr_vm_language_server_stdio_internal.h"

enum { MAX_TRACKED_ALLOCATIONS = 512, OPTIONAL_ALLOCATION_POINTS = 6 };

typedef struct TrackedAllocation {
    void *pointer;
    size_t ordinal;
} TrackedAllocation;

static TrackedAllocation allocations[MAX_TRACKED_ALLOCATIONS];
static size_t allocationOrdinal;
static size_t failureOrdinal;
static size_t injectedFailures;
static int failRemaining;
static int failures;

static void expect_true(int condition, const char *message) {
    if (!condition) {
        printf("Fail - %s\n", message);
        failures++;
    }
}

static void *tracked_malloc(size_t size) {
    size_t index;
    void *pointer;

    allocationOrdinal++;
    if (failureOrdinal != 0 &&
        (allocationOrdinal == failureOrdinal ||
         (failRemaining && allocationOrdinal > failureOrdinal))) {
        injectedFailures++;
        return NULL;
    }
    pointer = malloc(size);
    if (pointer == NULL) {
        return NULL;
    }
    for (index = 0; index < MAX_TRACKED_ALLOCATIONS; index++) {
        if (allocations[index].pointer == NULL) {
            allocations[index].pointer = pointer;
            allocations[index].ordinal = allocationOrdinal;
            return pointer;
        }
    }
    fprintf(stderr, "allocation tracking capacity exceeded\n");
    free(pointer);
    exit(2);
}

static void tracked_free(void *pointer) {
    size_t index;

    if (pointer == NULL) {
        return;
    }
    for (index = 0; index < MAX_TRACKED_ALLOCATIONS; index++) {
        if (allocations[index].pointer == pointer) {
            allocations[index].pointer = NULL;
            free(pointer);
            return;
        }
    }
    fprintf(stderr, "untracked cJSON free\n");
    exit(2);
}

static size_t ordinal_of(const void *pointer) {
    size_t index;

    for (index = 0; index < MAX_TRACKED_ALLOCATIONS; index++) {
        if (allocations[index].pointer == pointer && pointer != NULL) {
            return allocations[index].ordinal;
        }
    }
    expect_true(0, "optional publication allocation must be tracked");
    return 0;
}

static void check_and_release_leaks(void) {
    size_t index;
    size_t leaked = 0;

    for (index = 0; index < MAX_TRACKED_ALLOCATIONS; index++) {
        if (allocations[index].pointer != NULL) {
            leaked++;
            free(allocations[index].pointer);
            allocations[index].pointer = NULL;
        }
    }
    expect_true(leaked == 0, "capability publication must release every cJSON allocation");
}

static void run_publication(size_t failAt, int persistent, size_t *controlOrdinals) {
    SZrStdioServer server = {0};
    cJSON *params;
    cJSON *capabilities;
    const cJSON *rangeProvider;
    const cJSON *rangesSupport;
    const cJSON *inlineProvider;

    failureOrdinal = 0;
    params = cJSON_Parse("{\"capabilities\":{\"textDocument\":{"
                         "\"inlineCompletion\":{},\"rangeFormatting\":{\"rangesSupport\":true}}}}");
    capabilities = cJSON_CreateObject();
    expect_true(params != NULL && capabilities != NULL, "fault fixture setup must succeed");
    if (params == NULL || capabilities == NULL) {
        cJSON_Delete(params);
        cJSON_Delete(capabilities);
        check_and_release_leaks();
        return;
    }

    allocationOrdinal = 0;
    injectedFailures = 0;
    failureOrdinal = failAt;
    failRemaining = persistent;
    add_advanced_editor_capabilities(&server, params, capabilities);
    failureOrdinal = 0;
    rangeProvider = get_object_item(capabilities, ZR_LSP_FIELD_DOCUMENT_RANGE_FORMATTING_PROVIDER);
    rangesSupport = get_object_item(rangeProvider, "rangesSupport");
    inlineProvider = get_object_item(capabilities, ZR_LSP_FIELD_INLINE_COMPLETION_PROVIDER);

    expect_true((server.supportsRangesFormatting != ZR_FALSE) ==
                        (cJSON_IsObject(rangeProvider) && cJSON_IsTrue(rangesSupport)),
                "ranges dispatch support must equal successful optional publication");
    expect_true((server.supportsInlineCompletion != ZR_FALSE) == cJSON_IsTrue(inlineProvider),
                "inline dispatch support must equal successful optional publication");

    if (failAt == 0) {
        expect_true(server.supportsRangesFormatting && server.supportsInlineCompletion,
                    "control must publish both optional capabilities");
        /* Locate allocation sites from owned output, independent of earlier providers. */
        if (rangeProvider != NULL && rangesSupport != NULL && inlineProvider != NULL) {
            controlOrdinals[0] = ordinal_of(rangeProvider);
            controlOrdinals[1] = ordinal_of(rangesSupport);
            controlOrdinals[2] = ordinal_of(rangesSupport->string);
            controlOrdinals[3] = ordinal_of(rangeProvider->string);
            controlOrdinals[4] = ordinal_of(inlineProvider);
            controlOrdinals[5] = ordinal_of(inlineProvider->string);
        }
    } else {
        expect_true(injectedFailures != 0, "selected optional allocation failure must be reached");
        if (!persistent) {
            expect_true(injectedFailures == 1, "transient fault must fail exactly one allocation");
            if (failAt <= controlOrdinals[3]) {
                expect_true(cJSON_IsTrue(rangeProvider),
                            "transient range publication failure must retain ordinary range formatting");
            }
        }
    }

    cJSON_Delete(params);
    cJSON_Delete(capabilities);
    check_and_release_leaks();
}

int main(void) {
    cJSON_Hooks hooks = {tracked_malloc, tracked_free};
    size_t controlOrdinals[OPTIONAL_ALLOCATION_POINTS] = {0};
    size_t index;

    cJSON_InitHooks(&hooks);
    run_publication(0, 0, controlOrdinals);
    for (index = 0; index < OPTIONAL_ALLOCATION_POINTS; index++) {
        expect_true(controlOrdinals[index] != 0, "every optional allocation point must exist");
        if (controlOrdinals[index] != 0) {
            run_publication(controlOrdinals[index], 0, controlOrdinals);
            run_publication(controlOrdinals[index], 1, controlOrdinals);
        }
    }
    cJSON_InitHooks(NULL);
    if (failures != 0) {
        printf("Fail - optional capability allocation checks: %d failures\n", failures);
        return 1;
    }
    printf("Pass - optional capability control and 12 allocation failure checks\n");
    return 0;
}
