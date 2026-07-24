#include "lsp_property_code_actions.h"

#include "lsp_editor_features_internal.h"
#include "semantic/lsp_property_contract.h"
#include "semantic/semantic_analyzer_internal.h"
#include "zr_vm_parser/semantic_query.h"

#include <stdio.h>
#include <string.h>

static const TZrChar *lsp_property_action_access_text(
        EZrAccessModifier access) {
    switch (access) {
        case ZR_ACCESS_PUBLIC: return "pub";
        case ZR_ACCESS_PROTECTED: return "pro";
        case ZR_ACCESS_PRIVATE: return "pri";
        default: return ZR_NULL;
    }
}

static TZrBool lsp_property_action_append(
        SZrState *state,
        SZrArray *result,
        const TZrChar *title,
        SZrLspRange range,
        const TZrChar *newText) {
    SZrLspCodeAction *action;

    if (state == ZR_NULL || result == ZR_NULL || title == ZR_NULL ||
        newText == ZR_NULL) {
        return ZR_FALSE;
    }
    action = (SZrLspCodeAction *)ZrCore_Memory_RawMalloc(
            state->global,
            sizeof(SZrLspCodeAction));
    if (action == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(action, 0, sizeof(*action));
    action->title = lsp_editor_create_string(state, title, strlen(title));
    action->kind = lsp_editor_create_string(
            state,
            ZR_LSP_CODE_ACTION_KIND_REFACTOR_REWRITE,
            strlen(ZR_LSP_CODE_ACTION_KIND_REFACTOR_REWRITE));
    action->isPreferred = ZR_FALSE;
    ZrCore_Array_Init(
            state,
            &action->edits,
            sizeof(SZrLspTextEdit *),
            ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
    if (action->title == ZR_NULL || action->kind == ZR_NULL ||
        !lsp_editor_append_text_edit(
                state,
                &action->edits,
                range,
                newText,
                strlen(newText))) {
        ZrLanguageServer_Lsp_FreeTextEdits(state, &action->edits);
        ZrCore_Memory_RawFree(
                state->global,
                action,
                sizeof(SZrLspCodeAction));
        return ZR_FALSE;
    }
    ZrCore_Array_Push(state, result, &action);
    return ZR_TRUE;
}

static TZrBool lsp_property_action_find_owner(
        SZrSemanticAnalyzer *analyzer,
        SZrSymbol *propertySymbol,
        const SZrParserSemanticPropertyQuery *query,
        const SZrTypePrototypeInfo **outOwner,
        const SZrTypeMemberInfo **outMember) {
    const SZrTypePrototypeInfo *matchedOwner = ZR_NULL;
    const SZrTypeMemberInfo *matchedMember = ZR_NULL;

    if (outOwner != ZR_NULL) {
        *outOwner = ZR_NULL;
    }
    if (outMember != ZR_NULL) {
        *outMember = ZR_NULL;
    }
    if (analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL ||
        propertySymbol == ZR_NULL || propertySymbol->astNode == ZR_NULL ||
        query == ZR_NULL || outOwner == ZR_NULL || outMember == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize prototypeIndex = 0U;
         prototypeIndex < analyzer->compilerState->typePrototypes.length;
         prototypeIndex++) {
        const SZrTypePrototypeInfo *prototype =
                (const SZrTypePrototypeInfo *)ZrCore_Array_Get(
                        &analyzer->compilerState->typePrototypes,
                        prototypeIndex);
        if (prototype == ZR_NULL || prototype->isImportedNative ||
            prototype->type == ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE ||
            prototype->type == ZR_OBJECT_PROTOTYPE_TYPE_MODULE) {
            continue;
        }
        for (TZrSize memberIndex = 0U;
             memberIndex < prototype->members.length;
             memberIndex++) {
            const SZrTypeMemberInfo *member =
                    (const SZrTypeMemberInfo *)ZrCore_Array_Get(
                            (SZrArray *)&prototype->members,
                            memberIndex);
            if (member == ZR_NULL ||
                member->memberType != ZR_AST_PROPERTY_DECLARATION ||
                member->accessorRole != ZR_PROPERTY_ACCESSOR_ROLE_NONE ||
                member->propertySymbolId != query->propertySymbolId ||
                member->declarationNode != propertySymbol->astNode) {
                continue;
            }
            if (matchedMember != ZR_NULL) {
                return ZR_FALSE;
            }
            matchedOwner = prototype;
            matchedMember = member;
        }
    }
    if (matchedOwner == ZR_NULL || matchedMember == ZR_NULL) {
        return ZR_FALSE;
    }
    *outOwner = matchedOwner;
    *outMember = matchedMember;
    return ZR_TRUE;
}

static TZrBool lsp_property_action_line_indent(
        const TZrChar *content,
        TZrSize contentLength,
        TZrSize offset,
        TZrChar *buffer,
        TZrSize bufferSize) {
    TZrSize lineStart;
    TZrSize length;

    if (content == ZR_NULL || buffer == ZR_NULL || bufferSize == 0U ||
        offset > contentLength) {
        return ZR_FALSE;
    }
    lineStart = offset;
    while (lineStart > 0U && content[lineStart - 1U] != '\n') {
        lineStart--;
    }
    length = offset - lineStart;
    if (length >= bufferSize) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0U; index < length; index++) {
        if (content[lineStart + index] != ' ' &&
            content[lineStart + index] != '\t') {
            return ZR_FALSE;
        }
    }
    memcpy(buffer, content + lineStart, length);
    buffer[length] = '\0';
    return ZR_TRUE;
}

static TZrBool lsp_property_action_find_close_brace(
        const TZrChar *content,
        TZrSize contentLength,
        const SZrFileRange *declarationRange,
        TZrSize *outOffset) {
    TZrSize startOffset;
    TZrSize cursor;

    if (content == ZR_NULL || declarationRange == ZR_NULL ||
        outOffset == ZR_NULL ||
        declarationRange->start.offset >= contentLength ||
        declarationRange->end.offset > contentLength ||
        declarationRange->end.offset <= declarationRange->start.offset) {
        return ZR_FALSE;
    }
    startOffset = declarationRange->start.offset;
    cursor = declarationRange->end.offset;
    while (cursor > startOffset) {
        cursor--;
        if (content[cursor] == '}' &&
            lsp_editor_offset_is_code(content, contentLength, cursor)) {
            *outOffset = cursor;
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static TZrBool lsp_property_action_append_missing_accessor(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        const SZrTypePrototypeInfo *owner,
        const SZrParserSemanticPropertyQuery *property,
        const TZrChar *content,
        TZrSize contentLength,
        SZrArray *result) {
    SZrPropertyRequirementQuery requirement;
    const TZrChar *accessorText;
    const TZrChar *title;
    TZrSize closeOffset;
    TZrChar closeIndent[ZR_PARSER_DECLARATION_BUFFER_LENGTH];
    TZrChar editText[ZR_PARSER_DECLARATION_BUFFER_LENGTH];
    TZrInt32 written;

    if (state == ZR_NULL || analyzer == ZR_NULL || owner == ZR_NULL ||
        property == ZR_NULL || content == ZR_NULL || result == ZR_NULL ||
        owner->type != ZR_OBJECT_PROTOTYPE_TYPE_CLASS ||
        !ZrParser_Compiler_QueryPropertyRequirements(
                analyzer->compilerState,
                owner,
                property->propertySymbolId,
                &requirement) ||
        requirement.matchingContractCount != 1U) {
        return ZR_TRUE;
    }
    if (requirement.missingAccessorMask == ZR_PROPERTY_ACCESSOR_MASK_SET &&
        requirement.interfaceSetterSymbolId != ZR_SEMANTIC_ID_INVALID) {
        accessorText = "set";
        title = "Implement required set accessor";
    } else if (requirement.missingAccessorMask ==
                       ZR_PROPERTY_ACCESSOR_MASK_INIT &&
               requirement.interfaceInitializerSymbolId !=
                       ZR_SEMANTIC_ID_INVALID) {
        accessorText = "init";
        title = "Implement required init accessor";
    } else {
        return ZR_TRUE;
    }
    if (!lsp_property_action_find_close_brace(
                content,
                contentLength,
                &property->declarationRange,
                &closeOffset) ||
        !lsp_property_action_line_indent(
                content,
                contentLength,
                closeOffset,
                closeIndent,
                sizeof(closeIndent))) {
        return ZR_TRUE;
    }
    written = snprintf(
            editText,
            sizeof(editText),
            "    %s { }\n%s",
            accessorText,
            closeIndent);
    if (written <= 0 || (TZrSize)written >= sizeof(editText)) {
        return ZR_FALSE;
    }
    return lsp_property_action_append(
            state,
            result,
            title,
            lsp_editor_range_from_offsets(
                    content,
                    contentLength,
                    closeOffset,
                    closeOffset),
            editText);
}

static TZrBool lsp_property_action_has_field_collision(
        const SZrTypePrototypeInfo *owner,
        const TZrChar *fieldName) {
    if (owner == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_TRUE;
    }
    for (TZrSize index = 0U; index < owner->members.length; index++) {
        const SZrTypeMemberInfo *member =
                (const SZrTypeMemberInfo *)ZrCore_Array_Get(
                        (SZrArray *)&owner->members,
                        index);
        const TZrChar *memberName;
        if (member == ZR_NULL ||
            (member->memberType != ZR_AST_CLASS_FIELD &&
             member->memberType != ZR_AST_STRUCT_FIELD) ||
            member->name == ZR_NULL) {
            continue;
        }
        memberName = semantic_string_native(member->name);
        if (memberName != ZR_NULL && strcmp(memberName, fieldName) == 0) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static const TZrChar *lsp_property_action_accessor_prefix(
        EZrAccessModifier propertyAccess,
        EZrAccessModifier accessorAccess) {
    if (propertyAccess == accessorAccess) {
        return "";
    }
    return lsp_property_action_access_text(accessorAccess);
}

static TZrBool lsp_property_action_append_explicit_field(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        SZrSymbol *propertySymbol,
        const SZrTypePrototypeInfo *owner,
        const SZrParserSemanticPropertyQuery *query,
        const TZrChar *content,
        TZrSize contentLength,
        SZrArray *result) {
    const SZrPropertyDeclaration *declaration;
    const TZrChar *name;
    const TZrChar *ownerName;
    const TZrChar *propertyAccess;
    const TZrChar *getterAccess;
    const TZrChar *writeAccess;
    const TZrChar *writeKind;
    const TZrChar *target;
    TZrChar typeText[ZR_LSP_TYPE_BUFFER_LENGTH];
    TZrChar fieldName[ZR_PARSER_DECLARATION_BUFFER_LENGTH];
    TZrChar indent[ZR_PARSER_DECLARATION_BUFFER_LENGTH];
    TZrChar title[ZR_PARSER_DECLARATION_BUFFER_LENGTH];
    TZrChar editText[ZR_LSP_DOCUMENTATION_BUFFER_LENGTH];
    TZrInt32 fieldNameWritten;
    TZrInt32 titleWritten;
    TZrInt32 editWritten;

    if (state == ZR_NULL || analyzer == ZR_NULL || propertySymbol == ZR_NULL ||
        propertySymbol->astNode == ZR_NULL || owner == ZR_NULL ||
        query == ZR_NULL || content == ZR_NULL || result == ZR_NULL ||
        propertySymbol->astNode->type != ZR_AST_PROPERTY_DECLARATION ||
        query->referenceAccess != ZR_REFERENCE_ACCESS_NONE ||
        query->exportsWritableRef ||
        query->getterSymbolId == ZR_SEMANTIC_ID_INVALID ||
        query->modifierFlags != ZR_DECLARATION_MODIFIER_NONE) {
        return ZR_TRUE;
    }
    declaration = &propertySymbol->astNode->data.propertyDeclaration;
    if (declaration->decorators != ZR_NULL &&
        declaration->decorators->count > 0U) {
        return ZR_TRUE;
    }
    name = semantic_string_native(propertySymbol->name);
    ownerName = semantic_string_native(owner->name);
    propertyAccess = lsp_property_action_access_text(query->access);
    if (name == ZR_NULL || ownerName == ZR_NULL || propertyAccess == ZR_NULL ||
        !ZrLanguageServer_SemanticAnalyzer_FormatTypeId(
                analyzer->semanticContext,
                query->propertyTypeId,
                typeText,
                sizeof(typeText)) ||
        !lsp_property_action_line_indent(
                content,
                contentLength,
                query->declarationRange.start.offset,
                indent,
                sizeof(indent))) {
        return ZR_TRUE;
    }
    fieldNameWritten = snprintf(
            fieldName,
            sizeof(fieldName),
            "_%s",
            name);
    if (fieldNameWritten <= 1 ||
        (TZrSize)fieldNameWritten >= sizeof(fieldName) ||
        lsp_property_action_has_field_collision(owner, fieldName)) {
        return ZR_TRUE;
    }

    getterAccess = lsp_property_action_accessor_prefix(
            query->access,
            query->getterAccess);
    if (query->setterSymbolId != ZR_SEMANTIC_ID_INVALID) {
        writeKind = "set";
        writeAccess = lsp_property_action_accessor_prefix(
                query->access,
                query->setterAccess);
    } else if (query->initializerSymbolId != ZR_SEMANTIC_ID_INVALID) {
        writeKind = "init";
        writeAccess = lsp_property_action_accessor_prefix(
                query->access,
                query->initializerAccess);
    } else {
        writeKind = "set";
        writeAccess = "";
    }
    target = query->isStatic ? ownerName : "this";
    titleWritten = snprintf(
            title,
            sizeof(title),
            "Introduce explicit field for property %s",
            name);
    editWritten = snprintf(
            editText,
            sizeof(editText),
            "pri %svar %s: %s;\n"
            "%s%s %sproperty %s: %s {\n"
            "%s    %s%sget { return %s.%s; }\n"
            "%s    %s%s%s { %s.%s = value; }\n"
            "%s}",
            query->isStatic ? "static " : "",
            fieldName,
            typeText,
            indent,
            propertyAccess,
            query->isStatic ? "static " : "",
            name,
            typeText,
            indent,
            getterAccess,
            getterAccess[0] != '\0' ? " " : "",
            target,
            fieldName,
            indent,
            writeAccess,
            writeAccess[0] != '\0' ? " " : "",
            writeKind,
            target,
            fieldName,
            indent);
    if (titleWritten <= 0 || (TZrSize)titleWritten >= sizeof(title) ||
        editWritten <= 0 || (TZrSize)editWritten >= sizeof(editText)) {
        return ZR_FALSE;
    }
    return lsp_property_action_append(
            state,
            result,
            title,
            lsp_editor_range_from_offsets(
                    content,
                    contentLength,
                    query->declarationRange.start.offset,
                    query->declarationRange.end.offset),
            editText);
}

TZrBool ZrLanguageServer_LspPropertyCodeActions_Append(
        SZrState *state,
        SZrLspContext *context,
        SZrString *uri,
        const TZrChar *content,
        TZrSize contentLength,
        SZrLspRange requestedRange,
        SZrArray *result) {
    SZrSemanticAnalyzer *analyzer;
    SZrFilePosition position;
    SZrFileRange positionRange;
    SZrParserSemanticPropertyQuery query;
    SZrSymbol *propertySymbol;
    const SZrTypePrototypeInfo *owner;
    const SZrTypeMemberInfo *member;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL ||
        content == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    analyzer = ZrLanguageServer_Lsp_FindAnalyzer(state, context, uri);
    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL ||
        analyzer->compilerState == ZR_NULL) {
        return ZR_TRUE;
    }
    position = ZrLanguageServer_Lsp_GetDocumentFilePosition(
            context,
            uri,
            requestedRange.start);
    positionRange = ZrParser_FileRange_Create(position, position, ZR_NULL);
    if (!ZrParser_SemanticQuery_PropertyAt(
                analyzer->semanticContext,
                positionRange,
                ZR_NULL,
                &query)) {
        return ZR_TRUE;
    }
    propertySymbol =
            ZrLanguageServer_LspPropertyContract_FindSourceSymbolAt(
                    analyzer,
                    positionRange);
    if (propertySymbol == ZR_NULL || !propertySymbol->hasPropertyContract ||
        propertySymbol->semanticId != query.propertySymbolId ||
        !lsp_property_action_find_owner(
                analyzer,
                propertySymbol,
                &query,
                &owner,
                &member) ||
        member->propertyValueTypeId != query.propertyTypeId ||
        query.declarationRange.end.offset > contentLength) {
        return ZR_TRUE;
    }
    return lsp_property_action_append_missing_accessor(
                   state,
                   analyzer,
                   owner,
                   &query,
                   content,
                   contentLength,
                   result) &&
           lsp_property_action_append_explicit_field(
                   state,
                   analyzer,
                   propertySymbol,
                   owner,
                   &query,
                   content,
                   contentLength,
                   result);
}
