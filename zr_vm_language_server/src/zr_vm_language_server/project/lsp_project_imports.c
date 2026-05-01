#include "project/lsp_project_internal.h"

#include "zr_vm_core/memory.h"
#include "zr_vm_core/string.h"
#include "zr_vm_library/project.h"

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

static TZrBool file_range_contains_position(SZrFileRange range, SZrFileRange position);

static SZrFileRange normalize_import_module_path_location(SZrFileRange range, TZrSize moduleNameLength) {
    TZrInt32 spanColumns;
    TZrInt64 spanOffsets;

    if (moduleNameLength == 0) {
        return range;
    }

    spanColumns = range.end.line == range.start.line ? range.end.column - range.start.column : 0;
    spanOffsets = range.end.offset > range.start.offset ? (TZrInt64)(range.end.offset - range.start.offset) : 0;
    if (spanColumns > 1 || spanOffsets > 1) {
        return range;
    }

    if (range.end.column > (TZrInt32)(moduleNameLength + 2)) {
        range.start.column = range.end.column - (TZrInt32)moduleNameLength - 2;
        if (range.start.column < 1) {
            range.start.column = 1;
        }
        range.end.column = range.start.column + (TZrInt32)moduleNameLength;
    }

    if (range.end.offset > moduleNameLength + 3) {
        range.start.offset = range.end.offset - moduleNameLength - 3;
        range.end.offset = range.start.offset + moduleNameLength;
    }

    return range;
}

static SZrFileRange normalize_zero_width_identifier_location(SZrFileRange range, SZrString *identifierName) {
    TZrNativeString text = ZR_NULL;
    TZrSize length = 0;

    get_string_view(identifierName, &text, &length);
    if (text == ZR_NULL || length == 0 ||
        range.start.offset != range.end.offset ||
        range.start.line != range.end.line ||
        range.start.column != range.end.column) {
        return range;
    }

    if (range.start.offset >= length) {
        range.start.offset -= length;
    }
    if (range.start.column > (TZrInt32)length) {
        range.start.column -= (TZrInt32)length;
    }

    return range;
}

