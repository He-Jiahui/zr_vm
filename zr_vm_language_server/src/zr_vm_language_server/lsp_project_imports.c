#include "lsp_project_internal.h"

#include "zr_vm_core/memory.h"
#include "zr_vm_core/string.h"

#include <string.h>

static void get_string_view(SZrString *value, TZrNativeString *text, TZrSize *length) {
    if (text == ZR_NULL || length == ZR_NULL) {
        return;
    }

    *text = ZR_NULL;
    *length = 0;
    if (value == ZR_NULL) {
        return;
    }

    if (value->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        *text = ZrCore_String_GetNativeStringShort(value);
        *length = value->shortStringLength;
    } else {
        *text = ZrCore_String_GetNativeString(value);
        *length = value->longStringLength;
    }
}

static SZrString *create_string_from_const_text(SZrState *state, const TZrChar *text) {
    if (state == ZR_NULL || text == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_String_Create(state, (TZrNativeString)text, strlen(text));
}

static TZrBool normalize_module_key(const TZrChar *modulePath, TZrChar *buffer, TZrSize bufferSize) {
    TZrSize length;
    TZrSize writeIndex = 0;

    if (modulePath == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }

    length = strlen(modulePath);
    while (length > 0 && (modulePath[length - 1] == '/' || modulePath[length - 1] == '\\')) {
        length--;
    }

    if (length >= 4 && memcmp(modulePath + length - 4, ".zro", 4) == 0) {
        length -= 4;
    } else if (length >= 3 && memcmp(modulePath + length - 3, ".zr", 3) == 0) {
        length -= 3;
    }

    while (length > 0 && (*modulePath == '/' || *modulePath == '\\')) {
        modulePath++;
        length--;
    }

    if (length == 0) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < length && writeIndex + 1 < bufferSize; index++) {
        TZrChar current = modulePath[index];
        buffer[writeIndex++] = current == '\\' ? '/' : current;
    }

    buffer[writeIndex] = '\0';
    return writeIndex > 0;
}

void ZrLanguageServer_LspProject_FreeImportBindings(SZrState *state, SZrArray *bindings) {
    for (TZrSize index = 0; bindings != ZR_NULL && index < bindings->length; index++) {
        SZrLspImportBinding **bindingPtr =
            (SZrLspImportBinding **)ZrCore_Array_Get(bindings, index);
        if (bindingPtr != ZR_NULL && *bindingPtr != ZR_NULL) {
            ZrCore_Memory_RawFree(state->global, *bindingPtr, sizeof(SZrLspImportBinding));
        }
    }

    if (bindings != ZR_NULL && bindings->isValid) {
        ZrCore_Array_Free(state, bindings);
    }
}

static TZrBool append_import_binding(SZrState *state,
                                     SZrArray *bindings,
                                     SZrString *aliasName,
                                     const TZrChar *moduleNameText) {
    SZrLspImportBinding *binding;
    SZrString *moduleName;

    if (state == ZR_NULL || bindings == ZR_NULL || aliasName == ZR_NULL || moduleNameText == ZR_NULL) {
        return ZR_FALSE;
    }

    binding = (SZrLspImportBinding *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspImportBinding));
    moduleName = create_string_from_const_text(state, moduleNameText);
    if (binding == ZR_NULL || moduleName == ZR_NULL) {
        if (binding != ZR_NULL) {
            ZrCore_Memory_RawFree(state->global, binding, sizeof(SZrLspImportBinding));
        }
        return ZR_FALSE;
    }

    binding->aliasName = aliasName;
    binding->moduleName = moduleName;
    ZrCore_Array_Push(state, bindings, &binding);
    return ZR_TRUE;
}

