#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <crtdbg.h>
#endif

#include "zr_vm_core/callback.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_language_server/incremental_parser.h"
#include "zr_vm_language_server/reference_tracker.h"
#include "zr_vm_language_server/symbol_table.h"
#include "zr_vm_language_server/lsp_interface.h"
#include "interface/lsp_semantic_snapshot_cache.h"

typedef struct SLspArrayOomAllocator {
    TZrSize liveBytes;
    TZrSize failArrayAllocationsBeforeFailure;
    TZrPtr failResizePointer;
    TZrPtr capturedArrayAllocation;
    TZrSize failedResizeCount;
    TZrSize watchedRecordSize;
    TZrSize watchedRecordAllocations;
    TZrSize watchedRecordFrees;
    TZrBool captureNextArrayAllocation;
    TZrBool failNextMatchingAllocation;
    /* URI map buckets use the non-throwing HashSet_Rehash path, so one rejection is observable. */
    TZrPtr failBucketResizePointer;
    TZrSize failedBucketAllocationCount;
    TZrBool failNextBucketAllocation;
} SLspArrayOomAllocator;

static int g_failures;

/* RawMalloc/RawFree pass the exact record size with the untyped NONE tag, so a watched size
 * singles out one kind of native record without counting array buffers or GC objects. */
static TZrBool lsp_array_oom_is_watched_record(const SLspArrayOomAllocator *allocator,
                                               TZrSize size,
                                               TZrInt64 flag) {
    return allocator->watchedRecordSize != 0U && size == allocator->watchedRecordSize &&
           flag == ZR_MEMORY_NATIVE_TYPE_NONE;
}

/* Failure injection is attached to the real VM allocator used by production arrays. */
/* Failure injection is attached to the real VM allocator used by production arrays and URI maps. */
static TZrPtr lsp_array_oom_allocator(TZrPtr userData,
                                      TZrPtr pointer,
                                      TZrSize originalSize,
                                      TZrSize newSize,
                                      TZrInt64 flag) {
    SLspArrayOomAllocator *allocator = (SLspArrayOomAllocator *)userData;

    if (newSize != 0U && flag == ZR_MEMORY_NATIVE_TYPE_HASH_BUCKET &&
        ((pointer == ZR_NULL && allocator->failNextBucketAllocation) ||
         (pointer != ZR_NULL && pointer == allocator->failBucketResizePointer))) {
        allocator->failNextBucketAllocation = ZR_FALSE;
        allocator->failBucketResizePointer = ZR_NULL;
        allocator->failedBucketAllocationCount++;
        return ZR_NULL;
    }

    if (newSize != 0U && flag == ZR_MEMORY_NATIVE_TYPE_ARRAY) {
        TZrBool shouldFail = ZR_FALSE;
        if (pointer == ZR_NULL && allocator->failArrayAllocationsBeforeFailure > 0U) {
            allocator->failArrayAllocationsBeforeFailure--;
        } else if (pointer == ZR_NULL && allocator->failNextMatchingAllocation) {
            shouldFail = ZR_TRUE;
        }
        if (pointer != ZR_NULL && pointer == allocator->failResizePointer) {
            shouldFail = ZR_TRUE;
        }
        if (shouldFail) {
            if (pointer != ZR_NULL) {
                allocator->failedResizeCount++;
            }
            allocator->failNextMatchingAllocation = ZR_FALSE;
            allocator->failResizePointer = ZR_NULL;
            return ZR_NULL;
        }
    }

    if (newSize == 0U) {
        if (originalSize <= allocator->liveBytes) {
            allocator->liveBytes -= originalSize;
        }
        if (pointer != ZR_NULL && lsp_array_oom_is_watched_record(allocator, originalSize, flag)) {
            allocator->watchedRecordFrees++;
        }
        free(pointer);
        return ZR_NULL;
    }

    if (pointer == ZR_NULL) {
        TZrPtr result = malloc((size_t)newSize);
        if (result != ZR_NULL) {
            allocator->liveBytes += newSize;
            if (lsp_array_oom_is_watched_record(allocator, newSize, flag)) {
                allocator->watchedRecordAllocations++;
            }
            if (flag == ZR_MEMORY_NATIVE_TYPE_ARRAY &&
                allocator->captureNextArrayAllocation) {
                allocator->capturedArrayAllocation = result;
                allocator->captureNextArrayAllocation = ZR_FALSE;
            }
        }
        return result;
    }

    {
        TZrPtr result = realloc(pointer, (size_t)newSize);
        if (result != ZR_NULL) {
            if (newSize >= originalSize) {
                allocator->liveBytes += newSize - originalSize;
            } else {
                allocator->liveBytes -= originalSize - newSize;
            }
        }
        return result;
    }
}

