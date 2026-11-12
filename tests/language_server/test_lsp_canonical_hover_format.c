//
// 符号悬停在 canonical 类型存在但无法排版时的文案。
// 排版失败不得写成诊断 2020 的 “cannot infer exact type”；没有类型身份时仍保留该句。
// 另沿公开悬停入口确认：文本超过旧 128 字节类型缓冲的声明类型照常显示完整的 Resolved Type。
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "semantic/lsp_canonical_hover.h"
#include "zr_vm_core/callback.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_language_server/conf.h"
#include "zr_vm_language_server/lsp_interface.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/semantic.h"

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
    }
}

/* 长度取自文本本身，避免手写字节数把结尾的 NUL 也算进字符串。 */
static SZrString *test_string(SZrState *state, const TZrChar *text) {
    return ZrCore_String_Create(state, (TZrNativeString)text, strlen(text));
}

static const TZrChar *string_text(SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_NULL;
    }
    return value->shortStringLength < ZR_VM_LONG_STRING_FLAG
               ? ZrCore_String_GetNativeStringShort(value)
               : ZrCore_String_GetNativeString(value);
}

static const TZrChar *hover_text(SZrLspHover *hover) {
    SZrString **content;

    if (hover == ZR_NULL || hover->contents.length == 0U) {
        return ZR_NULL;
    }
    content = (SZrString **)ZrCore_Array_Get(&hover->contents, 0U);
    if (content == ZR_NULL || *content == ZR_NULL) {
        return ZR_NULL;
    }
    return string_text(*content);
}

static void free_hover(SZrState *state, SZrLspHover *hover) {
    if (state == ZR_NULL || hover == ZR_NULL) {
        return;
    }
    ZrCore_Array_Free(state, &hover->contents);
    ZrCore_Memory_RawFree(state->global, hover, sizeof(*hover));
}

static void fill_symbol(SZrParserSemanticSymbolQuery *symbol,
                        SZrString *name,
                        TZrTypeId typeId,
                        SZrString *signature) {
    memset(symbol, 0, sizeof(*symbol));
    symbol->symbolId = ZR_SEMANTIC_ID_FIRST;
    symbol->typeId = typeId;
    symbol->kind = ZR_SEMANTIC_SYMBOL_KIND_VARIABLE;
    symbol->displayName = name;
    symbol->signatureDisplay = signature;
}

static SZrLspHover *build_hover(SZrState *state,
                                SZrLspContext *context,
                                SZrString *uri,
                                SZrSemanticContext *semanticContext,
                                SZrString *name,
                                TZrTypeId typeId,
                                SZrString *signature) {
    SZrParserSemanticSymbolQuery symbol;
    SZrFileRange range;
    SZrLspHover *hover = ZR_NULL;

    memset(&range, 0, sizeof(range));
    fill_symbol(&symbol, name, typeId, signature);
    if (!ZrLanguageServer_LspCanonicalHover_BuildSymbol(
                state,
                context,
                uri,
                semanticContext,
                &symbol,
                range,
                ZR_NULL,
                ZR_NULL,
                &hover)) {
        free_hover(state, hover);
        return ZR_NULL;
    }
    return hover;
}

static void expect_hover(SZrState *state,
                         SZrLspContext *context,
                         SZrString *uri,
                         SZrSemanticContext *semanticContext,
                         SZrString *name,
                         TZrTypeId typeId,
                         SZrString *signature,
                         const TZrChar *required,
                         const TZrChar *forbidden,
                         const TZrChar *label) {
    SZrLspHover *hover = build_hover(
            state, context, uri, semanticContext, name, typeId, signature);
    const TZrChar *text = hover_text(hover);

    check(text != ZR_NULL, label);
    if (text == ZR_NULL) {
        printf("  hover was not produced\n");
    } else {
        if (required != ZR_NULL && strstr(text, required) == ZR_NULL) {
            printf("  missing \"%s\" in:\n%s\n", required, text);
            check(ZR_FALSE, label);
        }
        if (forbidden != ZR_NULL && strstr(text, forbidden) != ZR_NULL) {
            printf("  unexpected \"%s\" in:\n%s\n", forbidden, text);
            check(ZR_FALSE, label);
        }
    }
    free_hover(state, hover);
}

