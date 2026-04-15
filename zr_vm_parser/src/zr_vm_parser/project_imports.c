#include "zr_vm_parser/project_imports.h"

#include "zr_vm_library/project.h"

#include "zr_vm_core/string.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

static const TZrChar *project_imports_string_text(SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_NULL;
    }

    if (value->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        return ZrCore_String_GetNativeStringShort(value);
    }

    return ZrCore_String_GetNativeString(value);
}

static void project_imports_set_error(TZrChar *errorBuffer,
                                      TZrSize errorBufferSize,
                                      const TZrChar *message) {
    if (errorBuffer == ZR_NULL || errorBufferSize == 0) {
        return;
    }

    errorBuffer[0] = '\0';
    if (message == ZR_NULL) {
        return;
    }

    snprintf(errorBuffer, errorBufferSize, "%s", message);
}

static void project_imports_set_error_location(SZrFileRange *outErrorLocation, SZrAstNode *node) {
    if (outErrorLocation == ZR_NULL) {
        return;
    }

    if (node != ZR_NULL) {
        *outErrorLocation = node->location;
        return;
    }

    memset(outErrorLocation, 0, sizeof(*outErrorLocation));
}

static SZrAstNode *project_imports_parameter_node_from_ptr(SZrParameter *parameter) {
    if (parameter == ZR_NULL) {
        return ZR_NULL;
    }

    return (SZrAstNode *)((TZrBytePtr)parameter - offsetof(SZrAstNode, data.parameter));
}

static TZrBool project_imports_replace_string_literal_value(SZrState *state,
                                                            SZrAstNode *stringLiteralNode,
                                                            const TZrChar *text) {
    SZrString *value;

    if (state == ZR_NULL || stringLiteralNode == ZR_NULL || text == ZR_NULL ||
        stringLiteralNode->type != ZR_AST_STRING_LITERAL) {
        return ZR_FALSE;
    }

    value = stringLiteralNode->data.stringLiteral.value;
    if (value != ZR_NULL) {
        const TZrChar *existing = project_imports_string_text(value);
        if (existing != ZR_NULL && strcmp(existing, text) == 0) {
            return ZR_TRUE;
        }
    }

    value = ZrCore_String_CreateFromNative(state, (TZrNativeString)text);
    if (value == ZR_NULL) {
        return ZR_FALSE;
    }

    stringLiteralNode->data.stringLiteral.value = value;
    return ZR_TRUE;
}

static const TZrChar *project_imports_extract_explicit_module_key(SZrAstNode *ast, SZrAstNode **outNameNode) {
    SZrAstNode *moduleNode;
    SZrAstNode *nameNode;

    if (outNameNode != ZR_NULL) {
        *outNameNode = ZR_NULL;
    }

    if (ast == ZR_NULL || ast->type != ZR_AST_SCRIPT || ast->data.script.moduleName == ZR_NULL) {
        return ZR_NULL;
    }

    moduleNode = ast->data.script.moduleName;
    if (moduleNode->type != ZR_AST_MODULE_DECLARATION || moduleNode->data.moduleDeclaration.name == ZR_NULL) {
        return ZR_NULL;
    }

    nameNode = moduleNode->data.moduleDeclaration.name;
    if (nameNode->type != ZR_AST_STRING_LITERAL || nameNode->data.stringLiteral.value == ZR_NULL) {
        return ZR_NULL;
    }

    if (outNameNode != ZR_NULL) {
        *outNameNode = nameNode;
    }
    return project_imports_string_text(nameNode->data.stringLiteral.value);
}

static TZrBool project_imports_canonicalize_node(SZrState *state,
                                                 SZrAstNode *node,
                                                 const SZrLibrary_Project *project,
                                                 const TZrChar *currentModuleKey,
                                                 TZrChar *errorBuffer,
                                                 TZrSize errorBufferSize,
                                                 SZrFileRange *outErrorLocation);