static void check(TZrBool condition, const char *message) {
    if (condition) {
        printf("PASS: %s\n", message);
    } else {
        printf("FAIL: %s\n", message);
        g_failures++;
    }
}

static TZrSize live_bytes(SZrGlobalState *global) {
    return ((SLspArrayOomAllocator *)global->upstreamAllocationArguments)->liveBytes;
}

static void fail_array_allocation_after(SZrGlobalState *global, TZrSize successfulArrayAllocations) {
    SLspArrayOomAllocator *allocator =
            (SLspArrayOomAllocator *)global->upstreamAllocationArguments;
    allocator->failArrayAllocationsBeforeFailure = successfulArrayAllocations;
    allocator->failNextMatchingAllocation = ZR_TRUE;
}

typedef struct SLspArrayOomConstructCall {
    SZrSymbolTable *symbolTable;
    SZrLspSemanticSnapshotCache *cache;
    SZrReferenceTracker *tracker;
    SZrString *name;
    SZrSymbol *symbol;
    SZrIncrementalParser *parser;
} SLspArrayOomConstructCall;

static void call_cache_new(SZrState *state, TZrPtr userData) {
    SLspArrayOomConstructCall *call = (SLspArrayOomConstructCall *)userData;
    call->cache = ZrLanguageServer_LspSemanticSnapshotCache_New(state);
}

static void call_symbol_table_new(SZrState *state, TZrPtr userData) {
    SLspArrayOomConstructCall *call = (SLspArrayOomConstructCall *)userData;
    call->symbolTable = ZrLanguageServer_SymbolTable_New(state);
}

static void call_reference_tracker_new(SZrState *state, TZrPtr userData) {
    SLspArrayOomConstructCall *call = (SLspArrayOomConstructCall *)userData;
    call->tracker = ZrLanguageServer_ReferenceTracker_New(state, call->symbolTable);
}

static void call_symbol_new(SZrState *state, TZrPtr userData) {
    SLspArrayOomConstructCall *call = (SLspArrayOomConstructCall *)userData;
    call->symbol = ZrLanguageServer_Symbol_New(
            state, ZR_SYMBOL_VARIABLE, call->name, (SZrFileRange){0}, ZR_NULL,
            ZR_ACCESS_PRIVATE, ZR_NULL);
}

typedef struct SLspArrayOomAddReferenceCall {
    SZrReferenceTracker *tracker;
    SZrSymbol *symbol;
    TZrBool result;
} SLspArrayOomAddReferenceCall;

typedef struct SLspArrayOomAddSymbolReferenceCall {
    SZrState *state;
    SZrSymbol *symbol;
    TZrBool result;
} SLspArrayOomAddSymbolReferenceCall;

typedef struct SLspArrayOomUpdateDocumentCall {
    SZrLspContext *context;
    SZrString *uri;
    const char *content;
    TZrSize version;
    TZrBool result;
} SLspArrayOomUpdateDocumentCall;

typedef struct SLspArrayOomSnapshotCount {
    TZrSize count;
} SLspArrayOomSnapshotCount;

typedef struct SLspArrayOomParserUpdateCall {
    SZrIncrementalParser *parser;
    SZrString *uri;
    const char *content;
    TZrSize version;
    TZrBool openDocument;
    TZrBool result;
} SLspArrayOomParserUpdateCall;

static void call_incremental_parser_new(SZrState *state, TZrPtr userData) {
    SLspArrayOomConstructCall *call = (SLspArrayOomConstructCall *)userData;
    call->parser = ZrLanguageServer_IncrementalParser_New(state);
}

