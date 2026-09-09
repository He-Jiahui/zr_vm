#include "metadata/lsp_native_declaration_projection.h"
#include "metadata/lsp_metadata_provider.h"

#include <stdio.h>
#include <string.h>

#define ZR_LSP_VIRTUAL_TEXT_INITIAL_CAPACITY 2048U
#define ZR_LSP_VIRTUAL_RECORD_INITIAL_CAPACITY 32U

typedef struct SZrLspVirtualBuilder {
    SZrState *state;
    SZrString *uri;
    SZrArray text;
    SZrArray records;
    TZrSize offset;
    TZrInt32 line;
    TZrInt32 column;
} SZrLspVirtualBuilder;

static const TZrChar *virtual_documents_missing_exact_type_identifier(void) {
    return "ZrMissingExactType";
}

static TZrBool virtual_documents_type_text_is_specific(const TZrChar *typeText) {
    return typeText != ZR_NULL && typeText[0] != '\0' &&
           strcmp(typeText, virtual_documents_missing_exact_type_identifier()) != 0 &&
           strcmp(typeText, "object") != 0 &&
           strcmp(typeText, "unknown") != 0;
}

static const TZrChar *virtual_documents_type_text_or_placeholder(const TZrChar *typeText) {
    return virtual_documents_type_text_is_specific(typeText)
               ? typeText
               : virtual_documents_missing_exact_type_identifier();
}

static const TZrChar *virtual_documents_type_keyword(EZrObjectPrototypeType prototypeType) {
    switch (prototypeType) {
        case ZR_OBJECT_PROTOTYPE_TYPE_STRUCT:
            return "struct";
        case ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE:
            return "interface";
        case ZR_OBJECT_PROTOTYPE_TYPE_ENUM:
            return "enum";
        case ZR_OBJECT_PROTOTYPE_TYPE_UNION:
            return "union";
        case ZR_OBJECT_PROTOTYPE_TYPE_CLASS:
        default:
            return "class";
    }
}

static const TZrChar *virtual_documents_meta_method_name(EZrMetaType metaType) {
    switch (metaType) {
        case ZR_META_CONSTRUCTOR:
            return "@constructor";
        case ZR_META_DESTRUCTOR:
            return "@destructor";
        case ZR_META_ADD:
            return "@add";
        case ZR_META_SUB:
            return "@sub";
        case ZR_META_MUL:
            return "@mul";
        case ZR_META_DIV:
            return "@div";
        case ZR_META_MOD:
            return "@mod";
        case ZR_META_POW:
            return "@pow";
        case ZR_META_NEG:
            return "@neg";
        case ZR_META_COMPARE:
            return "@compare";
        case ZR_META_TO_BOOL:
            return "@toBool";
        case ZR_META_TO_STRING:
            return "@toString";
        case ZR_META_TO_INT:
            return "@toInt";
        case ZR_META_TO_UINT:
            return "@toUInt";
        case ZR_META_TO_FLOAT:
            return "@toFloat";
        case ZR_META_CALL:
            return "@call";
        case ZR_META_GETTER:
            return "@getter";
        case ZR_META_SETTER:
            return "@setter";
        case ZR_META_SHIFT_LEFT:
            return "@shiftLeft";
        case ZR_META_SHIFT_RIGHT:
            return "@shiftRight";
        case ZR_META_BIT_AND:
            return "@bitAnd";
        case ZR_META_BIT_OR:
            return "@bitOr";
        case ZR_META_BIT_XOR:
            return "@bitXor";
        case ZR_META_BIT_NOT:
            return "@bitNot";
        case ZR_META_GET_ITEM:
            return "@getItem";
        case ZR_META_SET_ITEM:
            return "@setItem";
        case ZR_META_CLOSE:
            return "@close";
        default:
            return "@meta";
    }
}

static void virtual_builder_init(SZrState *state, SZrString *uri, SZrLspVirtualBuilder *builder) {
    memset(builder, 0, sizeof(*builder));
    builder->state = state;
    builder->uri = uri;
    builder->line = 1;
    builder->column = 1;
    ZrCore_Array_Init(state, &builder->text, sizeof(TZrChar), ZR_LSP_VIRTUAL_TEXT_INITIAL_CAPACITY);
    ZrCore_Array_Init(state, &builder->records, sizeof(SZrLspVirtualRecord), ZR_LSP_VIRTUAL_RECORD_INITIAL_CAPACITY);
}