/* 驻留一个没有模块前缀、名字恰为 length 字节的具名类型；名字留在 nameBuffer 里供比对。 */
static TZrTypeId intern_named_type(SZrState *state,
                                   SZrSemanticContext *context,
                                   TZrSize length,
                                   TZrChar *nameBuffer,
                                   TZrSize nameBufferSize) {
    SZrString *name;

    if (length == 0U || length >= nameBufferSize) {
        return ZR_SEMANTIC_ID_INVALID;
    }
    memset(nameBuffer, 'w', length);
    nameBuffer[length] = '\0';
    name = ZrCore_String_Create(state, nameBuffer, length);
    if (name == ZR_NULL) {
        return ZR_SEMANTIC_ID_INVALID;
    }
    return ZrParser_CanonicalType_InternNominal(context, ZR_NULL, name, 0U);
}

/* 继续包裹数组，直到悬停缓冲也排不出来。先撞到的是排版的嵌套深度上限，
 * 那时文本还远短于缓冲。返回的类型节点必须仍然存在。 */
static TZrTypeId nest_until_format_fails(SZrSemanticContext *context,
                                         TZrTypeId elementTypeId) {
    TZrChar probe[ZR_LSP_HOVER_BUFFER_LENGTH];
    TZrTypeId typeId = elementTypeId;
    TZrUInt32 depth;

    for (depth = 0U; depth < 4096U; depth++) {
        TZrTypeId nested;

        if (!ZrParser_CanonicalType_Format(
                    context, typeId, probe, sizeof(probe))) {
            return typeId;
        }
        nested = ZrParser_CanonicalType_InternArray(
                context,
                typeId,
                1U,
                ZR_CANONICAL_ARRAY_STORAGE_MANAGED);
        if (nested == ZR_SEMANTIC_ID_INVALID) {
            return ZR_SEMANTIC_ID_INVALID;
        }
        typeId = nested;
    }
    return ZR_SEMANTIC_ID_INVALID;
}