/* Disk and open-document updates share the first-seen URI insert that has to fail closed. */
static void call_parser_update(SZrState *state, TZrPtr userData) {
    SLspArrayOomParserUpdateCall *call = (SLspArrayOomParserUpdateCall *)userData;
    call->result = call->openDocument
                           ? ZrLanguageServer_IncrementalParser_UpdateOpenDocument(
                                     state, call->parser, call->uri, call->content,
                                     strlen(call->content), call->version)
                           : ZrLanguageServer_IncrementalParser_UpdateFile(
                                     state, call->parser, call->uri, call->content,
                                     strlen(call->content), call->version);
}

static SZrString *parser_oom_uri(SZrState *state, TZrSize index) {
    char uriText[64];
    int uriLength = snprintf(uriText, sizeof(uriText), "file:///uri-map-oom-%u.zr", (unsigned)index);

    if (uriLength <= 0 || (TZrSize)uriLength >= sizeof(uriText)) {
        return ZR_NULL;
    }
    return ZrCore_String_Create(state, (TZrNativeString)uriText, (TZrSize)uriLength);
}

static void call_add_reference(SZrState *state, TZrPtr userData) {
    SLspArrayOomAddReferenceCall *call = (SLspArrayOomAddReferenceCall *)userData;
    call->result = ZrLanguageServer_ReferenceTracker_AddReference(
            state, call->tracker, call->symbol, (SZrFileRange){0}, ZR_REFERENCE_WRITE);
}

static void call_add_symbol_reference(SZrState *state, TZrPtr userData) {
    SLspArrayOomAddSymbolReferenceCall *call =
            (SLspArrayOomAddSymbolReferenceCall *)userData;
    call->result = ZrLanguageServer_Symbol_AddReference(
            state, call->symbol, (SZrFileRange){0});
}

static void call_update_document(SZrState *state, TZrPtr userData) {
    SLspArrayOomUpdateDocumentCall *call =
            (SLspArrayOomUpdateDocumentCall *)userData;
    call->result = ZrLanguageServer_Lsp_UpdateDocument(
            state, call->context, call->uri, call->content, strlen(call->content), call->version);
}

static void count_snapshot_analyzer(SZrSemanticAnalyzer *analyzer, void *userData) {
    SLspArrayOomSnapshotCount *count = (SLspArrayOomSnapshotCount *)userData;
    if (analyzer != ZR_NULL) {
        count->count++;
    }
}

static TZrSize snapshot_analyzer_count(const SZrLspContext *context) {
    SLspArrayOomSnapshotCount count = {0};
    ZrLanguageServer_LspSemanticSnapshotCache_VisitAnalyzers(
            context, count_snapshot_analyzer, &count);
    return count.count;
}

static void test_cache_new_releases_cache_on_array_init_failure(SZrState *state) {
    TZrSize before = live_bytes(state->global);
    SLspArrayOomConstructCall call = {0};

    fail_array_allocation_after(state->global, 0U);
    check(ZrCore_Exception_TryRun(state, call_cache_new, &call) == ZR_THREAD_STATUS_FINE &&
                  call.cache == ZR_NULL,
          "snapshot cache construction returns null when its entries array cannot initialize");
    check(live_bytes(state->global) == before,
          "snapshot cache construction releases its native object after array initialization fails");
}

static void test_symbol_table_new_releases_initialized_prefix_on_array_failure(SZrState *state) {
    const TZrSize failurePointCount = 2U;

    for (TZrSize failurePoint = 0U; failurePoint < failurePointCount; failurePoint++) {
        TZrSize before = live_bytes(state->global);
        SLspArrayOomConstructCall call = {0};

        fail_array_allocation_after(state->global, failurePoint);
        check(ZrCore_Exception_TryRun(state, call_symbol_table_new, &call) == ZR_THREAD_STATUS_FINE &&
                      call.symbolTable == ZR_NULL,
              "symbol table construction returns null when either top-level array cannot initialize");
        check(live_bytes(state->global) == before,
              "symbol table construction releases all earlier native allocations on array failure");
    }
}

static void test_reference_tracker_new_releases_tracker_on_array_init_failure(SZrState *state) {
    TZrSize before = live_bytes(state->global);
    SZrSymbolTable *table = ZrLanguageServer_SymbolTable_New(state);
    SLspArrayOomConstructCall call = {0};

    check(table != ZR_NULL, "reference tracker OOM test creates its real symbol table");
    if (table == ZR_NULL) {
        return;
    }

    before = live_bytes(state->global);
    call.symbolTable = table;
    fail_array_allocation_after(state->global, 0U);
    check(ZrCore_Exception_TryRun(state, call_reference_tracker_new, &call) == ZR_THREAD_STATUS_FINE &&
                  call.tracker == ZR_NULL,
          "reference tracker construction returns null when its array cannot initialize");
    check(live_bytes(state->global) == before,
          "reference tracker construction releases its native object after array failure");
    ZrLanguageServer_SymbolTable_Free(state, table);
}