static void virtual_builder_append_char(SZrLspVirtualBuilder *builder, TZrChar value) {
    if (builder == ZR_NULL || builder->state == ZR_NULL) {
        return;
    }

    ZrCore_Array_Push(builder->state, &builder->text, &value);
    builder->offset++;
    if (value == '\n') {
        builder->line++;
        builder->column = 1;
    } else {
        builder->column++;
    }
}

static void virtual_builder_append_text(SZrLspVirtualBuilder *builder, const TZrChar *text) {
    if (builder == ZR_NULL || text == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; text[index] != '\0'; index++) {
        virtual_builder_append_char(builder, text[index]);
    }
}

static SZrFilePosition virtual_builder_position(const SZrLspVirtualBuilder *builder) {
    if (builder == ZR_NULL) {
        return ZrParser_FilePosition_Create(0, 1, 1);
    }

    return ZrParser_FilePosition_Create(builder->offset, builder->line, builder->column);
}

static void virtual_builder_record_name(SZrLspVirtualBuilder *builder,
                                        EZrLspVirtualDeclarationKind kind,
                                        const void *declarationIdentity,
                                        const TZrChar *ownerName,
                                        const TZrChar *name,
                                        const TZrChar *targetModuleName) {
    SZrLspVirtualRecord record;
    SZrFilePosition start;
    SZrFilePosition end;

    if (builder == ZR_NULL || name == ZR_NULL) {
        return;
    }

    memset(&record, 0, sizeof(record));
    record.kind = kind;
    record.declarationIdentity = declarationIdentity;
    record.ownerName = ownerName;
    record.name = name;
    record.targetModuleName = targetModuleName;
    start = virtual_builder_position(builder);
    virtual_builder_append_text(builder, name);
    end = virtual_builder_position(builder);
    record.range = ZrParser_FileRange_Create(start, end, builder->uri);
    ZrCore_Array_Push(builder->state, &builder->records, &record);
}

static void virtual_builder_append_identifier_line(SZrLspVirtualBuilder *builder,
                                                   const TZrChar *prefix,
                                                   EZrLspVirtualDeclarationKind kind,
                                                   const void *declarationIdentity,
                                                   const TZrChar *ownerName,
                                                   const TZrChar *name,
                                                   const TZrChar *suffix,
                                                   const TZrChar *targetModuleName) {
    virtual_builder_append_text(builder, prefix);
    virtual_builder_record_name(builder, kind, declarationIdentity, ownerName, name, targetModuleName);
    virtual_builder_append_text(builder, suffix);
}

static void virtual_documents_format_generic_parameters(const ZrLibGenericParameterDescriptor *parameters,
                                                        TZrSize count,
                                                        TZrChar *buffer,
                                                        TZrSize bufferSize) {
    TZrSize used = 0;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }

    buffer[0] = '\0';
    if (parameters == ZR_NULL || count == 0) {
        return;
    }

    used += (TZrSize)snprintf(buffer + used, bufferSize - used, "<");
    for (TZrSize index = 0; index < count && used + 1 < bufferSize; index++) {
        const ZrLibGenericParameterDescriptor *parameter = &parameters[index];
        const TZrChar *separator = index > 0 ? ", " : "";
        const TZrChar *name = parameter->name != ZR_NULL ? parameter->name : "T";

        used += (TZrSize)snprintf(buffer + used, bufferSize - used, "%s%s", separator, name);
    }
    if (used + 2 < bufferSize) {
        snprintf(buffer + used, bufferSize - used, ">");
    }
}