static void test_symbol_hover_format_failure_wording(SZrState *state) {
    SZrLspContext *context = ZrLanguageServer_LspContext_New(state);
    SZrSemanticContext *semanticContext = ZrParser_SemanticContext_New(state);
    SZrString *uri = test_string(state, "file:///hover-format.zr");
    SZrString *name = test_string(state, "value");
    SZrString *signature = test_string(state, "fn(int) -> int");
    TZrChar moduleBytes[100];
    TZrChar nominalBytes[40];
    TZrChar expectedNominal[141];
    SZrString *moduleName;
    SZrString *nominalName;
    TZrTypeId intType;
    TZrTypeId unknownPrimitive;
    TZrTypeId longType;
    TZrTypeId unformattable;
    /* 类型 ID 单调分配，这个夹具只驻留几百个节点，远到不了这个值。 */
    const TZrTypeId danglingType = (TZrTypeId)0x7FFFFFFFU;
    TZrChar small[ZR_LSP_TYPE_BUFFER_LENGTH];
    TZrChar hoverSized[ZR_LSP_HOVER_BUFFER_LENGTH];

    memset(moduleBytes, 'm', sizeof(moduleBytes));
    memset(nominalBytes, 'n', sizeof(nominalBytes));
    memcpy(expectedNominal, moduleBytes, sizeof(moduleBytes));
    expectedNominal[sizeof(moduleBytes)] = '.';
    memcpy(expectedNominal + sizeof(moduleBytes) + 1U, nominalBytes, sizeof(nominalBytes));
    moduleName = ZrCore_String_Create(state, moduleBytes, sizeof(moduleBytes));
    nominalName = ZrCore_String_Create(state, nominalBytes, sizeof(nominalBytes));
    check(context != ZR_NULL && semanticContext != ZR_NULL && uri != ZR_NULL &&
                  name != ZR_NULL && signature != ZR_NULL && moduleName != ZR_NULL &&
                  nominalName != ZR_NULL,
          "hover format fixture allocates");
    if (context == ZR_NULL || semanticContext == ZR_NULL || uri == ZR_NULL ||
        name == ZR_NULL || signature == ZR_NULL || moduleName == ZR_NULL ||
        nominalName == ZR_NULL) {
        ZrParser_SemanticContext_Free(semanticContext);
        ZrLanguageServer_LspContext_Free(state, context);
        return;
    }

    intType = ZrParser_CanonicalType_InternPrimitive(
            semanticContext, ZR_VALUE_TYPE_INT64);
    unknownPrimitive = ZrParser_CanonicalType_InternPrimitive(
            semanticContext, ZR_VALUE_TYPE_UNKNOWN);
    longType = ZrParser_CanonicalType_InternNominal(
            semanticContext, moduleName, nominalName, 0U);
    check(intType != ZR_SEMANTIC_ID_INVALID &&
                  unknownPrimitive != ZR_SEMANTIC_ID_INVALID &&
                  longType != ZR_SEMANTIC_ID_INVALID,
          "canonical types intern");
    check(sizeof(expectedNominal) >= ZR_LSP_TYPE_BUFFER_LENGTH, "fixture exceeds the old type buffer");
    check(!ZrParser_CanonicalType_Format(
                  semanticContext, longType, small, sizeof(small)) &&
                  small[0] == '\0',
          "a type longer than the historical type buffer fails that format");
    check(ZrParser_CanonicalType_Format(
                  semanticContext, longType, hoverSized, sizeof(hoverSized)) &&
                  strlen(hoverSized) == sizeof(expectedNominal) &&
                  memcmp(hoverSized, expectedNominal, sizeof(expectedNominal)) == 0,
          "the same type still formats into the hover buffer");

    expect_hover(state,
                 context,
                 uri,
                 semanticContext,
                 name,
                 ZR_SEMANTIC_ID_INVALID,
                 ZR_NULL,
                 "\n\nType: cannot infer exact type",
                 "\n\nType: unknown",
                 "missing type identity keeps the inference wording");
    /* TypeId 非零但类型图里没有这个节点：同样没有类型身份，不能写成 unknown。 */
    check(ZrParser_CanonicalType_Find(semanticContext, danglingType) == ZR_NULL,
          "the dangling type id has no stored node");
    expect_hover(state,
                 context,
                 uri,
                 semanticContext,
                 name,
                 danglingType,
                 ZR_NULL,
                 "\n\nType: cannot infer exact type",
                 "\n\nType: unknown",
                 "a type id without a stored node keeps the inference wording");
    expect_hover(state,
                 context,
                 uri,
                 semanticContext,
                 name,
                 intType,
                 ZR_NULL,
                 "\n\nResolved Type: int",
                 "cannot infer exact type",
                 "formattable type stays a resolved type");
    expect_hover(state,
                 context,
                 uri,
                 semanticContext,
                 name,
                 unknownPrimitive,
                 ZR_NULL,
                 "\n\nResolved Type: unknown",
                 "cannot infer exact type",
                 "a formatted unknown primitive is not an inference failure");
    expect_hover(state,
                 context,
                 uri,
                 semanticContext,
                 name,
                 longType,
                 ZR_NULL,
                 "\n\nResolved Type: ",
                 "\n\nType: ",
                 "format failure of the old 128-byte buffer is not inference failure");
    {
        SZrLspHover *hover = build_hover(
                state, context, uri, semanticContext, name, longType, ZR_NULL);
        const TZrChar *text = hover_text(hover);
        check(text != ZR_NULL && strstr(text, hoverSized) != ZR_NULL,
              "hover publishes the full nominal name");
        free_hover(state, hover);
    }

    /* 整行的容量边界：刚好写得下时原样显示；多一个字节就按排版失败处理，不显示截断的名字。 */
    {
        static const TZrChar heading[] = "**variable**: value\n\nResolved Type: ";
        const TZrSize longestFit = ZR_LSP_HOVER_BUFFER_LENGTH - sizeof(heading);
        TZrChar typeName[ZR_LSP_HOVER_BUFFER_LENGTH + 1U];
        TZrChar expectedLine[ZR_LSP_HOVER_BUFFER_LENGTH];
        TZrChar probe[ZR_LSP_HOVER_BUFFER_LENGTH];
        TZrTypeId fitType;
        TZrTypeId lineOverflowType;
        TZrTypeId bufferOverflowType;
        SZrLspHover *hover;
        const TZrChar *text;

        fitType = intern_named_type(
                state, semanticContext, longestFit, typeName, sizeof(typeName));
        memcpy(expectedLine, heading, sizeof(heading) - 1U);
        memcpy(expectedLine + sizeof(heading) - 1U, typeName, longestFit + 1U);
        hover = build_hover(
                state, context, uri, semanticContext, name, fitType, ZR_NULL);
        text = hover_text(hover);
        check(fitType != ZR_SEMANTIC_ID_INVALID && text != ZR_NULL &&
                      strcmp(text, expectedLine) == 0,
              "the longest type that fits the hover line is shown in full");
        free_hover(state, hover);

        lineOverflowType = intern_named_type(
                state, semanticContext, longestFit + 1U, typeName, sizeof(typeName));
        check(lineOverflowType != ZR_SEMANTIC_ID_INVALID &&
                      ZrParser_CanonicalType_Format(
                              semanticContext, lineOverflowType, probe, sizeof(probe)) &&
                      strlen(probe) == longestFit + 1U,
              "one byte more still formats on its own");
        expect_hover(state,
                     context,
                     uri,
                     semanticContext,
                     name,
                     lineOverflowType,
                     ZR_NULL,
                     "\n\nType: unknown",
                     "cannot infer exact type",
                     "a type that overflows the hover line is not inference failure");
        expect_hover(state,
                     context,
                     uri,
                     semanticContext,
                     name,
                     lineOverflowType,
                     ZR_NULL,
                     ZR_NULL,
                     "www",
                     "a type that overflows the hover line is not shown truncated");

        bufferOverflowType = intern_named_type(
                state,
                semanticContext,
                ZR_LSP_HOVER_BUFFER_LENGTH,
                typeName,
                sizeof(typeName));
        check(bufferOverflowType != ZR_SEMANTIC_ID_INVALID &&
                      ZrParser_CanonicalType_Find(semanticContext, bufferOverflowType) !=
                              ZR_NULL &&
                      !ZrParser_CanonicalType_Format(
                              semanticContext, bufferOverflowType, probe, sizeof(probe)),
              "a stored type longer than the hover buffer cannot be formatted");
        expect_hover(state,
                     context,
                     uri,
                     semanticContext,
                     name,
                     bufferOverflowType,
                     ZR_NULL,
                     "\n\nType: unknown",
                     "cannot infer exact type",
                     "a type longer than the hover buffer is not inference failure");
    }

    unformattable = nest_until_format_fails(semanticContext, intType);
    check(unformattable != ZR_SEMANTIC_ID_INVALID &&
                  ZrParser_CanonicalType_Find(semanticContext, unformattable) !=
                          ZR_NULL &&
                  !ZrParser_CanonicalType_Format(
                          semanticContext,
                          unformattable,
                          hoverSized,
                          sizeof(hoverSized)),
          "nesting produces a stored type that still cannot be formatted");
    expect_hover(state,
                 context,
                 uri,
                 semanticContext,
                 name,
                 unformattable,
                 ZR_NULL,
                 "\n\nType: unknown",
                 "cannot infer exact type",
                 "unformattable stored type uses expression hover wording");
    expect_hover(state,
                 context,
                 uri,
                 semanticContext,
                 name,
                 unformattable,
                 signature,
                 "\n\nSignature: fn(int) -> int",
                 "\n\nType: ",
                 "an available signature is unchanged by type format failure");

    ZrParser_SemanticContext_Free(semanticContext);
    ZrLanguageServer_LspContext_Free(state, context);
}

