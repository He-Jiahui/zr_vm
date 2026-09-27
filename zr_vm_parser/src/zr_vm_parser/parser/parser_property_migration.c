#include "parser_internal.h"
#include "parser_property_migration.h"

#include <ctype.h>

/* 源文本中的字节切片；迁移建议必须保留原始拼写而非重新打印 AST。 */
typedef struct SZrLegacyPropertySlice {
    TZrSize start;
    TZrSize length;
} SZrLegacyPropertySlice;

/* 替换文本的有界缓冲区，仅在构建诊断期间有效。 */
typedef struct SZrLegacyPropertyTextBuilder {
    TZrChar *data;
    TZrSize length;
    TZrSize capacity;
} SZrLegacyPropertyTextBuilder;

/* 所有迁移切片都以当前 lexer 源文本为边界，避免诊断引用越界范围。 */
static TZrBool legacy_property_slice_is_valid(
        SZrParserState *ps,
        SZrLegacyPropertySlice slice) {
    return ps != ZR_NULL && ps->lexer != ZR_NULL &&
           slice.start <= ps->lexer->sourceLength &&
           slice.length <= ps->lexer->sourceLength - slice.start;
}
/* 将原始字节位置转换为 IDE 可展示的文件范围。 */
static SZrFileRange legacy_property_range_from_slice(
        SZrParserState *ps,
        SZrLegacyPropertySlice slice) {
    SZrFileRange range;

    memset(&range, 0, sizeof(range));
    if (!legacy_property_slice_is_valid(ps, slice)) {
        return range;
    }
    range.start = get_file_position_from_offset(ps->lexer, slice.start);
    range.end = get_file_position_from_offset(
            ps->lexer,
            slice.start + slice.length);
    range.source = ps->lexer->sourceName;
    return range;
}

/* 只裁掉类型拼写两端空白，保留用户写下的内部格式。 */
static void legacy_property_trim_slice(
        SZrParserState *ps,
        SZrLegacyPropertySlice *slice) {
    const TZrChar *source;

    if (!legacy_property_slice_is_valid(ps, *slice)) {
        slice->length = 0U;
        return;
    }
    source = ps->lexer->source;
    while (slice->length > 0U &&
           isspace((unsigned char)source[slice->start])) {
        slice->start++;
        slice->length--;
    }
    while (slice->length > 0U &&
           isspace((unsigned char)source[slice->start + slice->length - 1U])) {
        slice->length--;
    }
}

