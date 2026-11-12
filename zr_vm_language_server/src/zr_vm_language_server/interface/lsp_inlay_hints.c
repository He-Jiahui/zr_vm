#include "interface/lsp_interface_internal.h"
#include "interface/lsp_canonical_symbol_display.h"
#include "semantic/lsp_numeric_range_text.h"
#include "semantic/analyzer/semantic_analyzer_internal.h"
#include "zr_vm_parser/semantic_display.h"
#include "zr_vm_parser/semantic_query.h"
#include "zr_vm_parser/ast.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* 提示位置与请求范围均已转换到 LSP 坐标；比较时必须先按行再按 UTF-16 列排序。 */
static int lsp_inlay_compare_position(SZrLspPosition left, SZrLspPosition right) {
    if (left.line < right.line) {
        return -1;
    }
    if (left.line > right.line) {
        return 1;
    }
    if (left.character < right.character) {
        return -1;
    }
    if (left.character > right.character) {
        return 1;
    }
    return 0;
}

/* LSP Range 的 end 排他；空范围不包含任何提示，相邻分页不会重复边界提示。 */
static TZrBool lsp_inlay_position_in_range(SZrLspPosition position, SZrLspRange range) {
    return lsp_inlay_compare_position(position, range.start) >= 0 &&
           lsp_inlay_compare_position(position, range.end) < 0;
}

/* 标签和事实均使用有界缓冲区；失败向上传递，避免把截断类型展示为完整类型。 */
static TZrBool lsp_inlay_append_format(TZrChar *buffer,
                                       TZrSize bufferSize,
                                       TZrSize *used,
                                       const TZrChar *format,
                                       ...) {
    va_list args;
    int written;

    if (buffer == ZR_NULL || used == ZR_NULL || format == ZR_NULL || *used >= bufferSize) {
        return ZR_FALSE;
    }

    va_start(args, format);
    written = vsnprintf(buffer + *used, bufferSize - *used, format, args);
    va_end(args);

    if (written < 0 || (TZrSize)written >= bufferSize - *used) {
        buffer[bufferSize - 1u] = '\0';
        return ZR_FALSE;
    }

    *used += (TZrSize)written;
    return ZR_TRUE;
}

/* 仅从已解析的 canonical declaration 取类型；函数提示展示返回类型而非 callable 全型。 */
static TZrBool lsp_inlay_declaration_has_exact_canonical_type_text(
        SZrSemanticAnalyzer *analyzer,
        const SZrParserSemanticSymbolQuery *declaration,
        TZrChar *buffer,
        TZrSize bufferSize,
        const TZrChar **outTypeText) {
    if (outTypeText != ZR_NULL) {
        *outTypeText = ZR_NULL;
    }
    if (analyzer == ZR_NULL ||
        analyzer->semanticContext == ZR_NULL ||
        declaration == ZR_NULL ||
        buffer == ZR_NULL ||
        bufferSize == 0 ||
        outTypeText == ZR_NULL ||
        declaration->symbolId == ZR_SEMANTIC_ID_INVALID ||
        declaration->typeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }

    if (!ZrLanguageServer_Lsp_FormatCanonicalDeclarationType(
                analyzer, declaration, buffer, bufferSize)) {
        return ZR_FALSE;
    }

    if (declaration->kind == ZR_SEMANTIC_SYMBOL_KIND_FUNCTION) {
        const SZrCanonicalTypeNode *callable = ZrParser_CanonicalType_Find(
                analyzer->semanticContext, declaration->typeId);
        if (callable == ZR_NULL || callable->kind != ZR_CANONICAL_TYPE_FUNCTION ||
            !ZrParser_SemanticDisplay_FormatSourceType(
                    analyzer->semanticContext, callable->data.function.returnTypeId,
                    buffer, bufferSize)) {
            return ZR_FALSE;
        }
    }

    *outTypeText = buffer;
    return buffer[0] != '\0';
}

static const TZrChar *lsp_inlay_native_text(SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_NULL;
    }
    return ZrCore_String_GetNativeString(value);
}