static void test_symbol_new_releases_symbol_on_array_init_failure(SZrState *state) {
    SLspArrayOomConstructCall call = {0};
    TZrSize before;

    call.name = ZrCore_String_Create(state, (TZrNativeString)"symbol-oom", 10U);
    check(call.name != ZR_NULL, "symbol construction OOM test creates its real name string");
    if (call.name == ZR_NULL) {
        return;
    }
    before = live_bytes(state->global);
    fail_array_allocation_after(state->global, 0U);
    check(ZrCore_Exception_TryRun(state, call_symbol_new, &call) == ZR_THREAD_STATUS_FINE &&
                  call.symbol == ZR_NULL,
          "symbol construction returns null when its reference array cannot initialize");
    check(live_bytes(state->global) == before,
          "symbol construction releases its native object after reference-array failure");
}

static void test_add_reference_rolls_back_when_growth_fails(SZrState *state, TZrBool failSymbol) {
    TZrSize before;
    TZrSize initialCapacity;
    SZrSymbolTable *table = ZrLanguageServer_SymbolTable_New(state);
    SZrString *name = ZrCore_String_Create(state, (TZrNativeString)"target", 6U);
    SZrSymbol *symbol = ZrLanguageServer_Symbol_New(
            state, ZR_SYMBOL_VARIABLE, name, (SZrFileRange){0}, ZR_NULL,
            ZR_ACCESS_PRIVATE, ZR_NULL);
    SZrReferenceTracker *tracker = ZrLanguageServer_ReferenceTracker_New(state, table);

    check(table != ZR_NULL && symbol != ZR_NULL && tracker != ZR_NULL,
          "reference push OOM test creates real symbol and tracker objects");
    if (table == ZR_NULL || symbol == ZR_NULL || tracker == ZR_NULL) {
        ZrLanguageServer_ReferenceTracker_Free(state, tracker);
        ZrLanguageServer_Symbol_Free(state, symbol);
        ZrLanguageServer_SymbolTable_Free(state, table);
        return;
    }

    initialCapacity = failSymbol ? symbol->references.capacity : tracker->allReferences.capacity;
    for (TZrSize index = 0U; index < initialCapacity; index++) {
        check(ZrLanguageServer_ReferenceTracker_AddReference(
                      state, tracker, symbol, (SZrFileRange){0}, ZR_REFERENCE_READ),
              "reference entries fill without injected allocation failure");
        if (tracker->allReferences.length != index + 1U) {
            break;
        }
    }
    {
        SLspArrayOomAllocator *allocator =
                (SLspArrayOomAllocator *)state->global->upstreamAllocationArguments;
        TZrSize trackerLength = tracker->allReferences.length;
        TZrSize symbolLength = symbol->references.length;
        TZrSize symbolReferenceCount = symbol->referenceCount;
        SLspArrayOomAddReferenceCall call = {tracker, symbol, ZR_FALSE};
        before = live_bytes(state->global);
        allocator->failResizePointer = failSymbol ? symbol->references.head : tracker->allReferences.head;

        check(ZrCore_Exception_TryRun(state, call_add_reference, &call) == ZR_THREAD_STATUS_FINE &&
                      !call.result,
              "reference registration reports the injected index growth failure");
        check(tracker->allReferences.length == trackerLength &&
                      symbol->references.length == symbolLength &&
                      symbol->referenceCount == symbolReferenceCount,
              "index growth failure leaves both reference indexes unchanged");
        check(live_bytes(state->global) == before,
              "index growth failure releases the unindexed reference record");
    }

    ZrLanguageServer_ReferenceTracker_Free(state, tracker);
    ZrLanguageServer_Symbol_Free(state, symbol);
    ZrLanguageServer_SymbolTable_Free(state, table);
}

