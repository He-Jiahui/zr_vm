#include "compiler_import_type_alias.h"

#include <stdio.h>

#include "compiler/expression/compile_expression_internal.h"
#include "compiler_type_value_alias.h"
#include "type_inference_internal.h"
#include "zr_vm_core/string.h"

static void register_type_alias(SZrCompilerState *cs,
        const SZrIdentifier *binding, SZrString *qualifiedTypeName) {
    SZrInferredType aliasType;

    ZrParser_InferredType_InitFull(cs->state, &aliasType,
            ZR_VALUE_TYPE_OBJECT, ZR_FALSE, qualifiedTypeName);
    compiler_register_type_value_alias(cs, binding->name, &aliasType,
            binding->syntaxContext);
    ZrParser_InferredType_Free(cs->state, &aliasType);
}

static SZrString *build_qualified_type_name(SZrCompilerState *cs,
        SZrString *moduleName, SZrString *typeName) {
    TZrNativeString moduleNameText = ZrCore_String_GetNativeString(moduleName);
    TZrNativeString typeNameText = ZrCore_String_GetNativeString(typeName);
    TZrChar buffer[ZR_PARSER_TYPE_NAME_BUFFER_LENGTH];
    TZrInt32 written;

    if (moduleNameText == ZR_NULL || typeNameText == ZR_NULL) {
        return ZR_NULL;
    }
    written = snprintf(buffer, sizeof(buffer), "%s.%s", moduleNameText, typeNameText);
    if (written <= 0 || (TZrSize)written >= sizeof(buffer)) {
        return ZR_NULL;
    }
    return ZrCore_String_Create(cs->state, buffer, (TZrSize)written);
}

static TZrBool report_duplicate_type_name(SZrCompilerState *cs,
        SZrString *name, SZrFileRange location) {
    TZrNativeString nameText = ZrCore_String_GetNativeString(name);
    TZrChar errorMessage[ZR_PARSER_ERROR_BUFFER_LENGTH];

    if (nameText != ZR_NULL) {
        snprintf(errorMessage, sizeof(errorMessage),
                "Type name '%s' is already declared in this context", nameText);
    } else {
        snprintf(errorMessage, sizeof(errorMessage),
                "Type name is already declared in this context");
    }
    ZrParser_Compiler_Error(cs, errorMessage, location);
    return ZR_FALSE;
}

