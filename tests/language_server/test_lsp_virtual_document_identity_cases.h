#ifndef ZR_TEST_LSP_VIRTUAL_DOCUMENT_IDENTITY_CASES_H
#define ZR_TEST_LSP_VIRTUAL_DOCUMENT_IDENTITY_CASES_H

#include "../../zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_virtual_document_identity.h"
#include <stdint.h>

static void test_virtual_document_identity_round_trip(SZrState *state) {
    const TZrChar *summary = "LSP Virtual Identity Preserves Reserved Bytes And Uint64 Generation";
    SZrParityTimer timer;
    SZrLspVirtualDocumentIdentity identity = {0};
    SZrLspVirtualDocumentIdentity decoded;
    SZrString *uri;
    TZrBool passed;
    TEST_START(summary);
    identity.moduleName = ZrCore_String_CreateFromNative(state, "zr.test.\xF0\xA0\x80\x80");
    identity.projectUri = ZrCore_String_CreateFromNative(state, "file:///tmp/project & #%\".zrp");
    identity.originUri = ZrCore_String_CreateFromNative(state, "file:///tmp/native & #%\".so");
    identity.providerGeneration = UINT64_MAX;
    uri = ZrLanguageServer_LspVirtualDocumentIdentity_Create(state, &identity);
    passed = uri != ZR_NULL &&
            ZrLanguageServer_LspVirtualDocumentIdentity_Parse(state, uri, &decoded) &&
            ZrCore_String_Equal(identity.moduleName, decoded.moduleName) &&
            ZrCore_String_Equal(identity.projectUri, decoded.projectUri) &&
            ZrCore_String_Equal(identity.originUri, decoded.originUri) &&
            decoded.providerGeneration == UINT64_MAX;
    if (passed) TEST_PASS(timer, summary);
    else TEST_FAIL(timer, summary, "URI encoding must preserve every identity byte and all 64 generation bits");
}

static void test_virtual_document_identity_rejects_malformed(SZrState *state) {
    const TZrChar *summary = "LSP Virtual Identity Rejects Malformed Or Incomplete Scope";
    static TZrChar *invalid[] = {
        "zr-decompiled:/a.zr?%",
        "zr-decompiled:/a.zr?%0X",
        "zr-decompiled:/a.zr?%00",
        "zr-decompiled:/a.zr?{}",
        "zr-decompiled:/.zr?{\"project\":\"p\",\"origin\":\"o\",\"generation\":\"1\"}",
        "zr-decompiled:/a.zr?{\"project\":\"p\",\"origin\":\"o\",\"generation\":\"0\"}",
        "zr-decompiled:/a.zr?{\"project\":\"p\",\"origin\":\"o\",\"generation\":\"-1\"}",
        "zr-decompiled:/a.zr?{\"project\":\"p\",\"origin\":\"o\",\"generation\":\"18446744073709551616\"}",
        "zr-decompiled:/a.zr?{\"project\":\"p\",\"origin\":\"o\",\"generation\":1}",
        "zr-decompiled:/a.zr?{\"project\":\"p\",\"project\":\"q\",\"generation\":\"1\"}",
        "zr-decompiled:/a.zr?{\"project\":\"p\\u0000hidden\",\"origin\":\"o\",\"generation\":\"1\"}",
        "zr-decompiled:/a.zr?{\"project\":\"p\",\"origin\":\"o\",\"generation\":\"1\"}trailing"
    };
    SZrParityTimer timer;
    TZrBool passed = ZR_TRUE;
    TEST_START(summary);
    for (TZrSize index = 0U; index < sizeof(invalid) / sizeof(invalid[0]); index++) {
        SZrLspVirtualDocumentIdentity decoded;
        memset(&decoded, 0xA5, sizeof(decoded));
        if (ZrLanguageServer_LspVirtualDocumentIdentity_Parse(state,
                    ZrCore_String_CreateFromNative(state, invalid[index]), &decoded) ||
            decoded.moduleName != ZR_NULL || decoded.projectUri != ZR_NULL ||
            decoded.originUri != ZR_NULL || decoded.providerGeneration != 0U) {
            fprintf(stderr, "malformed virtual URI case %u was not rejected cleanly\n", (unsigned int)index);
            passed = ZR_FALSE;
        }
    }
    if (passed) TEST_PASS(timer, summary);
    else TEST_FAIL(timer, summary, "malformed scopes must fail with every output cleared");
}

#endif