static void test_symbol_reference_reports_growth_failure(SZrState *state) {
    SZrString *name = ZrCore_String_Create(state, (TZrNativeString)"symbol", 6U);
    SZrSymbol *symbol = ZrLanguageServer_Symbol_New(
            state, ZR_SYMBOL_VARIABLE, name, (SZrFileRange){0}, ZR_NULL,
            ZR_ACCESS_PRIVATE, ZR_NULL);
    SLspArrayOomAllocator *allocator =
            (SLspArrayOomAllocator *)state->global->upstreamAllocationArguments;
    TZrSize initialCapacity;

    check(symbol != ZR_NULL, "symbol reference OOM test creates a real symbol");
    if (symbol == ZR_NULL) {
        return;
    }

    initialCapacity = symbol->references.capacity;
    for (TZrSize index = 0U; index < initialCapacity; index++) {
        check(ZrLanguageServer_Symbol_AddReference(state, symbol, (SZrFileRange){0}),
              "symbol reference entries fill without injected allocation failure");
    }

    {
        TZrSize length = symbol->references.length;
        TZrSize referenceCount = symbol->referenceCount;
        SLspArrayOomAddSymbolReferenceCall call = {state, symbol, ZR_FALSE};
        allocator->failResizePointer = symbol->references.head;

        check(ZrCore_Exception_TryRun(state, call_add_symbol_reference, &call) ==
                          ZR_THREAD_STATUS_FINE &&
                      !call.result,
              "symbol reference registration returns false when its array cannot grow");
        check(symbol->references.length == length && symbol->referenceCount == referenceCount,
              "symbol reference growth failure leaves length and reference count unchanged");
    }

    ZrLanguageServer_Symbol_Free(state, symbol);
}

static void test_snapshot_capture_preserves_cache_when_growth_fails(SZrState *state) {
    SLspArrayOomAllocator *allocator =
            (SLspArrayOomAllocator *)state->global->upstreamAllocationArguments;
    SZrLspContext *context = ZrLanguageServer_LspContext_New(state);
    TZrBool observedFailure = ZR_FALSE;

    check(context != ZR_NULL, "snapshot cache OOM test creates a real LSP context");
    if (context == ZR_NULL) {
        return;
    }

    /* Replace the context's empty cache so the allocator can target its real entries buffer. */
    ZrLanguageServer_LspSemanticSnapshotCache_Free(state, context);
    allocator->captureNextArrayAllocation = ZR_TRUE;
    context->semanticSnapshotCache = ZrLanguageServer_LspSemanticSnapshotCache_New(state);
    check(context->semanticSnapshotCache != ZR_NULL &&
                  allocator->capturedArrayAllocation != ZR_NULL,
          "test tracks the real snapshot entries buffer allocated by Cache_New");
    if (context->semanticSnapshotCache == ZR_NULL ||
        allocator->capturedArrayAllocation == ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
        return;
    }

    for (TZrSize documentIndex = 0U; documentIndex < 12U && !observedFailure; documentIndex++) {
        char uriText[96];
        char sourceText[64];
        int uriLength = snprintf(uriText, sizeof(uriText),
                                 "file:///array-oom-%u.zr", (unsigned)documentIndex);
        SZrString *uri;
        SLspArrayOomUpdateDocumentCall updateCall;

        check(uriLength > 0 && (TZrSize)uriLength < sizeof(uriText),
              "snapshot OOM test constructs a bounded document URI");
        if (uriLength <= 0 || (TZrSize)uriLength >= sizeof(uriText)) {
            break;
        }
        uri = ZrCore_String_Create(state, (TZrNativeString)uriText, (TZrSize)uriLength);
        (void)snprintf(sourceText, sizeof(sourceText), "var value: int = %u;",
                       (unsigned)documentIndex);
        updateCall = (SLspArrayOomUpdateDocumentCall){
                context, uri, sourceText, 1U, ZR_FALSE};
        check(uri != ZR_NULL &&
                      ZrCore_Exception_TryRun(state, call_update_document, &updateCall) ==
                              ZR_THREAD_STATUS_FINE &&
                      updateCall.result,
              "initial real document update succeeds before snapshot capture");
        if (uri == ZR_NULL || !updateCall.result) {
            break;
        }

        {
            TZrSize countBefore = snapshot_analyzer_count(context);
            TZrSize failedResizeBefore = allocator->failedResizeCount;
            (void)snprintf(sourceText, sizeof(sourceText), "var value: int = %u;",
                           (unsigned)(documentIndex + 1U));
            updateCall = (SLspArrayOomUpdateDocumentCall){
                    context, uri, sourceText, 2U, ZR_FALSE};
            allocator->failResizePointer = allocator->capturedArrayAllocation;

            check(ZrCore_Exception_TryRun(state, call_update_document, &updateCall) ==
                                  ZR_THREAD_STATUS_FINE &&
                          updateCall.result,
                  "document update survives snapshot entries growth failure");
            if (allocator->failedResizeCount != failedResizeBefore) {
                observedFailure = ZR_TRUE;
                check(snapshot_analyzer_count(context) == countBefore,
                      "failed snapshot insertion leaves every previously captured analyzer reachable");
                check(updateCall.result,
                      "failed snapshot insertion leaves document update on its normal success path");
            }
        }
    }

    check(observedFailure,
          "real snapshot capture eventually reaches the injected entries-array growth failure");
    ZrLanguageServer_LspContext_Free(state, context);
}