static TZrBool register_explicit_type_name(SZrCompilerState *cs,
        const SZrIdentifier *binding, SZrFileRange location) {
    TZrBool isRoot;
    TZrBool publishRootName;
    const SZrScope *scope = cs->scopeStack.length > 0U
            ? ZrCore_Array_Get(&cs->scopeStack, cs->scopeStack.length - 1U)
            : ZR_NULL;

    if (!ZrParser_SyntaxContext_IsValid(binding->syntaxContext)) {
        ZrParser_Compiler_Error(cs,
                "Invalid syntax context for imported type binding", location);
        return ZR_FALSE;
    }
    isRoot = ZrParser_SyntaxContext_Equal(
            ZrParser_SyntaxContext_Normalize(
                    binding->syntaxContext, ZR_SYNTAX_NAMESPACE_LOCAL),
            ZrParser_SyntaxContext_Root());
    publishRootName = (TZrBool)(isRoot &&
            (scope == ZR_NULL || scope->preserveTypeValueAliases));
    if (compiler_find_current_type_value_alias(cs, binding->name,
                binding->syntaxContext) != ZR_NULL ||
        (publishRootName &&
         ((cs->typeEnv != ZR_NULL &&
           ZrParser_TypeEnvironment_LookupType(cs->typeEnv, binding->name)) ||
          find_type_declaration(cs, binding->name) != ZR_NULL))) {
        return report_duplicate_type_name(cs, binding->name, location);
    }

    /* The name-only registry represents outer root bindings; local and marked
     * aliases use the scoped map consumed by annotation inference. */
    if (publishRootName && cs->typeEnv != ZR_NULL &&
        !ZrParser_TypeEnvironment_RegisterType(cs->state, cs->typeEnv, binding->name)) {
        ZrParser_Compiler_Error(cs,
                "Failed to register explicit imported type binding", location);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool destructuring_entry_identifiers(SZrAstNode *entry,
        const SZrIdentifier **outBinding, SZrString **outFieldName,
        SZrFileRange *outLocation) {
    *outBinding = ZR_NULL;
    *outFieldName = ZR_NULL;
    if (entry == ZR_NULL) {
        return ZR_FALSE;
    }
    if (entry->type == ZR_AST_IDENTIFIER_LITERAL) {
        if (entry->data.identifier.name == ZR_NULL) {
            return ZR_FALSE;
        }
        *outBinding = &entry->data.identifier;
        *outFieldName = entry->data.identifier.name;
        *outLocation = entry->location;
        return ZR_TRUE;
    }
    if (entry->type == ZR_AST_KEY_VALUE_PAIR) {
        SZrKeyValuePair *pair = &entry->data.keyValuePair;

        if (pair->keyIsComputed || pair->key == ZR_NULL ||
            pair->key->type != ZR_AST_IDENTIFIER_LITERAL ||
            pair->key->data.identifier.name == ZR_NULL || pair->value == ZR_NULL ||
            pair->value->type != ZR_AST_IDENTIFIER_LITERAL ||
            pair->value->data.identifier.name == ZR_NULL) {
            return ZR_FALSE;
        }
        *outBinding = &pair->key->data.identifier;
        *outFieldName = pair->value->data.identifier.name;
        *outLocation = entry->location;
        return ZR_TRUE;
    }
    return ZR_FALSE;
}

TZrBool compiler_register_imported_destructured_type_aliases(
        SZrCompilerState *cs, SZrAstNode *node) {
    SZrVariableDeclaration *decl;
    SZrString *moduleName;
    SZrTypePrototypeInfo *modulePrototype;
    SZrDestructuringObject *destructuring;

    if (cs == ZR_NULL || node == ZR_NULL || node->type != ZR_AST_VARIABLE_DECLARATION) {
        return ZR_TRUE;
    }
    decl = &node->data.variableDeclaration;
    if (decl->pattern == ZR_NULL || decl->pattern->type != ZR_AST_DESTRUCTURING_OBJECT ||
        decl->value == ZR_NULL || decl->value->type != ZR_AST_IMPORT_EXPRESSION ||
        decl->value->data.importExpression.modulePath == ZR_NULL ||
        decl->value->data.importExpression.modulePath->type != ZR_AST_STRING_LITERAL ||
        decl->value->data.importExpression.modulePath->data.stringLiteral.value == ZR_NULL) {
        return ZR_TRUE;
    }

    moduleName = decl->value->data.importExpression.modulePath->data.stringLiteral.value;
    if (!ensure_import_module_compile_info(cs, moduleName)) {
        return ZR_TRUE;
    }
    modulePrototype = find_compiler_type_prototype(cs, moduleName);
    if (modulePrototype == ZR_NULL || modulePrototype->type != ZR_OBJECT_PROTOTYPE_TYPE_MODULE) {
        return ZR_TRUE;
    }
    destructuring = &decl->pattern->data.destructuringObject;
    if (destructuring->keys == ZR_NULL) {
        return ZR_TRUE;
    }
    for (TZrSize index = 0U; index < destructuring->keys->count; index++) {
        SZrAstNode *entry = destructuring->keys->nodes[index];
        const SZrIdentifier *binding;
        SZrString *fieldName;
        SZrFileRange bindingLocation;
        SZrTypeMemberInfo *memberInfo;
        SZrString *qualifiedTypeName;

        if (!destructuring_entry_identifiers(entry, &binding, &fieldName, &bindingLocation)) {
            continue;
        }
        memberInfo = find_compiler_type_member(cs, moduleName, fieldName);
        if (memberInfo == ZR_NULL || memberInfo->fieldTypeName == ZR_NULL ||
            !ZrCore_String_Equal(memberInfo->fieldTypeName, fieldName) ||
            find_compiler_type_prototype(cs, fieldName) == ZR_NULL) {
            continue;
        }
        if (!register_explicit_type_name(cs, binding, bindingLocation)) {
            return ZR_FALSE;
        }
        qualifiedTypeName = build_qualified_type_name(cs, moduleName, fieldName);
        if (qualifiedTypeName != ZR_NULL) {
            register_type_alias(cs, binding, qualifiedTypeName);
        }
    }
    return ZR_TRUE;
}