void ZrLanguageServer_LspProject_CollectImportBindings(SZrState *state, SZrAstNode *node, SZrArray *bindings) {
    if (state == ZR_NULL || node == ZR_NULL || bindings == ZR_NULL) {
        return;
    }

    switch (node->type) {
        case ZR_AST_SCRIPT:
            if (node->data.script.statements != ZR_NULL && node->data.script.statements->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.script.statements->count; index++) {
                    ZrLanguageServer_LspProject_CollectImportBindings(state,
                                                                      node->data.script.statements->nodes[index],
                                                                      bindings);
                }
            }
            break;

        case ZR_AST_BLOCK:
            if (node->data.block.body != ZR_NULL && node->data.block.body->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.block.body->count; index++) {
                    ZrLanguageServer_LspProject_CollectImportBindings(state,
                                                                      node->data.block.body->nodes[index],
                                                                      bindings);
                }
            }
            break;

        case ZR_AST_VARIABLE_DECLARATION:
            if (node->data.variableDeclaration.pattern != ZR_NULL &&
                node->data.variableDeclaration.pattern->type == ZR_AST_IDENTIFIER_LITERAL &&
                node->data.variableDeclaration.value != ZR_NULL &&
                node->data.variableDeclaration.value->type == ZR_AST_IMPORT_EXPRESSION &&
                node->data.variableDeclaration.value->data.importExpression.modulePath != ZR_NULL &&
                node->data.variableDeclaration.value->data.importExpression.modulePath->type == ZR_AST_STRING_LITERAL &&
                node->data.variableDeclaration.value->data.importExpression.modulePath->data.stringLiteral.value !=
                    ZR_NULL) {
                TZrChar normalizedModule[ZR_LIBRARY_MAX_PATH_LENGTH];
                TZrNativeString moduleText;
                TZrSize moduleLength;

                get_string_view(node->data.variableDeclaration.value->data.importExpression.modulePath
                                    ->data.stringLiteral.value,
                                &moduleText,
                                &moduleLength);
                if (moduleText != ZR_NULL && moduleLength < sizeof(normalizedModule)) {
                    memcpy(normalizedModule, moduleText, moduleLength);
                    normalizedModule[moduleLength] = '\0';
                    if (normalize_module_key(normalizedModule, normalizedModule, sizeof(normalizedModule))) {
                        append_import_binding(state,
                                              bindings,
                                              node->data.variableDeclaration.pattern->data.identifier.name,
                                              normalizedModule);
                    }
                }
            }
            break;

        case ZR_AST_FUNCTION_DECLARATION:
            ZrLanguageServer_LspProject_CollectImportBindings(state, node->data.functionDeclaration.body, bindings);
            break;

        case ZR_AST_TEST_DECLARATION:
            ZrLanguageServer_LspProject_CollectImportBindings(state, node->data.testDeclaration.body, bindings);
            break;

        default:
            break;
    }
}

static SZrLspImportBinding *find_import_binding_by_alias(SZrArray *bindings, SZrString *aliasName) {
    for (TZrSize index = 0; bindings != ZR_NULL && index < bindings->length; index++) {
        SZrLspImportBinding **bindingPtr =
            (SZrLspImportBinding **)ZrCore_Array_Get(bindings, index);
        if (bindingPtr != ZR_NULL && *bindingPtr != ZR_NULL &&
            ZrLanguageServer_Lsp_StringsEqual((*bindingPtr)->aliasName, aliasName)) {
            return *bindingPtr;
        }
    }

    return ZR_NULL;
}

static TZrBool file_range_contains_position(SZrFileRange range, SZrFileRange position) {
    if (!ZrLanguageServer_Lsp_StringsEqual(range.source, position.source)) {
        return ZR_FALSE;
    }

    if (range.start.offset > 0 && range.end.offset > 0 &&
        position.start.offset > 0 && position.end.offset > 0) {
        return range.start.offset <= position.start.offset && position.end.offset <= range.end.offset;
    }

    return (range.start.line < position.start.line ||
            (range.start.line == position.start.line && range.start.column <= position.start.column)) &&
           (position.end.line < range.end.line ||
            (position.end.line == range.end.line && position.end.column <= range.end.column));
}

static TZrBool primary_expression_get_imported_member(SZrAstNode *node,
                                                      SZrArray *bindings,
                                                      SZrLspImportedMemberHit *outHit) {
    SZrAstNode *receiverNode;
    SZrAstNode *memberNode;
    SZrLspImportBinding *binding;

    if (node == ZR_NULL || node->type != ZR_AST_PRIMARY_EXPRESSION || bindings == ZR_NULL || outHit == ZR_NULL) {
        return ZR_FALSE;
    }

    receiverNode = node->data.primaryExpression.property;
    if (receiverNode == ZR_NULL || receiverNode->type != ZR_AST_IDENTIFIER_LITERAL ||
        node->data.primaryExpression.members == ZR_NULL || node->data.primaryExpression.members->count == 0) {
        return ZR_FALSE;
    }

    memberNode = node->data.primaryExpression.members->nodes[0];
    if (memberNode == ZR_NULL || memberNode->type != ZR_AST_MEMBER_EXPRESSION ||
        memberNode->data.memberExpression.property == ZR_NULL ||
        memberNode->data.memberExpression.property->type != ZR_AST_IDENTIFIER_LITERAL) {
        return ZR_FALSE;
    }

    binding = find_import_binding_by_alias(bindings, receiverNode->data.identifier.name);
    if (binding == ZR_NULL) {
        return ZR_FALSE;
    }

    outHit->moduleName = binding->moduleName;
    outHit->memberName = memberNode->data.memberExpression.property->data.identifier.name;
    outHit->location = memberNode->data.memberExpression.property->location;
    return ZR_TRUE;
}

static TZrBool find_imported_member_hit_recursive(SZrAstNode *node,
                                                  SZrArray *bindings,
                                                  SZrFileRange position,
                                                  SZrLspImportedMemberHit *outHit);