static TZrBool project_imports_canonicalize_node_array(SZrState *state,
                                                       SZrAstNodeArray *nodes,
                                                       const SZrLibrary_Project *project,
                                                       const TZrChar *currentModuleKey,
                                                       TZrChar *errorBuffer,
                                                       TZrSize errorBufferSize,
                                                       SZrFileRange *outErrorLocation) {
    if (nodes == ZR_NULL || nodes->nodes == ZR_NULL) {
        return ZR_TRUE;
    }

    for (TZrSize index = 0; index < nodes->count; index++) {
        if (!project_imports_canonicalize_node(state,
                                               nodes->nodes[index],
                                               project,
                                               currentModuleKey,
                                               errorBuffer,
                                               errorBufferSize,
                                               outErrorLocation)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool project_imports_canonicalize_import_expression(SZrState *state,
                                                              SZrAstNode *node,
                                                              const SZrLibrary_Project *project,
                                                              const TZrChar *currentModuleKey,
                                                              TZrChar *errorBuffer,
                                                              TZrSize errorBufferSize,
                                                              SZrFileRange *outErrorLocation) {
    TZrChar resolvedModuleKey[ZR_LIBRARY_MAX_PATH_LENGTH];
    const TZrChar *rawSpecifier;
    SZrAstNode *modulePathNode;

    if (state == ZR_NULL || node == ZR_NULL || node->type != ZR_AST_IMPORT_EXPRESSION) {
        return ZR_FALSE;
    }

    modulePathNode = node->data.importExpression.modulePath;
    if (modulePathNode == ZR_NULL || modulePathNode->type != ZR_AST_STRING_LITERAL ||
        modulePathNode->data.stringLiteral.value == ZR_NULL) {
        return ZR_TRUE;
    }

    rawSpecifier = project_imports_string_text(modulePathNode->data.stringLiteral.value);
    if (rawSpecifier == ZR_NULL) {
        return ZR_TRUE;
    }

    if (!ZrLibrary_Project_ResolveImportModuleKey(project,
                                                  currentModuleKey,
                                                  rawSpecifier,
                                                  resolvedModuleKey,
                                                  sizeof(resolvedModuleKey),
                                                  errorBuffer,
                                                  errorBufferSize)) {
        project_imports_set_error_location(outErrorLocation, modulePathNode);
        return ZR_FALSE;
    }

    if (!project_imports_replace_string_literal_value(state, modulePathNode, resolvedModuleKey)) {
        project_imports_set_error(errorBuffer, errorBufferSize, "failed to store canonical import module key");
        project_imports_set_error_location(outErrorLocation, modulePathNode);
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static TZrBool project_imports_canonicalize_node(SZrState *state,
                                                 SZrAstNode *node,
                                                 const SZrLibrary_Project *project,
                                                 const TZrChar *currentModuleKey,
                                                 TZrChar *errorBuffer,
                                                 TZrSize errorBufferSize,
                                                 SZrFileRange *outErrorLocation) {
    if (node == ZR_NULL) {
        return ZR_TRUE;
    }

    switch (node->type) {
        case ZR_AST_SCRIPT:
            return project_imports_canonicalize_node_array(state,
                                                           node->data.script.statements,
                                                           project,
                                                           currentModuleKey,
                                                           errorBuffer,
                                                           errorBufferSize,
                                                           outErrorLocation);

        case ZR_AST_STRUCT_DECLARATION:
            return project_imports_canonicalize_node_array(state,
                                                           node->data.structDeclaration.members,
                                                           project,
                                                           currentModuleKey,
                                                           errorBuffer,
                                                           errorBufferSize,
                                                           outErrorLocation);

        case ZR_AST_CLASS_DECLARATION:
            return project_imports_canonicalize_node_array(state,
                                                           node->data.classDeclaration.decorators,
                                                           project,
                                                           currentModuleKey,
                                                           errorBuffer,
                                                           errorBufferSize,
                                                           outErrorLocation) &&
                   project_imports_canonicalize_node_array(state,
                                                           node->data.classDeclaration.members,
                                                           project,
                                                           currentModuleKey,
                                                           errorBuffer,
                                                           errorBufferSize,
                                                           outErrorLocation);

        case ZR_AST_INTERFACE_DECLARATION:
            return project_imports_canonicalize_node_array(state,
                                                           node->data.interfaceDeclaration.members,
                                                           project,
                                                           currentModuleKey,
                                                           errorBuffer,
                                                           errorBufferSize,
                                                           outErrorLocation);

        case ZR_AST_ENUM_DECLARATION:
            return project_imports_canonicalize_node_array(state,
                                                           node->data.enumDeclaration.decorators,
                                                           project,
                                                           currentModuleKey,
                                                           errorBuffer,
                                                           errorBufferSize,
                                                           outErrorLocation) &&
                   project_imports_canonicalize_node_array(state,
                                                           node->data.enumDeclaration.members,
                                                           project,
                                                           currentModuleKey,
                                                           errorBuffer,
                                                           errorBufferSize,
                                                           outErrorLocation);

        case ZR_AST_FUNCTION_DECLARATION:
            return project_imports_canonicalize_node_array(state,
                                                           node->data.functionDeclaration.decorators,
                                                           project,
                                                           currentModuleKey,
                                                           errorBuffer,
                                                           errorBufferSize,
                                                           outErrorLocation) &&
                   project_imports_canonicalize_node_array(state,
                                                           node->data.functionDeclaration.params,
                                                           project,
                                                           currentModuleKey,
                                                           errorBuffer,
                                                           errorBufferSize,
                                                           outErrorLocation) &&
                   project_imports_canonicalize_node(state,
                                                     project_imports_parameter_node_from_ptr(node->data.functionDeclaration.args),
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation) &&
                   project_imports_canonicalize_node(state,
                                                     node->data.functionDeclaration.body,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation);

        case ZR_AST_TEST_DECLARATION:
            return project_imports_canonicalize_node_array(state,
                                                           node->data.testDeclaration.params,
                                                           project,
                                                           currentModuleKey,
                                                           errorBuffer,
                                                           errorBufferSize,
                                                           outErrorLocation) &&
                   project_imports_canonicalize_node(state,
                                                     project_imports_parameter_node_from_ptr(node->data.testDeclaration.args),
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation) &&
                   project_imports_canonicalize_node(state,
                                                     node->data.testDeclaration.body,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation);

        case ZR_AST_COMPILE_TIME_DECLARATION:
            return project_imports_canonicalize_node(state,
                                                     node->data.compileTimeDeclaration.declaration,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation);

        case ZR_AST_EXTERN_BLOCK:
            return project_imports_canonicalize_node_array(state,
                                                           node->data.externBlock.declarations,
                                                           project,
                                                           currentModuleKey,
                                                           errorBuffer,
                                                           errorBufferSize,
                                                           outErrorLocation);

        case ZR_AST_VARIABLE_DECLARATION:
            return project_imports_canonicalize_node(state,
                                                     node->data.variableDeclaration.value,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation);

        case ZR_AST_STRUCT_FIELD:
            return project_imports_canonicalize_node(state,
                                                     node->data.structField.init,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation);

        case ZR_AST_STRUCT_METHOD:
            return project_imports_canonicalize_node_array(state,
                                                           node->data.structMethod.decorators,
                                                           project,
                                                           currentModuleKey,
                                                           errorBuffer,
                                                           errorBufferSize,
                                                           outErrorLocation) &&
                   project_imports_canonicalize_node_array(state,
                                                           node->data.structMethod.params,
                                                           project,
                                                           currentModuleKey,
                                                           errorBuffer,
                                                           errorBufferSize,
                                                           outErrorLocation) &&
                   project_imports_canonicalize_node(state,
                                                     project_imports_parameter_node_from_ptr(node->data.structMethod.args),
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation) &&
                   project_imports_canonicalize_node(state,
                                                     node->data.structMethod.body,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation);

        case ZR_AST_STRUCT_META_FUNCTION:
            return project_imports_canonicalize_node_array(state,
                                                           node->data.structMetaFunction.params,
                                                           project,
                                                           currentModuleKey,
                                                           errorBuffer,
                                                           errorBufferSize,
                                                           outErrorLocation) &&
                   project_imports_canonicalize_node(state,
                                                     project_imports_parameter_node_from_ptr(node->data.structMetaFunction.args),
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation) &&
                   project_imports_canonicalize_node(state,
                                                     node->data.structMetaFunction.body,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation);

        case ZR_AST_CLASS_FIELD:
            return project_imports_canonicalize_node_array(state,
                                                           node->data.classField.decorators,
                                                           project,
                                                           currentModuleKey,
                                                           errorBuffer,
                                                           errorBufferSize,
                                                           outErrorLocation) &&
                   project_imports_canonicalize_node(state,
                                                     node->data.classField.init,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation);

        case ZR_AST_CLASS_METHOD:
            return project_imports_canonicalize_node_array(state,
                                                           node->data.classMethod.decorators,
                                                           project,
                                                           currentModuleKey,
                                                           errorBuffer,
                                                           errorBufferSize,
                                                           outErrorLocation) &&
                   project_imports_canonicalize_node_array(state,
                                                           node->data.classMethod.params,
                                                           project,
                                                           currentModuleKey,
                                                           errorBuffer,
                                                           errorBufferSize,
                                                           outErrorLocation) &&
                   project_imports_canonicalize_node(state,
                                                     project_imports_parameter_node_from_ptr(node->data.classMethod.args),
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation) &&
                   project_imports_canonicalize_node(state,
                                                     node->data.classMethod.body,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation);

        case ZR_AST_CLASS_PROPERTY:
            return project_imports_canonicalize_node_array(state,
                                                           node->data.classProperty.decorators,
                                                           project,
                                                           currentModuleKey,
                                                           errorBuffer,
                                                           errorBufferSize,
                                                           outErrorLocation) &&
                   project_imports_canonicalize_node(state,
                                                     node->data.classProperty.modifier,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation);

        case ZR_AST_CLASS_META_FUNCTION:
            return project_imports_canonicalize_node_array(state,
                                                           node->data.classMetaFunction.params,
                                                           project,
                                                           currentModuleKey,
                                                           errorBuffer,
                                                           errorBufferSize,
                                                           outErrorLocation) &&
                   project_imports_canonicalize_node(state,
                                                     project_imports_parameter_node_from_ptr(node->data.classMetaFunction.args),
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation) &&
                   project_imports_canonicalize_node_array(state,
                                                           node->data.classMetaFunction.superArgs,
                                                           project,
                                                           currentModuleKey,
                                                           errorBuffer,
                                                           errorBufferSize,
                                                           outErrorLocation) &&
                   project_imports_canonicalize_node(state,
                                                     node->data.classMetaFunction.body,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation);

        case ZR_AST_PARAMETER:
            return project_imports_canonicalize_node_array(state,
                                                           node->data.parameter.decorators,
                                                           project,
                                                           currentModuleKey,
                                                           errorBuffer,
                                                           errorBufferSize,
                                                           outErrorLocation) &&
                   project_imports_canonicalize_node(state,
                                                     node->data.parameter.defaultValue,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation);

        case ZR_AST_DECORATOR_EXPRESSION:
            return project_imports_canonicalize_node(state,
                                                     node->data.decoratorExpression.expr,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation);

        case ZR_AST_PROPERTY_GET:
            return project_imports_canonicalize_node(state,
                                                     node->data.propertyGet.body,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation);

        case ZR_AST_PROPERTY_SET:
            return project_imports_canonicalize_node(state,
                                                     node->data.propertySet.body,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation);

        case ZR_AST_BINARY_EXPRESSION:
            return project_imports_canonicalize_node(state,
                                                     node->data.binaryExpression.left,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation) &&
                   project_imports_canonicalize_node(state,
                                                     node->data.binaryExpression.right,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation);

        case ZR_AST_LOGICAL_EXPRESSION:
            return project_imports_canonicalize_node(state,
                                                     node->data.logicalExpression.left,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation) &&
                   project_imports_canonicalize_node(state,
                                                     node->data.logicalExpression.right,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation);

        case ZR_AST_UNARY_EXPRESSION:
            return project_imports_canonicalize_node(state,
                                                     node->data.unaryExpression.argument,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation);

        case ZR_AST_TYPE_CAST_EXPRESSION:
            return project_imports_canonicalize_node(state,
                                                     node->data.typeCastExpression.expression,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation);

        case ZR_AST_ASSIGNMENT_EXPRESSION:
            return project_imports_canonicalize_node(state,
                                                     node->data.assignmentExpression.left,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation) &&
                   project_imports_canonicalize_node(state,
                                                     node->data.assignmentExpression.right,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation);

        case ZR_AST_CONDITIONAL_EXPRESSION:
            return project_imports_canonicalize_node(state,
                                                     node->data.conditionalExpression.test,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation) &&
                   project_imports_canonicalize_node(state,
                                                     node->data.conditionalExpression.consequent,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation) &&
                   project_imports_canonicalize_node(state,
                                                     node->data.conditionalExpression.alternate,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation);

        case ZR_AST_LAMBDA_EXPRESSION:
            return project_imports_canonicalize_node_array(state,
                                                           node->data.lambdaExpression.params,
                                                           project,
                                                           currentModuleKey,
                                                           errorBuffer,
                                                           errorBufferSize,
                                                           outErrorLocation) &&
                   project_imports_canonicalize_node(state,
                                                     project_imports_parameter_node_from_ptr(node->data.lambdaExpression.args),
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation) &&
                   project_imports_canonicalize_node(state,
                                                     node->data.lambdaExpression.block,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation);

        case ZR_AST_IF_EXPRESSION:
            return project_imports_canonicalize_node(state,
                                                     node->data.ifExpression.condition,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation) &&
                   project_imports_canonicalize_node(state,
                                                     node->data.ifExpression.thenExpr,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation) &&
                   project_imports_canonicalize_node(state,
                                                     node->data.ifExpression.elseExpr,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation);

        case ZR_AST_SWITCH_EXPRESSION:
            return project_imports_canonicalize_node(state,
                                                     node->data.switchExpression.expr,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation) &&
                   project_imports_canonicalize_node_array(state,
                                                           node->data.switchExpression.cases,
                                                           project,
                                                           currentModuleKey,
                                                           errorBuffer,
                                                           errorBufferSize,
                                                           outErrorLocation) &&
                   project_imports_canonicalize_node(state,
                                                     node->data.switchExpression.defaultCase,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation);

        case ZR_AST_SWITCH_CASE:
            return project_imports_canonicalize_node(state,
                                                     node->data.switchCase.value,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation) &&
                   project_imports_canonicalize_node(state,
                                                     node->data.switchCase.block,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation);

        case ZR_AST_SWITCH_DEFAULT:
            return project_imports_canonicalize_node(state,
                                                     node->data.switchDefault.block,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation);

        case ZR_AST_FUNCTION_CALL:
            return project_imports_canonicalize_node_array(state,
                                                           node->data.functionCall.args,
                                                           project,
                                                           currentModuleKey,
                                                           errorBuffer,
                                                           errorBufferSize,
                                                           outErrorLocation) &&
                   project_imports_canonicalize_node_array(state,
                                                           node->data.functionCall.genericArguments,
                                                           project,
                                                           currentModuleKey,
                                                           errorBuffer,
                                                           errorBufferSize,
                                                           outErrorLocation);

        case ZR_AST_MEMBER_EXPRESSION:
            return !node->data.memberExpression.computed ||
                   project_imports_canonicalize_node(state,
                                                     node->data.memberExpression.property,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation);

        case ZR_AST_PRIMARY_EXPRESSION:
            return project_imports_canonicalize_node(state,
                                                     node->data.primaryExpression.property,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation) &&
                   project_imports_canonicalize_node_array(state,
                                                           node->data.primaryExpression.members,
                                                           project,
                                                           currentModuleKey,
                                                           errorBuffer,
                                                           errorBufferSize,
                                                           outErrorLocation);

        case ZR_AST_IMPORT_EXPRESSION:
            return project_imports_canonicalize_import_expression(state,
                                                                  node,
                                                                  project,
                                                                  currentModuleKey,
                                                                  errorBuffer,
                                                                  errorBufferSize,
                                                                  outErrorLocation);

        case ZR_AST_TYPE_QUERY_EXPRESSION:
            return project_imports_canonicalize_node(state,
                                                     node->data.typeQueryExpression.operand,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation);

        case ZR_AST_PROTOTYPE_REFERENCE_EXPRESSION:
            return project_imports_canonicalize_node(state,
                                                     node->data.prototypeReferenceExpression.target,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation);

        case ZR_AST_CONSTRUCT_EXPRESSION:
            return project_imports_canonicalize_node(state,
                                                     node->data.constructExpression.target,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation) &&
                   project_imports_canonicalize_node_array(state,
                                                           node->data.constructExpression.args,
                                                           project,
                                                           currentModuleKey,
                                                           errorBuffer,
                                                           errorBufferSize,
                                                           outErrorLocation);

        case ZR_AST_ARRAY_LITERAL:
            return project_imports_canonicalize_node_array(state,
                                                           node->data.arrayLiteral.elements,
                                                           project,
                                                           currentModuleKey,
                                                           errorBuffer,
                                                           errorBufferSize,
                                                           outErrorLocation);

        case ZR_AST_OBJECT_LITERAL:
            return project_imports_canonicalize_node_array(state,
                                                           node->data.objectLiteral.properties,
                                                           project,
                                                           currentModuleKey,
                                                           errorBuffer,
                                                           errorBufferSize,
                                                           outErrorLocation);

        case ZR_AST_BLOCK:
            return project_imports_canonicalize_node_array(state,
                                                           node->data.block.body,
                                                           project,
                                                           currentModuleKey,
                                                           errorBuffer,
                                                           errorBufferSize,
                                                           outErrorLocation);

        case ZR_AST_EXPRESSION_STATEMENT:
            return project_imports_canonicalize_node(state,
                                                     node->data.expressionStatement.expr,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation);

        case ZR_AST_USING_STATEMENT:
            return project_imports_canonicalize_node(state,
                                                     node->data.usingStatement.resource,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation) &&
                   project_imports_canonicalize_node(state,
                                                     node->data.usingStatement.body,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation);

        case ZR_AST_RETURN_STATEMENT:
            return project_imports_canonicalize_node(state,
                                                     node->data.returnStatement.expr,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation);

        case ZR_AST_BREAK_CONTINUE_STATEMENT:
            return project_imports_canonicalize_node(state,
                                                     node->data.breakContinueStatement.expr,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation);

        case ZR_AST_THROW_STATEMENT:
            return project_imports_canonicalize_node(state,
                                                     node->data.throwStatement.expr,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation);

        case ZR_AST_OUT_STATEMENT:
            return project_imports_canonicalize_node(state,
                                                     node->data.outStatement.expr,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation);

        case ZR_AST_TRY_CATCH_FINALLY_STATEMENT:
            return project_imports_canonicalize_node(state,
                                                     node->data.tryCatchFinallyStatement.block,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation) &&
                   project_imports_canonicalize_node_array(state,
                                                           node->data.tryCatchFinallyStatement.catchClauses,
                                                           project,
                                                           currentModuleKey,
                                                           errorBuffer,
                                                           errorBufferSize,
                                                           outErrorLocation) &&
                   project_imports_canonicalize_node(state,
                                                     node->data.tryCatchFinallyStatement.finallyBlock,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation);

        case ZR_AST_CATCH_CLAUSE:
            return project_imports_canonicalize_node(state,
                                                     node->data.catchClause.block,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation);

        case ZR_AST_TEMPLATE_STRING_LITERAL:
            return project_imports_canonicalize_node_array(state,
                                                           node->data.templateStringLiteral.segments,
                                                           project,
                                                           currentModuleKey,
                                                           errorBuffer,
                                                           errorBufferSize,
                                                           outErrorLocation);

        case ZR_AST_INTERPOLATED_SEGMENT:
            return project_imports_canonicalize_node(state,
                                                     node->data.interpolatedSegment.expression,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation);

        case ZR_AST_KEY_VALUE_PAIR:
            return project_imports_canonicalize_node(state,
                                                     node->data.keyValuePair.key,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation) &&
                   project_imports_canonicalize_node(state,
                                                     node->data.keyValuePair.value,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation);

        case ZR_AST_UNPACK_LITERAL:
            return project_imports_canonicalize_node(state,
                                                     node->data.unpackLiteral.element,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation);

        case ZR_AST_GENERATOR_EXPRESSION:
            return project_imports_canonicalize_node(state,
                                                     node->data.generatorExpression.block,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation);

        case ZR_AST_WHILE_LOOP:
            return project_imports_canonicalize_node(state,
                                                     node->data.whileLoop.cond,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation) &&
                   project_imports_canonicalize_node(state,
                                                     node->data.whileLoop.block,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation);

        case ZR_AST_FOR_LOOP:
            return project_imports_canonicalize_node(state,
                                                     node->data.forLoop.init,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation) &&
                   project_imports_canonicalize_node(state,
                                                     node->data.forLoop.cond,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation) &&
                   project_imports_canonicalize_node(state,
                                                     node->data.forLoop.step,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation) &&
                   project_imports_canonicalize_node(state,
                                                     node->data.forLoop.block,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation);

        case ZR_AST_FOREACH_LOOP:
            return project_imports_canonicalize_node(state,
                                                     node->data.foreachLoop.expr,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation) &&
                   project_imports_canonicalize_node(state,
                                                     node->data.foreachLoop.block,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation);

        case ZR_AST_INTERMEDIATE_STATEMENT:
            return project_imports_canonicalize_node(state,
                                                     node->data.intermediateStatement.declaration,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation) &&
                   project_imports_canonicalize_node_array(state,
                                                           node->data.intermediateStatement.instructions,
                                                           project,
                                                           currentModuleKey,
                                                           errorBuffer,
                                                           errorBufferSize,
                                                           outErrorLocation);

        case ZR_AST_INTERMEDIATE_DECLARATION:
            return project_imports_canonicalize_node_array(state,
                                                           node->data.intermediateDeclaration.constants,
                                                           project,
                                                           currentModuleKey,
                                                           errorBuffer,
                                                           errorBufferSize,
                                                           outErrorLocation);

        case ZR_AST_INTERMEDIATE_CONSTANT:
            return project_imports_canonicalize_node(state,
                                                     node->data.intermediateConstant.value,
                                                     project,
                                                     currentModuleKey,
                                                     errorBuffer,
                                                     errorBufferSize,
                                                     outErrorLocation);

        default:
            return ZR_TRUE;
    }
}

ZR_PARSER_API TZrBool ZrParser_ProjectImports_CanonicalizeAst(SZrState *state,
                                                              SZrAstNode *ast,
                                                              SZrString *sourceName,
                                                              SZrString **outCurrentModuleKey,
                                                              TZrChar *errorBuffer,
                                                              TZrSize errorBufferSize,
                                                              SZrFileRange *outErrorLocation) {
    const SZrLibrary_Project *project;
    const TZrChar *sourceNameText;
    const TZrChar *explicitModuleKey;
    TZrChar currentModuleKeyBuffer[ZR_LIBRARY_MAX_PATH_LENGTH];
    SZrAstNode *moduleNameNode = ZR_NULL;
    SZrString *currentModuleKey = ZR_NULL;

    if (outCurrentModuleKey != ZR_NULL) {
        *outCurrentModuleKey = ZR_NULL;
    }
    if (errorBuffer != ZR_NULL && errorBufferSize > 0) {
        errorBuffer[0] = '\0';
    }
    if (outErrorLocation != ZR_NULL) {
        memset(outErrorLocation, 0, sizeof(*outErrorLocation));
    }

    if (state == ZR_NULL) {
        project_imports_set_error(errorBuffer, errorBufferSize, "invalid parser state");
        return ZR_FALSE;
    }

    project = ZrLibrary_Project_GetFromGlobal(state->global);
    sourceNameText = project_imports_string_text(sourceName);
    explicitModuleKey = project_imports_extract_explicit_module_key(ast, &moduleNameNode);

    if (!ZrLibrary_Project_DeriveCurrentModuleKey(project,
                                                  sourceNameText,
                                                  explicitModuleKey,
                                                  currentModuleKeyBuffer,
                                                  sizeof(currentModuleKeyBuffer),
                                                  errorBuffer,
                                                  errorBufferSize)) {
        project_imports_set_error_location(outErrorLocation, moduleNameNode != ZR_NULL ? moduleNameNode : ast);
        return ZR_FALSE;
    }

    if (moduleNameNode != ZR_NULL &&
        !project_imports_replace_string_literal_value(state, moduleNameNode, currentModuleKeyBuffer)) {
        project_imports_set_error(errorBuffer, errorBufferSize, "failed to store canonical module key");
        project_imports_set_error_location(outErrorLocation, moduleNameNode);
        return ZR_FALSE;
    }

    if (ast != ZR_NULL &&
        !project_imports_canonicalize_node(state,
                                           ast,
                                           project,
                                           currentModuleKeyBuffer,
                                           errorBuffer,
                                           errorBufferSize,
                                           outErrorLocation)) {
        return ZR_FALSE;
    }

    if (moduleNameNode != ZR_NULL && moduleNameNode->type == ZR_AST_STRING_LITERAL) {
        currentModuleKey = moduleNameNode->data.stringLiteral.value;
    }
    if (currentModuleKey == ZR_NULL) {
        currentModuleKey = ZrCore_String_CreateFromNative(state, (TZrNativeString)currentModuleKeyBuffer);
        if (currentModuleKey == ZR_NULL) {
            project_imports_set_error(errorBuffer, errorBufferSize, "failed to create canonical module key");
            project_imports_set_error_location(outErrorLocation, ast);
            return ZR_FALSE;
        }
    }

    if (outCurrentModuleKey != ZR_NULL) {
        *outCurrentModuleKey = currentModuleKey;
    }
    return ZR_TRUE;
}
