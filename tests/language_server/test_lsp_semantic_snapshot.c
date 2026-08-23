// Semantic snapshot identity and dependency-fence contract tests.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zr_vm_core/callback.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_language_server/lsp_interface.h"
#include "zr_vm_language_server/lsp_semantic_snapshot.h"

static int g_failures = 0;

static TZrPtr test_allocator(TZrPtr userData,
                             TZrPtr pointer,
                             TZrSize originalSize,
                             TZrSize newSize,
                             TZrInt64 flag) {
    ZR_UNUSED_PARAMETER(userData);
    ZR_UNUSED_PARAMETER(originalSize);
    ZR_UNUSED_PARAMETER(flag);

    if (newSize == 0U) {
        free(pointer);
        return ZR_NULL;
    }
    return pointer == ZR_NULL ? malloc(newSize) : realloc(pointer, newSize);
}

static void check(TZrBool condition, const TZrChar *message) {
    if (!condition) {
        printf("FAIL: %s\n", message);
        g_failures++;
    } else {
        printf("PASS: %s\n", message);
    }
}

static SZrString *test_string(SZrState *state, const TZrChar *text) {
    return ZrCore_String_Create(state, (TZrNativeString)text, strlen(text));
}

static void update_document(SZrState *state,
                            SZrLspContext *context,
                            SZrString *uri,
                            const TZrChar *content,
                            TZrSize version) {
    check(ZrLanguageServer_Lsp_UpdateDocument(
                  state, context, uri, content, strlen(content), version),
          "test document update must succeed");
}

static void test_snapshot_pins_current_content_and_ignores_unrelated_changes(
        SZrState *state) {
    SZrLspContext *context = ZrLanguageServer_LspContext_New(state);
    SZrString *primaryUri = test_string(state, "file:///snapshot-primary.zr");
    SZrString *unrelatedUri = test_string(state, "file:///snapshot-unrelated.zr");
    SZrLspSemanticSnapshot *snapshot;
    const SZrLspSemanticSnapshotIdentity *identity;
    TZrChar resultId[64] = {0};

    check(context != ZR_NULL && primaryUri != ZR_NULL && unrelatedUri != ZR_NULL,
          "snapshot test setup must allocate a context and canonical URIs");
    if (context == ZR_NULL || primaryUri == ZR_NULL || unrelatedUri == ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
        return;
    }

    update_document(state, context, primaryUri, "var primary: int = 1;", 1U);
    update_document(state, context, unrelatedUri, "var unrelated: int = 1;", 1U);

    snapshot = ZrLanguageServer_LspSemanticSnapshot_Acquire(state, context, primaryUri);
    check(snapshot != ZR_NULL,
          "acquire must capture an analyzed document snapshot");
    if (snapshot == ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
        return;
    }

    identity = ZrLanguageServer_LspSemanticSnapshot_GetIdentity(snapshot);
    check(identity != ZR_NULL && identity->documentGeneration != 0U &&
                  identity->semanticGeneration != 0U &&
                  identity->dependencyFingerprint != 0U,
          "snapshot identity must expose document, semantic, and fingerprint generations");
    check(ZrLanguageServer_LspSemanticSnapshot_ContentLength(snapshot) ==
                  strlen("var primary: int = 1;") &&
                  memcmp(ZrLanguageServer_LspSemanticSnapshot_Content(snapshot),
                         "var primary: int = 1;",
                         strlen("var primary: int = 1;")) == 0,
          "snapshot must retain the acquired content block");
    check(ZrLanguageServer_LspSemanticSnapshot_Validate(state, context, snapshot),
          "fresh snapshot fence must validate");
    ZrLanguageServer_LspSemanticSnapshot_SetActive(context, snapshot);
    check(ZrLanguageServer_LspSemanticSnapshot_GetActive(context) == snapshot,
          "active request scope must expose the acquired snapshot to cross-document consumers");
    ZrLanguageServer_LspSemanticSnapshot_SetActive(context, ZR_NULL);
    check(ZrLanguageServer_LspSemanticSnapshot_GetActive(context) == ZR_NULL,
          "active request scope must clear before snapshot release");
    ZrLanguageServer_LspSemanticSnapshot_FormatResultId(snapshot,
                                                         15U,
                                                         resultId,
                                                         sizeof(resultId));
    check(strncmp(resultId, "zr-snapshot:", strlen("zr-snapshot:")) == 0 &&
                  strstr(resultId, ":15") != ZR_NULL,
          "semantic result identifiers must derive from the shared snapshot identity");

    update_document(state, context, unrelatedUri, "var unrelated: int = 2;", 2U);
    check(ZrLanguageServer_LspSemanticSnapshot_Validate(state, context, snapshot),
          "an unrelated document update must not invalidate the active snapshot");

    ZrLanguageServer_LspSemanticSnapshot_Release(state, snapshot);
    ZrLanguageServer_LspContext_Free(state, context);
}