static TZrBool normalize_module_key(const TZrChar *modulePath, TZrChar *buffer, TZrSize bufferSize) {
    return ZrLibrary_Project_NormalizeModuleKey(modulePath, buffer, bufferSize);
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
                                     const TZrChar *moduleNameText,
                                     SZrFileRange aliasLocation,
                                     SZrFileRange modulePathLocation) {
    SZrLspImportBinding *binding;
    SZrString *moduleName;

    if (state == ZR_NULL || bindings == ZR_NULL || moduleNameText == ZR_NULL) {
        return ZR_FALSE;
    }

    if (aliasName != ZR_NULL && aliasLocation.source == ZR_NULL) {
        aliasLocation.source = modulePathLocation.source;
    }
    if (modulePathLocation.source == ZR_NULL && aliasName != ZR_NULL) {
        modulePathLocation.source = aliasLocation.source;
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
    binding->aliasLocation = aliasLocation;
    binding->modulePathLocation = modulePathLocation;
    ZrCore_Array_Push(state, bindings, &binding);
    return ZR_TRUE;
}

static TZrBool import_expression_get_module_path_literal(SZrAstNode *node, SZrAstNode **outLiteralNode) {
    if (outLiteralNode != ZR_NULL) {
        *outLiteralNode = ZR_NULL;
    }
    if (node == ZR_NULL ||
        node->type != ZR_AST_IMPORT_EXPRESSION ||
        node->data.importExpression.modulePath == ZR_NULL ||
        node->data.importExpression.modulePath->type != ZR_AST_STRING_LITERAL ||
        node->data.importExpression.modulePath->data.stringLiteral.value == ZR_NULL) {
        return ZR_FALSE;
    }

    if (outLiteralNode != ZR_NULL) {
        *outLiteralNode = node->data.importExpression.modulePath;
    }
    return ZR_TRUE;
}

static TZrBool expression_get_direct_import_module_path_literal(SZrAstNode *node, SZrAstNode **outLiteralNode) {
    if (outLiteralNode != ZR_NULL) {
        *outLiteralNode = ZR_NULL;
    }
    if (node == ZR_NULL) {
        return ZR_FALSE;
    }

    if (import_expression_get_module_path_literal(node, outLiteralNode)) {
        return ZR_TRUE;
    }

    if (node->type == ZR_AST_PRIMARY_EXPRESSION && node->data.primaryExpression.property != ZR_NULL) {
        return import_expression_get_module_path_literal(node->data.primaryExpression.property, outLiteralNode);
    }

    return ZR_FALSE;
}

static TZrBool append_import_binding_from_literal(SZrState *state,
                                                  SZrArray *bindings,
                                                  SZrString *aliasName,
                                                  SZrFileRange aliasLocation,
                                                  SZrAstNode *modulePathLiteral,
                                                  SZrFileRange fallbackLocation) {
    TZrChar rawModule[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrChar normalizedModule[ZR_LIBRARY_MAX_PATH_LENGTH];
    TZrNativeString moduleText;
    TZrSize moduleLength;
    SZrFileRange modulePathLocation;

    if (state == ZR_NULL || bindings == ZR_NULL || modulePathLiteral == ZR_NULL ||
        modulePathLiteral->type != ZR_AST_STRING_LITERAL ||
        modulePathLiteral->data.stringLiteral.value == ZR_NULL) {
        return ZR_FALSE;
    }

    get_string_view(modulePathLiteral->data.stringLiteral.value, &moduleText, &moduleLength);
    if (moduleText == ZR_NULL || moduleLength == 0 || moduleLength >= sizeof(rawModule)) {
        return ZR_FALSE;
    }

    memcpy(rawModule, moduleText, moduleLength);
    rawModule[moduleLength] = '\0';
    if (!normalize_module_key(rawModule, normalizedModule, sizeof(normalizedModule))) {
        return ZR_FALSE;
    }

    modulePathLocation =
        normalize_import_module_path_location(modulePathLiteral->location, moduleLength);
    if (modulePathLocation.source == ZR_NULL) {
        modulePathLocation.source = fallbackLocation.source;
    }
    if (aliasName != ZR_NULL && aliasLocation.source == ZR_NULL) {
        aliasLocation.source = fallbackLocation.source;
    }

    return append_import_binding(state,
                                 bindings,
                                 aliasName,
                                 normalizedModule,
                                 aliasLocation,
                                 modulePathLocation);
}

static void collect_import_bindings_in_node_array(SZrState *state, SZrAstNodeArray *nodes, SZrArray *bindings);

void ZrLanguageServer_LspProject_CollectImportBindings(SZrState *state, SZrAstNode *node, SZrArray *bindings) {
    SZrAstNode *modulePathLiteral = ZR_NULL;

    if (state == ZR_NULL || node == ZR_NULL || bindings == ZR_NULL) {
        return;
    }

    switch (node->type) {
        case ZR_AST_SCRIPT:
            collect_import_bindings_in_node_array(state, node->data.script.statements, bindings);
            break;

        case ZR_AST_STRUCT_DECLARATION:
            collect_import_bindings_in_node_array(state, node->data.structDeclaration.members, bindings);
            break;

        case ZR_AST_CLASS_DECLARATION:
            collect_import_bindings_in_node_array(state, node->data.classDeclaration.members, bindings);
            break;

        case ZR_AST_ENUM_DECLARATION:
            collect_import_bindings_in_node_array(state, node->data.enumDeclaration.members, bindings);
            break;

        case ZR_AST_INTERFACE_DECLARATION:
            collect_import_bindings_in_node_array(state, node->data.interfaceDeclaration.members, bindings);
            break;

        case ZR_AST_BLOCK:
            collect_import_bindings_in_node_array(state, node->data.block.body, bindings);
            break;

        case ZR_AST_VARIABLE_DECLARATION:
            if (expression_get_direct_import_module_path_literal(node->data.variableDeclaration.value,
                                                                 &modulePathLiteral)) {
                if (node->data.variableDeclaration.pattern != ZR_NULL &&
                    node->data.variableDeclaration.pattern->type == ZR_AST_IDENTIFIER_LITERAL &&
                    node->data.variableDeclaration.pattern->data.identifier.name != ZR_NULL) {
                    SZrFileRange aliasLocation = node->data.variableDeclaration.pattern->location;
                    if (aliasLocation.source == ZR_NULL) {
                        aliasLocation.source = node->location.source;
                    }
                    append_import_binding_from_literal(state,
                                                       bindings,
                                                       node->data.variableDeclaration.pattern->data.identifier.name,
                                                       aliasLocation,
                                                       modulePathLiteral,
                                                       node->location);
                } else {
                    append_import_binding_from_literal(state,
                                                       bindings,
                                                       ZR_NULL,
                                                       ZrParser_FileRange_Create(ZrParser_FilePosition_Create(0, 0, 0),
                                                                                 ZrParser_FilePosition_Create(0, 0, 0),
                                                                                 ZR_NULL),
                                                       modulePathLiteral,
                                                       node->location);
                }
                break;
            }
            ZrLanguageServer_LspProject_CollectImportBindings(state, node->data.variableDeclaration.value, bindings);
            break;

        case ZR_AST_FUNCTION_DECLARATION:
            ZrLanguageServer_LspProject_CollectImportBindings(state, node->data.functionDeclaration.body, bindings);
            break;

        case ZR_AST_TEST_DECLARATION:
            ZrLanguageServer_LspProject_CollectImportBindings(state, node->data.testDeclaration.body, bindings);
            break;

        case ZR_AST_COMPILE_TIME_DECLARATION:
            ZrLanguageServer_LspProject_CollectImportBindings(state,
                                                              node->data.compileTimeDeclaration.declaration,
                                                              bindings);
            break;

        case ZR_AST_EXTERN_BLOCK:
            collect_import_bindings_in_node_array(state, node->data.externBlock.declarations, bindings);
            break;

        case ZR_AST_STRUCT_FIELD:
            ZrLanguageServer_LspProject_CollectImportBindings(state, node->data.structField.init, bindings);
            break;

        case ZR_AST_STRUCT_METHOD:
            ZrLanguageServer_LspProject_CollectImportBindings(state, node->data.structMethod.body, bindings);
            break;

        case ZR_AST_STRUCT_META_FUNCTION:
            ZrLanguageServer_LspProject_CollectImportBindings(state, node->data.structMetaFunction.body, bindings);
            break;

        case ZR_AST_ENUM_MEMBER:
            ZrLanguageServer_LspProject_CollectImportBindings(state, node->data.enumMember.value, bindings);
            break;

        case ZR_AST_CLASS_FIELD:
            ZrLanguageServer_LspProject_CollectImportBindings(state, node->data.classField.init, bindings);
            break;

        case ZR_AST_CLASS_METHOD:
            ZrLanguageServer_LspProject_CollectImportBindings(state, node->data.classMethod.body, bindings);
            break;

        case ZR_AST_CLASS_PROPERTY:
            ZrLanguageServer_LspProject_CollectImportBindings(state, node->data.classProperty.modifier, bindings);
            break;

        case ZR_AST_CLASS_META_FUNCTION:
            collect_import_bindings_in_node_array(state, node->data.classMetaFunction.superArgs, bindings);
            ZrLanguageServer_LspProject_CollectImportBindings(state, node->data.classMetaFunction.body, bindings);
            break;

        case ZR_AST_PROPERTY_GET:
            ZrLanguageServer_LspProject_CollectImportBindings(state, node->data.propertyGet.body, bindings);
            break;

        case ZR_AST_PROPERTY_SET:
            ZrLanguageServer_LspProject_CollectImportBindings(state, node->data.propertySet.body, bindings);
            break;

        case ZR_AST_EXPRESSION_STATEMENT:
            ZrLanguageServer_LspProject_CollectImportBindings(state, node->data.expressionStatement.expr, bindings);
            break;

        case ZR_AST_USING_STATEMENT:
            ZrLanguageServer_LspProject_CollectImportBindings(state, node->data.usingStatement.resource, bindings);
            ZrLanguageServer_LspProject_CollectImportBindings(state, node->data.usingStatement.body, bindings);
            break;

        case ZR_AST_RETURN_STATEMENT:
            ZrLanguageServer_LspProject_CollectImportBindings(state, node->data.returnStatement.expr, bindings);
            break;

        case ZR_AST_BREAK_CONTINUE_STATEMENT:
            ZrLanguageServer_LspProject_CollectImportBindings(state,
                                                              node->data.breakContinueStatement.expr,
                                                              bindings);
            break;

        case ZR_AST_THROW_STATEMENT:
            ZrLanguageServer_LspProject_CollectImportBindings(state, node->data.throwStatement.expr, bindings);
            break;

        case ZR_AST_OUT_STATEMENT:
            ZrLanguageServer_LspProject_CollectImportBindings(state, node->data.outStatement.expr, bindings);
            break;

        case ZR_AST_CATCH_CLAUSE:
            ZrLanguageServer_LspProject_CollectImportBindings(state, node->data.catchClause.block, bindings);
            break;

        case ZR_AST_TRY_CATCH_FINALLY_STATEMENT:
            ZrLanguageServer_LspProject_CollectImportBindings(state,
                                                              node->data.tryCatchFinallyStatement.block,
                                                              bindings);
            collect_import_bindings_in_node_array(state, node->data.tryCatchFinallyStatement.catchClauses, bindings);
            ZrLanguageServer_LspProject_CollectImportBindings(state,
                                                              node->data.tryCatchFinallyStatement.finallyBlock,
                                                              bindings);
            break;

        case ZR_AST_ASSIGNMENT_EXPRESSION:
            ZrLanguageServer_LspProject_CollectImportBindings(state, node->data.assignmentExpression.left, bindings);
            ZrLanguageServer_LspProject_CollectImportBindings(state, node->data.assignmentExpression.right, bindings);
            break;

        case ZR_AST_BINARY_EXPRESSION:
            ZrLanguageServer_LspProject_CollectImportBindings(state, node->data.binaryExpression.left, bindings);
            ZrLanguageServer_LspProject_CollectImportBindings(state, node->data.binaryExpression.right, bindings);
            break;

        case ZR_AST_LOGICAL_EXPRESSION:
            ZrLanguageServer_LspProject_CollectImportBindings(state, node->data.logicalExpression.left, bindings);
            ZrLanguageServer_LspProject_CollectImportBindings(state, node->data.logicalExpression.right, bindings);
            break;

        case ZR_AST_CONDITIONAL_EXPRESSION:
            ZrLanguageServer_LspProject_CollectImportBindings(state, node->data.conditionalExpression.test, bindings);
            ZrLanguageServer_LspProject_CollectImportBindings(state,
                                                              node->data.conditionalExpression.consequent,
                                                              bindings);
            ZrLanguageServer_LspProject_CollectImportBindings(state,
                                                              node->data.conditionalExpression.alternate,
                                                              bindings);
            break;

        case ZR_AST_UNARY_EXPRESSION:
            ZrLanguageServer_LspProject_CollectImportBindings(state, node->data.unaryExpression.argument, bindings);
            break;

        case ZR_AST_TYPE_CAST_EXPRESSION:
            ZrLanguageServer_LspProject_CollectImportBindings(state,
                                                              node->data.typeCastExpression.expression,
                                                              bindings);
            break;

        case ZR_AST_LAMBDA_EXPRESSION:
            ZrLanguageServer_LspProject_CollectImportBindings(state, node->data.lambdaExpression.block, bindings);
            break;

        case ZR_AST_IF_EXPRESSION:
            ZrLanguageServer_LspProject_CollectImportBindings(state, node->data.ifExpression.condition, bindings);
            ZrLanguageServer_LspProject_CollectImportBindings(state, node->data.ifExpression.thenExpr, bindings);
            ZrLanguageServer_LspProject_CollectImportBindings(state, node->data.ifExpression.elseExpr, bindings);
            break;

        case ZR_AST_SWITCH_EXPRESSION:
            ZrLanguageServer_LspProject_CollectImportBindings(state, node->data.switchExpression.expr, bindings);
            collect_import_bindings_in_node_array(state, node->data.switchExpression.cases, bindings);
            ZrLanguageServer_LspProject_CollectImportBindings(state,
                                                              node->data.switchExpression.defaultCase,
                                                              bindings);
            break;

        case ZR_AST_SWITCH_CASE:
            ZrLanguageServer_LspProject_CollectImportBindings(state, node->data.switchCase.value, bindings);
            ZrLanguageServer_LspProject_CollectImportBindings(state, node->data.switchCase.block, bindings);
            break;

        case ZR_AST_SWITCH_DEFAULT:
            ZrLanguageServer_LspProject_CollectImportBindings(state, node->data.switchDefault.block, bindings);
            break;

        case ZR_AST_FUNCTION_CALL:
            collect_import_bindings_in_node_array(state, node->data.functionCall.args, bindings);
            break;

        case ZR_AST_MEMBER_EXPRESSION:
            if (node->data.memberExpression.computed) {
                ZrLanguageServer_LspProject_CollectImportBindings(state,
                                                                  node->data.memberExpression.property,
                                                                  bindings);
            }
            break;

        case ZR_AST_PRIMARY_EXPRESSION:
            if (expression_get_direct_import_module_path_literal(node, &modulePathLiteral)) {
                append_import_binding_from_literal(state,
                                                   bindings,
                                                   ZR_NULL,
                                                   ZrParser_FileRange_Create(ZrParser_FilePosition_Create(0, 0, 0),
                                                                             ZrParser_FilePosition_Create(0, 0, 0),
                                                                             ZR_NULL),
                                                   modulePathLiteral,
                                                   node->location);
                collect_import_bindings_in_node_array(state, node->data.primaryExpression.members, bindings);
            } else {
                ZrLanguageServer_LspProject_CollectImportBindings(state,
                                                                  node->data.primaryExpression.property,
                                                                  bindings);
                collect_import_bindings_in_node_array(state, node->data.primaryExpression.members, bindings);
            }
            break;

        case ZR_AST_IMPORT_EXPRESSION:
            if (import_expression_get_module_path_literal(node, &modulePathLiteral)) {
                append_import_binding_from_literal(state,
                                                   bindings,
                                                   ZR_NULL,
                                                   ZrParser_FileRange_Create(ZrParser_FilePosition_Create(0, 0, 0),
                                                                             ZrParser_FilePosition_Create(0, 0, 0),
                                                                             ZR_NULL),
                                                   modulePathLiteral,
                                                   node->location);
            }
            break;

        case ZR_AST_PROTOTYPE_REFERENCE_EXPRESSION:
            ZrLanguageServer_LspProject_CollectImportBindings(state,
                                                              node->data.prototypeReferenceExpression.target,
                                                              bindings);
            break;

        case ZR_AST_CONSTRUCT_EXPRESSION:
            ZrLanguageServer_LspProject_CollectImportBindings(state, node->data.constructExpression.target, bindings);
            collect_import_bindings_in_node_array(state, node->data.constructExpression.args, bindings);
            break;

        case ZR_AST_ARRAY_LITERAL:
            collect_import_bindings_in_node_array(state, node->data.arrayLiteral.elements, bindings);
            break;

        case ZR_AST_OBJECT_LITERAL:
            collect_import_bindings_in_node_array(state, node->data.objectLiteral.properties, bindings);
            break;

        case ZR_AST_KEY_VALUE_PAIR:
            ZrLanguageServer_LspProject_CollectImportBindings(state, node->data.keyValuePair.key, bindings);
            ZrLanguageServer_LspProject_CollectImportBindings(state, node->data.keyValuePair.value, bindings);
            break;

        case ZR_AST_UNPACK_LITERAL:
            ZrLanguageServer_LspProject_CollectImportBindings(state, node->data.unpackLiteral.element, bindings);
            break;

        case ZR_AST_GENERATOR_EXPRESSION:
            ZrLanguageServer_LspProject_CollectImportBindings(state, node->data.generatorExpression.block, bindings);
            break;

        case ZR_AST_WHILE_LOOP:
            ZrLanguageServer_LspProject_CollectImportBindings(state, node->data.whileLoop.cond, bindings);
            ZrLanguageServer_LspProject_CollectImportBindings(state, node->data.whileLoop.block, bindings);
            break;

        case ZR_AST_FOR_LOOP:
            ZrLanguageServer_LspProject_CollectImportBindings(state, node->data.forLoop.init, bindings);
            ZrLanguageServer_LspProject_CollectImportBindings(state, node->data.forLoop.cond, bindings);
            ZrLanguageServer_LspProject_CollectImportBindings(state, node->data.forLoop.step, bindings);
            ZrLanguageServer_LspProject_CollectImportBindings(state, node->data.forLoop.block, bindings);
            break;

        case ZR_AST_FOREACH_LOOP:
            ZrLanguageServer_LspProject_CollectImportBindings(state, node->data.foreachLoop.expr, bindings);
            ZrLanguageServer_LspProject_CollectImportBindings(state, node->data.foreachLoop.block, bindings);
            break;

        default:
            break;
    }
}

static void collect_import_bindings_in_node_array(SZrState *state, SZrAstNodeArray *nodes, SZrArray *bindings) {
    if (nodes == ZR_NULL || nodes->nodes == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < nodes->count; index++) {
        ZrLanguageServer_LspProject_CollectImportBindings(state, nodes->nodes[index], bindings);
    }
}

SZrLspImportBinding *ZrLanguageServer_LspProject_FindImportBindingByAlias(SZrArray *bindings, SZrString *aliasName) {
    for (TZrSize index = 0; bindings != ZR_NULL && index < bindings->length; index++) {
        SZrLspImportBinding **bindingPtr =
            (SZrLspImportBinding **)ZrCore_Array_Get(bindings, index);
        if (bindingPtr != ZR_NULL && *bindingPtr != ZR_NULL && (*bindingPtr)->aliasName != ZR_NULL &&
            ZrLanguageServer_Lsp_StringsEqual((*bindingPtr)->aliasName, aliasName)) {
            return *bindingPtr;
        }
    }

    return ZR_NULL;
}

static TZrBool variable_declaration_get_import_binding_hit(SZrAstNode *node,
                                                           SZrArray *bindings,
                                                           SZrFileRange position,
                                                           SZrLspImportBinding **outBinding,
                                                           SZrFileRange *outLocation) {
    SZrVariableDeclaration *varDecl;
    SZrLspImportBinding *binding;

    if (node == ZR_NULL || node->type != ZR_AST_VARIABLE_DECLARATION || bindings == ZR_NULL ||
        outBinding == ZR_NULL || outLocation == ZR_NULL) {
        return ZR_FALSE;
    }

    varDecl = &node->data.variableDeclaration;
    if (varDecl->pattern == ZR_NULL ||
        varDecl->pattern->type != ZR_AST_IDENTIFIER_LITERAL ||
        !expression_get_direct_import_module_path_literal(varDecl->value, ZR_NULL)) {
        return ZR_FALSE;
    }

    binding = ZrLanguageServer_LspProject_FindImportBindingByAlias(bindings,
                                                                   varDecl->pattern->data.identifier.name);
    if (binding == ZR_NULL) {
        return ZR_FALSE;
    }

    if (file_range_contains_position(binding->modulePathLocation, position)) {
        *outBinding = binding;
        *outLocation = binding->modulePathLocation;
        return ZR_TRUE;
    }

    if (!file_range_contains_position(varDecl->pattern->location, position)) {
        return ZR_FALSE;
    }

    *outBinding = binding;
    *outLocation = varDecl->pattern->location;
    return ZR_TRUE;
}

static TZrBool file_range_contains_position(SZrFileRange range, SZrFileRange position) {
    if (!ZrLanguageServer_Lsp_StringsEqual(range.source, position.source) &&
        range.source != ZR_NULL && position.source != ZR_NULL) {
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

    binding = ZrLanguageServer_LspProject_FindImportBindingByAlias(bindings, receiverNode->data.identifier.name);
    if (binding == ZR_NULL) {
        return ZR_FALSE;
    }

    outHit->moduleName = binding->moduleName;
    outHit->memberName = memberNode->data.memberExpression.property->data.identifier.name;
    outHit->receiverLocation =
        normalize_zero_width_identifier_location(receiverNode->location, receiverNode->data.identifier.name);
    outHit->location = memberNode->data.memberExpression.property->location;
    return ZR_TRUE;
}

static TZrBool primary_expression_get_import_binding_hit(SZrAstNode *node,
                                                         SZrArray *bindings,
                                                         SZrFileRange position,
                                                         SZrLspImportBinding **outBinding,
                                                         SZrFileRange *outLocation) {
    SZrAstNode *receiverNode;
    SZrLspImportBinding *binding;

    if (node == ZR_NULL || node->type != ZR_AST_PRIMARY_EXPRESSION || bindings == ZR_NULL ||
        outBinding == ZR_NULL || outLocation == ZR_NULL) {
        return ZR_FALSE;
    }

    receiverNode = node->data.primaryExpression.property;
    if (receiverNode == ZR_NULL || receiverNode->type != ZR_AST_IDENTIFIER_LITERAL) {
        return ZR_FALSE;
    }

    binding = ZrLanguageServer_LspProject_FindImportBindingByAlias(bindings,
                                                                   receiverNode->data.identifier.name);
    if (binding == ZR_NULL || !file_range_contains_position(receiverNode->location, position)) {
        return ZR_FALSE;
    }

    *outBinding = binding;
    *outLocation = normalize_zero_width_identifier_location(receiverNode->location,
                                                            receiverNode->data.identifier.name);
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

static TZrBool find_import_binding_hit_recursive(SZrAstNode *node,
                                                 SZrArray *bindings,
                                                 SZrFileRange position,
                                                 SZrLspImportBinding **outBinding,
                                                 SZrFileRange *outLocation);

static TZrBool find_import_binding_hit_in_node_array(SZrAstNodeArray *nodes,
                                                     SZrArray *bindings,
                                                     SZrFileRange position,
                                                     SZrLspImportBinding **outBinding,
                                                     SZrFileRange *outLocation) {
    if (nodes == ZR_NULL || nodes->nodes == ZR_NULL || bindings == ZR_NULL ||
        outBinding == ZR_NULL || outLocation == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < nodes->count; index++) {
        if (find_import_binding_hit_recursive(nodes->nodes[index],
                                              bindings,
                                              position,
                                              outBinding,
                                              outLocation)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool find_import_binding_hit_recursive(SZrAstNode *node,
                                                 SZrArray *bindings,
                                                 SZrFileRange position,
                                                 SZrLspImportBinding **outBinding,
                                                 SZrFileRange *outLocation) {
    if (node == ZR_NULL || bindings == ZR_NULL || outBinding == ZR_NULL || outLocation == ZR_NULL) {
        return ZR_FALSE;
    }

    if (variable_declaration_get_import_binding_hit(node, bindings, position, outBinding, outLocation) ||
        primary_expression_get_import_binding_hit(node, bindings, position, outBinding, outLocation)) {
        return ZR_TRUE;
    }

    switch (node->type) {
        case ZR_AST_SCRIPT:
            return find_import_binding_hit_in_node_array(node->data.script.statements,
                                                         bindings,
                                                         position,
                                                         outBinding,
                                                         outLocation);

        case ZR_AST_BLOCK:
            return find_import_binding_hit_in_node_array(node->data.block.body,
                                                         bindings,
                                                         position,
                                                         outBinding,
                                                         outLocation);

        case ZR_AST_FUNCTION_DECLARATION:
            return find_import_binding_hit_recursive(node->data.functionDeclaration.body,
                                                    bindings,
                                                    position,
                                                    outBinding,
                                                    outLocation);

        case ZR_AST_TEST_DECLARATION:
            return find_import_binding_hit_recursive(node->data.testDeclaration.body,
                                                    bindings,
                                                    position,
                                                    outBinding,
                                                    outLocation);

        case ZR_AST_STRUCT_METHOD:
            return find_import_binding_hit_recursive(node->data.structMethod.body,
                                                    bindings,
                                                    position,
                                                    outBinding,
                                                    outLocation);

        case ZR_AST_STRUCT_META_FUNCTION:
            return find_import_binding_hit_recursive(node->data.structMetaFunction.body,
                                                    bindings,
                                                    position,
                                                    outBinding,
                                                    outLocation);

        case ZR_AST_CLASS_METHOD:
            return find_import_binding_hit_recursive(node->data.classMethod.body,
                                                    bindings,
                                                    position,
                                                    outBinding,
                                                    outLocation);

        case ZR_AST_CLASS_META_FUNCTION:
            return find_import_binding_hit_recursive(node->data.classMetaFunction.body,
                                                    bindings,
                                                    position,
                                                    outBinding,
                                                    outLocation);

        case ZR_AST_PROPERTY_GET:
            return find_import_binding_hit_recursive(node->data.propertyGet.body,
                                                    bindings,
                                                    position,
                                                    outBinding,
                                                    outLocation);

        case ZR_AST_PROPERTY_SET:
            return find_import_binding_hit_recursive(node->data.propertySet.body,
                                                    bindings,
                                                    position,
                                                    outBinding,
                                                    outLocation);

        case ZR_AST_CLASS_PROPERTY:
            return find_import_binding_hit_recursive(node->data.classProperty.modifier,
                                                    bindings,
                                                    position,
                                                    outBinding,
                                                    outLocation);

        case ZR_AST_VARIABLE_DECLARATION:
            return find_import_binding_hit_recursive(node->data.variableDeclaration.value,
                                                    bindings,
                                                    position,
                                                    outBinding,
                                                    outLocation);

        case ZR_AST_STRUCT_FIELD:
            return find_import_binding_hit_recursive(node->data.structField.init,
                                                    bindings,
                                                    position,
                                                    outBinding,
                                                    outLocation);

        case ZR_AST_CLASS_FIELD:
            return find_import_binding_hit_recursive(node->data.classField.init,
                                                    bindings,
                                                    position,
                                                    outBinding,
                                                    outLocation);

        case ZR_AST_ENUM_MEMBER:
            return find_import_binding_hit_recursive(node->data.enumMember.value,
                                                    bindings,
                                                    position,
                                                    outBinding,
                                                    outLocation);

        case ZR_AST_RETURN_STATEMENT:
            return find_import_binding_hit_recursive(node->data.returnStatement.expr,
                                                    bindings,
                                                    position,
                                                    outBinding,
                                                    outLocation);

        case ZR_AST_EXPRESSION_STATEMENT:
            return find_import_binding_hit_recursive(node->data.expressionStatement.expr,
                                                    bindings,
                                                    position,
                                                    outBinding,
                                                    outLocation);

        case ZR_AST_USING_STATEMENT:
            return find_import_binding_hit_recursive(node->data.usingStatement.resource,
                                                    bindings,
                                                    position,
                                                    outBinding,
                                                    outLocation) ||
                   find_import_binding_hit_recursive(node->data.usingStatement.body,
                                                    bindings,
                                                    position,
                                                    outBinding,
                                                    outLocation);

        case ZR_AST_BREAK_CONTINUE_STATEMENT:
            return find_import_binding_hit_recursive(node->data.breakContinueStatement.expr,
                                                    bindings,
                                                    position,
                                                    outBinding,
                                                    outLocation);

        case ZR_AST_THROW_STATEMENT:
            return find_import_binding_hit_recursive(node->data.throwStatement.expr,
                                                    bindings,
                                                    position,
                                                    outBinding,
                                                    outLocation);

        case ZR_AST_OUT_STATEMENT:
            return find_import_binding_hit_recursive(node->data.outStatement.expr,
                                                    bindings,
                                                    position,
                                                    outBinding,
                                                    outLocation);

        case ZR_AST_TRY_CATCH_FINALLY_STATEMENT:
            return find_import_binding_hit_recursive(node->data.tryCatchFinallyStatement.block,
                                                    bindings,
                                                    position,
                                                    outBinding,
                                                    outLocation) ||
                   find_import_binding_hit_in_node_array(node->data.tryCatchFinallyStatement.catchClauses,
                                                        bindings,
                                                        position,
                                                        outBinding,
                                                        outLocation) ||
                   find_import_binding_hit_recursive(node->data.tryCatchFinallyStatement.finallyBlock,
                                                    bindings,
                                                    position,
                                                    outBinding,
                                                    outLocation);

        case ZR_AST_CATCH_CLAUSE:
            return find_import_binding_hit_recursive(node->data.catchClause.block,
                                                    bindings,
                                                    position,
                                                    outBinding,
                                                    outLocation);

        case ZR_AST_ASSIGNMENT_EXPRESSION:
            return find_import_binding_hit_recursive(node->data.assignmentExpression.left,
                                                    bindings,
                                                    position,
                                                    outBinding,
                                                    outLocation) ||
                   find_import_binding_hit_recursive(node->data.assignmentExpression.right,
                                                    bindings,
                                                    position,
                                                    outBinding,
                                                    outLocation);

        case ZR_AST_BINARY_EXPRESSION:
            return find_import_binding_hit_recursive(node->data.binaryExpression.left,
                                                    bindings,
                                                    position,
                                                    outBinding,
                                                    outLocation) ||
                   find_import_binding_hit_recursive(node->data.binaryExpression.right,
                                                    bindings,
                                                    position,
                                                    outBinding,
                                                    outLocation);

        case ZR_AST_LOGICAL_EXPRESSION:
            return find_import_binding_hit_recursive(node->data.logicalExpression.left,
                                                    bindings,
                                                    position,
                                                    outBinding,
                                                    outLocation) ||
                   find_import_binding_hit_recursive(node->data.logicalExpression.right,
                                                    bindings,
                                                    position,
                                                    outBinding,
                                                    outLocation);

        case ZR_AST_CONDITIONAL_EXPRESSION:
            return find_import_binding_hit_recursive(node->data.conditionalExpression.test,
                                                    bindings,
                                                    position,
                                                    outBinding,
                                                    outLocation) ||
                   find_import_binding_hit_recursive(node->data.conditionalExpression.consequent,
                                                    bindings,
                                                    position,
                                                    outBinding,
                                                    outLocation) ||
                   find_import_binding_hit_recursive(node->data.conditionalExpression.alternate,
                                                    bindings,
                                                    position,
                                                    outBinding,
                                                    outLocation);

        case ZR_AST_UNARY_EXPRESSION:
            return find_import_binding_hit_recursive(node->data.unaryExpression.argument,
                                                    bindings,
                                                    position,
                                                    outBinding,
                                                    outLocation);

        case ZR_AST_TYPE_CAST_EXPRESSION:
            return find_import_binding_hit_recursive(node->data.typeCastExpression.expression,
                                                    bindings,
                                                    position,
                                                    outBinding,
                                                    outLocation);

        case ZR_AST_LAMBDA_EXPRESSION:
            return find_import_binding_hit_recursive(node->data.lambdaExpression.block,
                                                    bindings,
                                                    position,
                                                    outBinding,
                                                    outLocation);

        case ZR_AST_IF_EXPRESSION:
            return find_import_binding_hit_recursive(node->data.ifExpression.condition,
                                                    bindings,
                                                    position,
                                                    outBinding,
                                                    outLocation) ||
                   find_import_binding_hit_recursive(node->data.ifExpression.thenExpr,
                                                    bindings,
                                                    position,
                                                    outBinding,
                                                    outLocation) ||
                   find_import_binding_hit_recursive(node->data.ifExpression.elseExpr,
                                                    bindings,
                                                    position,
                                                    outBinding,
                                                    outLocation);

        case ZR_AST_SWITCH_EXPRESSION:
            return find_import_binding_hit_recursive(node->data.switchExpression.expr,
                                                    bindings,
                                                    position,
                                                    outBinding,
                                                    outLocation) ||
                   find_import_binding_hit_in_node_array(node->data.switchExpression.cases,
                                                        bindings,
                                                        position,
                                                        outBinding,
                                                        outLocation) ||
                   find_import_binding_hit_recursive(node->data.switchExpression.defaultCase,
                                                    bindings,
                                                    position,
                                                    outBinding,
                                                    outLocation);

        case ZR_AST_SWITCH_CASE:
            return find_import_binding_hit_recursive(node->data.switchCase.value,
                                                    bindings,
                                                    position,
                                                    outBinding,
                                                    outLocation) ||
                   find_import_binding_hit_recursive(node->data.switchCase.block,
                                                    bindings,
                                                    position,
                                                    outBinding,
                                                    outLocation);

        case ZR_AST_SWITCH_DEFAULT:
            return find_import_binding_hit_recursive(node->data.switchDefault.block,
                                                    bindings,
                                                    position,
                                                    outBinding,
                                                    outLocation);

        case ZR_AST_FUNCTION_CALL:
            return find_import_binding_hit_in_node_array(node->data.functionCall.args,
                                                         bindings,
                                                         position,
                                                         outBinding,
                                                         outLocation);

        case ZR_AST_PRIMARY_EXPRESSION:
            return find_import_binding_hit_recursive(node->data.primaryExpression.property,
                                                    bindings,
                                                    position,
                                                    outBinding,
                                                    outLocation) ||
                   find_import_binding_hit_in_node_array(node->data.primaryExpression.members,
                                                        bindings,
                                                        position,
                                                        outBinding,
                                                        outLocation);

        case ZR_AST_MEMBER_EXPRESSION:
            return node->data.memberExpression.computed &&
                   find_import_binding_hit_recursive(node->data.memberExpression.property,
                                                    bindings,
                                                    position,
                                                    outBinding,
                                                    outLocation);

        case ZR_AST_ARRAY_LITERAL:
            return find_import_binding_hit_in_node_array(node->data.arrayLiteral.elements,
                                                         bindings,
                                                         position,
                                                         outBinding,
                                                         outLocation);

        case ZR_AST_OBJECT_LITERAL:
            return find_import_binding_hit_in_node_array(node->data.objectLiteral.properties,
                                                         bindings,
                                                         position,
                                                         outBinding,
                                                         outLocation);

        case ZR_AST_KEY_VALUE_PAIR:
            return find_import_binding_hit_recursive(node->data.keyValuePair.key,
                                                    bindings,
                                                    position,
                                                    outBinding,
                                                    outLocation) ||
                   find_import_binding_hit_recursive(node->data.keyValuePair.value,
                                                    bindings,
                                                    position,
                                                    outBinding,
                                                    outLocation);

        case ZR_AST_WHILE_LOOP:
            return find_import_binding_hit_recursive(node->data.whileLoop.cond,
                                                    bindings,
                                                    position,
                                                    outBinding,
                                                    outLocation) ||
                   find_import_binding_hit_recursive(node->data.whileLoop.block,
                                                    bindings,
                                                    position,
                                                    outBinding,
                                                    outLocation);

        case ZR_AST_FOR_LOOP:
            return find_import_binding_hit_recursive(node->data.forLoop.init,
                                                    bindings,
                                                    position,
                                                    outBinding,
                                                    outLocation) ||
                   find_import_binding_hit_recursive(node->data.forLoop.cond,
                                                    bindings,
                                                    position,
                                                    outBinding,
                                                    outLocation) ||
                   find_import_binding_hit_recursive(node->data.forLoop.step,
                                                    bindings,
                                                    position,
                                                    outBinding,
                                                    outLocation) ||
                   find_import_binding_hit_recursive(node->data.forLoop.block,
                                                    bindings,
                                                    position,
                                                    outBinding,
                                                    outLocation);

        case ZR_AST_FOREACH_LOOP:
            return find_import_binding_hit_recursive(node->data.foreachLoop.expr,
                                                    bindings,
                                                    position,
                                                    outBinding,
                                                    outLocation) ||
                   find_import_binding_hit_recursive(node->data.foreachLoop.block,
                                                    bindings,
                                                    position,
                                                    outBinding,
                                                    outLocation);

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

TZrBool ZrLanguageServer_LspProject_FindImportBindingHit(SZrAstNode *node,
                                                         SZrArray *bindings,
                                                         SZrFileRange position,
                                                         SZrLspImportBinding **outBinding,
                                                         SZrFileRange *outLocation) {
    return find_import_binding_hit_recursive(node, bindings, position, outBinding, outLocation);
}

static TZrBool append_lsp_location(SZrState *state, SZrArray *result, SZrString *uri, SZrFileRange range) {
    SZrLspLocation *location;

    if (state == ZR_NULL || result == ZR_NULL || uri == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrLspLocation *), ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY);
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

    if (node == ZR_NULL || bindings == ZR_NULL || moduleName == ZR_NULL || result == ZR_NULL) {
        return ZR_TRUE;
    }

    if (primary_expression_get_imported_member(node, bindings, &hit) &&
        ZrLanguageServer_Lsp_StringsEqual(hit.moduleName, moduleName) &&
        (memberName == ZR_NULL || ZrLanguageServer_Lsp_StringsEqual(hit.memberName, memberName))) {
        if (memberName == ZR_NULL) {
            if (!append_lsp_location(state, result, hit.receiverLocation.source, hit.receiverLocation) ||
                !append_lsp_location(state, result, hit.location.source, hit.location)) {
                return ZR_FALSE;
            }
        } else if (!append_lsp_location(state, result, hit.location.source, hit.location)) {
            return ZR_FALSE;
        }
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

TZrBool ZrLanguageServer_LspProject_AppendMatchingImportedModuleLocations(SZrState *state,
                                                                          SZrAstNode *node,
                                                                          SZrArray *bindings,
                                                                          SZrString *moduleName,
                                                                          SZrArray *result) {
    return append_matching_imported_member_locations_recursive(state,
                                                               node,
                                                               bindings,
                                                               moduleName,
                                                               ZR_NULL,
                                                               result);
}

TZrBool ZrLanguageServer_LspProject_AppendMatchingImportBindingLocations(SZrState *state,
                                                                         SZrArray *bindings,
                                                                         SZrString *moduleName,
                                                                         SZrArray *result) {
    if (bindings == ZR_NULL || moduleName == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < bindings->length; index++) {
        SZrLspImportBinding **bindingPtr =
            (SZrLspImportBinding **)ZrCore_Array_Get(bindings, index);

        if (bindingPtr == ZR_NULL || *bindingPtr == ZR_NULL ||
            (*bindingPtr)->aliasName == ZR_NULL ||
            !ZrLanguageServer_Lsp_StringsEqual((*bindingPtr)->moduleName, moduleName)) {
            continue;
        }

        if (!append_lsp_location(state, result, (*bindingPtr)->aliasLocation.source, (*bindingPtr)->aliasLocation)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

TZrBool ZrLanguageServer_LspProject_AppendMatchingImportTargetLocations(SZrState *state,
                                                                        SZrArray *bindings,
                                                                        SZrString *moduleName,
                                                                        SZrArray *result) {
    if (bindings == ZR_NULL || moduleName == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < bindings->length; index++) {
        SZrLspImportBinding **bindingPtr =
            (SZrLspImportBinding **)ZrCore_Array_Get(bindings, index);

        if (bindingPtr == ZR_NULL || *bindingPtr == ZR_NULL ||
            !ZrLanguageServer_Lsp_StringsEqual((*bindingPtr)->moduleName, moduleName)) {
            continue;
        }

        if (!append_lsp_location(state,
                                 result,
                                 (*bindingPtr)->modulePathLocation.source,
                                 (*bindingPtr)->modulePathLocation)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}