/* 沿 UpdateDocument 和 GetHover 走真实的符号悬停：声明类型的名字比旧的类型缓冲长。 */
static void test_public_hover_keeps_long_declared_type(SZrState *state) {
    SZrLspContext *context = ZrLanguageServer_LspContext_New(state);
    SZrString *uri = test_string(state, "file:///hover-long-type.zr");
    TZrChar typeName[ZR_LSP_TYPE_BUFFER_LENGTH + 17U];
    TZrChar source[ZR_LSP_HOVER_BUFFER_LENGTH];
    TZrChar expected[ZR_LSP_HOVER_BUFFER_LENGTH];
    SZrLspPosition usage;
    SZrLspHover *hover = ZR_NULL;
    const TZrChar *text;
    int sourceLength;
    int expectedLength;

    memset(typeName, 'e', sizeof(typeName) - 1U);
    typeName[0] = 'L';
    typeName[sizeof(typeName) - 1U] = '\0';
    sourceLength = snprintf(source,
                            sizeof(source),
                            "class %s { }\n"
                            "fn use(): void {\n"
                            "    var value: %s = null;\n"
                            "    value;\n"
                            "}\n",
                            typeName,
                            typeName);
    expectedLength = snprintf(expected,
                              sizeof(expected),
                              "**variable**: value\n\nResolved Type: %s",
                              typeName);
    /* 第 4 行的 value 是使用点；夹具只有 ASCII，字节列就是协议列。 */
    usage.line = 3;
    usage.character = 4;

    check(context != ZR_NULL && uri != ZR_NULL &&
                  strlen(typeName) >= ZR_LSP_TYPE_BUFFER_LENGTH &&
                  sourceLength > 0 && (TZrSize)sourceLength < sizeof(source) &&
                  expectedLength > 0 && (TZrSize)expectedLength < sizeof(expected),
          "public hover fixture is prepared");
    if (context == ZR_NULL || uri == ZR_NULL || sourceLength <= 0 ||
        (TZrSize)sourceLength >= sizeof(source)) {
        ZrLanguageServer_LspContext_Free(state, context);
        return;
    }

    check(ZrLanguageServer_Lsp_UpdateDocument(
                  state, context, uri, source, (TZrSize)sourceLength, 1U),
          "public hover fixture opens");
    check(ZrLanguageServer_Lsp_GetHover(state, context, uri, usage, &hover) &&
                  hover != ZR_NULL,
          "public hover answers at the variable use");
    text = hover_text(hover);
    if (text == ZR_NULL) {
        check(ZR_FALSE, "public hover has text");
    } else {
        if (strstr(text, expected) == ZR_NULL) {
            printf("  missing \"%s\" in:\n%s\n", expected, text);
            check(ZR_FALSE, "public hover shows the whole declared type");
        }
        if (strstr(text, "cannot infer exact type") != ZR_NULL ||
            strstr(text, "Type: unknown") != ZR_NULL) {
            printf("  unexpected unavailable-type wording in:\n%s\n", text);
            check(ZR_FALSE, "public hover does not report an unavailable type");
        }
    }
    free_hover(state, hover);
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
    test_symbol_hover_format_failure_wording(state);
    test_public_hover_keeps_long_declared_type(state);
    ZrCore_GlobalState_Free(global);
    printf("canonical hover format wording: %d failure(s)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