static void virtual_documents_format_parameters(const ZrLibParameterDescriptor *parameters,
                                                TZrSize count,
                                                TZrChar *buffer,
                                                TZrSize bufferSize) {
    TZrSize used = 0;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }

    buffer[0] = '\0';
    for (TZrSize index = 0; parameters != ZR_NULL && index < count && used + 1 < bufferSize; index++) {
        const ZrLibParameterDescriptor *parameter = &parameters[index];
        const TZrChar *separator = index > 0 ? ", " : "";
        const TZrChar *name = parameter->name != ZR_NULL ? parameter->name : "arg";
        const TZrChar *typeName = virtual_documents_type_text_or_placeholder(parameter->typeName);

        used += (TZrSize)snprintf(buffer + used, bufferSize - used, "%s%s: %s", separator, name, typeName);
    }
}

static const TZrChar *virtual_documents_find_type_hint_signature(const ZrLibModuleDescriptor *descriptor,
                                                                 const TZrChar *symbolName,
                                                                 const TZrChar *symbolKind) {
    if (descriptor == ZR_NULL || symbolName == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < descriptor->typeHintCount; index++) {
        const ZrLibTypeHintDescriptor *typeHint = &descriptor->typeHints[index];
        if (typeHint->symbolName == ZR_NULL ||
            strcmp(typeHint->symbolName, symbolName) != 0 ||
            (symbolKind != ZR_NULL && typeHint->symbolKind != ZR_NULL &&
             strcmp(typeHint->symbolKind, symbolKind) != 0)) {
            continue;
        }

        if (typeHint->signature != ZR_NULL && typeHint->signature[0] != '\0') {
            return typeHint->signature;
        }
    }

    return ZR_NULL;
}