static TZrBool lsp_inlay_hint_exists(SZrArray *result,
                                     SZrLspPosition position,
                                     TZrInt32 kind,
                                     const TZrChar *labelText) {
    if (result == ZR_NULL || !result->isValid || labelText == ZR_NULL) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0; index < result->length; index++) {
        SZrLspInlayHint **hintPtr = (SZrLspInlayHint **)ZrCore_Array_Get(result, index);
        const TZrChar *existing;
        if (hintPtr == ZR_NULL || *hintPtr == ZR_NULL) {
            continue;
        }
        existing = lsp_inlay_native_text((*hintPtr)->label);
        if ((*hintPtr)->kind == kind &&
            (*hintPtr)->position.line == position.line &&
            (*hintPtr)->position.character == position.character &&
            existing != ZR_NULL &&
            strcmp(existing, labelText) == 0) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

/* 插入文本只含精确类型或已解析形参名；事实后缀留在标签里。 */
static TZrBool lsp_inlay_append_exact_edit(SZrState *state,
                                           SZrLspInlayHint *hint,
                                           SZrLspPosition position,
                                           const TZrChar *editText) {
    SZrLspTextEdit *edit;

    if (state == ZR_NULL || hint == ZR_NULL) {
        return ZR_FALSE;
    }
    if (editText == ZR_NULL || editText[0] == '\0') {
        return ZR_TRUE;
    }
    if (!hint->textEdits.isValid) {
        ZrCore_Array_Init(state, &hint->textEdits, sizeof(SZrLspTextEdit *), 1);
    }

    edit = (SZrLspTextEdit *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspTextEdit));
    if (edit == ZR_NULL) {
        return ZR_FALSE;
    }
    edit->range.start = position;
    edit->range.end = position;
    edit->newText = ZrCore_String_Create(state, (TZrNativeString)editText, strlen(editText));
    if (edit->newText == ZR_NULL) {
        ZrCore_Memory_RawFree(state->global, edit, sizeof(SZrLspTextEdit));
        return ZR_FALSE;
    }
    ZrCore_Array_Push(state, &hint->textEdits, &edit);
    return ZR_TRUE;
}

/* 成功加入结果的提示持有原生结构体、VM 字符串和可选编辑；清理函数释放原生部分。 */
static TZrBool lsp_inlay_append_hint(SZrState *state,
                                     SZrArray *result,
                                     SZrLspPosition position,
                                     TZrInt32 kind,
                                     TZrBool paddingLeft,
                                     TZrBool paddingRight,
                                     const TZrChar *labelText,
                                     const TZrChar *editText) {
    SZrLspInlayHint *hint;

    if (state == ZR_NULL || result == ZR_NULL || labelText == ZR_NULL || labelText[0] == '\0') {
        return ZR_FALSE;
    }
    if (lsp_inlay_hint_exists(result, position, kind, labelText)) {
        return ZR_TRUE;
    }

    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspInlayHint *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    }

    hint = (SZrLspInlayHint *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspInlayHint));
    if (hint == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(hint, 0, sizeof(*hint));
    ZrCore_Array_Init(state, &hint->textEdits, sizeof(SZrLspTextEdit *), 1);
    hint->position = position;
    hint->kind = kind;
    hint->paddingLeft = paddingLeft;
    hint->paddingRight = paddingRight;
    hint->label = ZrCore_String_Create(state, (TZrNativeString)labelText, strlen(labelText));
    if (hint->label == ZR_NULL ||
        !lsp_inlay_append_exact_edit(state, hint, position, editText)) {
        ZrLanguageServer_Lsp_FreeTextEdits(state, &hint->textEdits);
        ZrCore_Memory_RawFree(state->global, hint, sizeof(SZrLspInlayHint));
        return ZR_FALSE;
    }

    ZrCore_Array_Push(state, result, &hint);
    return ZR_TRUE;
}