/* 在已验证的源码区间定位旧语法标点，不访问其他声明的文本。 */
static TZrBool legacy_property_find_char(
        SZrParserState *ps,
        TZrSize start,
        TZrSize end,
        TZrChar expected,
        TZrSize *outOffset) {
    if (ps == ZR_NULL || ps->lexer == ZR_NULL || outOffset == ZR_NULL ||
        start > end || end > ps->lexer->sourceLength) {
        return ZR_FALSE;
    }
    for (TZrSize offset = start; offset < end; offset++) {
        if (ps->lexer->source[offset] == expected) {
            *outOffset = offset;
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

/* class 旧访问器和 interface 旧签名使用不同 AST 形状，配对前统一取身份名。 */
static SZrString *legacy_property_entry_name(
        const SZrLegacyPropertyMigrationEntry *entry) {
    SZrAstNode *node;

    if (entry == ZR_NULL || entry->declaration == ZR_NULL) {
        return ZR_NULL;
    }
    node = entry->declaration;
    if (node->type == ZR_AST_INTERFACE_PROPERTY_SIGNATURE) {
        return node->data.interfacePropertySignature.name != ZR_NULL
                       ? node->data.interfacePropertySignature.name->name
                       : ZR_NULL;
    }
    if (node->type != ZR_AST_CLASS_PROPERTY ||
        node->data.classProperty.modifier == ZR_NULL) {
        return ZR_NULL;
    }
    if (node->data.classProperty.modifier->type == ZR_AST_PROPERTY_GET) {
        SZrIdentifier *name =
                node->data.classProperty.modifier->data.propertyGet.name;
        return name != ZR_NULL ? name->name : ZR_NULL;
    }
    if (node->data.classProperty.modifier->type == ZR_AST_PROPERTY_SET) {
        SZrIdentifier *name =
                node->data.classProperty.modifier->data.propertySet.name;
        return name != ZR_NULL ? name->name : ZR_NULL;
    }
    return ZR_NULL;
}

static TZrBool legacy_property_entry_is_getter(
        const SZrLegacyPropertyMigrationEntry *entry) {
    return entry != ZR_NULL && entry->declaration != ZR_NULL &&
           entry->declaration->type == ZR_AST_CLASS_PROPERTY &&
           entry->declaration->data.classProperty.modifier != ZR_NULL &&
           entry->declaration->data.classProperty.modifier->type ==
                   ZR_AST_PROPERTY_GET;
}

static TZrBool legacy_property_entry_is_setter(
        const SZrLegacyPropertyMigrationEntry *entry) {
    return entry != ZR_NULL && entry->declaration != ZR_NULL &&
           entry->declaration->type == ZR_AST_CLASS_PROPERTY &&
           entry->declaration->data.classProperty.modifier != ZR_NULL &&
           entry->declaration->data.classProperty.modifier->type ==
                   ZR_AST_PROPERTY_SET;
}

/* class 使用已保存名称位置；interface 缺该字段，需在声明源码范围找原拼写。 */
static TZrBool legacy_property_find_name_slice(
        SZrParserState *ps,
        const SZrLegacyPropertyMigrationEntry *entry,
        SZrLegacyPropertySlice *outSlice) {
    SZrAstNode *node;
    SZrString *name;
    const TZrChar *nameText;
    TZrSize nameLength;

    if (outSlice != ZR_NULL) {
        memset(outSlice, 0, sizeof(*outSlice));
    }
    if (ps == ZR_NULL || entry == ZR_NULL || entry->declaration == ZR_NULL ||
        outSlice == ZR_NULL) {
        return ZR_FALSE;
    }
    node = entry->declaration;
    if (node->type == ZR_AST_CLASS_PROPERTY &&
        node->data.classProperty.modifier != ZR_NULL) {
        SZrFileRange range =
                node->data.classProperty.modifier->type == ZR_AST_PROPERTY_GET
                        ? node->data.classProperty.modifier
                                  ->data.propertyGet.nameLocation
                        : node->data.classProperty.modifier
                                  ->data.propertySet.nameLocation;
        outSlice->start = range.start.offset;
        outSlice->length = range.end.offset - range.start.offset;
        return legacy_property_slice_is_valid(ps, *outSlice) &&
               outSlice->length > 0U;
    }

    name = legacy_property_entry_name(entry);
    if (node->type != ZR_AST_INTERFACE_PROPERTY_SIGNATURE ||
        name == ZR_NULL) {
        return ZR_FALSE;
    }
    nameText = ZrCore_String_GetNativeString(name);
    nameLength = ZrCore_String_GetByteLength(name);
    if (nameText == ZR_NULL || nameLength == 0U ||
        node->location.end.offset > ps->lexer->sourceLength) {
        return ZR_FALSE;
    }
    /* BUG: interface 名称按声明全文首次匹配；如 `pub get b: int;` 的 `b`
     * 先命中 `pub`，相关诊断指错位置，替换构建器也会找不到 get/set 而放弃自动修复。 */
    for (TZrSize offset = node->location.start.offset;
         offset + nameLength <= node->location.end.offset;
         offset++) {
        if (memcmp(ps->lexer->source + offset, nameText, nameLength) == 0) {
            outSlice->start = offset;
            outSlice->length = nameLength;
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

/* 自动替换需要原类型文本；无法从旧声明可靠切出时只报告不提供机器修复。 */
static TZrBool legacy_property_find_type_slice(
        SZrParserState *ps,
        const SZrLegacyPropertyMigrationEntry *entry,
        SZrLegacyPropertySlice *outSlice) {
    SZrLegacyPropertySlice nameSlice;
    SZrAstNode *node;
    TZrSize colonOffset;
    TZrSize typeEnd;

    if (outSlice != ZR_NULL) {
        memset(outSlice, 0, sizeof(*outSlice));
    }
    if (!legacy_property_find_name_slice(ps, entry, &nameSlice) ||
        outSlice == ZR_NULL) {
        return ZR_FALSE;
    }
    node = entry->declaration;
    typeEnd = node->location.end.offset;
    if (node->type == ZR_AST_CLASS_PROPERTY &&
        node->data.classProperty.modifier != ZR_NULL) {
        SZrAstNode *modifier = node->data.classProperty.modifier;
        SZrAstNode *body = modifier->type == ZR_AST_PROPERTY_GET
                                  ? modifier->data.propertyGet.body
                                  : modifier->data.propertySet.body;
        typeEnd = body != ZR_NULL
                          ? body->location.start.offset
                          : modifier->location.end.offset;
        if (modifier->type == ZR_AST_PROPERTY_SET) {
            TZrSize closeOffset;
            if (!legacy_property_find_char(
                        ps,
                        nameSlice.start + nameSlice.length,
                        typeEnd,
                        ')',
                        &closeOffset)) {
                return ZR_FALSE;
            }
            typeEnd = closeOffset;
        }
    } else {
        TZrSize semicolonOffset;
        if (legacy_property_find_char(
                    ps,
                    nameSlice.start + nameSlice.length,
                    typeEnd,
                    ';',
                    &semicolonOffset)) {
            typeEnd = semicolonOffset;
        }
    }
    if (!legacy_property_find_char(
                ps,
                nameSlice.start + nameSlice.length,
                typeEnd,
                ':',
                &colonOffset)) {
        return ZR_FALSE;
    }
    outSlice->start = colonOffset + 1U;
    outSlice->length = typeEnd - outSlice->start;
    legacy_property_trim_slice(ps, outSlice);
    return outSlice->length > 0U;
}

/* 相关信息指向旧访问器体；无体签名以访问器本身作回退范围。 */
static SZrFileRange legacy_property_body_range(
        const SZrLegacyPropertyMigrationEntry *entry) {
    SZrFileRange range;
    SZrAstNode *modifier;
    SZrAstNode *body;

    memset(&range, 0, sizeof(range));
    if (entry == ZR_NULL || entry->declaration == ZR_NULL ||
        entry->declaration->type != ZR_AST_CLASS_PROPERTY ||
        entry->declaration->data.classProperty.modifier == ZR_NULL) {
        return range;
    }
    modifier = entry->declaration->data.classProperty.modifier;
    body = modifier->type == ZR_AST_PROPERTY_GET
                   ? modifier->data.propertyGet.body
                   : modifier->data.propertySet.body;
    return body != ZR_NULL ? body->location : modifier->location;
}

/* 比较类型的原始字节，避免把语义上可能相同但拼写不同的两个声明自动合并。 */
static TZrBool legacy_property_slices_equal(
        SZrParserState *ps,
        SZrLegacyPropertySlice left,
        SZrLegacyPropertySlice right) {
    return legacy_property_slice_is_valid(ps, left) &&
           legacy_property_slice_is_valid(ps, right) &&
           left.length == right.length &&
           memcmp(
                   ps->lexer->source + left.start,
                   ps->lexer->source + right.start,
                   left.length) == 0;
}

static TZrBool legacy_property_names_equal(
        const SZrLegacyPropertyMigrationEntry *left,
        const SZrLegacyPropertyMigrationEntry *right) {
    SZrString *leftName = legacy_property_entry_name(left);
    SZrString *rightName = legacy_property_entry_name(right);
    return leftName != ZR_NULL && rightName != ZR_NULL &&
           ZrCore_String_Equal(leftName, rightName);
}

/* 只有相邻访问器的类型、可见性、static、修饰符及装饰器条件均相容才给配对修复。 */
static TZrBool legacy_property_contracts_match(
        SZrParserState *ps,
        const SZrLegacyPropertyMigrationEntry *left,
        const SZrLegacyPropertyMigrationEntry *right) {
    SZrLegacyPropertySlice leftType;
    SZrLegacyPropertySlice rightType;
    const SZrClassProperty *leftProperty;
    const SZrClassProperty *rightProperty;

    if (left == ZR_NULL || right == ZR_NULL ||
        left->declaration == ZR_NULL || right->declaration == ZR_NULL ||
        left->declaration->type != ZR_AST_CLASS_PROPERTY ||
        right->declaration->type != ZR_AST_CLASS_PROPERTY ||
        !legacy_property_names_equal(left, right) ||
        !legacy_property_find_type_slice(ps, left, &leftType) ||
        !legacy_property_find_type_slice(ps, right, &rightType) ||
        !legacy_property_slices_equal(ps, leftType, rightType)) {
        return ZR_FALSE;
    }
    leftProperty = &left->declaration->data.classProperty;
    rightProperty = &right->declaration->data.classProperty;
    return leftProperty->access == rightProperty->access &&
           leftProperty->isStatic == rightProperty->isStatic &&
           leftProperty->modifierFlags == rightProperty->modifierFlags &&
           leftProperty->decorators != ZR_NULL &&
           rightProperty->decorators != ZR_NULL &&
           leftProperty->decorators->count == 0U &&
           rightProperty->decorators->count == 0U;
}

/* 替换建议不能截断；缓冲区不足时放弃自动修复并保留纯诊断。 */
static TZrBool legacy_property_text_append(
        SZrLegacyPropertyTextBuilder *builder,
        const TZrChar *text,
        TZrSize length) {
    if (builder == ZR_NULL || builder->data == ZR_NULL || text == ZR_NULL ||
        length > builder->capacity - builder->length - 1U) {
        return ZR_FALSE;
    }
    memcpy(builder->data + builder->length, text, length);
    builder->length += length;
    builder->data[builder->length] = '\0';
    return ZR_TRUE;
}

/* 源切片加入替换文本前再次核对 lexer 边界。 */
static TZrBool legacy_property_text_append_slice(
        SZrParserState *ps,
        SZrLegacyPropertyTextBuilder *builder,
        SZrLegacyPropertySlice slice) {
    return legacy_property_slice_is_valid(ps, slice) &&
           legacy_property_text_append(
                   builder,
                   ps->lexer->source + slice.start,
                   slice.length);
}

/* 旧 setter 的非 value 参数名需要在统一属性体内补局部绑定，保持原体引用。 */
static TZrBool legacy_property_append_class_accessor(
        SZrParserState *ps,
        SZrLegacyPropertyTextBuilder *builder,
        const SZrLegacyPropertyMigrationEntry *entry) {
    SZrAstNode *modifier;
    SZrAstNode *body;
    SZrLegacyPropertySlice bodySlice;

    if (entry == ZR_NULL || entry->declaration == ZR_NULL ||
        entry->declaration->type != ZR_AST_CLASS_PROPERTY ||
        entry->declaration->data.classProperty.modifier == ZR_NULL) {
        return ZR_FALSE;
    }
    modifier = entry->declaration->data.classProperty.modifier;
    body = modifier->type == ZR_AST_PROPERTY_GET
                   ? modifier->data.propertyGet.body
                   : modifier->data.propertySet.body;
    if (modifier->type == ZR_AST_PROPERTY_GET) {
        if (!legacy_property_text_append(builder, "    get ", 8U)) {
            return ZR_FALSE;
        }
    } else if (modifier->type == ZR_AST_PROPERTY_SET) {
        if (!legacy_property_text_append(builder, "    set ", 8U)) {
            return ZR_FALSE;
        }
    } else {
        return ZR_FALSE;
    }
    if (body == ZR_NULL) {
        return legacy_property_text_append(builder, ";\n", 2U);
    }
    bodySlice.start = body->location.start.offset;
    bodySlice.length = body->location.end.offset - body->location.start.offset;
    if (!legacy_property_slice_is_valid(ps, bodySlice) ||
        bodySlice.length < 2U) {
        return ZR_FALSE;
    }
    if (modifier->type == ZR_AST_PROPERTY_SET &&
        modifier->data.propertySet.param != ZR_NULL &&
        modifier->data.propertySet.param->name != ZR_NULL &&
        strcmp(
                ZrCore_String_GetNativeString(
                        modifier->data.propertySet.param->name),
                "value") != 0) {
        const TZrChar *parameterName = ZrCore_String_GetNativeString(
                modifier->data.propertySet.param->name);
        TZrSize parameterNameLength = ZrCore_String_GetByteLength(
                modifier->data.propertySet.param->name);
        SZrLegacyPropertySlice bodyTail = {
            bodySlice.start + 1U,
            bodySlice.length - 1U
        };
        return legacy_property_text_append(builder, "{ var ", 6U) &&
               legacy_property_text_append(
                       builder,
                       parameterName,
                       parameterNameLength) &&
               legacy_property_text_append(builder, " = value;", 9U) &&
               legacy_property_text_append_slice(ps, builder, bodyTail) &&
               legacy_property_text_append(builder, "\n", 1U);
    }
    return legacy_property_text_append_slice(ps, builder, bodySlice) &&
           legacy_property_text_append(builder, "\n", 1U);
}

/* 把一对旧式访问器压成单个统一 property 替换；结果只借给诊断构建器，
 * 返回缓冲区仍由调用方释放。 */
static TZrChar *legacy_property_build_replacement(
        SZrParserState *ps,
        const SZrLegacyPropertyMigrationEntry *first,
        const SZrLegacyPropertyMigrationEntry *second,
        TZrSize *outCapacity) {
    SZrLegacyPropertyTextBuilder builder;
    SZrLegacyPropertySlice nameSlice;
    SZrLegacyPropertySlice typeSlice;
    SZrLegacyPropertySlice prefixSlice;
    SZrAstNode *node;
    TZrSize spanLength;

    if (outCapacity != ZR_NULL) {
        *outCapacity = 0U;
    }
    if (ps == ZR_NULL || first == ZR_NULL || first->declaration == ZR_NULL ||
        outCapacity == ZR_NULL ||
        !legacy_property_find_name_slice(ps, first, &nameSlice) ||
        !legacy_property_find_type_slice(ps, first, &typeSlice)) {
        return ZR_NULL;
    }
    node = first->declaration;
    spanLength = node->location.end.offset - node->location.start.offset;
    if (second != ZR_NULL && second->declaration != ZR_NULL) {
        spanLength += second->declaration->location.end.offset -
                      second->declaration->location.start.offset;
    }
    builder.capacity = spanLength + 512U;
    builder.data = (TZrChar *)ZrCore_Memory_RawMalloc(
            ps->state->global,
            builder.capacity);
    if (builder.data == ZR_NULL) {
        return ZR_NULL;
    }
    builder.length = 0U;
    builder.data[0] = '\0';

    prefixSlice.start = node->location.start.offset;
    if (node->type == ZR_AST_CLASS_PROPERTY) {
        prefixSlice.length =
                node->data.classProperty.modifier->location.start.offset -
                prefixSlice.start;
    } else {
        TZrSize keywordOffset;
        if (!legacy_property_find_char(
                    ps,
                    prefixSlice.start,
                    nameSlice.start,
                    'g',
                    &keywordOffset) &&
            !legacy_property_find_char(
                    ps,
                    prefixSlice.start,
                    nameSlice.start,
                    's',
                    &keywordOffset)) {
            ZrCore_Memory_RawFree(
                    ps->state->global,
                    builder.data,
                    builder.capacity);
            return ZR_NULL;
        }
        prefixSlice.length = keywordOffset - prefixSlice.start;
    }

    if (!legacy_property_text_append_slice(ps, &builder, prefixSlice) ||
        !legacy_property_text_append(&builder, "property ", 9U) ||
        !legacy_property_text_append_slice(ps, &builder, nameSlice) ||
        !legacy_property_text_append(&builder, ": ", 2U) ||
        !legacy_property_text_append_slice(ps, &builder, typeSlice) ||
        !legacy_property_text_append(&builder, " {\n", 3U)) {
        ZrCore_Memory_RawFree(
                ps->state->global,
                builder.data,
                builder.capacity);
        return ZR_NULL;
    }
    if (node->type == ZR_AST_INTERFACE_PROPERTY_SIGNATURE) {
        if ((node->data.interfacePropertySignature.hasGet &&
             !legacy_property_text_append(&builder, "    get;\n", 9U)) ||
            (node->data.interfacePropertySignature.hasSet &&
             !legacy_property_text_append(&builder, "    set;\n", 9U))) {
            ZrCore_Memory_RawFree(
                    ps->state->global,
                    builder.data,
                    builder.capacity);
            return ZR_NULL;
        }
    } else if (!legacy_property_append_class_accessor(ps, &builder, first) ||
               (second != ZR_NULL &&
                !legacy_property_append_class_accessor(ps, &builder, second))) {
        ZrCore_Memory_RawFree(
                ps->state->global,
                builder.data,
                builder.capacity);
        return ZR_NULL;
    }
    if (!legacy_property_text_append(&builder, "}", 1U)) {
        ZrCore_Memory_RawFree(
                ps->state->global,
                builder.data,
                builder.capacity);
        return ZR_NULL;
    }
    *outCapacity = builder.capacity;
    return builder.data;
}

/* 为迁移入口发布结构化错误；只有无歧义的源切片才携带机器适用的替换。 */
static void legacy_property_publish_diagnostic(
        SZrParserState *ps,
        const SZrLegacyPropertyMigrationEntry *first,
        const SZrLegacyPropertyMigrationEntry *second,
        TZrBool allowFix) {
    SZrLegacyPropertySlice nameSlice;
    SZrLegacyPropertySlice typeSlice;
    SZrFileRange declarationRange;
    SZrFileRange nameRange;
    SZrFileRange typeRange;
    SZrFileRange bodyRange;
    SZrStructuredDiagnostic diagnostic;
    TZrChar *replacement = ZR_NULL;
    TZrSize replacementCapacity = 0U;

    if (ps == ZR_NULL || first == ZR_NULL || first->declaration == ZR_NULL ||
        !legacy_property_find_name_slice(ps, first, &nameSlice)) {
        return;
    }
    declarationRange = first->declaration->location;
    if (second != ZR_NULL && second->declaration != ZR_NULL) {
        declarationRange = ZrParser_FileRange_Merge(
                declarationRange,
                second->declaration->location);
    }
    nameRange = legacy_property_range_from_slice(ps, nameSlice);
    memset(&typeRange, 0, sizeof(typeRange));
    if (legacy_property_find_type_slice(ps, first, &typeSlice)) {
        typeRange = legacy_property_range_from_slice(ps, typeSlice);
    }
    bodyRange = legacy_property_body_range(first);
    if (allowFix) {
        replacement = legacy_property_build_replacement(
                ps,
                first,
                second,
                &replacementCapacity);
    }
    if (!ZrParser_DiagnosticBuilder_BuildLegacyPropertySyntax(
                ps->state,
                &diagnostic,
                declarationRange,
                nameRange,
                typeRange.source != ZR_NULL ? &typeRange : ZR_NULL,
                bodyRange.source != ZR_NULL ? &bodyRange : ZR_NULL,
                replacement)) {
        if (replacement != ZR_NULL) {
            ZrCore_Memory_RawFree(
                    ps->state->global,
                    replacement,
                    replacementCapacity);
        }
        return;
    }
    if (second != ZR_NULL) {
        SZrFileRange secondBody = legacy_property_body_range(second);
        if (secondBody.source != ZR_NULL) {
            (void)ZrParser_StructuredDiagnostic_AddRelatedInformation(
                    ps->state,
                    &diagnostic,
                    secondBody,
                    "Paired legacy accessor body");
        }
    }
    report_structured_parser_error(ps, &diagnostic, ps->lexer->t.token);
    ZrParser_StructuredDiagnostic_Free(ps->state, &diagnostic);
    if (replacement != ZR_NULL) {
        ZrCore_Memory_RawFree(
                ps->state->global,
                replacement,
                replacementCapacity);
    }
}

/* class/interface 声明各自创建一个短期收集器；成员 AST 尚未交付正常编译器。 */
void parser_property_migration_collection_init(
        SZrState *state,
        SZrLegacyPropertyMigrationCollection *collection) {
    if (collection == ZR_NULL) {
        return;
    }
    memset(collection, 0, sizeof(*collection));
    if (state != ZR_NULL) {
        ZrCore_Array_Init(
                state,
                &collection->entries,
                sizeof(SZrLegacyPropertyMigrationEntry),
                ZR_PARSER_INITIAL_CAPACITY_TINY);
    }
}

/* 成功后收集器暂持 declaration，直到 publish_and_free；失败则仍归调用者。 */
TZrBool parser_property_migration_collection_append(
        SZrState *state,
        SZrLegacyPropertyMigrationCollection *collection,
        SZrAstNode *declaration) {
    SZrLegacyPropertyMigrationEntry entry;

    /* BUG: Array_Init 分配 head 失败仍将 isValid 置真；这里通过校验后
     * Array_Push 会断言空 head，OOM 时无法可靠返回失败给调用方。 */
    if (state == ZR_NULL || collection == ZR_NULL || declaration == ZR_NULL ||
        !collection->entries.isValid) {
        return ZR_FALSE;
    }
    entry.declaration = declaration;
    entry.memberOrdinal = collection->nextMemberOrdinal++;
    ZrCore_Array_Push(state, &collection->entries, &entry);
    return ZR_TRUE;
}

/* 普通成员也占一个序号，禁止把被其他成员隔开的旧 get/set 配成一组。 */
void parser_property_migration_collection_mark_current_member(
        SZrLegacyPropertyMigrationCollection *collection) {
    if (collection != ZR_NULL) {
        collection->nextMemberOrdinal++;
    }
}

/* 同名同类型旧声明若不相邻，单项修复也应降级，避免产生重复属性。 */
static TZrBool legacy_property_entries_share_identity(
        SZrParserState *ps,
        const SZrLegacyPropertyMigrationEntry *left,
        const SZrLegacyPropertyMigrationEntry *right) {
    SZrLegacyPropertySlice leftType;
    SZrLegacyPropertySlice rightType;

    return legacy_property_names_equal(left, right) &&
           legacy_property_find_type_slice(ps, left, &leftType) &&
           legacy_property_find_type_slice(ps, right, &rightType) &&
           legacy_property_slices_equal(ps, leftType, rightType);
}

/* 迁移解析完成后先处理相邻 getter/setter，再处理单项或歧义项；
 * 无论诊断是否成功构建，最终都释放暂存 AST，不交给声明成员数组。 */
void parser_property_migration_collection_publish_and_free(
        SZrParserState *ps,
        SZrLegacyPropertyMigrationCollection *collection) {
    TZrBool *handled = ZR_NULL;
    TZrSize handledSize;

    if (ps == ZR_NULL || collection == ZR_NULL ||
        !collection->entries.isValid) {
        return;
    }
    handledSize = collection->entries.length * sizeof(TZrBool);
    if (handledSize > 0U) {
        handled = (TZrBool *)ZrCore_Memory_RawMalloc(
                ps->state->global,
                handledSize);
        if (handled != ZR_NULL) {
            memset(handled, 0, handledSize);
        }
    }

    for (TZrSize index = 0U;
         handled != ZR_NULL && index + 1U < collection->entries.length;
         index++) {
        SZrLegacyPropertyMigrationEntry *left =
                (SZrLegacyPropertyMigrationEntry *)ZrCore_Array_Get(
                        &collection->entries,
                        index);
        SZrLegacyPropertyMigrationEntry *right =
                (SZrLegacyPropertyMigrationEntry *)ZrCore_Array_Get(
                        &collection->entries,
                        index + 1U);
        TZrBool oppositeRoles;

        if (handled[index] || handled[index + 1U] || left == ZR_NULL ||
            right == ZR_NULL || left->declaration == ZR_NULL ||
            right->declaration == ZR_NULL ||
            left->declaration->type != ZR_AST_CLASS_PROPERTY ||
            right->declaration->type != ZR_AST_CLASS_PROPERTY ||
            right->memberOrdinal != left->memberOrdinal + 1U) {
            continue;
        }
        oppositeRoles =
                (legacy_property_entry_is_getter(left) &&
                 legacy_property_entry_is_setter(right)) ||
                (legacy_property_entry_is_setter(left) &&
                 legacy_property_entry_is_getter(right));
        if (!oppositeRoles) {
            continue;
        }
        if (legacy_property_contracts_match(ps, left, right)) {
            legacy_property_publish_diagnostic(
                    ps,
                    left,
                    right,
                    ZR_TRUE);
        } else {
            legacy_property_publish_diagnostic(
                    ps,
                    left,
                    ZR_NULL,
                    ZR_FALSE);
            legacy_property_publish_diagnostic(
                    ps,
                    right,
                    ZR_NULL,
                    ZR_FALSE);
        }
        handled[index] = ZR_TRUE;
        handled[index + 1U] = ZR_TRUE;
    }

    for (TZrSize index = 0U; index < collection->entries.length; index++) {
        SZrLegacyPropertyMigrationEntry *entry =
                (SZrLegacyPropertyMigrationEntry *)ZrCore_Array_Get(
                        &collection->entries,
                        index);
        TZrBool hasIdentityConflict = ZR_FALSE;

        if (entry == ZR_NULL || entry->declaration == ZR_NULL ||
            (handled != ZR_NULL && handled[index])) {
            continue;
        }
        for (TZrSize otherIndex = 0U;
             otherIndex < collection->entries.length;
             otherIndex++) {
            SZrLegacyPropertyMigrationEntry *other;
            if (otherIndex == index) {
                continue;
            }
            other = (SZrLegacyPropertyMigrationEntry *)ZrCore_Array_Get(
                    &collection->entries,
                    otherIndex);
            if (legacy_property_entries_share_identity(ps, entry, other)) {
                hasIdentityConflict = ZR_TRUE;
                break;
            }
        }
        legacy_property_publish_diagnostic(
                ps,
                entry,
                ZR_NULL,
                !hasIdentityConflict);
    }

    for (TZrSize index = 0U; index < collection->entries.length; index++) {
        SZrLegacyPropertyMigrationEntry *entry =
                (SZrLegacyPropertyMigrationEntry *)ZrCore_Array_Get(
                        &collection->entries,
                        index);
        if (entry != ZR_NULL && entry->declaration != ZR_NULL) {
            ZrParser_Ast_Free(ps->state, entry->declaration);
            entry->declaration = ZR_NULL;
        }
    }
    if (handled != ZR_NULL) {
        ZrCore_Memory_RawFree(ps->state->global, handled, handledSize);
    }
    ZrCore_Array_Free(ps->state, &collection->entries);
    memset(collection, 0, sizeof(*collection));
}