static TZrBool virtual_documents_try_extract_parameters_from_signature(const TZrChar *signatureText,
                                                                       TZrChar *buffer,
                                                                       TZrSize bufferSize) {
    const TZrChar *openParen;
    const TZrChar *cursor;
    TZrInt32 depth = 0;
    TZrSize parameterLength;

    if (buffer != ZR_NULL && bufferSize > 0) {
        buffer[0] = '\0';
    }
    if (signatureText == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    openParen = strchr(signatureText, '(');
    if (openParen == ZR_NULL) {
        return ZR_FALSE;
    }

    for (cursor = openParen; *cursor != '\0'; cursor++) {
        if (*cursor == '(') {
            depth++;
            continue;
        }
        if (*cursor != ')') {
            continue;
        }

        depth--;
        if (depth != 0) {
            continue;
        }

        parameterLength = (TZrSize)(cursor - openParen - 1);
        if (parameterLength + 1 > bufferSize) {
            return ZR_FALSE;
        }
        memcpy(buffer, openParen + 1, parameterLength);
        buffer[parameterLength] = '\0';
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static void virtual_documents_format_parameters_with_arity(const ZrLibParameterDescriptor *parameters,
                                                           TZrSize parameterCount,
                                                           TZrUInt16 minArgumentCount,
                                                           TZrUInt16 maxArgumentCount,
                                                           const TZrChar *signatureHint,
                                                           TZrChar *buffer,
                                                           TZrSize bufferSize) {
    virtual_documents_format_parameters(parameters, parameterCount, buffer, bufferSize);
    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }
    if (buffer[0] != '\0') {
        return;
    }
    if (parameterCount > 0) {
        return;
    }
    if (signatureHint != ZR_NULL &&
        virtual_documents_try_extract_parameters_from_signature(signatureHint, buffer, bufferSize)) {
        return;
    }
    if (maxArgumentCount == 0) {
        return;
    }
    if (minArgumentCount == maxArgumentCount) {
        (void)snprintf(buffer,
                       bufferSize,
                       "/* arity %u */",
                       (unsigned int)minArgumentCount);
    } else {
        (void)snprintf(buffer,
                       bufferSize,
                       "/* arity %u..%u */",
                       (unsigned int)minArgumentCount,
                       (unsigned int)maxArgumentCount);
    }
}

static void virtual_documents_format_type_tail(const ZrLibTypeDescriptor *typeDescriptor,
                                               TZrChar *buffer,
                                               TZrSize bufferSize) {
    TZrSize used = 0;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }

    buffer[0] = '\0';
    if (typeDescriptor == ZR_NULL) {
        return;
    }

    if (typeDescriptor->extendsTypeName != ZR_NULL && typeDescriptor->extendsTypeName[0] != '\0') {
        used += (TZrSize)snprintf(buffer + used,
                                  bufferSize - used,
                                  " extends %s",
                                  typeDescriptor->extendsTypeName);
    }

    if (typeDescriptor->implementsTypeNames != ZR_NULL && typeDescriptor->implementsTypeCount > 0 &&
        used + 1 < bufferSize) {
        used += (TZrSize)snprintf(buffer + used, bufferSize - used, " implements ");
        for (TZrSize index = 0; index < typeDescriptor->implementsTypeCount && used + 1 < bufferSize; index++) {
            const TZrChar *implementsName = typeDescriptor->implementsTypeNames[index];

            used += (TZrSize)snprintf(buffer + used,
                                      bufferSize - used,
                                      "%s%s",
                                      index > 0 ? ", " : "",
                                      virtual_documents_type_text_or_placeholder(implementsName));
        }
    }
}

static void virtual_documents_append_module_header(SZrLspVirtualBuilder *builder,
                                                   const ZrLibModuleDescriptor *descriptor) {
    if (builder == ZR_NULL || descriptor == ZR_NULL || descriptor->moduleName == ZR_NULL) {
        return;
    }

    virtual_builder_append_text(builder, "native extern(\"");
    virtual_builder_record_name(builder,
                                ZR_LSP_VIRTUAL_DECLARATION_MODULE,
                                descriptor,
                                ZR_NULL,
                                descriptor->moduleName,
                                ZR_NULL);
    virtual_builder_append_text(builder, "\") {\n");
}

static void virtual_documents_append_module_links(SZrLspVirtualBuilder *builder,
                                                  const ZrLibModuleDescriptor *descriptor) {
    for (TZrSize index = 0; descriptor != ZR_NULL && index < descriptor->moduleLinkCount; index++) {
        const ZrLibModuleLinkDescriptor *link = &descriptor->moduleLinks[index];
        TZrChar suffix[ZR_LSP_DOCUMENTATION_BUFFER_LENGTH];

        snprintf(suffix,
                 sizeof(suffix),
                 ": %s;\n",
                 link->moduleName != ZR_NULL ? link->moduleName : "");
        virtual_builder_append_identifier_line(builder,
                                               "    pub module ",
                                               ZR_LSP_VIRTUAL_DECLARATION_MODULE_LINK,
                                               link,
                                               ZR_NULL,
                                               link->name != ZR_NULL ? link->name : "",
                                               suffix,
                                               link->moduleName);
    }
}

static void virtual_documents_append_constants(SZrLspVirtualBuilder *builder,
                                               const ZrLibModuleDescriptor *descriptor) {
    for (TZrSize index = 0; descriptor != ZR_NULL && index < descriptor->constantCount; index++) {
        const ZrLibConstantDescriptor *constantDescriptor = &descriptor->constants[index];
        TZrChar suffix[ZR_LSP_DOCUMENTATION_BUFFER_LENGTH];

        snprintf(suffix,
                 sizeof(suffix),
                 ": %s;\n",
                 virtual_documents_type_text_or_placeholder(constantDescriptor->typeName));
        virtual_builder_append_identifier_line(builder,
                                               "    pub const ",
                                               ZR_LSP_VIRTUAL_DECLARATION_CONSTANT,
                                               constantDescriptor,
                                               ZR_NULL,
                                               constantDescriptor->name != ZR_NULL ? constantDescriptor->name : "",
                                               suffix,
                                               ZR_NULL);
    }
}

static void virtual_documents_append_functions(SZrLspVirtualBuilder *builder,
                                               const ZrLibModuleDescriptor *descriptor) {
    TZrChar parameters[ZR_LSP_DOCUMENTATION_BUFFER_LENGTH];
    TZrChar suffix[ZR_LSP_DOCUMENTATION_BUFFER_LENGTH];

    for (TZrSize index = 0; descriptor != ZR_NULL && index < descriptor->functionCount; index++) {
        const ZrLibFunctionDescriptor *functionDescriptor = &descriptor->functions[index];
        const TZrChar *signatureHint =
            virtual_documents_find_type_hint_signature(descriptor, functionDescriptor->name, "function");

        virtual_documents_format_parameters_with_arity(functionDescriptor->parameters,
                                                       functionDescriptor->parameterCount,
                                                       functionDescriptor->minArgumentCount,
                                                       functionDescriptor->maxArgumentCount,
                                                       signatureHint,
                                                       parameters,
                                                       sizeof(parameters));
        snprintf(suffix,
                 sizeof(suffix),
                 "(%s): %s;\n",
                 parameters,
                 virtual_documents_type_text_or_placeholder(functionDescriptor->returnTypeName));
        virtual_builder_append_identifier_line(builder,
                                               "    pub ",
                                               ZR_LSP_VIRTUAL_DECLARATION_FUNCTION,
                                               functionDescriptor,
                                               ZR_NULL,
                                               functionDescriptor->name != ZR_NULL ? functionDescriptor->name : "",
                                               suffix,
                                               ZR_NULL);
    }
}

static void virtual_documents_append_type_fields(SZrLspVirtualBuilder *builder,
                                                 const ZrLibTypeDescriptor *typeDescriptor) {
    for (TZrSize index = 0; typeDescriptor != ZR_NULL && index < typeDescriptor->fieldCount; index++) {
        const ZrLibFieldDescriptor *fieldDescriptor = &typeDescriptor->fields[index];
        TZrChar suffix[ZR_LSP_DOCUMENTATION_BUFFER_LENGTH];

        snprintf(suffix,
                 sizeof(suffix),
                 ": %s;\n",
                 virtual_documents_type_text_or_placeholder(fieldDescriptor->typeName));
        virtual_builder_append_identifier_line(builder,
                                               "        pub var ",
                                               ZR_LSP_VIRTUAL_DECLARATION_FIELD,
                                               fieldDescriptor,
                                               typeDescriptor->name,
                                               fieldDescriptor->name != ZR_NULL ? fieldDescriptor->name : "",
                                               suffix,
                                               ZR_NULL);
    }
}

static void virtual_documents_append_type_methods(SZrLspVirtualBuilder *builder,
                                                  const ZrLibTypeDescriptor *typeDescriptor) {
    TZrChar parameters[ZR_LSP_DOCUMENTATION_BUFFER_LENGTH];
    TZrChar prefix[ZR_LSP_TEXT_BUFFER_LENGTH];
    TZrChar suffix[ZR_LSP_DOCUMENTATION_BUFFER_LENGTH];

    for (TZrSize index = 0; typeDescriptor != ZR_NULL && index < typeDescriptor->methodCount; index++) {
        const ZrLibMethodDescriptor *methodDescriptor = &typeDescriptor->methods[index];

        virtual_documents_format_parameters_with_arity(methodDescriptor->parameters,
                                                        methodDescriptor->parameterCount,
                                                        methodDescriptor->minArgumentCount,
                                                        methodDescriptor->maxArgumentCount,
                                                        ZR_NULL,
                                                        parameters,
                                                        sizeof(parameters));
        snprintf(prefix,
                 sizeof(prefix),
                 "        pub %s",
                 methodDescriptor->isStatic ? "static " : "");
        snprintf(suffix,
                 sizeof(suffix),
                 "(%s): %s;\n",
                 parameters,
                 virtual_documents_type_text_or_placeholder(methodDescriptor->returnTypeName));
        virtual_builder_append_identifier_line(builder,
                                               prefix,
                                               ZR_LSP_VIRTUAL_DECLARATION_METHOD,
                                               methodDescriptor,
                                               typeDescriptor->name,
                                               methodDescriptor->name != ZR_NULL ? methodDescriptor->name : "",
                                               suffix,
                                               ZR_NULL);
    }
}

static void virtual_documents_append_type_meta_methods(SZrLspVirtualBuilder *builder,
                                                       const ZrLibTypeDescriptor *typeDescriptor) {
    TZrChar parameters[ZR_LSP_DOCUMENTATION_BUFFER_LENGTH];
    TZrChar suffix[ZR_LSP_DOCUMENTATION_BUFFER_LENGTH];

    for (TZrSize index = 0; typeDescriptor != ZR_NULL && index < typeDescriptor->metaMethodCount; index++) {
        const ZrLibMetaMethodDescriptor *metaMethodDescriptor = &typeDescriptor->metaMethods[index];
        const TZrChar *metaName = virtual_documents_meta_method_name(metaMethodDescriptor->metaType);

        virtual_documents_format_parameters_with_arity(metaMethodDescriptor->parameters,
                                                         metaMethodDescriptor->parameterCount,
                                                         metaMethodDescriptor->minArgumentCount,
                                                         metaMethodDescriptor->maxArgumentCount,
                                                         ZR_NULL,
                                                         parameters,
                                                         sizeof(parameters));
        snprintf(suffix,
                 sizeof(suffix),
                 "(%s): %s;\n",
                 parameters,
                 virtual_documents_type_text_or_placeholder(metaMethodDescriptor->returnTypeName));
        virtual_builder_append_identifier_line(builder,
                                               "        pub ",
                                               ZR_LSP_VIRTUAL_DECLARATION_META_METHOD,
                                               metaMethodDescriptor,
                                               typeDescriptor->name,
                                               metaName,
                                               suffix,
                                               ZR_NULL);
    }
}

static void virtual_documents_append_type_declaration(SZrLspVirtualBuilder *builder,
                                                      const ZrLibTypeDescriptor *typeDescriptor) {
    TZrChar genericParameters[ZR_LSP_TEXT_BUFFER_LENGTH];
    TZrChar typeTail[ZR_LSP_DOCUMENTATION_BUFFER_LENGTH];
    TZrChar prefix[ZR_LSP_TEXT_BUFFER_LENGTH];
    TZrChar suffix[ZR_LSP_DOCUMENTATION_BUFFER_LENGTH];

    if (builder == ZR_NULL || typeDescriptor == ZR_NULL || typeDescriptor->name == ZR_NULL) {
        return;
    }

    virtual_documents_format_generic_parameters(typeDescriptor->genericParameters,
                                                typeDescriptor->genericParameterCount,
                                                genericParameters,
                                                sizeof(genericParameters));
    virtual_documents_format_type_tail(typeDescriptor, typeTail, sizeof(typeTail));
    snprintf(prefix, sizeof(prefix), "    pub %s ", virtual_documents_type_keyword(typeDescriptor->prototypeType));
    snprintf(suffix, sizeof(suffix), "%s%s {\n", genericParameters, typeTail);
    virtual_builder_append_identifier_line(builder,
                                           prefix,
                                           ZR_LSP_VIRTUAL_DECLARATION_TYPE,
                                           typeDescriptor,
                                           ZR_NULL,
                                           typeDescriptor->name,
                                           suffix,
                                           ZR_NULL);
    virtual_documents_append_type_fields(builder, typeDescriptor);
    virtual_documents_append_type_methods(builder, typeDescriptor);
    virtual_documents_append_type_meta_methods(builder, typeDescriptor);
    if (typeDescriptor->enumMembers != ZR_NULL && typeDescriptor->enumMemberCount > 0) {
        for (TZrSize index = 0; index < typeDescriptor->enumMemberCount; index++) {
            const ZrLibEnumMemberDescriptor *enumMember = &typeDescriptor->enumMembers[index];
            virtual_builder_append_identifier_line(builder,
                                                   "        pub enum ",
                                                   ZR_LSP_VIRTUAL_DECLARATION_FIELD,
                                                   enumMember,
                                                   typeDescriptor->name,
                                                   enumMember->name != ZR_NULL ? enumMember->name : "",
                                                   ";\n",
                                                   ZR_NULL);
        }
    }
    virtual_builder_append_text(builder, "    }\n");
}

TZrBool ZrLanguageServer_LspNativeDeclarationProjection_Build(SZrState *state,
                                       const ZrLibModuleDescriptor *descriptor,
                                       SZrString *uri,
                                       SZrString **outText,
                                       SZrArray *outRecords) {
    SZrLspVirtualBuilder builder;
    TZrChar terminator = '\0';

    if (outText != ZR_NULL) {
        *outText = ZR_NULL;
    }
    if (state == ZR_NULL || descriptor == ZR_NULL || uri == ZR_NULL) {
        return ZR_FALSE;
    }

    virtual_builder_init(state, uri, &builder);
    virtual_documents_append_module_header(&builder, descriptor);
    virtual_documents_append_module_links(&builder, descriptor);
    virtual_documents_append_constants(&builder, descriptor);
    virtual_documents_append_functions(&builder, descriptor);
    for (TZrSize index = 0; index < descriptor->typeCount; index++) {
        virtual_documents_append_type_declaration(&builder, &descriptor->types[index]);
    }
    virtual_builder_append_text(&builder, "}\n");
    ZrCore_Array_Push(state, &builder.text, &terminator);

    if (outText != ZR_NULL) {
        *outText = ZrCore_String_Create(state,
                                        (TZrNativeString)builder.text.head,
                                        builder.text.length > 0 ? builder.text.length - 1 : 0);
    }
    if (outRecords != ZR_NULL) {
        *outRecords = builder.records;
        builder.records.isValid = ZR_FALSE;
    }

    ZrCore_Array_Free(state, &builder.text);
    if (builder.records.isValid) {
        ZrCore_Array_Free(state, &builder.records);
    }
    return ZR_TRUE;
}

TZrBool ZrLanguageServer_LspNativeDeclarationProjection_Find(
        SZrState *state,
        const ZrLibModuleDescriptor *descriptor,
        SZrString *uri,
        EZrLspVirtualDeclarationKind kind,
        const void *declarationIdentity,
        SZrFileRange *outRange) {
    SZrArray records = {0};
    const SZrLspVirtualRecord *match = ZR_NULL;
    TZrBool found = ZR_FALSE;

    if (outRange != ZR_NULL) {
        memset(outRange, 0, sizeof(*outRange));
    }
    if (state == ZR_NULL || descriptor == ZR_NULL || uri == ZR_NULL ||
        declarationIdentity == ZR_NULL || outRange == ZR_NULL ||
        !ZrLanguageServer_LspNativeDeclarationProjection_Build(
                state, descriptor, uri, ZR_NULL, &records)) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0U; index < records.length; index++) {
        const SZrLspVirtualRecord *candidate =
                (const SZrLspVirtualRecord *)ZrCore_Array_Get(&records, index);
        if (candidate == ZR_NULL || candidate->kind != kind ||
            candidate->declarationIdentity != declarationIdentity) {
            continue;
        }
        if (match != ZR_NULL) {
            goto cleanup;
        }
        match = candidate;
    }
    if (match != ZR_NULL) {
        *outRange = match->range;
        found = ZR_TRUE;
    }

cleanup:
    ZrCore_Array_Free(state, &records);
    return found;
}