/* 初始化式的数值事实附在已确证类型后；区间展示复用 LSP 共享文案。 */
static TZrBool lsp_inlay_append_numeric_fact_detail(TZrChar *buffer,
                                                    TZrSize bufferSize,
                                                    TZrSize *used,
                                                    const SZrSemanticNumericFact *fact) {
    if (fact == ZR_NULL) {
        return ZR_TRUE;
    }

    if (fact->hasRange) {
        if (!lsp_inlay_append_format(buffer, bufferSize, used, ", ") ||
            !ZrLanguageServer_LspNumericRangeText_AppendRange(
                buffer,
                bufferSize,
                used,
                fact,
                ZR_TRUE)) {
            return ZR_FALSE;
        }
    }

    if (fact->hasUnsignedRange &&
        !lsp_inlay_append_format(buffer,
                                 bufferSize,
                                 used,
                                 ", unsigned %llu..%llu",
                                 (unsigned long long)fact->minUnsignedValue,
                                 (unsigned long long)fact->maxUnsignedValue)) {
        return ZR_FALSE;
    }

    if (fact->mayOverflow &&
        !lsp_inlay_append_format(buffer, bufferSize, used, ", may overflow")) {
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

/* 逻辑值与短路性质来自同一初始化节点的 parser 事实，不扩散到整个声明。 */
static TZrBool lsp_inlay_append_logical_fact_detail(TZrChar *buffer,
                                                    TZrSize bufferSize,
                                                    TZrSize *used,
                                                    const SZrSemanticLogicalFact *fact) {
    if (fact == ZR_NULL) {
        return ZR_TRUE;
    }

    if (fact->hasKnownValue &&
        !lsp_inlay_append_format(buffer,
                                 bufferSize,
                                 used,
                                 ", logical %s",
                                 fact->knownValue ? "true" : "false")) {
        return ZR_FALSE;
    }

    if (fact->kind == ZR_SEMANTIC_LOGICAL_FACT_SHORT_CIRCUIT &&
        !lsp_inlay_append_format(buffer, bufferSize, used, ", short-circuits")) {
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

/* 类型是提示的必要部分；初始化式事实可选，且只从变量声明绑定的 AST 节点取得。 */
static TZrBool lsp_inlay_build_label_text(SZrState *state,
                                          SZrSemanticAnalyzer *analyzer,
                                          const SZrParserSemanticSymbolQuery *declaration,
                                          const TZrChar *typeText,
                                          TZrChar *buffer,
                                          TZrSize bufferSize) {
    SZrAstNode *astNode;
    SZrAstNode *initializer;
    const SZrSemanticNumericFact *numericFact = ZR_NULL;
    const SZrSemanticLogicalFact *logicalFact = ZR_NULL;
    TZrSize used = 0;

    if (state == ZR_NULL ||
        declaration == ZR_NULL ||
        typeText == ZR_NULL ||
        buffer == ZR_NULL ||
        bufferSize == 0) {
        return ZR_FALSE;
    }

    if (!lsp_inlay_append_format(buffer, bufferSize, &used, "%s", typeText)) {
        return ZR_FALSE;
    }

    astNode = (SZrAstNode *)declaration->declarationNode;
    initializer = astNode != ZR_NULL && astNode->type == ZR_AST_VARIABLE_DECLARATION
                      ? astNode->data.variableDeclaration.value
                      : ZR_NULL;
    if (initializer == ZR_NULL || analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL) {
        return ZR_TRUE;
    }

    numericFact = ZrParser_SemanticFacts_FindNumericByNode(analyzer->semanticContext, initializer);
    logicalFact = ZrParser_SemanticFacts_FindLogicalByNode(analyzer->semanticContext, initializer);

    return lsp_inlay_append_numeric_fact_detail(buffer, bufferSize, &used, numericFact) &&
           lsp_inlay_append_logical_fact_detail(buffer, bufferSize, &used, logicalFact);
}

static TZrBool lsp_inlay_source_is_document(SZrString *source, SZrString *uri);

#define ZR_LSP_INLAY_SOURCE_TYPE_MAX_DEPTH 16U

/* 名义类型只有在本文档里有同名的类型声明时，裸名才一定能写回源码；
 * 模块别名、限定导入的类型和内部类型的展示名都不是可写的类型标注。 */
static TZrBool lsp_inlay_nominal_is_declared_in_document(SZrSemanticAnalyzer *analyzer,
                                                        SZrString *uri,
                                                        const SZrCanonicalNominalType *nominal) {
    const SZrArray *symbols = &analyzer->semanticContext->symbols;

    if (nominal->name == ZR_NULL ||
        (nominal->moduleIdentity != ZR_NULL &&
         ZrCore_String_GetByteLength(nominal->moduleIdentity) > 0U) ||
        !symbols->isValid) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0; index < symbols->length; index++) {
        const SZrSemanticSymbolRecord *symbol =
                (const SZrSemanticSymbolRecord *)ZrCore_Array_Get((SZrArray *)symbols, index);
        if (symbol == ZR_NULL || symbol->kind != ZR_SEMANTIC_SYMBOL_KIND_TYPE ||
            symbol->name == ZR_NULL || symbol->astNode == ZR_NULL ||
            !ZrCore_String_Equal(symbol->name, (SZrString *)nominal->name) ||
            !lsp_inlay_source_is_document(symbol->location.source, uri)) {
            continue;
        }
        switch (symbol->astNode->type) {
            case ZR_AST_CLASS_DECLARATION:
            case ZR_AST_STRUCT_DECLARATION:
            case ZR_AST_INTERFACE_DECLARATION:
                return ZR_TRUE;
            default:
                break;
        }
    }
    return ZR_FALSE;
}

/* 展示文本只在逐节点都对应源码类型语法时才作为插入文本。
 * 浮点（展示为 double/float，源码拼写与宽度不一一对应）、null、可空、闭包等只保留标签。 */
static TZrBool lsp_inlay_type_is_source_writable(SZrSemanticAnalyzer *analyzer,
                                                 SZrString *uri,
                                                 TZrTypeId typeId,
                                                 TZrSize depth) {
    const SZrCanonicalTypeNode *node;

    if (depth > ZR_LSP_INLAY_SOURCE_TYPE_MAX_DEPTH || typeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    node = ZrParser_CanonicalType_Find(analyzer->semanticContext, typeId);
    if (node == ZR_NULL) {
        return ZR_FALSE;
    }
    switch (node->kind) {
        case ZR_CANONICAL_TYPE_PRIMITIVE:
            switch (node->data.primitive.valueType) {
                case ZR_VALUE_TYPE_BOOL:
                case ZR_VALUE_TYPE_INT8:
                case ZR_VALUE_TYPE_INT16:
                case ZR_VALUE_TYPE_INT32:
                case ZR_VALUE_TYPE_INT64:
                case ZR_VALUE_TYPE_UINT8:
                case ZR_VALUE_TYPE_UINT16:
                case ZR_VALUE_TYPE_UINT32:
                case ZR_VALUE_TYPE_UINT64:
                case ZR_VALUE_TYPE_STRING:
                case ZR_VALUE_TYPE_OBJECT:
                    return ZR_TRUE;
                default:
                    return ZR_FALSE;
            }
        case ZR_CANONICAL_TYPE_NOMINAL:
            return lsp_inlay_nominal_is_declared_in_document(analyzer, uri, &node->data.nominal);
        case ZR_CANONICAL_TYPE_GENERIC_PARAMETER:
            return ZR_TRUE;
        case ZR_CANONICAL_TYPE_GENERIC_INSTANCE: {
            TZrTypeId definitionTypeId = node->data.genericInstance.definitionTypeId;
            TZrSize argumentCount = node->data.genericInstance.arguments.length;

            if (!lsp_inlay_type_is_source_writable(analyzer, uri, definitionTypeId, depth + 1U)) {
                return ZR_FALSE;
            }
            for (TZrSize index = 0; index < argumentCount; index++) {
                const SZrCanonicalGenericArgument *argument;
                TZrTypeId argumentTypeId;

                /* 递归中的 Find 不新增节点，但仍按下标重新取，避免持有跨调用的元素指针。 */
                node = ZrParser_CanonicalType_Find(analyzer->semanticContext, typeId);
                if (node == ZR_NULL || node->kind != ZR_CANONICAL_TYPE_GENERIC_INSTANCE) {
                    return ZR_FALSE;
                }
                argument = (const SZrCanonicalGenericArgument *)ZrCore_Array_Get(
                        (SZrArray *)&node->data.genericInstance.arguments, index);
                if (argument == ZR_NULL) {
                    return ZR_FALSE;
                }
                if (argument->kind == ZR_CANONICAL_GENERIC_ARGUMENT_TYPE) {
                    argumentTypeId = argument->data.typeId;
                    if (!lsp_inlay_type_is_source_writable(analyzer, uri, argumentTypeId, depth + 1U)) {
                        return ZR_FALSE;
                    }
                }
            }
            return ZR_TRUE;
        }
        case ZR_CANONICAL_TYPE_ARRAY:
            return node->data.array.storageKind == ZR_CANONICAL_ARRAY_STORAGE_MANAGED &&
                   lsp_inlay_type_is_source_writable(
                           analyzer, uri, node->data.array.elementTypeId, depth + 1U);
        default:
            return ZR_FALSE;
    }
}

/* 声明提示展示的类型：函数取返回类型，其余取声明类型。 */
static TZrTypeId lsp_inlay_declaration_display_type_id(SZrSemanticAnalyzer *analyzer,
                                                       const SZrParserSemanticSymbolQuery *declaration) {
    if (declaration->kind == ZR_SEMANTIC_SYMBOL_KIND_FUNCTION) {
        const SZrCanonicalTypeNode *callable =
                ZrParser_CanonicalType_Find(analyzer->semanticContext, declaration->typeId);
        return callable != ZR_NULL && callable->kind == ZR_CANONICAL_TYPE_FUNCTION
                       ? callable->data.function.returnTypeId
                       : ZR_SEMANTIC_ID_INVALID;
    }
    return declaration->typeId;
}

/* 隐式类型的变量、字段及隐式返回类型才需要提示；显式标注处不重复呈现。 */
static TZrBool lsp_inlay_try_append_declaration_hint(SZrState *state,
                                                SZrLspContext *context,
                                                SZrString *uri,
                                                SZrSemanticAnalyzer *analyzer,
                                                const SZrParserSemanticSymbolQuery *declaration,
                                                SZrLspRange range,
                                                SZrArray *result) {
    TZrChar typeBuffer[ZR_LSP_LONG_TEXT_BUFFER_LENGTH];
    TZrChar labelBuffer[ZR_LSP_LONG_TEXT_BUFFER_LENGTH];
    const TZrChar *typeText = ZR_NULL;
    SZrLspPosition position;
    SZrAstNode *astNode;

    if (!lsp_inlay_declaration_has_exact_canonical_type_text(
                analyzer, declaration, typeBuffer, sizeof(typeBuffer), &typeText)) {
        return ZR_TRUE;
    }

    astNode = declaration != ZR_NULL
            ? (SZrAstNode *)declaration->declarationNode
            : ZR_NULL;
    if (astNode == ZR_NULL) {
        return ZR_TRUE;
    }

    /* 提示位置贴近声明或函数体入口；位置转换使用当前文档内容以保留 UTF-16 列。 */
    /* TODO: PositionFromFilePositionForDocument 在快照获取失败时返回 0:0；
     * 核查缓存分析器仍可用而定位快照失败的请求，避免把异处提示误放到文档开头。 */
    switch (astNode->type) {
        case ZR_AST_VARIABLE_DECLARATION:
            if (astNode->data.variableDeclaration.typeInfo != ZR_NULL ||
                astNode->data.variableDeclaration.pattern == ZR_NULL ||
                astNode->data.variableDeclaration.pattern->type != ZR_AST_IDENTIFIER_LITERAL) {
                return ZR_TRUE;
            }
            position = ZrLanguageServer_Lsp_PositionFromFilePositionForDocument(
                    context, uri, declaration->declarationRange.end);
            break;

        case ZR_AST_CLASS_FIELD:
            if (astNode->data.classField.typeInfo != ZR_NULL) {
                return ZR_TRUE;
            }
            position = ZrLanguageServer_Lsp_PositionFromFilePositionForDocument(
                    context, uri, declaration->declarationRange.end);
            break;

        case ZR_AST_STRUCT_FIELD:
            if (astNode->data.structField.typeInfo != ZR_NULL) {
                return ZR_TRUE;
            }
            position = ZrLanguageServer_Lsp_PositionFromFilePositionForDocument(
                    context, uri, declaration->declarationRange.end);
            break;

        case ZR_AST_FUNCTION_DECLARATION:
            if (astNode->data.functionDeclaration.returnType != ZR_NULL) {
                return ZR_TRUE;
            }
            position = astNode->data.functionDeclaration.body != ZR_NULL
                           ? ZrLanguageServer_Lsp_PositionFromFilePositionForDocument(
                                 context,
                                 uri,
                                 astNode->data.functionDeclaration.body->location.start)
                           : ZrLanguageServer_Lsp_PositionFromFilePositionForDocument(
                                 context,
                                 uri,
                                 declaration->declarationRange.end);
            break;

        case ZR_AST_CLASS_METHOD:
            if (astNode->data.classMethod.returnType != ZR_NULL) {
                return ZR_TRUE;
            }
            position = astNode->data.classMethod.body != ZR_NULL
                           ? ZrLanguageServer_Lsp_PositionFromFilePositionForDocument(
                                 context,
                                 uri,
                                 astNode->data.classMethod.body->location.start)
                           : ZrLanguageServer_Lsp_PositionFromFilePositionForDocument(
                                 context,
                                 uri,
                                 declaration->declarationRange.end);
            break;

        case ZR_AST_STRUCT_METHOD:
            if (astNode->data.structMethod.returnType != ZR_NULL) {
                return ZR_TRUE;
            }
            position = astNode->data.structMethod.body != ZR_NULL
                           ? ZrLanguageServer_Lsp_PositionFromFilePositionForDocument(
                                 context,
                                 uri,
                                 astNode->data.structMethod.body->location.start)
                           : ZrLanguageServer_Lsp_PositionFromFilePositionForDocument(
                                 context,
                                 uri,
                                 declaration->declarationRange.end);
            break;

        default:
            return ZR_TRUE;
    }

    if (!lsp_inlay_position_in_range(position, range)) {
        return ZR_TRUE;
    }

    if (!lsp_inlay_build_label_text(
                state, analyzer, declaration, typeText, labelBuffer, sizeof(labelBuffer))) {
        return ZR_FALSE;
    }

    {
        TZrChar displayLabel[ZR_LSP_LONG_TEXT_BUFFER_LENGTH];
        TZrChar editText[ZR_LSP_LONG_TEXT_BUFFER_LENGTH];
        TZrBool insertsBeforeBody = astNode->type == ZR_AST_FUNCTION_DECLARATION ||
                                    astNode->type == ZR_AST_CLASS_METHOD ||
                                    astNode->type == ZR_AST_STRUCT_METHOD;
        int displayLength = snprintf(displayLabel, sizeof(displayLabel), ": %s", labelBuffer);
        int editLength = snprintf(editText,
                                  sizeof(editText),
                                  insertsBeforeBody ? ": %s " : ": %s",
                                  typeText);
        /* struct 字段的声明范围止于下一个成员，提示锚点不在字段名后，不能在那里插入标注。 */
        TZrBool editable = astNode->type != ZR_AST_STRUCT_FIELD &&
                           lsp_inlay_type_is_source_writable(
                                   analyzer,
                                   uri,
                                   lsp_inlay_declaration_display_type_id(analyzer, declaration),
                                   0U);
        if (displayLength <= 0 || (TZrSize)displayLength >= sizeof(displayLabel) ||
            editLength <= 0 || (TZrSize)editLength >= sizeof(editText)) {
            return ZR_FALSE;
        }
        return lsp_inlay_append_hint(state,
                                     result,
                                     position,
                                     ZR_LSP_INLAY_HINT_KIND_TYPE,
                                     ZR_TRUE,
                                     ZR_FALSE,
                                     displayLabel,
                                     editable ? editText : ZR_NULL);
    }
}

static const SZrAstNodeArray *lsp_inlay_callable_parameters(const SZrAstNode *declaration) {
    if (declaration == ZR_NULL) {
        return ZR_NULL;
    }
    switch (declaration->type) {
        case ZR_AST_FUNCTION_DECLARATION:
            return declaration->data.functionDeclaration.params;
        case ZR_AST_EXTERN_FUNCTION_DECLARATION:
            return declaration->data.externFunctionDeclaration.params;
        case ZR_AST_EXTERN_DELEGATE_DECLARATION:
            return declaration->data.externDelegateDeclaration.params;
        case ZR_AST_CLASS_METHOD:
            return declaration->data.classMethod.params;
        case ZR_AST_STRUCT_METHOD:
            return declaration->data.structMethod.params;
        case ZR_AST_INTERFACE_METHOD_SIGNATURE:
            return declaration->data.interfaceMethodSignature.params;
        default:
            return ZR_NULL;
    }
}

static TZrBool lsp_inlay_source_is_document(SZrString *source, SZrString *uri) {
    return source != ZR_NULL &&
           (ZrLanguageServer_Lsp_StringsEqual(source, uri) ||
            ZrLanguageServer_Lsp_UrisResolveToSameNativePath(source, uri));
}

/* 形参名只来自已解析目标的声明 AST；具名实参和外部未解析调用不发明名字。
 * 每个位置实参都有标签；编辑只给接受后仍是合法调用的那一个。 */
static TZrBool lsp_inlay_append_parameter_hints(SZrState *state,
                                                SZrLspContext *context,
                                                SZrString *uri,
                                                SZrSemanticAnalyzer *analyzer,
                                                SZrLspRange range,
                                                SZrArray *result) {
    SZrParserSemanticQueryScope scope;
    SZrArray *facts;

    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL) {
        return ZR_TRUE;
    }

    facts = &analyzer->semanticContext->expressionFacts;
    if (!facts->isValid) {
        return ZR_TRUE;
    }

    memset(&scope, 0, sizeof(scope));
    scope.kind = ZR_PARSER_SEMANTIC_QUERY_SCOPE_NODE;
    scope.root = analyzer->ast;

    for (TZrSize index = 0; index < facts->length; index++) {
        const SZrSemanticExpressionFact *fact =
                (const SZrSemanticExpressionFact *)ZrCore_Array_Get(facts, index);
        SZrParserSemanticCallQuery query;
        const SZrSemanticSymbolRecord *symbol;
        const SZrAstNodeArray *parameters;
        TZrSize lastPositionalIndex;
        TZrBool hasPositional;

        if (fact == ZR_NULL || !fact->hasCallInfo ||
            !lsp_inlay_source_is_document(fact->range.source, uri)) {
            continue;
        }
        if (!ZrParser_SemanticQuery_CallAt(analyzer->semanticContext,
                                           fact->range,
                                           &scope,
                                           &query) ||
            !query.hasResolvedTarget ||
            query.argumentMappings == ZR_NULL ||
            !query.argumentMappings->isValid) {
            continue;
        }

        symbol = ZrParser_Semantic_FindSymbolById(analyzer->semanticContext, query.targetSymbolId);
        parameters = lsp_inlay_callable_parameters(symbol != ZR_NULL ? symbol->astNode : ZR_NULL);
        if (parameters == ZR_NULL || parameters->nodes == ZR_NULL) {
            continue;
        }

        /* 位置实参不能跟在具名实参后面，所以只有最后一个位置实参能单独改成具名。 */
        lastPositionalIndex = 0;
        hasPositional = ZR_FALSE;
        for (TZrSize mappingIndex = 0; mappingIndex < query.argumentMappings->length; mappingIndex++) {
            const SZrSemanticCallArgumentFact *mapping =
                    (const SZrSemanticCallArgumentFact *)ZrCore_Array_Get(
                            (SZrArray *)query.argumentMappings, mappingIndex);
            if (mapping != ZR_NULL && !mapping->isNamed &&
                (!hasPositional || mapping->argumentIndex > lastPositionalIndex)) {
                lastPositionalIndex = mapping->argumentIndex;
                hasPositional = ZR_TRUE;
            }
        }

        for (TZrSize mappingIndex = 0; mappingIndex < query.argumentMappings->length; mappingIndex++) {
            const SZrSemanticCallArgumentFact *mapping =
                    (const SZrSemanticCallArgumentFact *)ZrCore_Array_Get(
                            (SZrArray *)query.argumentMappings, mappingIndex);
            SZrAstNode *parameterNode;
            SZrParameter *parameter;
            const TZrChar *name;
            SZrLspPosition position;
            TZrChar label[ZR_LSP_LONG_TEXT_BUFFER_LENGTH];
            TZrChar editText[ZR_LSP_LONG_TEXT_BUFFER_LENGTH];
            int labelLength;
            int editLength;

            if (mapping == ZR_NULL || mapping->isNamed ||
                mapping->parameterIndex >= parameters->count) {
                continue;
            }
            parameterNode = parameters->nodes[mapping->parameterIndex];
            if (parameterNode == ZR_NULL || parameterNode->type != ZR_AST_PARAMETER) {
                continue;
            }
            parameter = &parameterNode->data.parameter;
            name = parameter->name != ZR_NULL ? lsp_inlay_native_text(parameter->name->name) : ZR_NULL;
            if (name == ZR_NULL || name[0] == '\0') {
                continue;
            }

            position = ZrLanguageServer_Lsp_PositionFromFilePositionForDocument(
                    context, uri, mapping->argumentRange.start);
            if (!lsp_inlay_position_in_range(position, range)) {
                continue;
            }

            labelLength = snprintf(label, sizeof(label), "%s:", name);
            editLength = snprintf(editText, sizeof(editText), "%s: ", name);
            if (labelLength <= 0 || (TZrSize)labelLength >= sizeof(label) ||
                editLength <= 0 || (TZrSize)editLength >= sizeof(editText)) {
                return ZR_FALSE;
            }
            if (!lsp_inlay_append_hint(state,
                                       result,
                                       position,
                                       ZR_LSP_INLAY_HINT_KIND_PARAMETER,
                                       ZR_FALSE,
                                       ZR_TRUE,
                                       label,
                                       mapping->argumentIndex == lastPositionalIndex
                                               ? editText
                                               : ZR_NULL)) {
                return ZR_FALSE;
            }
        }
    }

    return ZR_TRUE;
}

/** @brief 从当前文档的规范声明查询中生成指定范围内的类型提示。
 *  @pre uri 已在 context 中可分析，result 为未初始化或由同一 state 持有的提示指针数组；调用后用 FreeInlayHints 清理。
 *  @note 按声明源 URI 过滤项目级查询，再投影为文档位置；中途失败会清掉已加入的提示。 */
TZrBool ZrLanguageServer_Lsp_GetInlayHints(SZrState *state,
                                           SZrLspContext *context,
                                           SZrString *uri,
                                           SZrLspRange range,
                                           SZrArray *result) {
    SZrSemanticAnalyzer *analyzer;
    SZrParserSemanticQueryScope scope;
    SZrArray declarations;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspInlayHint *), ZR_LSP_ARRAY_INITIAL_CAPACITY);
    }

    analyzer = ZrLanguageServer_Lsp_GetOrCreateAnalyzer(state, context, uri);
    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL || analyzer->ast == ZR_NULL) {
        return ZR_FALSE;
    }

    /* 项目级分析可能包含其他文件声明；先查询当前 AST 范围，再以下面的 source 校验收口。 */
    memset(&scope, 0, sizeof(scope));
    scope.kind = ZR_PARSER_SEMANTIC_QUERY_SCOPE_NODE;
    scope.root = analyzer->ast;
    ZrCore_Array_Construct(&declarations);
    if (!ZrParser_SemanticQuery_DeclaredSymbols(
                analyzer->semanticContext, &scope, &declarations)) {
        ZrCore_Array_Free(state, &declarations);
        return ZR_FALSE;
    }

    /* 查询可能汇入项目内其他源文件；同一路径的不同 URI 写法也视为当前文档。 */
    for (TZrSize index = 0U; index < declarations.length; index++) {
        const SZrParserSemanticSymbolQuery *declaration =
                (const SZrParserSemanticSymbolQuery *)ZrCore_Array_Get(
                        &declarations, index);
        if (declaration == ZR_NULL || declaration->declarationRange.source == ZR_NULL ||
            (!ZrLanguageServer_Lsp_StringsEqual(declaration->declarationRange.source, uri) &&
             !ZrLanguageServer_Lsp_UrisResolveToSameNativePath(
                     declaration->declarationRange.source, uri))) {
            continue;
        }

        /* 失败时不能把部分结果交给 stdio/Wasm；回收原生项后由上层返回空集合。 */
        if (!lsp_inlay_try_append_declaration_hint(
                    state, context, uri, analyzer, declaration, range, result)) {
            ZrCore_Array_Free(state, &declarations);
            ZrLanguageServer_Lsp_FreeInlayHints(state, result);
            return ZR_FALSE;
        }
    }
    ZrCore_Array_Free(state, &declarations);

    if (!lsp_inlay_append_parameter_hints(state, context, uri, analyzer, range, result)) {
        ZrLanguageServer_Lsp_FreeInlayHints(state, result);
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

/** @brief 释放 GetInlayHints 的原生提示项和数组缓冲区。
 *  @pre state 与创建提示时使用同一全局分配器；label 为 VM 字符串，由 GC 管理。 */
void ZrLanguageServer_Lsp_FreeInlayHints(SZrState *state, SZrArray *result) {
    if (state == ZR_NULL || result == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < result->length; index++) {
        SZrLspInlayHint **hintPtr = (SZrLspInlayHint **)ZrCore_Array_Get(result, index);
        if (hintPtr != ZR_NULL && *hintPtr != ZR_NULL) {
            ZrLanguageServer_Lsp_FreeTextEdits(state, &(*hintPtr)->textEdits);
            ZrCore_Memory_RawFree(state->global, *hintPtr, sizeof(SZrLspInlayHint));
        }
    }

    ZrCore_Array_Free(state, result);
}