static void test_snapshot_rejects_direct_dependency_and_provider_changes(
        SZrState *state) {
    SZrLspContext *context = ZrLanguageServer_LspContext_New(state);
    SZrString *primaryUri = test_string(state, "file:///snapshot-main.zr");
    SZrString *dependencyUri = test_string(state, "file:///snapshot-dependency.zr");
    SZrString *unrelatedUri = test_string(state, "file:///snapshot-other.zr");
    SZrLspSemanticSnapshot *snapshot;
    TZrUInt64 fingerprintBeforeDependency;
    TZrUInt64 fingerprintAfterDependency;
    SZrArray tokens = {0};

    check(context != ZR_NULL && primaryUri != ZR_NULL && dependencyUri != ZR_NULL &&
                  unrelatedUri != ZR_NULL,
          "dependency fence setup must allocate context and URIs");
    if (context == ZR_NULL || primaryUri == ZR_NULL || dependencyUri == ZR_NULL ||
        unrelatedUri == ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
        return;
    }

    update_document(state, context, primaryUri, "var main: int = 1;", 1U);
    update_document(state, context, dependencyUri, "pub var dependency: int = 1;", 1U);
    update_document(state, context, unrelatedUri, "var other: int = 1;", 1U);

    snapshot = ZrLanguageServer_LspSemanticSnapshot_Acquire(state, context, primaryUri);
    check(snapshot != ZR_NULL, "dependency fence requires an acquired snapshot");
    if (snapshot == ZR_NULL) {
        ZrLanguageServer_LspContext_Free(state, context);
        return;
    }

    fingerprintBeforeDependency =
            ZrLanguageServer_LspSemanticSnapshot_GetIdentity(snapshot)->dependencyFingerprint;
    ZrLanguageServer_LspSemanticSnapshot_SetActive(context, snapshot);
    ZrCore_Array_Init(state, &tokens, sizeof(TZrUInt32), 8U);
    check(ZrLanguageServer_Lsp_GetSemanticTokens(state, context, dependencyUri, &tokens),
          "cross-document semantic reads must resolve through the active snapshot");
    ZrCore_Array_Free(state, &tokens);
    ZrLanguageServer_LspSemanticSnapshot_SetActive(context, ZR_NULL);
    fingerprintAfterDependency =
            ZrLanguageServer_LspSemanticSnapshot_GetIdentity(snapshot)->dependencyFingerprint;
    check(fingerprintAfterDependency != fingerprintBeforeDependency,
          "cross-document analyzer reads must update the shared snapshot fingerprint");

    update_document(state, context, unrelatedUri, "var other: int = 2;", 2U);
    check(ZrLanguageServer_LspSemanticSnapshot_Validate(state, context, snapshot),
          "unread dependency changes must not invalidate the fence");

    update_document(state, context, dependencyUri, "pub var dependency: int = 2;", 2U);
    check(!ZrLanguageServer_LspSemanticSnapshot_Validate(state, context, snapshot),
          "a tracked dependency update must invalidate the fence");
    ZrLanguageServer_LspSemanticSnapshot_Release(state, snapshot);

    snapshot = ZrLanguageServer_LspSemanticSnapshot_Acquire(state, context, primaryUri);
    check(snapshot != ZR_NULL, "provider invalidation requires a fresh snapshot");
    if (snapshot != ZR_NULL) {
        ZrLanguageServer_LspSemanticSnapshot_ProviderChanged(context);
        check(!ZrLanguageServer_LspSemanticSnapshot_Validate(state, context, snapshot),
              "provider generation changes must invalidate the fence");
        ZrLanguageServer_LspSemanticSnapshot_Release(state, snapshot);
    }

    snapshot = ZrLanguageServer_LspSemanticSnapshot_Acquire(state, context, primaryUri);
    check(snapshot != ZR_NULL, "direct update requires a fresh snapshot");
    if (snapshot != ZR_NULL) {
        update_document(state, context, primaryUri, "var main: int = 2;", 2U);
        check(!ZrLanguageServer_LspSemanticSnapshot_Validate(state, context, snapshot),
              "the owning document update must invalidate the fence");
        ZrLanguageServer_LspSemanticSnapshot_Release(state, snapshot);
    }

    ZrLanguageServer_LspContext_Free(state, context);
}

int main(void) {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 0, &callbacks);
    SZrState *state;

    if (global == ZR_NULL || global->mainThreadState == ZR_NULL) {
        fprintf(stderr, "unable to create test runtime\n");
        return 1;
    }
    state = global->mainThreadState;
    ZrCore_GlobalState_InitRegistry(state, global);

    test_snapshot_pins_current_content_and_ignores_unrelated_changes(state);
    test_snapshot_rejects_direct_dependency_and_provider_changes(state);

    ZrCore_GlobalState_Free(global);
    printf("LSP semantic snapshot: %d failure(s)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