TZrBool ZrLanguageServer_LspNativeDeclarationProjection_ResolveMember(
        SZrState *state,
        SZrLspResolvedMetadataMember *resolvedMember) {
    const void *identity;
    EZrLspVirtualDeclarationKind kind;

    if (resolvedMember == ZR_NULL) {
        return ZR_FALSE;
    }
    switch (resolvedMember->memberKind) {
        case ZR_LSP_METADATA_MEMBER_MODULE:
            identity = resolvedMember->moduleLinkDescriptor;
            kind = ZR_LSP_VIRTUAL_DECLARATION_MODULE_LINK;
            break;
        case ZR_LSP_METADATA_MEMBER_CONSTANT:
            identity = resolvedMember->constantDescriptor;
            kind = ZR_LSP_VIRTUAL_DECLARATION_CONSTANT;
            break;
        case ZR_LSP_METADATA_MEMBER_FUNCTION:
            identity = resolvedMember->functionDescriptor;
            kind = ZR_LSP_VIRTUAL_DECLARATION_FUNCTION;
            break;
        case ZR_LSP_METADATA_MEMBER_TYPE:
            identity = resolvedMember->typeDescriptor;
            kind = ZR_LSP_VIRTUAL_DECLARATION_TYPE;
            break;
        default:
            return ZR_FALSE;
    }
    return ZrLanguageServer_LspNativeDeclarationProjection_Find(
            state, resolvedMember->module.nativeDescriptor,
            resolvedMember->declarationUri, kind, identity,
            &resolvedMember->declarationRange);
}