/* Callers own each returned highlight record; release them before the array buffer. */
static TZrBool request_and_release_highlights(SZrState *state,
                                              SZrLspContext *context,
                                              SZrString *uri,
                                              SZrLspPosition position,
                                              TZrSize *outCount) {
    SZrArray highlights;
    TZrBool found;

    ZrCore_Array_Init(state, &highlights, sizeof(SZrLspDocumentHighlight *), 4U);
    found = ZrLanguageServer_Lsp_GetDocumentHighlights(state, context, uri, position, &highlights);
    *outCount = highlights.length;
    for (TZrSize index = 0U; index < highlights.length; index++) {
        SZrLspDocumentHighlight **slot =
                (SZrLspDocumentHighlight **)ZrCore_Array_Get(&highlights, index);
        if (slot != ZR_NULL && *slot != ZR_NULL) {
            ZrCore_Memory_RawFree(state->global, *slot, sizeof(SZrLspDocumentHighlight));
        }
    }
    ZrCore_Array_Free(state, &highlights);
    return found;
}

/* An external highlight request collects one SZrLspLocation per reference only to project it
 * into the caller's highlights, so every collected record must be freed before it returns. */
static void check_highlight_request_releases_locations(SZrState *state,
                                                       SZrLspContext *context,
                                                       SZrString *uri,
                                                       SZrLspPosition position,
                                                       TZrSize minimumHighlights,
                                                       const char *label) {
    SLspArrayOomAllocator *allocator =
            (SLspArrayOomAllocator *)state->global->upstreamAllocationArguments;
    TZrSize highlightCount = 0U;
    TZrBool found;
    char message[192];

    /* The first request may populate analyzer and metadata caches; measure a repeated one. */
    (void)request_and_release_highlights(state, context, uri, position, &highlightCount);

    allocator->watchedRecordSize = sizeof(SZrLspLocation);
    allocator->watchedRecordAllocations = 0U;
    allocator->watchedRecordFrees = 0U;
    found = request_and_release_highlights(state, context, uri, position, &highlightCount);
    allocator->watchedRecordSize = 0U;

    printf("INFO: %s highlights=%u locations allocated=%u freed=%u\n",
           label,
           (unsigned)highlightCount,
           (unsigned)allocator->watchedRecordAllocations,
           (unsigned)allocator->watchedRecordFrees);
    (void)snprintf(message, sizeof(message),
                   "%s highlight request returns the same-document ranges", label);
    check(found && highlightCount >= minimumHighlights, message);
    (void)snprintf(message, sizeof(message),
                   "%s highlight request collects temporary locations", label);
    check(allocator->watchedRecordAllocations > 0U, message);
    (void)snprintf(message, sizeof(message),
                   "%s highlight request frees every collected location", label);
    check(allocator->watchedRecordAllocations == allocator->watchedRecordFrees, message);
}