static TZrBool find_imported_member_hit_in_node_array(SZrAstNodeArray *nodes,
                                                      SZrArray *bindings,
                                                      SZrFileRange position,
                                                      SZrLspImportedMemberHit *outHit) {
    if (nodes == ZR_NULL || nodes->nodes == ZR_NULL || bindings == ZR_NULL || outHit == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < nodes->count; index++) {
        if (find_imported_member_hit_recursive(nodes->nodes[index], bindings, position, outHit)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool find_imported_member_hit_recursive(SZrAstNode *node,
                                                  SZrArray *bindings,
                                                  SZrFileRange position,
                                                  SZrLspImportedMemberHit *outHit) {
    SZrLspImportedMemberHit hit;

    if (node == ZR_NULL || bindings == ZR_NULL || outHit == ZR_NULL) {
        return ZR_FALSE;
    }

    if (primary_expression_get_imported_member(node, bindings, &hit)) {
        if (file_range_contains_position(hit.location, position)) {
            *outHit = hit;
            return ZR_TRUE;
        }
    }

    switch (node->type) {
        case ZR_AST_SCRIPT:
            return find_imported_member_hit_in_node_array(node->data.script.statements, bindings, position, outHit);

        case ZR_AST_BLOCK:
            return find_imported_member_hit_in_node_array(node->data.block.body, bindings, position, outHit);

        case ZR_AST_FUNCTION_DECLARATION:
            return find_imported_member_hit_recursive(node->data.functionDeclaration.body, bindings, position, outHit);

        case ZR_AST_TEST_DECLARATION:
            return find_imported_member_hit_recursive(node->data.testDeclaration.body, bindings, position, outHit);

        case ZR_AST_STRUCT_METHOD:
            return find_imported_member_hit_recursive(node->data.structMethod.body, bindings, position, outHit);

        case ZR_AST_STRUCT_META_FUNCTION:
            return find_imported_member_hit_recursive(node->data.structMetaFunction.body, bindings, position, outHit);

        case ZR_AST_CLASS_METHOD:
            return find_imported_member_hit_recursive(node->data.classMethod.body, bindings, position, outHit);

        case ZR_AST_CLASS_META_FUNCTION:
            return find_imported_member_hit_recursive(node->data.classMetaFunction.body, bindings, position, outHit);

        case ZR_AST_PROPERTY_GET:
            return find_imported_member_hit_recursive(node->data.propertyGet.body, bindings, position, outHit);

        case ZR_AST_PROPERTY_SET:
            return find_imported_member_hit_recursive(node->data.propertySet.body, bindings, position, outHit);

        case ZR_AST_CLASS_PROPERTY:
            return find_imported_member_hit_recursive(node->data.classProperty.modifier, bindings, position, outHit);

        case ZR_AST_VARIABLE_DECLARATION:
            return find_imported_member_hit_recursive(node->data.variableDeclaration.value, bindings, position, outHit);

        case ZR_AST_STRUCT_FIELD:
            return find_imported_member_hit_recursive(node->data.structField.init, bindings, position, outHit);

        case ZR_AST_CLASS_FIELD:
            return find_imported_member_hit_recursive(node->data.classField.init, bindings, position, outHit);

        case ZR_AST_ENUM_MEMBER:
            return find_imported_member_hit_recursive(node->data.enumMember.value, bindings, position, outHit);

        case ZR_AST_RETURN_STATEMENT:
            return find_imported_member_hit_recursive(node->data.returnStatement.expr, bindings, position, outHit);

        case ZR_AST_EXPRESSION_STATEMENT:
            return find_imported_member_hit_recursive(node->data.expressionStatement.expr, bindings, position, outHit);

        case ZR_AST_USING_STATEMENT:
            return find_imported_member_hit_recursive(node->data.usingStatement.resource, bindings, position, outHit) ||
                   find_imported_member_hit_recursive(node->data.usingStatement.body, bindings, position, outHit);

        case ZR_AST_BREAK_CONTINUE_STATEMENT:
            return find_imported_member_hit_recursive(node->data.breakContinueStatement.expr,
                                                      bindings,
                                                      position,
                                                      outHit);

        case ZR_AST_THROW_STATEMENT:
            return find_imported_member_hit_recursive(node->data.throwStatement.expr, bindings, position, outHit);

        case ZR_AST_OUT_STATEMENT:
            return find_imported_member_hit_recursive(node->data.outStatement.expr, bindings, position, outHit);

        case ZR_AST_TRY_CATCH_FINALLY_STATEMENT:
            return find_imported_member_hit_recursive(node->data.tryCatchFinallyStatement.block,
                                                      bindings,
                                                      position,
                                                      outHit) ||
                   find_imported_member_hit_in_node_array(node->data.tryCatchFinallyStatement.catchClauses,
                                                          bindings,
                                                          position,
                                                          outHit) ||
                   find_imported_member_hit_recursive(node->data.tryCatchFinallyStatement.finallyBlock,
                                                      bindings,
                                                      position,
                                                      outHit);

        case ZR_AST_CATCH_CLAUSE:
            return find_imported_member_hit_recursive(node->data.catchClause.block, bindings, position, outHit);

        case ZR_AST_ASSIGNMENT_EXPRESSION:
            return find_imported_member_hit_recursive(node->data.assignmentExpression.left, bindings, position, outHit) ||
                   find_imported_member_hit_recursive(node->data.assignmentExpression.right,
                                                      bindings,
                                                      position,
                                                      outHit);

        case ZR_AST_BINARY_EXPRESSION:
            return find_imported_member_hit_recursive(node->data.binaryExpression.left, bindings, position, outHit) ||
                   find_imported_member_hit_recursive(node->data.binaryExpression.right,
                                                      bindings,
                                                      position,
                                                      outHit);

        case ZR_AST_LOGICAL_EXPRESSION:
            return find_imported_member_hit_recursive(node->data.logicalExpression.left, bindings, position, outHit) ||
                   find_imported_member_hit_recursive(node->data.logicalExpression.right,
                                                      bindings,
                                                      position,
                                                      outHit);

        case ZR_AST_CONDITIONAL_EXPRESSION:
            return find_imported_member_hit_recursive(node->data.conditionalExpression.test,
                                                      bindings,
                                                      position,
                                                      outHit) ||
                   find_imported_member_hit_recursive(node->data.conditionalExpression.consequent,
                                                      bindings,
                                                      position,
                                                      outHit) ||
                   find_imported_member_hit_recursive(node->data.conditionalExpression.alternate,
                                                      bindings,
                                                      position,
                                                      outHit);

        case ZR_AST_UNARY_EXPRESSION:
            return find_imported_member_hit_recursive(node->data.unaryExpression.argument, bindings, position, outHit);

        case ZR_AST_TYPE_CAST_EXPRESSION:
            return find_imported_member_hit_recursive(node->data.typeCastExpression.expression,
                                                      bindings,
                                                      position,
                                                      outHit);

        case ZR_AST_LAMBDA_EXPRESSION:
            return find_imported_member_hit_recursive(node->data.lambdaExpression.block, bindings, position, outHit);

        case ZR_AST_IF_EXPRESSION:
            return find_imported_member_hit_recursive(node->data.ifExpression.condition, bindings, position, outHit) ||
                   find_imported_member_hit_recursive(node->data.ifExpression.thenExpr, bindings, position, outHit) ||
                   find_imported_member_hit_recursive(node->data.ifExpression.elseExpr, bindings, position, outHit);

        case ZR_AST_SWITCH_EXPRESSION:
            return find_imported_member_hit_recursive(node->data.switchExpression.expr, bindings, position, outHit) ||
                   find_imported_member_hit_in_node_array(node->data.switchExpression.cases,
                                                          bindings,
                                                          position,
                                                          outHit) ||
                   find_imported_member_hit_recursive(node->data.switchExpression.defaultCase,
                                                      bindings,
                                                      position,
                                                      outHit);

        case ZR_AST_SWITCH_CASE:
            return find_imported_member_hit_recursive(node->data.switchCase.value, bindings, position, outHit) ||
                   find_imported_member_hit_recursive(node->data.switchCase.block, bindings, position, outHit);

        case ZR_AST_SWITCH_DEFAULT:
            return find_imported_member_hit_recursive(node->data.switchDefault.block, bindings, position, outHit);

        case ZR_AST_FUNCTION_CALL:
            return find_imported_member_hit_in_node_array(node->data.functionCall.args, bindings, position, outHit);

        case ZR_AST_PRIMARY_EXPRESSION:
            return find_imported_member_hit_recursive(node->data.primaryExpression.property, bindings, position, outHit) ||
                   find_imported_member_hit_in_node_array(node->data.primaryExpression.members,
                                                          bindings,
                                                          position,
                                                          outHit);

        case ZR_AST_MEMBER_EXPRESSION:
            return node->data.memberExpression.computed &&
                   find_imported_member_hit_recursive(node->data.memberExpression.property,
                                                      bindings,
                                                      position,
                                                      outHit);

        case ZR_AST_ARRAY_LITERAL:
            return find_imported_member_hit_in_node_array(node->data.arrayLiteral.elements, bindings, position, outHit);

        case ZR_AST_OBJECT_LITERAL:
            return find_imported_member_hit_in_node_array(node->data.objectLiteral.properties,
                                                          bindings,
                                                          position,
                                                          outHit);

        case ZR_AST_KEY_VALUE_PAIR:
            return find_imported_member_hit_recursive(node->data.keyValuePair.key, bindings, position, outHit) ||
                   find_imported_member_hit_recursive(node->data.keyValuePair.value, bindings, position, outHit);

        case ZR_AST_WHILE_LOOP:
            return find_imported_member_hit_recursive(node->data.whileLoop.cond, bindings, position, outHit) ||
                   find_imported_member_hit_recursive(node->data.whileLoop.block, bindings, position, outHit);

        case ZR_AST_FOR_LOOP:
            return find_imported_member_hit_recursive(node->data.forLoop.init, bindings, position, outHit) ||
                   find_imported_member_hit_recursive(node->data.forLoop.cond, bindings, position, outHit) ||
                   find_imported_member_hit_recursive(node->data.forLoop.step, bindings, position, outHit) ||
                   find_imported_member_hit_recursive(node->data.forLoop.block, bindings, position, outHit);

        case ZR_AST_FOREACH_LOOP:
            return find_imported_member_hit_recursive(node->data.foreachLoop.expr, bindings, position, outHit) ||
                   find_imported_member_hit_recursive(node->data.foreachLoop.block, bindings, position, outHit);

        default:
            break;
    }

    return ZR_FALSE;
}

TZrBool ZrLanguageServer_LspProject_FindImportedMemberHit(SZrAstNode *node,
                                                          SZrArray *bindings,
                                                          SZrFileRange position,
                                                          SZrLspImportedMemberHit *outHit) {
    return find_imported_member_hit_recursive(node, bindings, position, outHit);
}

static TZrBool append_lsp_location(SZrState *state, SZrArray *result, SZrString *uri, SZrFileRange range) {
    SZrLspLocation *location;

    if (state == ZR_NULL || result == ZR_NULL || uri == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspLocation *), 4);
    }

    location = (SZrLspLocation *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspLocation));
    if (location == ZR_NULL) {
        return ZR_FALSE;
    }

    location->uri = uri;
    location->range = ZrLanguageServer_LspRange_FromFileRange(range);
    ZrCore_Array_Push(state, result, &location);
    return ZR_TRUE;
}

static TZrBool append_matching_imported_member_locations_recursive(SZrState *state,
                                                                   SZrAstNode *node,
                                                                   SZrArray *bindings,
                                                                   SZrString *moduleName,
                                                                   SZrString *memberName,
                                                                   SZrArray *result);

static TZrBool append_matching_imported_member_locations_in_node_array(SZrState *state,
                                                                       SZrAstNodeArray *nodes,
                                                                       SZrArray *bindings,
                                                                       SZrString *moduleName,
                                                                       SZrString *memberName,
                                                                       SZrArray *result) {
    if (nodes == ZR_NULL || nodes->nodes == ZR_NULL) {
        return ZR_TRUE;
    }

    for (TZrSize index = 0; index < nodes->count; index++) {
        if (!append_matching_imported_member_locations_recursive(state,
                                                                 nodes->nodes[index],
                                                                 bindings,
                                                                 moduleName,
                                                                 memberName,
                                                                 result)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool append_matching_imported_member_locations_recursive(SZrState *state,
                                                                   SZrAstNode *node,
                                                                   SZrArray *bindings,
                                                                   SZrString *moduleName,
                                                                   SZrString *memberName,
                                                                   SZrArray *result) {
    SZrLspImportedMemberHit hit;

    if (node == ZR_NULL || bindings == ZR_NULL || moduleName == ZR_NULL || memberName == ZR_NULL ||
        result == ZR_NULL) {
        return ZR_TRUE;
    }

    if (primary_expression_get_imported_member(node, bindings, &hit) &&
        ZrLanguageServer_Lsp_StringsEqual(hit.moduleName, moduleName) &&
        ZrLanguageServer_Lsp_StringsEqual(hit.memberName, memberName) &&
        !append_lsp_location(state, result, hit.location.source, hit.location)) {
        return ZR_FALSE;
    }

    switch (node->type) {
        case ZR_AST_SCRIPT:
            return append_matching_imported_member_locations_in_node_array(state,
                                                                           node->data.script.statements,
                                                                           bindings,
                                                                           moduleName,
                                                                           memberName,
                                                                           result);

        case ZR_AST_BLOCK:
            return append_matching_imported_member_locations_in_node_array(state,
                                                                           node->data.block.body,
                                                                           bindings,
                                                                           moduleName,
                                                                           memberName,
                                                                           result);

        case ZR_AST_FUNCTION_DECLARATION:
            return append_matching_imported_member_locations_recursive(state,
                                                                      node->data.functionDeclaration.body,
                                                                      bindings,
                                                                      moduleName,
                                                                      memberName,
                                                                      result);

        case ZR_AST_TEST_DECLARATION:
            return append_matching_imported_member_locations_recursive(state,
                                                                      node->data.testDeclaration.body,
                                                                      bindings,
                                                                      moduleName,
                                                                      memberName,
                                                                      result);

        case ZR_AST_STRUCT_METHOD:
            return append_matching_imported_member_locations_recursive(state,
                                                                      node->data.structMethod.body,
                                                                      bindings,
                                                                      moduleName,
                                                                      memberName,
                                                                      result);

        case ZR_AST_STRUCT_META_FUNCTION:
            return append_matching_imported_member_locations_recursive(state,
                                                                      node->data.structMetaFunction.body,
                                                                      bindings,
                                                                      moduleName,
                                                                      memberName,
                                                                      result);

        case ZR_AST_CLASS_METHOD:
            return append_matching_imported_member_locations_recursive(state,
                                                                      node->data.classMethod.body,
                                                                      bindings,
                                                                      moduleName,
                                                                      memberName,
                                                                      result);

        case ZR_AST_CLASS_META_FUNCTION:
            return append_matching_imported_member_locations_recursive(state,
                                                                      node->data.classMetaFunction.body,
                                                                      bindings,
                                                                      moduleName,
                                                                      memberName,
                                                                      result);

        case ZR_AST_PROPERTY_GET:
            return append_matching_imported_member_locations_recursive(state,
                                                                      node->data.propertyGet.body,
                                                                      bindings,
                                                                      moduleName,
                                                                      memberName,
                                                                      result);

        case ZR_AST_PROPERTY_SET:
            return append_matching_imported_member_locations_recursive(state,
                                                                      node->data.propertySet.body,
                                                                      bindings,
                                                                      moduleName,
                                                                      memberName,
                                                                      result);

        case ZR_AST_CLASS_PROPERTY:
            return append_matching_imported_member_locations_recursive(state,
                                                                      node->data.classProperty.modifier,
                                                                      bindings,
                                                                      moduleName,
                                                                      memberName,
                                                                      result);

        case ZR_AST_VARIABLE_DECLARATION:
            return append_matching_imported_member_locations_recursive(state,
                                                                      node->data.variableDeclaration.value,
                                                                      bindings,
                                                                      moduleName,
                                                                      memberName,
                                                                      result);

        case ZR_AST_STRUCT_FIELD:
            return append_matching_imported_member_locations_recursive(state,
                                                                      node->data.structField.init,
                                                                      bindings,
                                                                      moduleName,
                                                                      memberName,
                                                                      result);

        case ZR_AST_CLASS_FIELD:
            return append_matching_imported_member_locations_recursive(state,
                                                                      node->data.classField.init,
                                                                      bindings,
                                                                      moduleName,
                                                                      memberName,
                                                                      result);

        case ZR_AST_ENUM_MEMBER:
            return append_matching_imported_member_locations_recursive(state,
                                                                      node->data.enumMember.value,
                                                                      bindings,
                                                                      moduleName,
                                                                      memberName,
                                                                      result);

        case ZR_AST_RETURN_STATEMENT:
            return append_matching_imported_member_locations_recursive(state,
                                                                      node->data.returnStatement.expr,
                                                                      bindings,
                                                                      moduleName,
                                                                      memberName,
                                                                      result);

        case ZR_AST_EXPRESSION_STATEMENT:
            return append_matching_imported_member_locations_recursive(state,
                                                                      node->data.expressionStatement.expr,
                                                                      bindings,
                                                                      moduleName,
                                                                      memberName,
                                                                      result);

        case ZR_AST_USING_STATEMENT:
            return append_matching_imported_member_locations_recursive(state,
                                                                      node->data.usingStatement.resource,
                                                                      bindings,
                                                                      moduleName,
                                                                      memberName,
                                                                      result) &&
                   append_matching_imported_member_locations_recursive(state,
                                                                      node->data.usingStatement.body,
                                                                      bindings,
                                                                      moduleName,
                                                                      memberName,
                                                                      result);

        case ZR_AST_BREAK_CONTINUE_STATEMENT:
            return append_matching_imported_member_locations_recursive(state,
                                                                      node->data.breakContinueStatement.expr,
                                                                      bindings,
                                                                      moduleName,
                                                                      memberName,
                                                                      result);

        case ZR_AST_THROW_STATEMENT:
            return append_matching_imported_member_locations_recursive(state,
                                                                      node->data.throwStatement.expr,
                                                                      bindings,
                                                                      moduleName,
                                                                      memberName,
                                                                      result);

        case ZR_AST_OUT_STATEMENT:
            return append_matching_imported_member_locations_recursive(state,
                                                                      node->data.outStatement.expr,
                                                                      bindings,
                                                                      moduleName,
                                                                      memberName,
                                                                      result);

        case ZR_AST_TRY_CATCH_FINALLY_STATEMENT:
            return append_matching_imported_member_locations_recursive(state,
                                                                      node->data.tryCatchFinallyStatement.block,
                                                                      bindings,
                                                                      moduleName,
                                                                      memberName,
                                                                      result) &&
                   append_matching_imported_member_locations_in_node_array(state,
                                                                           node->data.tryCatchFinallyStatement.catchClauses,
                                                                           bindings,
                                                                           moduleName,
                                                                           memberName,
                                                                           result) &&
                   append_matching_imported_member_locations_recursive(state,
                                                                      node->data.tryCatchFinallyStatement.finallyBlock,
                                                                      bindings,
                                                                      moduleName,
                                                                      memberName,
                                                                      result);

        case ZR_AST_CATCH_CLAUSE:
            return append_matching_imported_member_locations_recursive(state,
                                                                      node->data.catchClause.block,
                                                                      bindings,
                                                                      moduleName,
                                                                      memberName,
                                                                      result);

        case ZR_AST_ASSIGNMENT_EXPRESSION:
            return append_matching_imported_member_locations_recursive(state,
                                                                      node->data.assignmentExpression.left,
                                                                      bindings,
                                                                      moduleName,
                                                                      memberName,
                                                                      result) &&
                   append_matching_imported_member_locations_recursive(state,
                                                                      node->data.assignmentExpression.right,
                                                                      bindings,
                                                                      moduleName,
                                                                      memberName,
                                                                      result);

        case ZR_AST_BINARY_EXPRESSION:
            return append_matching_imported_member_locations_recursive(state,
                                                                      node->data.binaryExpression.left,
                                                                      bindings,
                                                                      moduleName,
                                                                      memberName,
                                                                      result) &&
                   append_matching_imported_member_locations_recursive(state,
                                                                      node->data.binaryExpression.right,
                                                                      bindings,
                                                                      moduleName,
                                                                      memberName,
                                                                      result);

        case ZR_AST_LOGICAL_EXPRESSION:
            return append_matching_imported_member_locations_recursive(state,
                                                                      node->data.logicalExpression.left,
                                                                      bindings,
                                                                      moduleName,
                                                                      memberName,
                                                                      result) &&
                   append_matching_imported_member_locations_recursive(state,
                                                                      node->data.logicalExpression.right,
                                                                      bindings,
                                                                      moduleName,
                                                                      memberName,
                                                                      result);

        case ZR_AST_CONDITIONAL_EXPRESSION:
            return append_matching_imported_member_locations_recursive(state,
                                                                      node->data.conditionalExpression.test,
                                                                      bindings,
                                                                      moduleName,
                                                                      memberName,
                                                                      result) &&
                   append_matching_imported_member_locations_recursive(state,
                                                                      node->data.conditionalExpression.consequent,
                                                                      bindings,
                                                                      moduleName,
                                                                      memberName,
                                                                      result) &&
                   append_matching_imported_member_locations_recursive(state,
                                                                      node->data.conditionalExpression.alternate,
                                                                      bindings,
                                                                      moduleName,
                                                                      memberName,
                                                                      result);

        case ZR_AST_UNARY_EXPRESSION:
            return append_matching_imported_member_locations_recursive(state,
                                                                      node->data.unaryExpression.argument,
                                                                      bindings,
                                                                      moduleName,
                                                                      memberName,
                                                                      result);

        case ZR_AST_TYPE_CAST_EXPRESSION:
            return append_matching_imported_member_locations_recursive(state,
                                                                      node->data.typeCastExpression.expression,
                                                                      bindings,
                                                                      moduleName,
                                                                      memberName,
                                                                      result);

        case ZR_AST_LAMBDA_EXPRESSION:
            return append_matching_imported_member_locations_recursive(state,
                                                                      node->data.lambdaExpression.block,
                                                                      bindings,
                                                                      moduleName,
                                                                      memberName,
                                                                      result);

        case ZR_AST_IF_EXPRESSION:
            return append_matching_imported_member_locations_recursive(state,
                                                                      node->data.ifExpression.condition,
                                                                      bindings,
                                                                      moduleName,
                                                                      memberName,
                                                                      result) &&
                   append_matching_imported_member_locations_recursive(state,
                                                                      node->data.ifExpression.thenExpr,
                                                                      bindings,
                                                                      moduleName,
                                                                      memberName,
                                                                      result) &&
                   append_matching_imported_member_locations_recursive(state,
                                                                      node->data.ifExpression.elseExpr,
                                                                      bindings,
                                                                      moduleName,
                                                                      memberName,
                                                                      result);

        case ZR_AST_SWITCH_EXPRESSION:
            return append_matching_imported_member_locations_recursive(state,
                                                                      node->data.switchExpression.expr,
                                                                      bindings,
                                                                      moduleName,
                                                                      memberName,
                                                                      result) &&
                   append_matching_imported_member_locations_in_node_array(state,
                                                                           node->data.switchExpression.cases,
                                                                           bindings,
                                                                           moduleName,
                                                                           memberName,
                                                                           result) &&
                   append_matching_imported_member_locations_recursive(state,
                                                                      node->data.switchExpression.defaultCase,
                                                                      bindings,
                                                                      moduleName,
                                                                      memberName,
                                                                      result);

        case ZR_AST_SWITCH_CASE:
            return append_matching_imported_member_locations_recursive(state,
                                                                      node->data.switchCase.value,
                                                                      bindings,
                                                                      moduleName,
                                                                      memberName,
                                                                      result) &&
                   append_matching_imported_member_locations_recursive(state,
                                                                      node->data.switchCase.block,
                                                                      bindings,
                                                                      moduleName,
                                                                      memberName,
                                                                      result);

        case ZR_AST_SWITCH_DEFAULT:
            return append_matching_imported_member_locations_recursive(state,
                                                                      node->data.switchDefault.block,
                                                                      bindings,
                                                                      moduleName,
                                                                      memberName,
                                                                      result);

        case ZR_AST_FUNCTION_CALL:
            return append_matching_imported_member_locations_in_node_array(state,
                                                                           node->data.functionCall.args,
                                                                           bindings,
                                                                           moduleName,
                                                                           memberName,
                                                                           result);

        case ZR_AST_PRIMARY_EXPRESSION:
            return append_matching_imported_member_locations_recursive(state,
                                                                      node->data.primaryExpression.property,
                                                                      bindings,
                                                                      moduleName,
                                                                      memberName,
                                                                      result) &&
                   append_matching_imported_member_locations_in_node_array(state,
                                                                           node->data.primaryExpression.members,
                                                                           bindings,
                                                                           moduleName,
                                                                           memberName,
                                                                           result);

        case ZR_AST_MEMBER_EXPRESSION:
            return !node->data.memberExpression.computed ||
                   append_matching_imported_member_locations_recursive(state,
                                                                      node->data.memberExpression.property,
                                                                      bindings,
                                                                      moduleName,
                                                                      memberName,
                                                                      result);

        case ZR_AST_ARRAY_LITERAL:
            return append_matching_imported_member_locations_in_node_array(state,
                                                                           node->data.arrayLiteral.elements,
                                                                           bindings,
                                                                           moduleName,
                                                                           memberName,
                                                                           result);

        case ZR_AST_OBJECT_LITERAL:
            return append_matching_imported_member_locations_in_node_array(state,
                                                                           node->data.objectLiteral.properties,
                                                                           bindings,
                                                                           moduleName,
                                                                           memberName,
                                                                           result);

        case ZR_AST_KEY_VALUE_PAIR:
            return append_matching_imported_member_locations_recursive(state,
                                                                      node->data.keyValuePair.key,
                                                                      bindings,
                                                                      moduleName,
                                                                      memberName,
                                                                      result) &&
                   append_matching_imported_member_locations_recursive(state,
                                                                      node->data.keyValuePair.value,
                                                                      bindings,
                                                                      moduleName,
                                                                      memberName,
                                                                      result);

        case ZR_AST_WHILE_LOOP:
            return append_matching_imported_member_locations_recursive(state,
                                                                      node->data.whileLoop.cond,
                                                                      bindings,
                                                                      moduleName,
                                                                      memberName,
                                                                      result) &&
                   append_matching_imported_member_locations_recursive(state,
                                                                      node->data.whileLoop.block,
                                                                      bindings,
                                                                      moduleName,
                                                                      memberName,
                                                                      result);

        case ZR_AST_FOR_LOOP:
            return append_matching_imported_member_locations_recursive(state,
                                                                      node->data.forLoop.init,
                                                                      bindings,
                                                                      moduleName,
                                                                      memberName,
                                                                      result) &&
                   append_matching_imported_member_locations_recursive(state,
                                                                      node->data.forLoop.cond,
                                                                      bindings,
                                                                      moduleName,
                                                                      memberName,
                                                                      result) &&
                   append_matching_imported_member_locations_recursive(state,
                                                                      node->data.forLoop.step,
                                                                      bindings,
                                                                      moduleName,
                                                                      memberName,
                                                                      result) &&
                   append_matching_imported_member_locations_recursive(state,
                                                                      node->data.forLoop.block,
                                                                      bindings,
                                                                      moduleName,
                                                                      memberName,
                                                                      result);

        case ZR_AST_FOREACH_LOOP:
            return append_matching_imported_member_locations_recursive(state,
                                                                      node->data.foreachLoop.expr,
                                                                      bindings,
                                                                      moduleName,
                                                                      memberName,
                                                                      result) &&
                   append_matching_imported_member_locations_recursive(state,
                                                                      node->data.foreachLoop.block,
                                                                      bindings,
                                                                      moduleName,
                                                                      memberName,
                                                                      result);

        default:
            break;
    }

    return ZR_TRUE;
}

TZrBool ZrLanguageServer_LspProject_AppendMatchingImportedMemberLocations(SZrState *state,
                                                                          SZrAstNode *node,
                                                                          SZrArray *bindings,
                                                                          SZrString *moduleName,
                                                                          SZrString *memberName,
                                                                          SZrArray *result) {
    return append_matching_imported_member_locations_recursive(state,
                                                               node,
                                                               bindings,
                                                               moduleName,
                                                               memberName,
                                                               result);
}