static void test_external_highlights_release_collected_locations(SZrState *state) {
    /* The import literal resolves to an external module declaration and the receiver member to
     * an external type member; each target kind collects its references through its own path. */
    static const char *content =
            "var system = import(\"zr.system\");\n"
            "system.console;\n"
            "var {LinkedList} = import(\"zr.container\");\n"
            "var list: LinkedList<int> = null;\n"
            "list.addLast(1);\n"
            "list.addLast(2);\n";
    static const char *uriText = "file:///array-oom-external-highlights.zr";
    const SZrLspPosition importLiteralPosition = {0, 21};
    const SZrLspPosition typeMemberPosition = {4, 5};
    SZrLspContext *context = ZrLanguageServer_LspContext_New(state);
    SZrString *uri = ZrCore_String_Create(state, (TZrNativeString)uriText, strlen(uriText));
    TZrBool opened = context != ZR_NULL && uri != ZR_NULL &&
                     ZrLanguageServer_Lsp_UpdateDocument(
                             state, context, uri, content, strlen(content), 1U);

    check(opened, "external highlight test opens a real document importing native modules");
    if (opened) {
        check_highlight_request_releases_locations(
                state, context, uri, importLiteralPosition, 1U, "external module declaration");
        check_highlight_request_releases_locations(
                state, context, uri, typeMemberPosition, 2U, "external type member");
    }
    if (context != ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
    }
}
/* A parser whose URI map never got buckets could not store any document, so none is handed out. */
static void test_incremental_parser_new_fails_when_uri_map_cannot_initialize(SZrState *state) {
    SLspArrayOomAllocator *allocator =
            (SLspArrayOomAllocator *)state->global->upstreamAllocationArguments;
    TZrSize before = live_bytes(state->global);
    SLspArrayOomConstructCall call = {0};

    allocator->failNextBucketAllocation = ZR_TRUE;
    check(ZrCore_Exception_TryRun(state, call_incremental_parser_new, &call) == ZR_THREAD_STATUS_FINE &&
                  call.parser == ZR_NULL,
          "incremental parser construction returns null when its URI map buckets cannot initialize");
    check(allocator->failedBucketAllocationCount == 1U,
          "incremental parser construction reaches the injected URI map bucket failure");
    check(live_bytes(state->global) == before,
          "incremental parser construction releases its native object after URI map initialization fails");
    if (call.parser != ZR_NULL) {
        ZrLanguageServer_IncrementalParser_Free(state, call.parser);
    }
}

/* The first URI past the load threshold must grow the buckets; a rejected growth may not publish it. */
static void test_parser_insert_fails_when_uri_map_cannot_grow(SZrState *state, TZrBool openDocument) {
    const char *content = "var value: int = 1;";
    SLspArrayOomAllocator *allocator =
            (SLspArrayOomAllocator *)state->global->upstreamAllocationArguments;
    SZrIncrementalParser *parser = ZrLanguageServer_IncrementalParser_New(state);
    SZrString *storedUri = ZR_NULL;
    SZrString *rejectedUri;
    SLspArrayOomParserUpdateCall call;

    check(parser != ZR_NULL, "URI map growth OOM test creates a real incremental parser");
    if (parser == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0U;
         parser->uriToFileMap.elementCount < parser->uriToFileMap.resizeThreshold && index < 64U;
         index++) {
        SZrString *uri = parser_oom_uri(state, index);

        if (uri == ZR_NULL ||
            !ZrLanguageServer_IncrementalParser_UpdateFile(
                    state, parser, uri, content, strlen(content), 1U)) {
            break;
        }
        storedUri = uri;
    }
    rejectedUri = parser_oom_uri(state, 64U);
    check(storedUri != ZR_NULL && rejectedUri != ZR_NULL &&
                  parser->uriToFileMap.elementCount == parser->uriToFileMap.resizeThreshold,
          "document inserts fill the URI map to its growth threshold without injected failure");
    if (storedUri == ZR_NULL || rejectedUri == ZR_NULL ||
        parser->uriToFileMap.elementCount != parser->uriToFileMap.resizeThreshold) {
        ZrLanguageServer_IncrementalParser_Free(state, parser);
        return;
    }

    call = (SLspArrayOomParserUpdateCall){parser, rejectedUri, content, 1U, openDocument, ZR_TRUE};
    {
        TZrSize before = live_bytes(state->global);
        TZrSize elementCount = parser->uriToFileMap.elementCount;
        TZrSize capacity = parser->uriToFileMap.capacity;

        allocator->failBucketResizePointer = parser->uriToFileMap.buckets;
        check(ZrCore_Exception_TryRun(state, call_parser_update, &call) == ZR_THREAD_STATUS_FINE &&
                      !call.result,
              "first-seen document update reports the injected URI map growth failure");
        check(allocator->failedBucketAllocationCount == 1U,
              "first-seen document update reaches the injected URI map bucket failure");
        check(live_bytes(state->global) == before,
              "rejected document insert releases the file version it could not store");
        check(ZrLanguageServer_IncrementalParser_GetFileVersion(parser, rejectedUri) == ZR_NULL &&
                      parser->uriToFileMap.elementCount == elementCount &&
                      parser->uriToFileMap.capacity == capacity,
              "rejected document insert leaves no untracked document in the URI map");
        check(ZrLanguageServer_IncrementalParser_GetFileVersion(parser, storedUri) != ZR_NULL,
              "rejected document insert keeps previously stored documents reachable");
    }

    {
        SZrFileVersion *fileVersion;

        call.result = ZR_FALSE;
        check(ZrCore_Exception_TryRun(state, call_parser_update, &call) == ZR_THREAD_STATUS_FINE &&
                      call.result,
              "the same document insert succeeds once the URI map can grow");
        fileVersion = ZrLanguageServer_IncrementalParser_GetFileVersion(parser, rejectedUri);
        check(fileVersion != ZR_NULL && fileVersion->version == 1U &&
                      fileVersion->isOpenDocument == openDocument,
              "retried document insert publishes the version with its update provenance");
    }

    ZrLanguageServer_IncrementalParser_Free(state, parser);
}
int main(int argc, char **argv) {
    SLspArrayOomAllocator allocator = {0};
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global;
    SZrState *state;

#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, 0);
    _CrtSetReportMode(_CRT_ERROR, 0);
#endif

    if (argc != 2) {
        fprintf(stderr, "expected one LSP array OOM case name\n");
        return 2;
    }
    global = ZrCore_GlobalState_New(lsp_array_oom_allocator, &allocator, 0U, &callbacks);
    if (global == ZR_NULL || global->mainThreadState == ZR_NULL) {
        fprintf(stderr, "unable to create test runtime\n");
        return 1;
    }
    state = global->mainThreadState;
    ZrCore_GlobalState_InitRegistry(state, global);

    if (strcmp(argv[1], "cache_new") == 0) {
        test_cache_new_releases_cache_on_array_init_failure(state);
    } else if (strcmp(argv[1], "symbol_table_new") == 0) {
        test_symbol_table_new_releases_initialized_prefix_on_array_failure(state);
    } else if (strcmp(argv[1], "reference_tracker_new") == 0) {
        test_reference_tracker_new_releases_tracker_on_array_init_failure(state);
    } else if (strcmp(argv[1], "symbol_new") == 0) {
        test_symbol_new_releases_symbol_on_array_init_failure(state);
    } else if (strcmp(argv[1], "add_reference") == 0) {
        test_add_reference_rolls_back_when_growth_fails(state, ZR_FALSE);
    } else if (strcmp(argv[1], "add_reference_symbol") == 0) {
        test_add_reference_rolls_back_when_growth_fails(state, ZR_TRUE);
    } else if (strcmp(argv[1], "symbol_reference") == 0) {
        test_symbol_reference_reports_growth_failure(state);
    } else if (strcmp(argv[1], "snapshot_capture") == 0) {
        test_snapshot_capture_preserves_cache_when_growth_fails(state);
    } else if (strcmp(argv[1], "external_highlight_locations") == 0) {
        test_external_highlights_release_collected_locations(state);
    } else if (strcmp(argv[1], "incremental_parser_new") == 0) {
        test_incremental_parser_new_fails_when_uri_map_cannot_initialize(state);
    } else if (strcmp(argv[1], "incremental_parser_insert") == 0) {
        test_parser_insert_fails_when_uri_map_cannot_grow(state, ZR_FALSE);
    } else if (strcmp(argv[1], "incremental_parser_insert_open") == 0) {
        test_parser_insert_fails_when_uri_map_cannot_grow(state, ZR_TRUE);
    } else {
        fprintf(stderr, "unknown LSP array OOM case: %s\n", argv[1]);
        ZrCore_GlobalState_Free(global);
        return 2;
    }

    ZrCore_GlobalState_Free(global);
    printf("LSP array OOM regressions: %d failure(s)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
