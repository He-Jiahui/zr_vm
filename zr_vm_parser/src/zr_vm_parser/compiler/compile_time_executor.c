//
// Created by Auto on 2025/01/XX.
//

#include "zr_vm_parser/compiler.h"
#include "compile_expression_internal.h"
#include "compile_time_binding_metadata.h"
#include "compile_time_declaration_patch_attributes.h"
#include "compile_time_declaration_patch_diagnostics.h"
#include "compile_time_declaration_patch_interfaces.h"
#include "compile_time_declaration_patch_transaction.h"
#include "compile_time_decorator_identity.h"
#include "compile_time_executor_internal.h"
#include "compile_tool_binding.h"
#include "compile_tool_execution_scope.h"
#include "compile_tool_evaluator.h"
#include "compile_tool_project_provider.h"
#include "compiler_attribute_binding.h"
#include "compiler_decorator_contract.h"
#include "comptime_runtime_contract.h"
#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/compile_tool.h"
#include "zr_vm_parser/declaration_transform_contract.h"
#include "zr_vm_parser/type_inference.h"

#include "zr_vm_core/array.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/reflection.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_library/project.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static TZrBool ct_prepare_build_fact_node(
        SZrCompilerState *cs,
        SZrAstNode *node,
        TZrBool runtimeContext);

static TZrBool ct_prepare_build_fact_node_with_context(
        SZrCompilerState *cs,
        SZrAstNode *node,
        TZrBool runtimeContext,
        TZrBool moduleDeclarationList);

static TZrBool ct_prepare_build_fact_scoped_array(
        SZrCompilerState *cs,
        SZrAstNodeArray *nodes,
        TZrBool runtimeContext);

static TZrBool ct_prepare_build_fact_predeclare_function_shadow(
        SZrCompilerState *cs,
        SZrAstNode *node);

static TZrBool ct_prepare_build_fact_module_bindings(
        SZrCompilerState *cs,
        SZrAstNodeArray *nodes);

static const SZrParserCompileToolModuleDescriptor *ct_compile_tool_import_descriptor(
        const SZrState *state,
        const SZrAstNode *node);

static TZrBool ct_prepare_build_fact_array(
        SZrCompilerState *cs,
        SZrAstNodeArray *nodes,
        TZrBool runtimeContext) {
    for (TZrSize index = 0; nodes != ZR_NULL && index < nodes->count; index++) {
        if (!ct_prepare_build_fact_node(cs, nodes->nodes[index], runtimeContext)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

TZrBool ZrParser_CompileToolExecution_DeclareImport(
        SZrCompilerState *cs,
        SZrAstNode *node) {
    const SZrParserCompileToolModuleDescriptor *provider;
    const SZrCompileToolBinding *existing;
    SZrString *aliasName;
    const TZrChar *moduleName;

    if (cs == ZR_NULL || node == ZR_NULL ||
        !compiler_is_compile_tool_import_declaration(cs->state, node)) {
        return ZR_FALSE;
    }
    provider = ct_compile_tool_import_descriptor(cs->state, node);
    aliasName = node->data.variableDeclaration.pattern->data.identifier.name;
    moduleName = ZrCore_String_GetNativeString(
            node->data.variableDeclaration.value->data.importExpression
                    .modulePath->data.stringLiteral.value);
    existing = ZrParser_CompileToolBinding_Resolve(cs, aliasName);
    if (existing != ZR_NULL &&
        (existing->kind == ZR_COMPILE_TOOL_BINDING_SHADOW ||
         (existing->kind == ZR_COMPILE_TOOL_BINDING_PROVIDER &&
          ((existing->provider == provider && provider != ZR_NULL) ||
           (provider == ZR_NULL && existing->provider != ZR_NULL &&
            existing->provider->moduleName != ZR_NULL &&
            strcmp(existing->provider->moduleName, moduleName) == 0 &&
            existing->resolvedArtifact != ZR_NULL))))) {
        return ZR_TRUE;
    }
    if (provider != ZR_NULL) {
        return ZrParser_CompileToolBinding_DeclareProvider(
                cs, aliasName, provider);
    }
    return ZrParser_CompileToolProjectProvider_Declare(
            cs, aliasName, moduleName, node->location);
}

static TZrBool ct_prepare_build_fact_module_array(
        SZrCompilerState *cs,
        SZrAstNodeArray *nodes) {
    if (!ct_prepare_build_fact_module_bindings(cs, nodes)) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0; nodes != ZR_NULL && index < nodes->count; index++) {
        if (!ct_prepare_build_fact_node_with_context(
                    cs, nodes->nodes[index], ZR_TRUE, ZR_TRUE)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool ct_prepare_build_fact_predeclare_function_shadow(
        SZrCompilerState *cs,
        SZrAstNode *node) {
    if (node == ZR_NULL) {
        return ZR_TRUE;
    }

    if (node->type == ZR_AST_FUNCTION_DECLARATION) {
        SZrIdentifier *name = node->data.functionDeclaration.name;
        const SZrCompileToolBinding *existing;

        if (name == ZR_NULL || name->name == ZR_NULL) {
            return ZR_TRUE;
        }
        existing = ZrParser_CompileToolBinding_Resolve(cs, name->name);
        return (existing != ZR_NULL &&
                existing->kind == ZR_COMPILE_TOOL_BINDING_SHADOW) ||
               ZrParser_CompileToolBinding_DeclareShadow(cs, name->name);
    }

    return ZR_TRUE;
}

static TZrBool ct_prepare_build_fact_scoped_array(
        SZrCompilerState *cs,
        SZrAstNodeArray *nodes,
        TZrBool runtimeContext) {
    TZrSize mark = ZrParser_CompileToolBinding_Mark(cs);
    TZrBool succeeded = ZR_TRUE;

    for (TZrSize index = 0; nodes != ZR_NULL && index < nodes->count; index++) {
        if (!ct_prepare_build_fact_predeclare_function_shadow(
                    cs, nodes->nodes[index])) {
            succeeded = ZR_FALSE;
            break;
        }
    }
    if (succeeded) {
        succeeded = ct_prepare_build_fact_array(cs, nodes, runtimeContext);
    }
    ZrParser_CompileToolBinding_Restore(cs, mark);
    return succeeded;
}

static TZrBool ct_prepare_build_fact_pattern_shadows(
        SZrCompilerState *cs,
        SZrAstNode *pattern) {
    SZrAstNodeArray *keys = ZR_NULL;

    if (pattern == ZR_NULL) {
        return ZR_TRUE;
    }
    if (pattern->type == ZR_AST_IDENTIFIER_LITERAL) {
        return ZrParser_CompileToolBinding_DeclareShadow(
                cs, pattern->data.identifier.name);
    }
    if (pattern->type == ZR_AST_KEY_VALUE_PAIR) {
        return ct_prepare_build_fact_pattern_shadows(
                cs, pattern->data.keyValuePair.value);
    }
    if (pattern->type == ZR_AST_UNPACK_LITERAL) {
        return ct_prepare_build_fact_pattern_shadows(
                cs, pattern->data.unpackLiteral.element);
    }
    if (pattern->type == ZR_AST_DESTRUCTURING_OBJECT) {
        keys = pattern->data.destructuringObject.keys;
    } else if (pattern->type == ZR_AST_DESTRUCTURING_ARRAY) {
        keys = pattern->data.destructuringArray.keys;
    } else {
        return ZR_TRUE;
    }

    for (TZrSize index = 0; keys != ZR_NULL && index < keys->count; index++) {
        if (!ct_prepare_build_fact_pattern_shadows(cs, keys->nodes[index])) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool ct_prepare_build_fact_decorators(
        SZrCompilerState *cs,
        SZrAstNodeArray *decorators) {
    return ct_prepare_build_fact_array(cs, decorators, ZR_FALSE);
}

static TZrBool ct_prepare_build_fact_parameter(
        SZrCompilerState *cs,
        SZrParameter *parameter,
        TZrBool declareShadow) {
    if (parameter == ZR_NULL ||
        !ct_prepare_build_fact_decorators(cs, parameter->decorators) ||
        !ct_prepare_build_fact_node(cs, parameter->defaultValue, ZR_TRUE)) {
        return parameter == ZR_NULL ? ZR_TRUE : ZR_FALSE;
    }
    return !declareShadow || parameter->name == ZR_NULL ||
           parameter->name->name == ZR_NULL ||
           ZrParser_CompileToolBinding_DeclareShadow(
                   cs, parameter->name->name);
}

static TZrBool ct_prepare_build_fact_parameters(
        SZrCompilerState *cs,
        SZrAstNodeArray *params,
        SZrParameter *args) {
    for (TZrSize index = 0; params != ZR_NULL && index < params->count; index++) {
        SZrAstNode *parameterNode = params->nodes[index];
        if (parameterNode != ZR_NULL &&
            parameterNode->type == ZR_AST_PARAMETER &&
            !ct_prepare_build_fact_parameter(
                    cs, &parameterNode->data.parameter, ZR_TRUE)) {
            return ZR_FALSE;
        }
    }
    return ct_prepare_build_fact_parameter(cs, args, ZR_TRUE);
}

static const SZrParserCompileToolModuleDescriptor *ct_compile_tool_import_descriptor(
        const SZrState *state,
        const SZrAstNode *node) {
    const SZrAstNode *modulePath;

    if (!compiler_is_compile_tool_import_declaration(state, node)) {
        return ZR_NULL;
    }

    modulePath = node->data.variableDeclaration.value->data.importExpression.modulePath;
    return ZrParser_CompileTool_FindModule(
            ZrCore_String_GetNativeString(modulePath->data.stringLiteral.value));
}

static TZrBool ct_prepare_build_fact_module_bindings(
        SZrCompilerState *cs,
        SZrAstNodeArray *nodes) {
    for (TZrSize index = 0; nodes != ZR_NULL && index < nodes->count; index++) {
        if (!ct_prepare_build_fact_predeclare_function_shadow(
                    cs, nodes->nodes[index])) {
            return ZR_FALSE;
        }
    }

    for (TZrSize index = 0; nodes != ZR_NULL && index < nodes->count; index++) {
        SZrAstNode *node = nodes->nodes[index];

        if (node == ZR_NULL) {
            continue;
        }
        if (compiler_is_compile_tool_import_declaration(cs->state, node)) {
            if (!ZrParser_CompileToolExecution_DeclareImport(cs, node)) {
                return ZR_FALSE;
            }
            continue;
        }
        if (node->type == ZR_AST_COMPILE_TIME_DECLARATION) {
            SZrCompileTimeDeclaration *declaration =
                    &node->data.compileTimeDeclaration;

            if (declaration->declarationType == ZR_COMPILE_TIME_FUNCTION) {
                if (!ZrParser_CompileTimeDeclaration_Execute(cs, node) ||
                    cs->hasCompileTimeError || cs->hasError ||
                    cs->hasFatalError) {
                    return ZR_FALSE;
                }
                continue;
            }
            if (!declaration->isConditionalPruning) {
                continue;
            }
            if (!declaration->buildFactsEvaluated) {
                if (!ZrParser_CompileTimeDeclaration_Execute(cs, node) ||
                    cs->hasCompileTimeError || cs->hasError ||
                    cs->hasFatalError) {
                    return ZR_FALSE;
                }
                declaration->buildFactsEvaluated = ZR_TRUE;
            }
            if (declaration->selectedBranch == ZR_NULL) {
                continue;
            }
            if (declaration->selectedBranch->type == ZR_AST_BLOCK) {
                if (!ct_prepare_build_fact_module_bindings(
                            cs,
                            declaration->selectedBranch->data.block.body)) {
                    return ZR_FALSE;
                }
            } else if (!ct_prepare_build_fact_predeclare_function_shadow(
                               cs, declaration->selectedBranch)) {
                return ZR_FALSE;
            }
        }
    }
    return ZR_TRUE;
}

static TZrBool ct_prepare_runtime_identifier(
        SZrCompilerState *cs,
        SZrAstNode *node,
        SZrString *name,
        TZrBool runtimeContext) {
    const SZrCompileToolBinding *binding;

    if (!runtimeContext || name == ZR_NULL) {
        return ZR_TRUE;
    }

    binding = ZrParser_CompileToolBinding_Resolve(cs, name);
    if (binding == ZR_NULL || binding->kind != ZR_COMPILE_TOOL_BINDING_PROVIDER) {
        return ZR_TRUE;
    }

    ZrParser_CompileTime_Error(cs,
                               ZR_COMPILE_TIME_ERROR_ERROR,
                               "compiletool.phase_mismatch: CompileTool binding cannot be used in runtime code",
                               node->location);
    return ZR_FALSE;
}

static TZrBool ct_prepare_function_body(
        SZrCompilerState *cs,
        SZrAstNodeArray *params,
        SZrParameter *args,
        SZrAstNodeArray *decorators,
        SZrAstNode *body) {
    TZrSize mark = ZrParser_CompileToolBinding_Mark(cs);
    TZrBool succeeded = ct_prepare_build_fact_decorators(cs, decorators) &&
                         ct_prepare_build_fact_parameters(cs, params, args) &&
                         ct_prepare_build_fact_node(cs, body, ZR_TRUE);
    ZrParser_CompileToolBinding_Restore(cs, mark);
    return succeeded;
}

static TZrBool ct_prepare_signature_parameters(
        SZrCompilerState *cs,
        SZrAstNodeArray *params,
        SZrParameter *args,
        SZrAstNodeArray *decorators) {
    TZrSize mark = ZrParser_CompileToolBinding_Mark(cs);
    TZrBool succeeded = ct_prepare_build_fact_decorators(cs, decorators) &&
                         ct_prepare_build_fact_parameters(cs, params, args);
    ZrParser_CompileToolBinding_Restore(cs, mark);
    return succeeded;
}

static TZrBool ct_prepare_build_fact_node(
        SZrCompilerState *cs,
        SZrAstNode *node,
        TZrBool runtimeContext) {
    return ct_prepare_build_fact_node_with_context(
            cs, node, runtimeContext, ZR_FALSE);
}

static TZrBool ct_prepare_build_fact_node_with_context(
        SZrCompilerState *cs,
        SZrAstNode *node,
        TZrBool runtimeContext,
        TZrBool moduleDeclarationList) {
    if (cs == ZR_NULL || node == ZR_NULL) {
        return ZR_TRUE;
    }

    if (node->type == ZR_AST_COMPILE_TIME_DECLARATION) {
        SZrCompileTimeDeclaration *declaration = &node->data.compileTimeDeclaration;
        if (!declaration->isConditionalPruning &&
            declaration->declarationType == ZR_COMPILE_TIME_STATEMENT) {
            if (!moduleDeclarationList) {
                ZrParser_CompileTime_Error(
                        cs,
                        ZR_COMPILE_TIME_ERROR_ERROR,
                        "comptime.module_scope_only: comptime blocks are only allowed at module scope",
                        node->location);
                return ZR_FALSE;
            }
            return ZR_TRUE;
        }

        if (declaration->declarationType == ZR_COMPILE_TIME_FUNCTION) {
            if (!ZrParser_CompileTimeDeclaration_Execute(cs, node) ||
                cs->hasCompileTimeError || cs->hasError || cs->hasFatalError) {
                return ZR_FALSE;
            }
            return ZR_TRUE;
        }

        if (!declaration->isConditionalPruning) {
            return ZR_TRUE;
        }

        if (!declaration->buildFactsEvaluated) {
            if (!ZrParser_CompileTimeDeclaration_Execute(cs, node) ||
                cs->hasCompileTimeError || cs->hasError || cs->hasFatalError) {
                return ZR_FALSE;
            }
            declaration->buildFactsEvaluated = ZR_TRUE;
        }

        if (declaration->selectedBranch == ZR_NULL) {
            return ZR_TRUE;
        }
        if (moduleDeclarationList &&
            declaration->selectedBranch->type == ZR_AST_BLOCK) {
            return ct_prepare_build_fact_module_array(
                    cs, declaration->selectedBranch->data.block.body);
        }
        return ct_prepare_build_fact_node_with_context(
                cs,
                declaration->selectedBranch,
                runtimeContext,
                moduleDeclarationList);
    }

    switch (node->type) {
        case ZR_AST_SCRIPT:
            return ct_prepare_build_fact_module_array(
                    cs, node->data.script.statements);
        case ZR_AST_BLOCK:
            return ct_prepare_build_fact_scoped_array(
                    cs,
                    node->data.block.body,
                    (TZrBool)(runtimeContext || moduleDeclarationList));
        case ZR_AST_FUNCTION_DECLARATION:
            return ct_prepare_function_body(
                    cs,
                    node->data.functionDeclaration.params,
                    node->data.functionDeclaration.args,
                    node->data.functionDeclaration.decorators,
                    node->data.functionDeclaration.body);
        case ZR_AST_CLASS_DECLARATION:
            return (TZrBool)(ct_prepare_build_fact_decorators(
                                     cs, node->data.classDeclaration.decorators) &&
                             ct_prepare_build_fact_array(
                                     cs, node->data.classDeclaration.members, ZR_FALSE));
        case ZR_AST_INTERFACE_DECLARATION:
            return ct_prepare_build_fact_array(
                    cs, node->data.interfaceDeclaration.members, ZR_FALSE);
        case ZR_AST_INTERFACE_METHOD_SIGNATURE:
            return ct_prepare_signature_parameters(
                    cs,
                    node->data.interfaceMethodSignature.params,
                    node->data.interfaceMethodSignature.args,
                    ZR_NULL);
        case ZR_AST_INTERFACE_META_SIGNATURE:
            return ct_prepare_signature_parameters(
                    cs,
                    node->data.interfaceMetaSignature.params,
                    node->data.interfaceMetaSignature.args,
                    ZR_NULL);
        case ZR_AST_EXTERN_BLOCK:
            return ct_prepare_build_fact_array(
                    cs, node->data.externBlock.declarations, ZR_FALSE);
        case ZR_AST_EXTERN_FUNCTION_DECLARATION:
            return ct_prepare_signature_parameters(
                    cs,
                    node->data.externFunctionDeclaration.params,
                    node->data.externFunctionDeclaration.args,
                    node->data.externFunctionDeclaration.decorators);
        case ZR_AST_EXTERN_DELEGATE_DECLARATION:
            return ct_prepare_signature_parameters(
                    cs,
                    node->data.externDelegateDeclaration.params,
                    node->data.externDelegateDeclaration.args,
                    node->data.externDelegateDeclaration.decorators);
        case ZR_AST_STRUCT_DECLARATION:
            return (TZrBool)(ct_prepare_build_fact_decorators(
                                     cs, node->data.structDeclaration.decorators) &&
                             ct_prepare_build_fact_array(
                                     cs, node->data.structDeclaration.members, ZR_FALSE));
        case ZR_AST_ENUM_DECLARATION:
            return (TZrBool)(ct_prepare_build_fact_decorators(
                                     cs, node->data.enumDeclaration.decorators) &&
                             ct_prepare_build_fact_array(
                                     cs, node->data.enumDeclaration.members, ZR_FALSE));
        case ZR_AST_ENUM_MEMBER:
            return (TZrBool)(ct_prepare_build_fact_decorators(
                                     cs, node->data.enumMember.decorators) &&
                             ct_prepare_build_fact_node(
                                     cs, node->data.enumMember.value, ZR_TRUE));
        case ZR_AST_UNION_DECLARATION:
            return (TZrBool)(ct_prepare_build_fact_decorators(
                                     cs, node->data.unionDeclaration.decorators) &&
                             ct_prepare_build_fact_array(
                                     cs, node->data.unionDeclaration.variants, ZR_FALSE));
        case ZR_AST_UNION_VARIANT: {
            TZrSize mark = ZrParser_CompileToolBinding_Mark(cs);
            TZrBool succeeded = ct_prepare_build_fact_decorators(
                                        cs, node->data.unionVariant.decorators) &&
                                ct_prepare_build_fact_parameters(
                                        cs, node->data.unionVariant.fields, ZR_NULL);
            ZrParser_CompileToolBinding_Restore(cs, mark);
            return succeeded;
        }
        case ZR_AST_CLASS_METHOD:
            return ct_prepare_function_body(
                    cs,
                    node->data.classMethod.params,
                    node->data.classMethod.args,
                    node->data.classMethod.decorators,
                    node->data.classMethod.body);
        case ZR_AST_CLASS_META_FUNCTION:
            return ct_prepare_function_body(
                    cs,
                    node->data.classMetaFunction.params,
                    node->data.classMetaFunction.args,
                    ZR_NULL,
                    node->data.classMetaFunction.body);
        case ZR_AST_STRUCT_METHOD:
            return ct_prepare_function_body(
                    cs,
                    node->data.structMethod.params,
                    node->data.structMethod.args,
                    node->data.structMethod.decorators,
                    node->data.structMethod.body);
        case ZR_AST_STRUCT_META_FUNCTION:
            return ct_prepare_function_body(
                    cs,
                    node->data.structMetaFunction.params,
                    node->data.structMetaFunction.args,
                    ZR_NULL,
                    node->data.structMetaFunction.body);
        case ZR_AST_CLASS_FIELD:
            return (TZrBool)(ct_prepare_build_fact_decorators(
                                     cs, node->data.classField.decorators) &&
                             ct_prepare_build_fact_node(
                                     cs, node->data.classField.init, ZR_TRUE));
        case ZR_AST_STRUCT_FIELD:
            return (TZrBool)(ct_prepare_build_fact_decorators(
                                     cs, node->data.structField.decorators) &&
                             ct_prepare_build_fact_node(
                                     cs, node->data.structField.init, ZR_TRUE));
        case ZR_AST_CLASS_PROPERTY:
            return (TZrBool)(ct_prepare_build_fact_decorators(
                                     cs, node->data.classProperty.decorators) &&
                             ct_prepare_build_fact_node(
                                     cs, node->data.classProperty.modifier, ZR_FALSE));
        case ZR_AST_PROPERTY_GET:
            return ct_prepare_build_fact_node(
                    cs, node->data.propertyGet.body, ZR_TRUE);
        case ZR_AST_PROPERTY_SET: {
            TZrSize mark = ZrParser_CompileToolBinding_Mark(cs);
            SZrIdentifier *parameter = node->data.propertySet.param;
            TZrBool succeeded =
                    (parameter == ZR_NULL || parameter->name == ZR_NULL ||
                     ZrParser_CompileToolBinding_DeclareShadow(
                             cs, parameter->name)) &&
                    ct_prepare_build_fact_node(
                            cs, node->data.propertySet.body, ZR_TRUE);
            ZrParser_CompileToolBinding_Restore(cs, mark);
            return succeeded;
        }
        case ZR_AST_PROPERTY_DECLARATION:
            return (TZrBool)(ct_prepare_build_fact_decorators(
                                     cs, node->data.propertyDeclaration.decorators) &&
                             ct_prepare_build_fact_array(
                                     cs,
                                     node->data.propertyDeclaration.accessors,
                                     ZR_FALSE));
        case ZR_AST_PROPERTY_ACCESSOR:
            return ct_prepare_build_fact_node(cs, node->data.propertyAccessor.body, ZR_TRUE);
        case ZR_AST_LAMBDA_EXPRESSION:
            return ct_prepare_function_body(
                    cs,
                    node->data.lambdaExpression.params,
                    node->data.lambdaExpression.args,
                    ZR_NULL,
                    node->data.lambdaExpression.block);
        case ZR_AST_VARIABLE_DECLARATION:
            if (compiler_is_compile_tool_import_declaration(cs->state, node)) {
                SZrString *aliasName = node->data.variableDeclaration.pattern->data.identifier.name;
                if (moduleDeclarationList) {
                    const SZrCompileToolBinding *existing =
                            ZrParser_CompileToolBinding_Resolve(cs, aliasName);
                    if (existing != ZR_NULL &&
                        existing->kind == ZR_COMPILE_TOOL_BINDING_SHADOW) {
                        return ZR_TRUE;
                    }
                    return ZrParser_CompileToolExecution_DeclareImport(cs, node);
                }
                ZrParser_CompileTime_Error(cs,
                                           ZR_COMPILE_TIME_ERROR_ERROR,
                                           "compiletool.phase_mismatch: CompileTool imports cannot be bound in runtime code",
                                           node->location);
                return ZR_FALSE;
            }
            if (!ct_prepare_build_fact_node(cs, node->data.variableDeclaration.value, runtimeContext)) {
                return ZR_FALSE;
            }
            return ct_prepare_build_fact_pattern_shadows(
                    cs, node->data.variableDeclaration.pattern);
        case ZR_AST_EXPRESSION_STATEMENT:
            return ct_prepare_build_fact_node(cs, node->data.expressionStatement.expr, runtimeContext);
        case ZR_AST_RETURN_STATEMENT:
            return ct_prepare_build_fact_node(cs, node->data.returnStatement.expr, runtimeContext);
        case ZR_AST_THROW_STATEMENT:
            return ct_prepare_build_fact_node(cs, node->data.throwStatement.expr, runtimeContext);
        case ZR_AST_OUT_STATEMENT:
            return ct_prepare_build_fact_node(cs, node->data.outStatement.expr, runtimeContext);
        case ZR_AST_YIELD_STATEMENT:
            return ct_prepare_build_fact_node(cs, node->data.yieldStatement.expr, runtimeContext);
        case ZR_AST_BREAK_CONTINUE_STATEMENT:
            return ct_prepare_build_fact_node(cs, node->data.breakContinueStatement.expr, runtimeContext);
        case ZR_AST_IDENTIFIER_LITERAL:
            return ct_prepare_runtime_identifier(cs, node, node->data.identifier.name, runtimeContext);
        case ZR_AST_ASSIGNMENT_EXPRESSION:
            return (TZrBool)(ct_prepare_build_fact_node(cs, node->data.assignmentExpression.left, runtimeContext) &&
                             ct_prepare_build_fact_node(cs, node->data.assignmentExpression.right, runtimeContext));
        case ZR_AST_BINARY_EXPRESSION:
            return (TZrBool)(ct_prepare_build_fact_node(cs, node->data.binaryExpression.left, runtimeContext) &&
                             ct_prepare_build_fact_node(cs, node->data.binaryExpression.right, runtimeContext));
        case ZR_AST_LOGICAL_EXPRESSION:
            return (TZrBool)(ct_prepare_build_fact_node(cs, node->data.logicalExpression.left, runtimeContext) &&
                             ct_prepare_build_fact_node(cs, node->data.logicalExpression.right, runtimeContext));
        case ZR_AST_CONDITIONAL_EXPRESSION:
            return (TZrBool)(ct_prepare_build_fact_node(cs, node->data.conditionalExpression.test, runtimeContext) &&
                             ct_prepare_build_fact_node(cs, node->data.conditionalExpression.consequent, runtimeContext) &&
                             ct_prepare_build_fact_node(cs, node->data.conditionalExpression.alternate, runtimeContext));
        case ZR_AST_UNARY_EXPRESSION:
            return ct_prepare_build_fact_node(cs, node->data.unaryExpression.argument, runtimeContext);
        case ZR_AST_AWAIT_EXPRESSION:
            return ct_prepare_build_fact_node(cs, node->data.awaitExpression.operand, runtimeContext);
        case ZR_AST_TYPE_CAST_EXPRESSION:
            return ct_prepare_build_fact_node(cs, node->data.typeCastExpression.expression, runtimeContext);
        case ZR_AST_FUNCTION_CALL:
            return (TZrBool)(ct_prepare_build_fact_array(
                                     cs,
                                     node->data.functionCall.genericArguments,
                                     runtimeContext) &&
                             ct_prepare_build_fact_array(
                                     cs, node->data.functionCall.args, runtimeContext));
        case ZR_AST_SPREAD_ARGUMENT:
            return ct_prepare_build_fact_node(cs, node->data.spreadArgument.expression, runtimeContext);
        case ZR_AST_MEMBER_EXPRESSION:
            return node->data.memberExpression.computed
                       ? ct_prepare_build_fact_node(cs, node->data.memberExpression.property, runtimeContext)
                       : ZR_TRUE;
        case ZR_AST_PRIMARY_EXPRESSION:
            if (node->data.primaryExpression.property != ZR_NULL &&
                node->data.primaryExpression.property->type == ZR_AST_IDENTIFIER_LITERAL &&
                !ct_prepare_runtime_identifier(
                        cs,
                        node,
                        node->data.primaryExpression.property->data.identifier.name,
                        runtimeContext)) {
                return ZR_FALSE;
            }
            return ct_prepare_build_fact_array(cs, node->data.primaryExpression.members, runtimeContext);
        case ZR_AST_TYPE_QUERY_EXPRESSION:
            return ct_prepare_build_fact_node(cs, node->data.typeQueryExpression.operand, runtimeContext);
        case ZR_AST_PROTOTYPE_REFERENCE_EXPRESSION:
            return ct_prepare_build_fact_node(cs, node->data.prototypeReferenceExpression.target, runtimeContext);
        case ZR_AST_CONSTRUCT_EXPRESSION:
            return (TZrBool)(ct_prepare_build_fact_node(cs, node->data.constructExpression.target, runtimeContext) &&
                             ct_prepare_build_fact_array(cs, node->data.constructExpression.args, runtimeContext));
        case ZR_AST_STRUCT_INIT_EXPRESSION:
            return ct_prepare_build_fact_array(cs, node->data.structInitExpression.args, runtimeContext);
        case ZR_AST_TEMPLATE_STRING_LITERAL:
            return ct_prepare_build_fact_array(cs, node->data.templateStringLiteral.segments, runtimeContext);
        case ZR_AST_INTERPOLATED_SEGMENT:
            return ct_prepare_build_fact_node(cs, node->data.interpolatedSegment.expression, runtimeContext);
        case ZR_AST_ARRAY_LITERAL:
            return ct_prepare_build_fact_array(cs, node->data.arrayLiteral.elements, runtimeContext);
        case ZR_AST_OBJECT_LITERAL:
            return ct_prepare_build_fact_array(cs, node->data.objectLiteral.properties, runtimeContext);
        case ZR_AST_KEY_VALUE_PAIR:
            return (TZrBool)(ct_prepare_build_fact_node(cs, node->data.keyValuePair.key, runtimeContext) &&
                              ct_prepare_build_fact_node(cs, node->data.keyValuePair.value, runtimeContext));
        case ZR_AST_DECORATOR_EXPRESSION:
            return ct_prepare_build_fact_node(
                    cs, node->data.decoratorExpression.expr, ZR_FALSE);
        case ZR_AST_PARAMETER:
            return ct_prepare_build_fact_parameter(
                    cs, &node->data.parameter, ZR_FALSE);
        case ZR_AST_UNPACK_LITERAL:
            return ct_prepare_build_fact_node(cs, node->data.unpackLiteral.element, runtimeContext);
        case ZR_AST_GENERATOR_EXPRESSION:
            return ct_prepare_build_fact_node(cs, node->data.generatorExpression.block, ZR_TRUE);
        case ZR_AST_IF_EXPRESSION:
            return (TZrBool)(ct_prepare_build_fact_node(cs, node->data.ifExpression.condition, runtimeContext) &&
                             ct_prepare_build_fact_node(cs, node->data.ifExpression.thenExpr, runtimeContext) &&
                             ct_prepare_build_fact_node(cs, node->data.ifExpression.elseExpr, runtimeContext));
        case ZR_AST_SWITCH_EXPRESSION:
            return (TZrBool)(ct_prepare_build_fact_node(cs, node->data.switchExpression.expr, runtimeContext) &&
                             ct_prepare_build_fact_array(cs, node->data.switchExpression.cases, runtimeContext) &&
                             ct_prepare_build_fact_node(cs, node->data.switchExpression.defaultCase, runtimeContext));
        case ZR_AST_SWITCH_CASE:
            return (TZrBool)(ct_prepare_build_fact_node(cs, node->data.switchCase.value, runtimeContext) &&
                             ct_prepare_build_fact_node(cs, node->data.switchCase.block, runtimeContext));
        case ZR_AST_SWITCH_DEFAULT:
            return ct_prepare_build_fact_node(cs, node->data.switchDefault.block, runtimeContext);
        case ZR_AST_WHILE_LOOP:
            return (TZrBool)(ct_prepare_build_fact_node(cs, node->data.whileLoop.cond, runtimeContext) &&
                             ct_prepare_build_fact_node(cs, node->data.whileLoop.block, runtimeContext));
        case ZR_AST_FOR_LOOP: {
            TZrSize mark = ZrParser_CompileToolBinding_Mark(cs);
            TZrBool succeeded =
                    ct_prepare_build_fact_node(
                            cs, node->data.forLoop.init, runtimeContext) &&
                    ct_prepare_build_fact_node(
                            cs, node->data.forLoop.cond, runtimeContext) &&
                    ct_prepare_build_fact_node(
                            cs, node->data.forLoop.step, runtimeContext) &&
                    ct_prepare_build_fact_node(
                            cs, node->data.forLoop.block, runtimeContext);
            ZrParser_CompileToolBinding_Restore(cs, mark);
            return succeeded;
        }
        case ZR_AST_FOREACH_LOOP: {
            TZrSize mark;
            TZrBool succeeded;
            if (!ct_prepare_build_fact_node(
                        cs, node->data.foreachLoop.expr, runtimeContext)) {
                return ZR_FALSE;
            }
            mark = ZrParser_CompileToolBinding_Mark(cs);
            succeeded = ct_prepare_build_fact_pattern_shadows(
                                cs, node->data.foreachLoop.pattern) &&
                        ct_prepare_build_fact_node(
                                cs, node->data.foreachLoop.block, runtimeContext);
            ZrParser_CompileToolBinding_Restore(cs, mark);
            return succeeded;
        }
        case ZR_AST_USING_STATEMENT: {
            TZrSize mark;
            TZrBool succeeded;
            if (!ct_prepare_build_fact_node(
                        cs, node->data.usingStatement.resource, runtimeContext)) {
                return ZR_FALSE;
            }
            mark = ZrParser_CompileToolBinding_Mark(cs);
            succeeded = ct_prepare_build_fact_pattern_shadows(
                                cs, node->data.usingStatement.pattern) &&
                        ct_prepare_build_fact_node(
                                cs, node->data.usingStatement.body, runtimeContext);
            ZrParser_CompileToolBinding_Restore(cs, mark);
            return succeeded &&
                   ct_prepare_build_fact_node(
                           cs, node->data.usingStatement.elseBody, runtimeContext);
        }
        case ZR_AST_TRY_CATCH_FINALLY_STATEMENT:
            return (TZrBool)(ct_prepare_build_fact_node(cs, node->data.tryCatchFinallyStatement.block, runtimeContext) &&
                             ct_prepare_build_fact_array(cs, node->data.tryCatchFinallyStatement.catchClauses, runtimeContext) &&
                             ct_prepare_build_fact_node(cs, node->data.tryCatchFinallyStatement.finallyBlock, runtimeContext));
        case ZR_AST_CATCH_CLAUSE: {
            TZrSize mark = ZrParser_CompileToolBinding_Mark(cs);
            TZrBool succeeded = ct_prepare_build_fact_parameters(
                                        cs,
                                        node->data.catchClause.pattern,
                                        ZR_NULL) &&
                                ct_prepare_build_fact_node(
                                        cs,
                                        node->data.catchClause.block,
                                        runtimeContext);
            ZrParser_CompileToolBinding_Restore(cs, mark);
            return succeeded;
        }
        default:
            break;
    }

    return ZR_TRUE;
}

TZrBool ZrParser_CompileTime_PrepareBuildFacts(SZrState *state, SZrAstNode *ast) {
    return ZrParser_CompileTime_PrepareBuildFactsWithCache(
            state, ast, ZR_NULL, ZR_NULL, 0U);
}

TZrBool ZrParser_CompileTime_PrepareBuildFactsWithCache(
        SZrState *state,
        SZrAstNode *ast,
        SZrParserSourceComptimeCache *cache,
        const TZrChar *source,
        TZrSize sourceLength) {
    SZrCompilerState cs;
    TZrBool succeeded;
    const TZrByte *inputSnapshot = ZR_NULL;
    TZrSize inputSnapshotSize = 0U;
    TZrBool hasInputSnapshot = ZR_FALSE;

    if (state == ZR_NULL || ast == ZR_NULL || ast->type != ZR_AST_SCRIPT) {
        return ZR_FALSE;
    }

    if (cache != ZR_NULL) {
        inputSnapshot = cache->inputSnapshot;
        inputSnapshotSize = cache->inputSnapshotSize;
        hasInputSnapshot = (TZrBool)(inputSnapshot != ZR_NULL || inputSnapshotSize != 0U);
        cache->outputSnapshot = ZR_NULL;
        cache->outputSnapshotSize = 0U;
        cache->inputSnapshotAccepted = ZR_FALSE;
        cache->hitCount = 0U;
        cache->missCount = 0U;
    }

    ZrParser_CompilerState_Init(&cs, state);
    if (source != ZR_NULL && sourceLength > 0U) {
        SZrParserSha256Context sourceHash;

        ZrParser_Sha256_Init(&sourceHash);
        if (ZrParser_Sha256_Update(
                    &sourceHash, (const TZrByte *)source, sourceLength)) {
            ZrParser_Sha256_Final(
                    &sourceHash, cs.comptimeSourceDigest);
            cs.hasComptimeSourceDigest = ZR_TRUE;
        }
    }
    if (hasInputSnapshot) {
        cache->inputSnapshotAccepted = ZrParser_ComptimeCache_ImportSnapshot(
                &cs, inputSnapshot, inputSnapshotSize);
    }
    succeeded = ZrParser_CompileTime_PrepareBuildFactsInCompilerState(&cs, ast);

    if (cache != ZR_NULL) {
        cache->hitCount = cs.comptimeCacheHitCount;
        cache->missCount = cs.comptimeCacheMissCount;
        if (succeeded && !ZrParser_ComptimeCache_ExportSnapshot(
                                 &cs, &cache->outputSnapshot, &cache->outputSnapshotSize)) {
            succeeded = ZR_FALSE;
        }
    }

    ZrParser_CompilerState_Free(&cs);
    return succeeded;
}

TZrBool ZrParser_CompileTime_PrepareBuildFactsInCompilerState(SZrCompilerState *cs, SZrAstNode *ast) {
    if (cs == ZR_NULL || ast == ZR_NULL || ast->type != ZR_AST_SCRIPT) {
        return ZR_FALSE;
    }

    cs->currentAst = ast;
    cs->scriptAst = ast;
    ZrParser_CompileToolBinding_Reset(cs);
    return ct_prepare_build_fact_module_array(cs, ast->data.script.statements);
}

static TZrBool ct_eval_object_literal(SZrCompilerState *cs,
                                    SZrAstNode *node,
                                    SZrCompileTimeFrame *frame,
                                    SZrTypeValue *result);
static TZrBool ct_eval_assignment(SZrCompilerState *cs,
                                SZrAstNode *node,
                                SZrCompileTimeFrame *frame,
                                SZrTypeValue *result);
static TZrBool ct_eval_runtime_projected_call_arg(SZrCompilerState *cs,
                                                  SZrCompileTimeFunction *func,
                                                  SZrFunctionCall *call,
                                                  SZrString *paramName,
                                                  TZrSize paramIndex,
                                                  SZrCompileTimeFrame *frame,
                                                  SZrTypeValue *result);
static TZrBool ct_invoke_runtime_callable_with_values(SZrCompilerState *cs,
                                                      SZrAstNode *callSite,
                                                      const SZrTypeValue *callableValue,
                                                      TZrSize argCount,
                                                      const SZrTypeValue *argValues,
                                                      SZrTypeValue *result);
static TZrBool ct_call_runtime_projected_compile_time_function(SZrCompilerState *cs,
                                                               SZrAstNode *callSite,
                                                               SZrCompileTimeFunction *func,
                                                               SZrFunctionCall *call,
                                                               SZrCompileTimeFrame *frame,
                                                               SZrTypeValue *result);
static TZrBool ct_eval_member_access(SZrCompilerState *cs,
                                   SZrAstNode *callSite,
                                   const SZrTypeValue *baseValue,
                                   SZrMemberExpression *memberExpr,
                                   SZrCompileTimeFrame *frame,
                                   SZrTypeValue *result);
static TZrBool ct_call_value(SZrCompilerState *cs,
                           SZrAstNode *callSite,
                           const SZrTypeValue *callableValue,
                           SZrFunctionCall *call,
                           SZrCompileTimeFrame *frame,
                           SZrTypeValue *result);
static TZrBool ct_eval_call_arg(SZrCompilerState *cs,
                              SZrFunctionCall *call,
                              SZrParameter *param,
                              TZrSize paramIndex,
                              SZrCompileTimeFrame *frame,
                              SZrTypeValue *result);

static TZrBool ct_make_string_value(SZrState *state, const TZrChar *text, SZrTypeValue *result) {
    SZrString *stringValue;

    if (state == ZR_NULL || text == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    stringValue = ZrCore_String_Create(state, (TZrNativeString)text, strlen(text));
    if (stringValue == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(state, result, ZR_CAST_RAW_OBJECT_AS_SUPER(stringValue));
    result->type = ZR_VALUE_TYPE_STRING;
    return ZR_TRUE;
}

static TZrBool ct_set_object_field_value(SZrState *state,
                                         SZrObject *object,
                                         const TZrChar *fieldName,
                                         const SZrTypeValue *value) {
    SZrTypeValue key;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ct_make_string_value(state, fieldName, &key)) {
        return ZR_FALSE;
    }

    ZrCore_Object_SetValue(state, object, &key, value);
    return ZR_TRUE;
}

static TZrBool ct_set_object_field_string(SZrState *state,
                                          SZrObject *object,
                                          const TZrChar *fieldName,
                                          const TZrChar *valueText) {
    SZrTypeValue value;

    if (!ct_make_string_value(state, valueText != ZR_NULL ? valueText : "", &value)) {
        return ZR_FALSE;
    }

    return ct_set_object_field_value(state, object, fieldName, &value);
}

static TZrBool ct_set_object_field_bool(SZrState *state,
                                        SZrObject *object,
                                        const TZrChar *fieldName,
                                        TZrBool valueBool) {
    SZrTypeValue value;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsUInt(state, &value, valueBool ? 1u : 0u);
    value.type = ZR_VALUE_TYPE_BOOL;
    return ct_set_object_field_value(state, object, fieldName, &value);
}

static TZrBool ct_set_object_field_object(SZrState *state,
                                          SZrObject *object,
                                          const TZrChar *fieldName,
                                          SZrObject *fieldObject,
                                          EZrValueType valueType) {
    SZrTypeValue value;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL || fieldObject == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(state, &value, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldObject));
    value.type = valueType;
    return ct_set_object_field_value(state, object, fieldName, &value);
}

static SZrString *ct_decorator_target_type_name(SZrType *typeInfo) {
    while (typeInfo != ZR_NULL && typeInfo->subType != ZR_NULL) {
        typeInfo = typeInfo->subType;
    }
    if (typeInfo == ZR_NULL || typeInfo->name == ZR_NULL) {
        return ZR_NULL;
    }

    if (typeInfo->name->type == ZR_AST_IDENTIFIER_LITERAL) {
        return typeInfo->name->data.identifier.name;
    }

    if (typeInfo->name->type == ZR_AST_GENERIC_TYPE &&
        typeInfo->name->data.genericType.name != ZR_NULL) {
        return typeInfo->name->data.genericType.name->name;
    }

    return ZR_NULL;
}

static const TZrChar *ct_expected_type_decorator_target_name(EZrObjectPrototypeType targetType) {
    switch (targetType) {
        case ZR_OBJECT_PROTOTYPE_TYPE_CLASS:
            return "Class";
        case ZR_OBJECT_PROTOTYPE_TYPE_STRUCT:
            return "Struct";
        default:
            return "TypeView";
    }
}

static const TZrChar *ct_module_name_text(SZrCompilerState *cs) {
    SZrAstNode *moduleNode;
    SZrAstNode *nameNode;

    if (cs != ZR_NULL && cs->currentModuleKey != ZR_NULL) {
        return ZrCore_String_GetNativeString(cs->currentModuleKey);
    }

    if (cs == ZR_NULL ||
        cs->scriptAst == ZR_NULL ||
        cs->scriptAst->type != ZR_AST_SCRIPT ||
        cs->scriptAst->data.script.moduleName == ZR_NULL) {
        return ZR_NULL;
    }

    moduleNode = cs->scriptAst->data.script.moduleName;
    if (moduleNode->type != ZR_AST_MODULE_DECLARATION || moduleNode->data.moduleDeclaration.name == ZR_NULL) {
        return ZR_NULL;
    }

    nameNode = moduleNode->data.moduleDeclaration.name;
    if (nameNode->type != ZR_AST_STRING_LITERAL || nameNode->data.stringLiteral.value == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_String_GetNativeString(nameNode->data.stringLiteral.value);
}

static TZrBool ct_validate_named_decorator_target_param(SZrCompilerState *cs,
                                                        SZrParameter *param,
                                                        const TZrChar *expectedName,
                                                        const TZrChar *decoratorKind,
                                                        SZrFileRange location) {
    SZrString *typeName;

    if (cs == ZR_NULL || param == ZR_NULL) {
        return ZR_FALSE;
    }

    typeName = ct_decorator_target_type_name(param->typeInfo);
    if (typeName != ZR_NULL &&
        ((expectedName != ZR_NULL && ct_string_equals(typeName, expectedName)) ||
         ct_string_equals(typeName, "DeclarationView") ||
         ((expectedName != ZR_NULL &&
           (strcmp(expectedName, "Class") == 0 ||
            strcmp(expectedName, "Struct") == 0 ||
            strcmp(expectedName, "TypeView") == 0)) &&
          ct_string_equals(typeName, "TypeView")))) {
        return ZR_TRUE;
    }

    ZrParser_CompileTime_Error(cs,
                               ZR_COMPILE_TIME_ERROR_ERROR,
                               decoratorKind != ZR_NULL
                                       ? decoratorKind
                                       : "Compile-time decorator target parameter must use a canonical declaration view",
                               location);
    return ZR_FALSE;
}

static SZrObject *ct_new_object(SZrCompilerState *cs) {
    SZrObject *object;

    if (cs == ZR_NULL || cs->state == ZR_NULL ||
        !ZrParser_ComptimeRuntime_Consume(
                cs,
                ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES,
                sizeof(SZrObject),
                (SZrFileRange){{0, 0, 0}, {0, 0, 0}, ZR_NULL}) ||
        !ZrParser_ComptimeRuntime_Consume(
                cs,
                ZR_PARSER_COMPTIME_BUDGET_AGGREGATE_COUNT,
                1U,
                (SZrFileRange){{0, 0, 0}, {0, 0, 0}, ZR_NULL})) {
        return ZR_NULL;
    }

    object = ZrCore_Object_New(cs->state, ZR_NULL);
    if (object != ZR_NULL) {
        ZrCore_Object_Init(cs->state, object);
    }
    return object;
}

static SZrObject *ct_new_array(SZrCompilerState *cs) {
    SZrObject *array;

    if (cs == ZR_NULL || cs->state == ZR_NULL ||
        !ZrParser_ComptimeRuntime_Consume(
                cs,
                ZR_PARSER_COMPTIME_BUDGET_HEAP_BYTES,
                sizeof(SZrObject),
                (SZrFileRange){{0, 0, 0}, {0, 0, 0}, ZR_NULL}) ||
        !ZrParser_ComptimeRuntime_Consume(
                cs,
                ZR_PARSER_COMPTIME_BUDGET_AGGREGATE_COUNT,
                1U,
                (SZrFileRange){{0, 0, 0}, {0, 0, 0}, ZR_NULL})) {
        return ZR_NULL;
    }

    array = ZrCore_Object_NewCustomized(
            cs->state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_ARRAY);
    if (array != ZR_NULL) {
        ZrCore_Object_Init(cs->state, array);
    }
    return array;
}

typedef struct SZrResolvedCompileTimeDecoratorBinding {
    SZrString *name;
    SZrCompileTimeFunction *decoratorFunction;
    SZrFunctionCall *constructorCall;
} SZrResolvedCompileTimeDecoratorBinding;

static SZrImportedCompileTimeModule *ct_find_imported_compile_time_module_alias(SZrCompilerState *cs,
                                                                                SZrString *aliasName) {
    if (cs == ZR_NULL || aliasName == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = cs->importedCompileTimeModuleAliases.length;
         index > 0U;
         index--) {
        SZrImportedCompileTimeModuleAlias *alias =
                (SZrImportedCompileTimeModuleAlias *)ZrCore_Array_Get(
                        &cs->importedCompileTimeModuleAliases,
                        index - 1U);
        if (alias != ZR_NULL && alias->aliasName != ZR_NULL && alias->module != ZR_NULL &&
            ZrCore_String_Equal(alias->aliasName, aliasName)) {
            return alias->module;
        }
    }

    return ZR_NULL;
}

static SZrCompileTimeFunction *ct_find_imported_compile_time_function(const SZrImportedCompileTimeModule *module,
                                                                      SZrString *name) {
    if (module == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < module->compileTimeFunctions.length; index++) {
        SZrCompileTimeFunction **funcPtr =
                (SZrCompileTimeFunction **)ZrCore_Array_Get((SZrArray *)&module->compileTimeFunctions, index);
        if (funcPtr != ZR_NULL && *funcPtr != ZR_NULL && (*funcPtr)->name != ZR_NULL &&
            (*funcPtr)->isExported &&
            ZrCore_String_Equal((*funcPtr)->name, name)) {
            return *funcPtr;
        }
    }

    return ZR_NULL;
}

static SZrFunctionCompileTimeVariableInfo *ct_find_imported_compile_time_variable(
        const SZrImportedCompileTimeModule *module,
        SZrString *name) {
    if (module == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < module->compileTimeVariables.length; index++) {
        SZrFunctionCompileTimeVariableInfo **infoPtr =
                (SZrFunctionCompileTimeVariableInfo **)ZrCore_Array_Get((SZrArray *)&module->compileTimeVariables, index);
        if (infoPtr != ZR_NULL && *infoPtr != ZR_NULL && (*infoPtr)->name != ZR_NULL &&
            ZrCore_String_Equal((*infoPtr)->name, name)) {
            return *infoPtr;
        }
    }

    return ZR_NULL;
}

static TZrBool ct_try_get_static_decorator_member_name(SZrCompilerState *cs,
                                                       SZrAstNode *decoratorNode,
                                                       SZrAstNode *memberNode,
                                                       SZrString **outName) {
    if (outName != ZR_NULL) {
        *outName = ZR_NULL;
    }

    if (cs == ZR_NULL || decoratorNode == ZR_NULL || memberNode == ZR_NULL ||
        memberNode->type != ZR_AST_MEMBER_EXPRESSION) {
        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                   "Compile-time decorators only support identifier(.identifier)* with an optional final call",
                                   decoratorNode != ZR_NULL ? decoratorNode->location
                                                            : (SZrFileRange){{0, 0, 0}, {0, 0, 0}, ZR_NULL});
        return ZR_FALSE;
    }

    if (memberNode->data.memberExpression.computed ||
        memberNode->data.memberExpression.property == ZR_NULL ||
        memberNode->data.memberExpression.property->type != ZR_AST_IDENTIFIER_LITERAL ||
        memberNode->data.memberExpression.property->data.identifier.name == ZR_NULL) {
        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                   "Compile-time decorators only support identifier(.identifier)* with an optional final call",
                                   decoratorNode->location);
        return ZR_FALSE;
    }

    if (outName != ZR_NULL) {
        *outName = memberNode->data.memberExpression.property->data.identifier.name;
    }
    return ZR_TRUE;
}

static TZrBool ct_try_resolve_compile_time_function_from_value(SZrCompilerState *cs,
                                                               const SZrTypeValue *value,
                                                               SZrCompileTimeFunction **outFunction) {
    if (outFunction != ZR_NULL) {
        *outFunction = ZR_NULL;
    }

    if (cs == ZR_NULL || value == ZR_NULL) {
        return ZR_FALSE;
    }

    if (outFunction != ZR_NULL && ct_value_try_get_compile_time_function(cs, value, outFunction)) {
        return ZR_TRUE;
    }

    for (TZrSize index = 0; index < cs->compileTimeFunctions.length; index++) {
        SZrCompileTimeFunction **funcPtr =
                (SZrCompileTimeFunction **)ZrCore_Array_Get(&cs->compileTimeFunctions, index);
        SZrTypeValue projectedValue;

        if (funcPtr == ZR_NULL || *funcPtr == ZR_NULL) {
            continue;
        }

        if (!ct_value_from_compile_time_function(cs, *funcPtr, &projectedValue)) {
            continue;
        }

        if (ZrCore_Value_Equal(cs->state, &projectedValue, (SZrTypeValue *)value)) {
            if (outFunction != ZR_NULL) {
                *outFunction = *funcPtr;
            }
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool ct_try_resolve_decorator_root_value(SZrCompilerState *cs,
                                                   SZrString *rootName,
                                                   SZrTypeValue *outValue) {
    SZrCompileTimeFunction *rootFunction = ZR_NULL;
    SZrImportedCompileTimeModule *importedModule;
    SZrObjectModule *moduleObject;

    if (cs == ZR_NULL || rootName == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZrParser_Compiler_TryGetCompileTimeValue(cs, rootName, outValue)) {
        return ZR_TRUE;
    }

    rootFunction = find_compile_time_function(cs, rootName);
    if (rootFunction != ZR_NULL && ct_value_from_compile_time_function(cs, rootFunction, outValue)) {
        return ZR_TRUE;
    }

    importedModule = ct_find_imported_compile_time_module_alias(cs, rootName);
    if (importedModule == ZR_NULL || importedModule->moduleName == ZR_NULL) {
        return ZR_FALSE;
    }

    moduleObject = ZrCore_Module_ImportByPath(cs->state, importedModule->moduleName);
    if (moduleObject == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(cs->state, outValue, ZR_CAST_RAW_OBJECT_AS_SUPER(moduleObject));
    outValue->type = ZR_VALUE_TYPE_OBJECT;
    return ZR_TRUE;
}

static TZrBool ct_resolve_compile_time_decorator_binding(
        SZrCompilerState *cs,
        SZrAstNode *decoratorNode,
        SZrResolvedCompileTimeDecoratorBinding *binding) {
    SZrAstNode *expr;
    SZrPrimaryExpression *primary = ZR_NULL;
    SZrString *rootName = ZR_NULL;
    SZrString *leafName = ZR_NULL;
    SZrImportedCompileTimeModule *importedModule = ZR_NULL;
    TZrSize memberCount = 0;
    TZrSize chainCount = 0;
    TZrBool hasValueChain = ZR_FALSE;
    SZrTypeValue currentValue;

    if (binding != ZR_NULL) {
        ZrCore_Memory_RawSet(binding, 0, sizeof(*binding));
    }

    if (cs == ZR_NULL || decoratorNode == ZR_NULL || decoratorNode->type != ZR_AST_DECORATOR_EXPRESSION) {
        return ZR_FALSE;
    }

    expr = decoratorNode->data.decoratorExpression.expr;
    if (expr == ZR_NULL) {
        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                       "Compile-time decorator expression is empty",
                                   decoratorNode->location);
        return ZR_FALSE;
    }

    if (expr->type == ZR_AST_IDENTIFIER_LITERAL) {
        rootName = expr->data.identifier.name;
        if (rootName == ZR_NULL) {
            ZrParser_CompileTime_Error(cs,
                                       ZR_COMPILE_TIME_ERROR_ERROR,
                                       "Compile-time decorators require a named identifier path",
                                       decoratorNode->location);
            return ZR_FALSE;
        }

        if (binding != ZR_NULL) {
            binding->name = rootName;
            binding->decoratorFunction = find_compile_time_function(cs, rootName);
            return binding->decoratorFunction != ZR_NULL ? ZR_TRUE : ZR_FALSE;
        }
        return find_compile_time_function(cs, rootName) != ZR_NULL ? ZR_TRUE : ZR_FALSE;
    }

    if (expr->type != ZR_AST_PRIMARY_EXPRESSION) {
        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                   "Compile-time decorators only support identifier(.identifier)* with an optional final call",
                                   decoratorNode->location);
        return ZR_FALSE;
    }

    primary = &expr->data.primaryExpression;
    if (primary->property == ZR_NULL || primary->property->type != ZR_AST_IDENTIFIER_LITERAL ||
        primary->property->data.identifier.name == ZR_NULL) {
        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                   "Compile-time decorators require a named identifier path",
                                   decoratorNode->location);
        return ZR_FALSE;
    }
    rootName = primary->property->data.identifier.name;

    memberCount = primary->members != ZR_NULL ? primary->members->count : 0;
    if (binding != ZR_NULL) {
        binding->constructorCall = ZR_NULL;
    }
    for (TZrSize index = 0; index < memberCount; index++) {
        SZrAstNode *memberNode = primary->members->nodes[index];

        if (memberNode == ZR_NULL) {
            ZrParser_CompileTime_Error(cs,
                                       ZR_COMPILE_TIME_ERROR_ERROR,
                                       "Compile-time decorators only support identifier(.identifier)* with an optional final call",
                                       decoratorNode->location);
            return ZR_FALSE;
        }

        if (memberNode->type == ZR_AST_FUNCTION_CALL) {
            if (index + 1 != memberCount) {
                ZrParser_CompileTime_Error(cs,
                                           ZR_COMPILE_TIME_ERROR_ERROR,
                                           "Compile-time decorators only support a final constructor-style call",
                                           decoratorNode->location);
                return ZR_FALSE;
            }
            if (binding != ZR_NULL) {
                binding->constructorCall = &memberNode->data.functionCall;
            }
            continue;
        }

        if (!ct_try_get_static_decorator_member_name(cs, decoratorNode, memberNode, &leafName)) {
            return ZR_FALSE;
        }
        chainCount++;
    }

    leafName = chainCount > 0 ? leafName : rootName;
    importedModule = ct_find_imported_compile_time_module_alias(cs, rootName);

    if (chainCount == 0) {
        if (binding != ZR_NULL) {
            binding->name = rootName;
            binding->decoratorFunction = find_compile_time_function(cs, rootName);
            return binding->decoratorFunction != ZR_NULL ? ZR_TRUE : ZR_FALSE;
        }
        return find_compile_time_function(cs, rootName) != ZR_NULL ? ZR_TRUE : ZR_FALSE;
    }

    if (importedModule != ZR_NULL && chainCount == 1) {
        SZrCompileTimeFunction *importedFunction = ct_find_imported_compile_time_function(importedModule, leafName);

        if (importedFunction != ZR_NULL) {
            if (binding != ZR_NULL) {
                binding->name = leafName;
                binding->decoratorFunction = importedFunction;
            }
            return ZR_TRUE;
        }
    }

    if (importedModule != ZR_NULL && chainCount > 0 && primary->members != ZR_NULL) {
        SZrString *rootMemberName = ZR_NULL;
        SZrString *relativePath = ZR_NULL;
        SZrFunctionCompileTimeVariableInfo *importedVariable;
        const SZrFunctionCompileTimePathBinding *pathBinding;

        if (!ct_try_get_static_decorator_member_name(cs, decoratorNode, primary->members->nodes[0], &rootMemberName)) {
            return ZR_FALSE;
        }

        importedVariable = ct_find_imported_compile_time_variable(importedModule, rootMemberName);
        if (importedVariable != ZR_NULL &&
            ZrParser_CompileTimeBinding_BuildStaticMemberPath(cs->state,
                                                              primary->members,
                                                              1,
                                                              chainCount,
                                                              &relativePath)) {
            pathBinding = ZrParser_CompileTimeBinding_FindPath(importedVariable, relativePath);
            if (pathBinding != ZR_NULL && pathBinding->targetName != ZR_NULL) {
                SZrCompileTimeFunction *importedFunction = ZR_NULL;

                if (pathBinding->targetKind == ZR_COMPILE_TIME_BINDING_TARGET_FUNCTION) {
                    importedFunction = ct_find_imported_compile_time_function(importedModule, pathBinding->targetName);
                }

                if (importedFunction != ZR_NULL) {
                    if (binding != ZR_NULL) {
                        binding->name = pathBinding->targetName;
                        binding->decoratorFunction = importedFunction;
                    }
                    return ZR_TRUE;
                }
            }
        }
    }

    if (!ct_try_resolve_decorator_root_value(cs, rootName, &currentValue)) {
        return ZR_FALSE;
    }
    hasValueChain = ZR_TRUE;

    for (TZrSize index = 0; index < memberCount; index++) {
        SZrAstNode *memberNode = primary->members->nodes[index];
        const SZrTypeValue *memberValue;
        SZrTypeValue keyValue;
        SZrString *memberName = ZR_NULL;

        if (memberNode == ZR_NULL || memberNode->type == ZR_AST_FUNCTION_CALL) {
            continue;
        }

        if (!ct_try_get_static_decorator_member_name(cs, decoratorNode, memberNode, &memberName)) {
            return ZR_FALSE;
        }

        if (currentValue.type != ZR_VALUE_TYPE_OBJECT && currentValue.type != ZR_VALUE_TYPE_ARRAY) {
            hasValueChain = ZR_FALSE;
            break;
        }

        ZrCore_Value_InitAsRawObject(cs->state, &keyValue, ZR_CAST_RAW_OBJECT_AS_SUPER(memberName));
        keyValue.type = ZR_VALUE_TYPE_STRING;
        memberValue = ZrCore_Object_GetValue(cs->state, ZR_CAST_OBJECT(cs->state, currentValue.value.object), &keyValue);
        if (memberValue == ZR_NULL) {
            hasValueChain = ZR_FALSE;
            break;
        }

        currentValue = *memberValue;
    }

    if (!hasValueChain) {
        return ZR_FALSE;
    }

    if (binding != ZR_NULL) {
        binding->name = leafName;
        if (ct_try_resolve_compile_time_function_from_value(cs, &currentValue, &binding->decoratorFunction)) {
            return ZR_TRUE;
        }
    } else {
        SZrCompileTimeFunction *resolvedFunction = ZR_NULL;
        if (ct_try_resolve_compile_time_function_from_value(cs, &currentValue, &resolvedFunction)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

ZR_PARSER_API TZrBool ZrParser_Compiler_IsCompileTimeDecorator(SZrCompilerState *cs,
                                                               SZrAstNode *decoratorNode) {
    SZrResolvedCompileTimeDecoratorBinding binding;

    if (cs == ZR_NULL || decoratorNode == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ct_resolve_compile_time_decorator_binding(cs, decoratorNode, &binding)) {
        return ZR_FALSE;
    }

    return (binding.decoratorFunction != ZR_NULL &&
            binding.decoratorFunction->isDeclarationTransform &&
            !binding.decoratorFunction->isRuntimeProjection)
                   ? ZR_TRUE
                   : ZR_FALSE;
}

static TZrBool ct_build_type_decorator_snapshot(SZrCompilerState *cs,
                                                SZrTypePrototypeInfo *info,
                                                SZrFileRange location,
                                                SZrTypeValue *result) {
    const SZrSemanticSymbolRecord *symbol;
    SZrObject *snapshotObject;
    SZrObject *metadataObject;
    SZrObject *decoratorsArray;
    SZrTypeValue symbolIdValue;
    const TZrChar *kindName = "type";
    const TZrChar *nameText;
    TZrChar qualifiedName[ZR_PARSER_ERROR_BUFFER_LENGTH];
    const TZrChar *moduleNameText = ZR_NULL;

    if (cs == ZR_NULL || info == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    snapshotObject = ct_new_object(cs);
    metadataObject = ct_new_object(cs);
    decoratorsArray = ct_new_array(cs);
    if (snapshotObject == ZR_NULL || metadataObject == ZR_NULL || decoratorsArray == ZR_NULL) {
        return ZR_FALSE;
    }

    if (info->type == ZR_OBJECT_PROTOTYPE_TYPE_CLASS) {
        kindName = "class";
    } else if (info->type == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT) {
        kindName = "struct";
    }

    nameText = info->name != ZR_NULL ? ZrCore_String_GetNativeString(info->name) : "type";
    moduleNameText = ct_module_name_text(cs);
    symbol = ZrParser_Semantic_FindSymbolByNameAndKind(
            cs->semanticContext, info->name, ZR_SEMANTIC_SYMBOL_KIND_TYPE);
    if (symbol == ZR_NULL || symbol->id == ZR_SEMANTIC_ID_INVALID) {
        ZrParser_CompileTime_Error(
                cs,
                ZR_COMPILE_TIME_ERROR_ERROR,
                "declaration_transform.view: target type has no semantic symbol",
                location);
        return ZR_FALSE;
    }
    ZrCore_Value_InitAsUInt(cs->state, &symbolIdValue, symbol->id);

    if (moduleNameText != ZR_NULL && moduleNameText[0] != '\0') {
        snprintf(qualifiedName, sizeof(qualifiedName), "%s.%s", moduleNameText, nameText != ZR_NULL ? nameText : "type");
    } else {
        snprintf(qualifiedName, sizeof(qualifiedName), "%s", nameText != ZR_NULL ? nameText : "type");
    }

    if (!ct_set_object_field_string(cs->state, snapshotObject, "kind", kindName) ||
        !ct_set_object_field_value(cs->state, snapshotObject, "symbolId", &symbolIdValue) ||
        !ct_set_object_field_string(cs->state, snapshotObject, "name", nameText != ZR_NULL ? nameText : "type") ||
        !ct_set_object_field_string(cs->state, snapshotObject, "qualifiedName", qualifiedName) ||
        !ct_set_object_field_object(cs->state, snapshotObject, "metadata", metadataObject, ZR_VALUE_TYPE_OBJECT) ||
        !ct_set_object_field_object(cs->state, snapshotObject, "decorators", decoratorsArray, ZR_VALUE_TYPE_ARRAY) ||
        !ct_set_object_field_bool(cs->state, snapshotObject, "mutable", ZR_FALSE) ||
        !ct_set_object_field_string(cs->state, snapshotObject, "phase", "Expansion")) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(cs->state, result, ZR_CAST_RAW_OBJECT_AS_SUPER(snapshotObject));
    result->type = ZR_VALUE_TYPE_OBJECT;
    return ZR_TRUE;
}

static TZrBool ct_build_function_decorator_snapshot(SZrCompilerState *cs,
                                                    SZrFunction *function,
                                                    SZrTypeValue *result) {
    SZrObject *snapshotObject;
    SZrObject *metadataObject;
    SZrObject *decoratorsArray;
    const TZrChar *nameText;

    if (cs == ZR_NULL || function == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    snapshotObject = ct_new_object(cs);
    metadataObject = ct_new_object(cs);
    decoratorsArray = ct_new_array(cs);
    if (snapshotObject == ZR_NULL || metadataObject == ZR_NULL || decoratorsArray == ZR_NULL) {
        return ZR_FALSE;
    }

    nameText = function->functionName != ZR_NULL ? ZrCore_String_GetNativeString(function->functionName) : "function";
    if (cs->currentFunctionNode == ZR_NULL || function->functionName == ZR_NULL ||
        !ZrParser_CompileTime_EnsureDecoratorSnapshotSymbol(
                cs,
                snapshotObject,
                cs->currentFunctionNode,
                function->functionName,
                ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
                ZR_SEMANTIC_ID_INVALID,
                ZR_NULL)) {
        return ZR_FALSE;
    }
    if (!ct_set_object_field_string(cs->state, snapshotObject, "kind", "function") ||
        !ct_set_object_field_string(cs->state, snapshotObject, "name", nameText != ZR_NULL ? nameText : "function") ||
        !ct_set_object_field_string(cs->state, snapshotObject, "qualifiedName", nameText != ZR_NULL ? nameText : "function") ||
        !ct_set_object_field_object(cs->state, snapshotObject, "metadata", metadataObject, ZR_VALUE_TYPE_OBJECT) ||
        !ct_set_object_field_object(cs->state, snapshotObject, "decorators", decoratorsArray, ZR_VALUE_TYPE_ARRAY) ||
        !ct_set_object_field_bool(cs->state, snapshotObject, "mutable", ZR_FALSE) ||
        !ct_set_object_field_string(cs->state, snapshotObject, "phase", "compileTime")) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(cs->state, result, ZR_CAST_RAW_OBJECT_AS_SUPER(snapshotObject));
    result->type = ZR_VALUE_TYPE_OBJECT;
    return ZR_TRUE;
}

static TZrBool ct_build_parameter_decorator_snapshot(SZrCompilerState *cs,
                                                     SZrAstNode *parameterNode,
                                                     TZrUInt32 position,
                                                     const SZrFunctionMetadataParameter *parameterInfo,
                                                     SZrTypeValue *result) {
    SZrObject *snapshotObject;
    SZrObject *metadataObject;
    SZrObject *decoratorsArray;
    const TZrChar *nameText = "arg";
    const TZrChar *typeNameText = "object";
    TZrChar qualifiedName[ZR_PARSER_ERROR_BUFFER_LENGTH];
    SZrTypeValue positionValue;

    if (cs == ZR_NULL || parameterNode == ZR_NULL || parameterInfo == ZR_NULL || result == ZR_NULL ||
        parameterNode->type != ZR_AST_PARAMETER) {
        return ZR_FALSE;
    }

    if (parameterInfo->name != ZR_NULL) {
        const TZrChar *nativeName = ZrCore_String_GetNativeString(parameterInfo->name);
        if (nativeName != ZR_NULL && nativeName[0] != '\0') {
            nameText = nativeName;
        }
    }

    if (parameterInfo->type.typeName != ZR_NULL) {
        const TZrChar *nativeTypeName = ZrCore_String_GetNativeString(parameterInfo->type.typeName);
        if (nativeTypeName != ZR_NULL && nativeTypeName[0] != '\0') {
            typeNameText = nativeTypeName;
        }
    } else if (parameterInfo->type.isArray) {
        typeNameText = "array";
    } else if (parameterInfo->type.baseType == ZR_VALUE_TYPE_STRING) {
        typeNameText = "string";
    } else if (parameterInfo->type.baseType == ZR_VALUE_TYPE_BOOL) {
        typeNameText = "bool";
    } else if (parameterInfo->type.baseType == ZR_VALUE_TYPE_CLOSURE ||
               parameterInfo->type.baseType == ZR_VALUE_TYPE_FUNCTION) {
        typeNameText = "function";
    } else if (ZR_VALUE_IS_TYPE_INT(parameterInfo->type.baseType)) {
        typeNameText = "int";
    } else if (ZR_VALUE_IS_TYPE_FLOAT(parameterInfo->type.baseType)) {
        typeNameText = "float";
    }

    snapshotObject = ct_new_object(cs);
    metadataObject = ct_new_object(cs);
    decoratorsArray = ct_new_array(cs);
    if (snapshotObject == ZR_NULL || metadataObject == ZR_NULL || decoratorsArray == ZR_NULL) {
        return ZR_FALSE;
    }

    snprintf(qualifiedName, sizeof(qualifiedName), "%s[%u]", nameText, (unsigned int)position);
    ZrCore_Value_InitAsInt(cs->state, &positionValue, position);
    if (parameterInfo->name == ZR_NULL ||
        !ZrParser_CompileTime_EnsureDecoratorSnapshotSymbol(
                cs,
                snapshotObject,
                parameterNode,
                parameterInfo->name,
                ZR_SEMANTIC_SYMBOL_KIND_PARAMETER,
                ZR_SEMANTIC_ID_INVALID,
                ZR_NULL)) {
        return ZR_FALSE;
    }
    if (!ct_set_object_field_string(cs->state, snapshotObject, "kind", "parameter") ||
        !ct_set_object_field_string(cs->state, snapshotObject, "name", nameText) ||
        !ct_set_object_field_string(cs->state, snapshotObject, "qualifiedName", qualifiedName) ||
        !ct_set_object_field_string(cs->state, snapshotObject, "typeName", typeNameText) ||
        !ct_set_object_field_value(cs->state, snapshotObject, "position", &positionValue) ||
        !ct_set_object_field_object(cs->state, snapshotObject, "metadata", metadataObject, ZR_VALUE_TYPE_OBJECT) ||
        !ct_set_object_field_object(cs->state, snapshotObject, "decorators", decoratorsArray, ZR_VALUE_TYPE_ARRAY) ||
        !ct_set_object_field_bool(cs->state, snapshotObject, "hasDefaultValue", parameterInfo->hasDefaultValue) ||
        !ct_set_object_field_bool(cs->state, snapshotObject, "mutable", ZR_FALSE) ||
        !ct_set_object_field_string(cs->state, snapshotObject, "phase", "compileTime")) {
        return ZR_FALSE;
    }

    if (parameterInfo->hasDefaultValue &&
        !ct_set_object_field_value(cs->state, snapshotObject, "defaultValue", &parameterInfo->defaultValue)) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(cs->state, result, ZR_CAST_RAW_OBJECT_AS_SUPER(snapshotObject));
    result->type = ZR_VALUE_TYPE_OBJECT;
    return ZR_TRUE;
}

static TZrBool ct_member_logical_name_text(SZrAstNode *memberNode,
                                           const SZrTypeMemberInfo *memberInfo,
                                           const TZrChar **outNameText) {
    const TZrChar *nameText = ZR_NULL;

    if (outNameText != ZR_NULL) {
        *outNameText = ZR_NULL;
    }

    if (memberNode == ZR_NULL || memberInfo == ZR_NULL || outNameText == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (memberNode->type) {
        case ZR_AST_CLASS_FIELD:
            if (memberNode->data.classField.name != ZR_NULL && memberNode->data.classField.name->name != ZR_NULL) {
                nameText = ZrCore_String_GetNativeString(memberNode->data.classField.name->name);
            }
            break;
        case ZR_AST_STRUCT_FIELD:
            if (memberNode->data.structField.name != ZR_NULL && memberNode->data.structField.name->name != ZR_NULL) {
                nameText = ZrCore_String_GetNativeString(memberNode->data.structField.name->name);
            }
            break;
        case ZR_AST_CLASS_METHOD:
            if (memberNode->data.classMethod.name != ZR_NULL && memberNode->data.classMethod.name->name != ZR_NULL) {
                nameText = ZrCore_String_GetNativeString(memberNode->data.classMethod.name->name);
            }
            break;
        case ZR_AST_STRUCT_METHOD:
            if (memberNode->data.structMethod.name != ZR_NULL && memberNode->data.structMethod.name->name != ZR_NULL) {
                nameText = ZrCore_String_GetNativeString(memberNode->data.structMethod.name->name);
            }
            break;
        case ZR_AST_CLASS_PROPERTY:
            if (memberNode->data.classProperty.modifier != ZR_NULL) {
                if (memberNode->data.classProperty.modifier->type == ZR_AST_PROPERTY_GET &&
                    memberNode->data.classProperty.modifier->data.propertyGet.name != ZR_NULL &&
                    memberNode->data.classProperty.modifier->data.propertyGet.name->name != ZR_NULL) {
                    nameText = ZrCore_String_GetNativeString(
                            memberNode->data.classProperty.modifier->data.propertyGet.name->name);
                } else if (memberNode->data.classProperty.modifier->type == ZR_AST_PROPERTY_SET &&
                           memberNode->data.classProperty.modifier->data.propertySet.name != ZR_NULL &&
                           memberNode->data.classProperty.modifier->data.propertySet.name->name != ZR_NULL) {
                    nameText = ZrCore_String_GetNativeString(
                            memberNode->data.classProperty.modifier->data.propertySet.name->name);
                }
            }
            break;
        case ZR_AST_PROPERTY_DECLARATION:
            if (memberNode->data.propertyDeclaration.name != ZR_NULL &&
                memberNode->data.propertyDeclaration.name->name != ZR_NULL) {
                nameText = ZrCore_String_GetNativeString(
                        memberNode->data.propertyDeclaration.name->name);
            }
            break;
        default:
            break;
    }

    if (nameText == ZR_NULL && memberInfo->name != ZR_NULL) {
        nameText = ZrCore_String_GetNativeString(memberInfo->name);
    }

    *outNameText = nameText;
    return nameText != ZR_NULL && nameText[0] != '\0';
}

static const TZrChar *ct_expected_member_decorator_target_name(SZrAstNode *memberNode) {
    if (memberNode == ZR_NULL) {
        return ZR_NULL;
    }

    switch (memberNode->type) {
        case ZR_AST_CLASS_FIELD:
        case ZR_AST_STRUCT_FIELD:
        case ZR_AST_ENUM_MEMBER:
        case ZR_AST_UNION_VARIANT:
            return "Field";
        case ZR_AST_CLASS_METHOD:
        case ZR_AST_STRUCT_METHOD:
            return "Method";
        case ZR_AST_CLASS_PROPERTY:
        case ZR_AST_PROPERTY_DECLARATION:
            return "Property";
        default:
            return ZR_NULL;
    }
}

static const TZrChar *ct_member_snapshot_kind_name(SZrAstNode *memberNode) {
    const TZrChar *targetName = ct_expected_member_decorator_target_name(memberNode);

    if (targetName == ZR_NULL) {
        return ZR_NULL;
    }
    if (strcmp(targetName, "Field") == 0) {
        return "field";
    }
    if (strcmp(targetName, "Method") == 0) {
        return "method";
    }
    if (strcmp(targetName, "Property") == 0) {
        return "property";
    }
    return ZR_NULL;
}

static TZrBool ct_build_member_decorator_snapshot(SZrCompilerState *cs,
                                                  SZrAstNode *memberNode,
                                                  SZrTypeMemberInfo *memberInfo,
                                                  SZrTypeValue *result) {
    SZrObject *snapshotObject;
    SZrObject *metadataObject;
    SZrObject *decoratorsArray;
    const TZrChar *kindName;
    const TZrChar *memberNameText;
    const TZrChar *moduleNameText;
    const TZrChar *typeNameText;
    TZrChar qualifiedName[ZR_PARSER_TEXT_BUFFER_LENGTH];
    SZrTypeValue parameterCountValue;
    EZrSemanticSymbolKind symbolKind;
    TZrTypeId ownerTypeId;

    if (cs == ZR_NULL || memberNode == ZR_NULL || memberInfo == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    kindName = ct_member_snapshot_kind_name(memberNode);
    if (kindName == ZR_NULL || !ct_member_logical_name_text(memberNode, memberInfo, &memberNameText)) {
        return ZR_FALSE;
    }

    snapshotObject = ct_new_object(cs);
    metadataObject = ct_new_object(cs);
    decoratorsArray = ct_new_array(cs);
    if (snapshotObject == ZR_NULL || metadataObject == ZR_NULL || decoratorsArray == ZR_NULL) {
        return ZR_FALSE;
    }

    moduleNameText = ct_module_name_text(cs);
    typeNameText = cs->currentTypeName != ZR_NULL ? ZrCore_String_GetNativeString(cs->currentTypeName) : ZR_NULL;
    if (moduleNameText != ZR_NULL && moduleNameText[0] != '\0' &&
        typeNameText != ZR_NULL && typeNameText[0] != '\0') {
        snprintf(qualifiedName,
                 sizeof(qualifiedName),
                 "%s.%s.%s",
                 moduleNameText,
                 typeNameText,
                 memberNameText);
    } else if (typeNameText != ZR_NULL && typeNameText[0] != '\0') {
        snprintf(qualifiedName, sizeof(qualifiedName), "%s.%s", typeNameText, memberNameText);
    } else {
        snprintf(qualifiedName, sizeof(qualifiedName), "%s", memberNameText);
    }

    ZrCore_Value_InitAsInt(cs->state, &parameterCountValue, memberInfo->parameterCount);
    symbolKind = strcmp(kindName, "field") == 0
                         ? ZR_SEMANTIC_SYMBOL_KIND_FIELD
                 : strcmp(kindName, "property") == 0
                         ? ZR_SEMANTIC_SYMBOL_KIND_PROPERTY
                         : ZR_SEMANTIC_SYMBOL_KIND_FUNCTION;
    ownerTypeId = cs->semanticContext != ZR_NULL && cs->currentTypeName != ZR_NULL
                          ? ZrParser_CanonicalType_FromName(
                                    cs->semanticContext, cs->currentTypeName)
                          : ZR_SEMANTIC_ID_INVALID;
    if (!ZrParser_CompileTime_EnsureDecoratorSnapshotSymbol(
                cs,
                snapshotObject,
                memberNode,
                memberInfo->name,
                symbolKind,
                ownerTypeId,
                &memberInfo->symbolId)) {
        return ZR_FALSE;
    }
    if (!ct_set_object_field_string(cs->state, snapshotObject, "kind", kindName) ||
        !ct_set_object_field_string(cs->state, snapshotObject, "name", memberNameText) ||
        !ct_set_object_field_string(cs->state, snapshotObject, "qualifiedName", qualifiedName) ||
        !ct_set_object_field_object(cs->state, snapshotObject, "metadata", metadataObject, ZR_VALUE_TYPE_OBJECT) ||
        !ct_set_object_field_object(cs->state, snapshotObject, "decorators", decoratorsArray, ZR_VALUE_TYPE_ARRAY) ||
        !ct_set_object_field_bool(cs->state, snapshotObject, "mutable", ZR_FALSE) ||
        !ct_set_object_field_string(cs->state, snapshotObject, "phase", "compileTime") ||
        !ct_set_object_field_bool(cs->state, snapshotObject, "isStatic", memberInfo->isStatic) ||
        !ct_set_object_field_bool(cs->state, snapshotObject, "isConst", memberInfo->isConst) ||
        !ct_set_object_field_value(cs->state, snapshotObject, "parameterCount", &parameterCountValue)) {
        return ZR_FALSE;
    }

    if (memberInfo->fieldTypeName != ZR_NULL &&
        !ct_set_object_field_string(cs->state,
                                    snapshotObject,
                                    "typeName",
                                    ZrCore_String_GetNativeString(memberInfo->fieldTypeName))) {
        return ZR_FALSE;
    }

    if (memberInfo->returnTypeName != ZR_NULL &&
        !ct_set_object_field_string(cs->state,
                                    snapshotObject,
                                    "returnTypeName",
                                    ZrCore_String_GetNativeString(memberInfo->returnTypeName))) {
        return ZR_FALSE;
    } else if ((strcmp(kindName, "method") == 0 || strcmp(kindName, "property") == 0) &&
               !ct_set_object_field_string(cs->state, snapshotObject, "returnTypeName", "void")) {
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(cs->state, result, ZR_CAST_RAW_OBJECT_AS_SUPER(snapshotObject));
    result->type = ZR_VALUE_TYPE_OBJECT;
    return ZR_TRUE;
}

static const SZrTypeValue *ct_get_object_field(
        SZrCompilerState *cs,
        SZrObject *object,
        const TZrChar *name) {
    SZrTypeValue key;

    if (cs == ZR_NULL || object == ZR_NULL || name == ZR_NULL ||
        !ct_make_string_value(cs->state, name, &key)) {
        return ZR_NULL;
    }
    return ZrCore_Object_GetValue(cs->state, object, &key);
}

static TZrBool ct_is_declaration_patch_field(SZrState *state, const SZrTypeValue *key) {
    SZrString *name;

    if (state == ZR_NULL || key == ZR_NULL || key->type != ZR_VALUE_TYPE_STRING ||
        key->value.object == ZR_NULL) {
        return ZR_FALSE;
    }
    name = ZR_CAST_STRING(state, key->value.object);
    return (TZrBool)(ct_string_equals(name, "target") ||
                     ct_string_equals(name, "additions") ||
                     ct_string_equals(name, "interfaceAdds") ||
                     ct_string_equals(name, "attributeAdds") ||
                     ct_string_equals(name, "diagnostics") ||
                     ct_string_equals(name, "__zrCompileToolTypeRole"));
}

static TZrBool ct_read_nonnegative_integer(
        const SZrTypeValue *value,
        TZrUInt64 *result) {
    if (value == ZR_NULL || result == ZR_NULL ||
        !ZR_VALUE_IS_TYPE_INT(value->type) ||
        (!ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type) &&
         value->value.nativeObject.nativeInt64 < 0)) {
        return ZR_FALSE;
    }
    *result = ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)
                      ? value->value.nativeObject.nativeUInt64
                      : (TZrUInt64)value->value.nativeObject.nativeInt64;
    return ZR_TRUE;
}

static TZrSize ct_compile_time_array_count(SZrState *state, const SZrTypeValue *value) {
    SZrObject *array;

    if (state == ZR_NULL || value == ZR_NULL ||
        value->type != ZR_VALUE_TYPE_ARRAY || value->value.object == ZR_NULL) {
        return 0U;
    }
    array = ZR_CAST_OBJECT(state, value->value.object);
    if (array == ZR_NULL ||
        !ZrCore_Object_SuperArrayMaterializeGeneric(state, array)) {
        return 0U;
    }
    return ZrCore_Object_SuperArrayLength(array);
}

static const SZrTypeValue *ct_compile_time_array_at(
        SZrCompilerState *cs,
        const SZrTypeValue *arrayValue,
        TZrSize index) {
    SZrObject *array;
    SZrTypeValue key;

    if (cs == ZR_NULL || arrayValue == ZR_NULL ||
        arrayValue->type != ZR_VALUE_TYPE_ARRAY ||
        arrayValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }
    array = ZR_CAST_OBJECT(cs->state, arrayValue->value.object);
    if (array == ZR_NULL) {
        return ZR_NULL;
    }
    ZrCore_Value_InitAsInt(cs->state, &key, (TZrInt64)index);
    return ZrCore_Object_GetValue(cs->state, array, &key);
}

static TZrBool ct_decode_generated_field(
        SZrCompilerState *cs,
        const SZrTypeValue *value,
        SZrParserGeneratedDeclaration *addition,
        SZrString **canonicalTypeName,
        SZrFileRange location) {
    SZrObject *object;
    const SZrTypeValue *roleValue;
    const SZrTypeValue *nameValue;
    const SZrTypeValue *typeValue;
    const SZrTypeValue *visibilityValue;
    const SZrTypeValue *mutabilityValue;
    SZrReflectionTypeIdentity identity;
    TZrUInt64 role;
    TZrUInt64 visibility;
    TZrUInt64 mutability;

    if (cs == ZR_NULL || value == ZR_NULL || addition == ZR_NULL ||
        canonicalTypeName == ZR_NULL || value->type != ZR_VALUE_TYPE_OBJECT ||
        value->value.object == ZR_NULL) {
        return ZR_FALSE;
    }
    object = ZR_CAST_OBJECT(cs->state, value->value.object);
    roleValue = ct_get_object_field(cs, object, "__zrCompileToolTypeRole");
    nameValue = ct_get_object_field(cs, object, "name");
    typeValue = ct_get_object_field(cs, object, "type");
    visibilityValue = ct_get_object_field(cs, object, "visibility");
    mutabilityValue = ct_get_object_field(cs, object, "mutability");
    if (!ct_read_nonnegative_integer(roleValue, &role) ||
        role != ZR_PARSER_COMPILE_TOOL_TYPE_GENERATED_FIELD ||
        nameValue == ZR_NULL || nameValue->type != ZR_VALUE_TYPE_STRING ||
        nameValue->value.object == ZR_NULL || typeValue == ZR_NULL ||
        typeValue->type != ZR_VALUE_TYPE_OBJECT || typeValue->value.object == ZR_NULL ||
        !ZrCore_Reflection_ReadTypeIdObject(
                cs->state,
                ZR_CAST_OBJECT(cs->state, typeValue->value.object),
                &identity,
                canonicalTypeName) ||
        identity.canonicalTypeId == ZR_SEMANTIC_ID_INVALID ||
        !ct_read_nonnegative_integer(visibilityValue, &visibility) ||
        !ct_read_nonnegative_integer(mutabilityValue, &mutability)) {
        ZrParser_CompileTime_Error(
                cs, ZR_COMPILE_TIME_ERROR_ERROR,
                "declaration_transform.generated_field: invalid typed GeneratedField",
                location);
        return ZR_FALSE;
    }
    ZrCore_Memory_RawSet(addition, 0, sizeof(*addition));
    addition->kind = ZR_PARSER_GENERATED_DECLARATION_FIELD;
    addition->name = ZrCore_String_GetNativeString(
            ZR_CAST_STRING(cs->state, nameValue->value.object));
    addition->typeId = identity.canonicalTypeId;
    addition->visibility = (EZrParserGeneratedVisibility)visibility;
    addition->mutability = (EZrParserGeneratedMutability)mutability;
    return ZR_TRUE;
}

static TZrBool ct_apply_declaration_transform_patch(
        SZrCompilerState *cs,
        SZrTypePrototypeInfo *info,
        SZrString *transformDecoratorName,
        const SZrTypeValue *patchValue,
        SZrFileRange location) {
    SZrParserDeclarationView view;
    SZrParserDeclarationPatch patch;
    EZrParserDeclarationPatchError patchError;
    SZrObject *patchObject;
    const SZrTypeValue *roleValue;
    const SZrTypeValue *targetValue;
    const SZrTypeValue *additionsValue;
    const SZrTypeValue *interfaceAddsValue;
    const SZrTypeValue *attributeAddsValue;
    const SZrTypeValue *diagnosticsValue;
    const SZrSemanticSymbolRecord *symbol;
    SZrParserGeneratedDeclaration *additions = ZR_NULL;
    SZrParserCompileTimePatchAttributeAdds attributeAdds;
    SZrParserCompileTimePatchInterfaceAdds interfaceAdds;
    SZrString **additionTypeNames = ZR_NULL;
    const TZrChar **existingMemberNames = ZR_NULL;
    TZrSize additionCount = 0U;
    TZrSize existingMemberCount = 0U;
    TZrUInt64 role;
    TZrUInt64 targetSymbolId;
    TZrBool hasErrorDiagnostic = ZR_FALSE;
    TZrBool result = ZR_FALSE;

    ZrCore_Memory_RawSet(&attributeAdds, 0, sizeof(attributeAdds));
    ZrCore_Memory_RawSet(&interfaceAdds, 0, sizeof(interfaceAdds));

    if (cs == ZR_NULL || info == ZR_NULL || patchValue == ZR_NULL ||
        patchValue->type != ZR_VALUE_TYPE_OBJECT || patchValue->value.object == ZR_NULL) {
        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                   "declaration_transform.patch_shape: expected declaration.Patch object",
                                   location);
        return ZR_FALSE;
    }

    patchObject = ZR_CAST_OBJECT(cs->state, patchValue->value.object);
    if (patchObject == ZR_NULL || !patchObject->nodeMap.isValid ||
        patchObject->nodeMap.elementCount != 6U) {
        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                   "declaration_transform.patch_shape: expected a typed declaration.Patch",
                                   location);
        return ZR_FALSE;
    }
    for (TZrSize bucketIndex = 0; bucketIndex < patchObject->nodeMap.capacity; bucketIndex++) {
        SZrHashKeyValuePair *pair = patchObject->nodeMap.buckets[bucketIndex];
        while (pair != ZR_NULL) {
            if (!ct_is_declaration_patch_field(cs->state, &pair->key)) {
                ZrParser_CompileTime_Error(cs,
                                           ZR_COMPILE_TIME_ERROR_ERROR,
                                           "declaration_transform.patch_shape: Patch contains an unknown field",
                                           location);
                return ZR_FALSE;
            }
            pair = pair->next;
        }
    }

    roleValue = ct_get_object_field(cs, patchObject, "__zrCompileToolTypeRole");
    targetValue = ct_get_object_field(cs, patchObject, "target");
    additionsValue = ct_get_object_field(cs, patchObject, "additions");
    interfaceAddsValue = ct_get_object_field(cs, patchObject, "interfaceAdds");
    attributeAddsValue = ct_get_object_field(cs, patchObject, "attributeAdds");
    diagnosticsValue = ct_get_object_field(cs, patchObject, "diagnostics");
    if (!ct_read_nonnegative_integer(roleValue, &role) ||
        role != ZR_PARSER_COMPILE_TOOL_TYPE_PATCH ||
        !ct_read_nonnegative_integer(targetValue, &targetSymbolId) ||
        targetSymbolId == ZR_SEMANTIC_ID_INVALID ||
        targetSymbolId > (TZrUInt64)UINT32_MAX ||
        additionsValue == ZR_NULL || additionsValue->type != ZR_VALUE_TYPE_ARRAY ||
        interfaceAddsValue == ZR_NULL || interfaceAddsValue->type != ZR_VALUE_TYPE_ARRAY ||
        attributeAddsValue == ZR_NULL || attributeAddsValue->type != ZR_VALUE_TYPE_ARRAY ||
        diagnosticsValue == ZR_NULL || diagnosticsValue->type != ZR_VALUE_TYPE_ARRAY) {
        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                   "declaration_transform.patch_shape: invalid typed declaration.Patch",
                                   location);
        return ZR_FALSE;
    }
    ZrCore_Memory_RawSet(&view, 0, sizeof(view));
    ZrCore_Memory_RawSet(&patch, 0, sizeof(patch));
    symbol = ZrParser_Semantic_FindSymbolByNameAndKind(
            cs->semanticContext, info->name, ZR_SEMANTIC_SYMBOL_KIND_TYPE);
    if (symbol == ZR_NULL || symbol->id == ZR_SEMANTIC_ID_INVALID) {
        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                   "declaration_transform.view: target type has no semantic symbol",
                                   location);
        return ZR_FALSE;
    }
    view.symbolId = symbol->id;
    view.kind = ZR_PARSER_DECLARATION_KIND_TYPE;
    view.name = info->name != ZR_NULL ? ZrCore_String_GetNativeString(info->name) : ZR_NULL;
    view.sourceRange = location;
    patch.targetSymbolId = (TZrSymbolId)targetSymbolId;
    patch.expansionRound = 0U;

    if (!ZrParser_CompileTime_PreparePatchInterfaceAdds(
                cs, info, interfaceAddsValue, location, &interfaceAdds)) {
        goto cleanup;
    }
    patch.interfaceAdds = interfaceAdds.typeIds;
    patch.interfaceAddCount = interfaceAdds.count;
    if (!ZrParser_CompileTime_PreparePatchAttributeAdds(
                cs, info, attributeAddsValue, location, &attributeAdds)) {
        goto cleanup;
    }
    patch.attributeAdds = attributeAdds.contractData;
    patch.attributeAddCount = attributeAdds.count;

    existingMemberCount = info->members.length;
    if (existingMemberCount != 0U) {
        if (existingMemberCount >
            SIZE_MAX / sizeof(*existingMemberNames)) {
            ZrParser_CompileTime_Error(
                    cs,
                    ZR_COMPILE_TIME_ERROR_ERROR,
                    "declaration_transform.patch_budget: existing member budget exceeded",
                    location);
            goto cleanup;
        }
        existingMemberNames = (const TZrChar **)ZrCore_Memory_RawMallocWithType(
                cs->state->global,
                existingMemberCount * sizeof(*existingMemberNames),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (existingMemberNames == ZR_NULL) {
            goto cleanup;
        }
        for (TZrSize index = 0; index < existingMemberCount; index++) {
            SZrTypeMemberInfo *member =
                    (SZrTypeMemberInfo *)ZrCore_Array_Get(&info->members, index);
            existingMemberNames[index] = member != ZR_NULL && member->name != ZR_NULL
                                                 ? ZrCore_String_GetNativeString(member->name)
                                                 : ZR_NULL;
        }
        view.existingMemberNames = existingMemberNames;
        view.existingMemberCount = existingMemberCount;
    }

    additionCount = ct_compile_time_array_count(cs->state, additionsValue);
    if (additionCount > ZR_PARSER_DECLARATION_TRANSFORM_MAX_ADDITIONS ||
        additionCount > SIZE_MAX / sizeof(*additions) ||
        additionCount > SIZE_MAX / sizeof(*additionTypeNames)) {
        ZrParser_CompileTime_Error(
                cs,
                ZR_COMPILE_TIME_ERROR_ERROR,
                "declaration_transform.patch_budget: generated declaration budget exceeded",
                location);
        goto cleanup;
    }
    if (additionCount != 0U) {
        additions = (SZrParserGeneratedDeclaration *)ZrCore_Memory_RawMallocWithType(
                cs->state->global,
                additionCount * sizeof(*additions),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        additionTypeNames = (SZrString **)ZrCore_Memory_RawMallocWithType(
                cs->state->global,
                additionCount * sizeof(*additionTypeNames),
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (additions == ZR_NULL || additionTypeNames == ZR_NULL) {
            goto cleanup;
        }
        ZrCore_Memory_RawSet(additions, 0, additionCount * sizeof(*additions));
        ZrCore_Memory_RawSet(
                additionTypeNames, 0, additionCount * sizeof(*additionTypeNames));
        for (TZrSize index = 0; index < additionCount; index++) {
            const SZrTypeValue *additionValue =
                    ct_compile_time_array_at(cs, additionsValue, index);
            if (!ct_decode_generated_field(
                        cs,
                        additionValue,
                        &additions[index],
                        &additionTypeNames[index],
                        location)) {
                goto cleanup;
            }
        }
    }
    patch.additions = additions;
    patch.additionCount = additionCount;

    patchError = ZrParser_DeclarationPatch_Validate(&view, &patch);
    if (patchError != ZR_PARSER_DECLARATION_PATCH_VALID) {
        TZrChar message[ZR_PARSER_ERROR_BUFFER_LENGTH];
        snprintf(message,
                 sizeof(message),
                 "declaration_transform.patch_%s: invalid declaration patch",
                 ZrParser_DeclarationPatch_ErrorName(patchError));
        ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR, message, location);
        goto cleanup;
    }
    if (!ZrParser_CompileTime_ProcessPatchDiagnostics(
                cs,
                diagnosticsValue,
                patch.targetSymbolId,
                location,
                &hasErrorDiagnostic) ||
        hasErrorDiagnostic) {
        goto cleanup;
    }
    if (!ZrParser_CompileTime_CommitDeclarationPatchAtomic(
                cs,
                info,
                additions,
                additionTypeNames,
                additionCount,
                &interfaceAdds,
                &attributeAdds,
                transformDecoratorName,
                patch.targetSymbolId,
                location,
                ZR_NULL,
                ZR_NULL)) {
        goto cleanup;
    }
    result = ZR_TRUE;

cleanup:
    ZrParser_CompileTime_FreePatchAttributeAdds(cs, &attributeAdds);
    ZrParser_CompileTime_FreePatchInterfaceAdds(cs, &interfaceAdds);
    if (additionTypeNames != ZR_NULL) {
        ZR_MEMORY_RAW_FREE_LIST(cs->state->global, additionTypeNames, additionCount);
    }
    if (additions != ZR_NULL) {
        ZR_MEMORY_RAW_FREE_LIST(cs->state->global, additions, additionCount);
    }
    if (existingMemberNames != ZR_NULL) {
        ZR_MEMORY_RAW_FREE_LIST(
                cs->state->global, existingMemberNames, existingMemberCount);
    }
    return result;
}

static TZrBool ct_apply_type_decorator_patch(SZrCompilerState *cs,
                                             SZrTypePrototypeInfo *info,
                                             SZrString *decoratorName,
                                             const SZrTypeValue *patchValue,
                                             TZrBool isDeclarationTransform,
                                             SZrFileRange location) {
    if (cs == ZR_NULL || info == ZR_NULL || decoratorName == ZR_NULL || patchValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!isDeclarationTransform) {
        ZrParser_CompileTime_Error(
                cs,
                ZR_COMPILE_TIME_ERROR_ERROR,
                "decorator.runtime_removed: only a declarationTransform comptime fn returning declaration.Patch may decorate a type",
                location);
        return ZR_FALSE;
    }
    if (!ct_apply_declaration_transform_patch(
                cs,
                info,
                decoratorName,
                patchValue,
                location)) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool ct_apply_function_decorator_patch(SZrCompilerState *cs,
                                                 SZrFunction *function,
                                                 SZrString *decoratorName,
                                                 const SZrTypeValue *targetSnapshot,
                                                 const SZrTypeValue *patchValue,
                                                 SZrFileRange location) {
    TZrBool isTypedPatch = ZR_FALSE;

    if (cs == ZR_NULL || function == ZR_NULL || decoratorName == ZR_NULL || patchValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (patchValue->type != ZR_VALUE_TYPE_OBJECT || patchValue->value.object == ZR_NULL) {
        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                   "Compile-time decorator must return an object patch",
                                   location);
        return ZR_FALSE;
    }

    if (!ZrParser_CompileTime_ValidateLeafDeclarationPatch(
                cs, targetSnapshot, patchValue, location, &isTypedPatch)) {
        return ZR_FALSE;
    }
    if (!isTypedPatch) {
        ZrParser_CompileTime_Error(
                cs,
                ZR_COMPILE_TIME_ERROR_ERROR,
                "declaration_transform.patch_shape: function transform must return a typed declaration.Patch",
                location);
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static TZrBool ct_apply_member_decorator_patch(SZrCompilerState *cs,
                                               SZrTypeMemberInfo *memberInfo,
                                               SZrString *decoratorName,
                                               const SZrTypeValue *targetSnapshot,
                                               const SZrTypeValue *patchValue,
                                               SZrFileRange location) {
    SZrTypeDecoratorInfo decoratorInfo;
    TZrBool isTypedPatch = ZR_FALSE;

    if (cs == ZR_NULL || memberInfo == ZR_NULL || decoratorName == ZR_NULL || patchValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (patchValue->type != ZR_VALUE_TYPE_OBJECT || patchValue->value.object == ZR_NULL) {
        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                   "Compile-time decorator must return an object patch",
                                   location);
        return ZR_FALSE;
    }

    if (!ZrParser_CompileTime_ValidateLeafDeclarationPatch(
                cs, targetSnapshot, patchValue, location, &isTypedPatch)) {
        return ZR_FALSE;
    }
    if (!isTypedPatch) {
        ZrParser_CompileTime_Error(
                cs,
                ZR_COMPILE_TIME_ERROR_ERROR,
                "declaration_transform.patch_shape: member transform must return a typed declaration.Patch",
                location);
        return ZR_FALSE;
    }

    if (!memberInfo->decorators.isValid || memberInfo->decorators.head == ZR_NULL ||
        memberInfo->decorators.capacity == 0 || memberInfo->decorators.elementSize == 0) {
        ZrCore_Array_Init(cs->state,
                          &memberInfo->decorators,
                          sizeof(SZrTypeDecoratorInfo),
                          ZR_PARSER_INITIAL_CAPACITY_TINY);
    }

    decoratorInfo.name = decoratorName;
    ZrCore_Array_Push(cs->state, &memberInfo->decorators, &decoratorInfo);
    return ZR_TRUE;
}

static TZrBool ct_apply_parameter_decorator_patch(SZrCompilerState *cs,
                                                  SZrFunctionMetadataParameter *parameterInfo,
                                                  const SZrTypeValue *targetSnapshot,
                                                  const SZrTypeValue *patchValue,
                                                  SZrFileRange location) {
    TZrBool isTypedPatch = ZR_FALSE;

    if (cs == ZR_NULL || parameterInfo == ZR_NULL || patchValue == ZR_NULL) {
        return ZR_FALSE;
    }

    if (patchValue->type != ZR_VALUE_TYPE_OBJECT || patchValue->value.object == ZR_NULL) {
        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                   "Compile-time decorator must return an object patch",
                                   location);
        return ZR_FALSE;
    }

    if (!ZrParser_CompileTime_ValidateLeafDeclarationPatch(
                cs, targetSnapshot, patchValue, location, &isTypedPatch)) {
        return ZR_FALSE;
    }
    if (!isTypedPatch) {
        ZrParser_CompileTime_Error(
                cs,
                ZR_COMPILE_TIME_ERROR_ERROR,
                "declaration_transform.patch_shape: parameter transform must return a typed declaration.Patch",
                location);
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static TZrBool ct_execute_compile_time_decorator_function(SZrCompilerState *cs,
                                                          SZrCompileTimeFunction *decoratorFunction,
                                                          SZrFunctionCall *constructorCall,
                                                          const SZrTypeValue *targetSnapshot,
                                                          const TZrChar *expectedTargetName,
                                                          SZrTypeValue *patchResult,
                                                          SZrFileRange location) {
    SZrFunctionDeclaration *decl;
    SZrCompileTimeFrame frame;
    TZrBool success = ZR_FALSE;
    TZrBool didReturn = ZR_FALSE;
    TZrSize expectedArgumentCount = 0;
    EZrParserComptimeContext oldComptimeContext;
    SZrCompileToolExecutionScope executionScope;

    if (cs == ZR_NULL || decoratorFunction == ZR_NULL || targetSnapshot == ZR_NULL || patchResult == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!decoratorFunction->isDeclarationTransform ||
        decoratorFunction->isRuntimeProjection) {
        ZrParser_CompileTime_Error(
                cs,
                ZR_COMPILE_TIME_ERROR_ERROR,
                "decorator.runtime_removed: only compiler-owned declarationTransform functions may execute as decorators",
                location);
        return ZR_FALSE;
    }

    if (decoratorFunction->declaration == ZR_NULL ||
        decoratorFunction->declaration->type != ZR_AST_FUNCTION_DECLARATION) {
        return ZR_FALSE;
    }

    decl = &decoratorFunction->declaration->data.functionDeclaration;
    if (decl->params == ZR_NULL || decl->params->count == 0 || decl->params->nodes[0] == ZR_NULL ||
        decl->params->nodes[0]->type != ZR_AST_PARAMETER) {
        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                   "Compile-time decorator function must declare a target parameter",
                                   location);
        return ZR_FALSE;
    }

    if (!ct_validate_named_decorator_target_param(cs,
                                                  &decl->params->nodes[0]->data.parameter,
                                                  expectedTargetName,
                                                  "Compile-time decorator function target must use a canonical declaration view",
                                                  location)) {
        return ZR_FALSE;
    }

    expectedArgumentCount = decl->params->count > 0 ? decl->params->count - 1 : 0;
    if (constructorCall != ZR_NULL && constructorCall->args != ZR_NULL && decl->args == ZR_NULL &&
        constructorCall->args->count > expectedArgumentCount) {
        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                   "Too many arguments for compile-time decorator function",
                                   location);
        return ZR_FALSE;
    }

    if (!ZrParser_CompileToolExecutionScope_EnterFunction(
                cs, decoratorFunction, &executionScope)) {
        return ZR_FALSE;
    }

    ct_frame_init(cs, &frame, ZR_NULL);
    if (!ct_frame_set(cs,
                      &frame,
                      decl->params->nodes[0]->data.parameter.name != ZR_NULL
                              ? decl->params->nodes[0]->data.parameter.name->name
                              : ZR_NULL,
                      targetSnapshot)) {
        ct_frame_free(cs, &frame);
        ZrParser_CompileToolExecutionScope_Leave(cs, &executionScope);
        return ZR_FALSE;
    }

    for (TZrSize paramIndex = 1; paramIndex < decl->params->count; paramIndex++) {
        SZrAstNode *paramNode = decl->params->nodes[paramIndex];
        SZrParameter *param;
        SZrTypeValue argValue;

        if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
            continue;
        }

        param = &paramNode->data.parameter;
        if (param->name == ZR_NULL || param->name->name == ZR_NULL ||
            !ct_eval_call_arg(cs, constructorCall, param, paramIndex - 1, &frame, &argValue) ||
            !ct_frame_set(cs, &frame, param->name->name, &argValue)) {
            ct_frame_free(cs, &frame);
            ZrParser_CompileToolExecutionScope_Leave(cs, &executionScope);
            return ZR_FALSE;
        }
    }

    if (decl->body == ZR_NULL) {
        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                   "Compile-time decorator function body is null",
                                   location);
        ct_frame_free(cs, &frame);
        ZrParser_CompileToolExecutionScope_Leave(cs, &executionScope);
        return ZR_FALSE;
    }

    oldComptimeContext = cs->comptimeContext;
    cs->comptimeContext = ZR_PARSER_COMPTIME_CONTEXT_DECLARATION_TRANSFORM;
    success = decl->body->type == ZR_AST_BLOCK
                      ? execute_compile_time_block(cs, decl->body, &frame, &didReturn, patchResult)
                      : execute_compile_time_statement(cs, decl->body, &frame, &didReturn, patchResult);
    cs->comptimeContext = oldComptimeContext;
    if (success && !didReturn) {
        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                   "declaration_transform.return: transform must return declaration.Patch",
                                   location);
        success = ZR_FALSE;
    }

    ct_frame_free(cs, &frame);
    ZrParser_CompileToolExecutionScope_Leave(cs, &executionScope);
    return success;
}

ZR_PARSER_API TZrBool ZrParser_Compiler_ApplyCompileTimeTypeDecorators(SZrCompilerState *cs,
                                                                       SZrAstNode *typeNode,
                                                                       SZrAstNodeArray *decorators,
                                                                       SZrTypePrototypeInfo *info) {
    SZrTypeValue targetSnapshot;
    const TZrChar *expectedTargetName;
    TZrSize compileTimeDecoratorCount = 0;
    SZrTypeValue *patchValues = ZR_NULL;
    SZrString **decoratorNames = ZR_NULL;
    SZrAstNode **compileTimeDecoratorNodes = ZR_NULL;
    TZrBool *declarationTransformFlags = ZR_NULL;
    TZrBool success = ZR_FALSE;

    if (cs == ZR_NULL || info == ZR_NULL) {
        return ZR_FALSE;
    }

    if (decorators == ZR_NULL || decorators->count == 0) {
        return ZR_TRUE;
    }

    if (!ZrParser_DecoratorContract_ValidateNoRuntimeDecorators(
                cs,
                decorators,
                typeNode != ZR_NULL && typeNode->type == ZR_AST_CLASS_DECLARATION)) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < decorators->count; index++) {
        SZrAstNode *decoratorNode = decorators->nodes[index];
        if (decoratorNode == ZR_NULL) {
            continue;
        }

        if (ZrParser_Compiler_IsCompileTimeDecorator(cs, decoratorNode)) {
            compileTimeDecoratorCount++;
        }
        if (cs->hasError) {
            goto cleanup;
        }
    }

    if (compileTimeDecoratorCount == 0) {
        return ZR_TRUE;
    }

    patchValues = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(cs->state->global,
                                                                  sizeof(SZrTypeValue) * compileTimeDecoratorCount,
                                                                  ZR_MEMORY_NATIVE_TYPE_ARRAY);
    decoratorNames = (SZrString **)ZrCore_Memory_RawMallocWithType(cs->state->global,
                                                                   sizeof(SZrString *) * compileTimeDecoratorCount,
                                                                   ZR_MEMORY_NATIVE_TYPE_ARRAY);
    compileTimeDecoratorNodes = (SZrAstNode **)ZrCore_Memory_RawMallocWithType(cs->state->global,
                                                                                sizeof(SZrAstNode *) * compileTimeDecoratorCount,
                                                                   ZR_MEMORY_NATIVE_TYPE_ARRAY);
    declarationTransformFlags = (TZrBool *)ZrCore_Memory_RawMallocWithType(
            cs->state->global,
            sizeof(TZrBool) * compileTimeDecoratorCount,
            ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (patchValues == ZR_NULL || decoratorNames == ZR_NULL || compileTimeDecoratorNodes == ZR_NULL ||
        declarationTransformFlags == ZR_NULL) {
        goto cleanup;
    }

    ZrCore_Memory_RawSet(patchValues, 0, sizeof(SZrTypeValue) * compileTimeDecoratorCount);
    ZrCore_Memory_RawSet(decoratorNames, 0, sizeof(SZrString *) * compileTimeDecoratorCount);
    ZrCore_Memory_RawSet(compileTimeDecoratorNodes, 0, sizeof(SZrAstNode *) * compileTimeDecoratorCount);
    ZrCore_Memory_RawSet(declarationTransformFlags, 0, sizeof(TZrBool) * compileTimeDecoratorCount);

    if (!ct_build_type_decorator_snapshot(cs,
                                          info,
                                          typeNode != ZR_NULL ? typeNode->location
                                                              : (SZrFileRange){{0, 0, 0}, {0, 0, 0}, ZR_NULL},
                                          &targetSnapshot)) {
        goto cleanup;
    }

    expectedTargetName = ct_expected_type_decorator_target_name(info->type);

    for (TZrSize index = 0, compileIndex = 0; index < decorators->count; index++) {
        SZrAstNode *decoratorNode = decorators->nodes[index];
        SZrResolvedCompileTimeDecoratorBinding binding;

        if (decoratorNode == ZR_NULL) {
            continue;
        }

        if (!ZrParser_Compiler_IsCompileTimeDecorator(cs, decoratorNode)) {
            if (cs->hasError) {
                goto cleanup;
            }
            continue;
        }

        if (!ct_resolve_compile_time_decorator_binding(cs, decoratorNode, &binding)) {
            if (cs->hasError) {
                goto cleanup;
            }
            continue;
        }

        decoratorNames[compileIndex] = binding.name;
        compileTimeDecoratorNodes[compileIndex] = decoratorNode;
        declarationTransformFlags[compileIndex] =
                (TZrBool)(binding.decoratorFunction != ZR_NULL &&
                          binding.decoratorFunction->isDeclarationTransform);
        if (binding.decoratorFunction != ZR_NULL &&
            !ct_execute_compile_time_decorator_function(cs,
                                                        binding.decoratorFunction,
                                                        binding.constructorCall,
                                                        &targetSnapshot,
                                                        expectedTargetName,
                                                        &patchValues[compileIndex],
                                                        decoratorNode->location)) {
            goto cleanup;
        }
        compileIndex++;
    }

    for (TZrSize index = compileTimeDecoratorCount; index > 0; index--) {
        if (!ct_apply_type_decorator_patch(cs,
                                           info,
                                           decoratorNames[index - 1],
                                           &patchValues[index - 1],
                                           declarationTransformFlags[index - 1],
                                           compileTimeDecoratorNodes[index - 1] != ZR_NULL
                                                   ? compileTimeDecoratorNodes[index - 1]->location
                                                   : typeNode->location)) {
            goto cleanup;
        }
    }

    success = ZR_TRUE;

cleanup:
    if (patchValues != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      patchValues,
                                      sizeof(SZrTypeValue) * compileTimeDecoratorCount,
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    if (decoratorNames != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      decoratorNames,
                                      sizeof(SZrString *) * compileTimeDecoratorCount,
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    if (compileTimeDecoratorNodes != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      compileTimeDecoratorNodes,
                                      sizeof(SZrAstNode *) * compileTimeDecoratorCount,
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    if (declarationTransformFlags != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      declarationTransformFlags,
                                      sizeof(TZrBool) * compileTimeDecoratorCount,
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    return success;
}

TZrBool ZrParser_CompileTime_RegisterDecoratorFunctionIfAvailable(SZrCompilerState *cs,
                                                                  SZrAstNode *node,
                                                                  SZrFileRange location) {
    TZrBool hasTransform = ZR_FALSE;
    SZrCompileTimeFunction *record;

    if (cs == ZR_NULL || node == ZR_NULL || node->type != ZR_AST_FUNCTION_DECLARATION) {
        return ZR_FALSE;
    }

    if (!ZrParser_Metadata_FunctionHasRole(
                cs,
                node,
                ZR_PARSER_ATTRIBUTE_ROLE_DECLARATION_TRANSFORM,
                &hasTransform)) {
        return ZR_FALSE;
    }
    if (!hasTransform) {
        return ZR_TRUE;
    }
    if (!register_compile_time_function_declaration(cs, node, location)) {
        return ZR_FALSE;
    }
    record = find_compile_time_function(
            cs, node->data.functionDeclaration.name->name);
    if (record == ZR_NULL || record->isRuntimeProjection) {
        ZrParser_Compiler_Error(
                cs,
                "declaration_transform.registration: transform must be compiler-owned source comptime code",
                location);
        return ZR_FALSE;
    }
    record->isDeclarationTransform = ZR_TRUE;
    return ZR_TRUE;
}

TZrBool ZrParser_CompileTime_ApplyFunctionDecorators(SZrCompilerState *cs,
                                                     SZrAstNodeArray *decorators,
                                                     SZrFunction *function,
                                                     SZrFileRange location) {
    SZrTypeValue targetSnapshot;
    TZrSize compileTimeDecoratorCount = 0;
    SZrTypeValue *patchValues = ZR_NULL;
    SZrString **decoratorNames = ZR_NULL;
    SZrAstNode **compileTimeDecoratorNodes = ZR_NULL;
    TZrBool success = ZR_FALSE;

    if (cs == ZR_NULL || function == ZR_NULL) {
        return ZR_FALSE;
    }

    if (decorators == ZR_NULL || decorators->count == 0) {
        return ZR_TRUE;
    }

    if (!ZrParser_DecoratorContract_ValidateNoRuntimeDecorators(
                cs, decorators, ZR_FALSE)) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < decorators->count; index++) {
        SZrAstNode *decoratorNode = decorators->nodes[index];

        if (decoratorNode == ZR_NULL) {
            continue;
        }

        if (ZrParser_Compiler_IsCompileTimeDecorator(cs, decoratorNode)) {
            compileTimeDecoratorCount++;
        }
        if (cs->hasError) {
            goto cleanup;
        }
    }

    if (compileTimeDecoratorCount == 0) {
        return ZR_TRUE;
    }

    patchValues = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(cs->state->global,
                                                                  sizeof(SZrTypeValue) * compileTimeDecoratorCount,
                                                                  ZR_MEMORY_NATIVE_TYPE_ARRAY);
    decoratorNames = (SZrString **)ZrCore_Memory_RawMallocWithType(cs->state->global,
                                                                   sizeof(SZrString *) * compileTimeDecoratorCount,
                                                                   ZR_MEMORY_NATIVE_TYPE_ARRAY);
    compileTimeDecoratorNodes = (SZrAstNode **)ZrCore_Memory_RawMallocWithType(cs->state->global,
                                                                                sizeof(SZrAstNode *) * compileTimeDecoratorCount,
                                                                                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (patchValues == ZR_NULL || decoratorNames == ZR_NULL || compileTimeDecoratorNodes == ZR_NULL) {
        goto cleanup;
    }

    ZrCore_Memory_RawSet(patchValues, 0, sizeof(SZrTypeValue) * compileTimeDecoratorCount);
    ZrCore_Memory_RawSet(decoratorNames, 0, sizeof(SZrString *) * compileTimeDecoratorCount);
    ZrCore_Memory_RawSet(compileTimeDecoratorNodes, 0, sizeof(SZrAstNode *) * compileTimeDecoratorCount);

    if (!ct_build_function_decorator_snapshot(cs, function, &targetSnapshot)) {
        goto cleanup;
    }

    for (TZrSize index = 0, compileIndex = 0; index < decorators->count; index++) {
        SZrAstNode *decoratorNode = decorators->nodes[index];
        SZrResolvedCompileTimeDecoratorBinding binding;

        if (decoratorNode == ZR_NULL) {
            continue;
        }

        if (!ZrParser_Compiler_IsCompileTimeDecorator(cs, decoratorNode)) {
            if (cs->hasError) {
                goto cleanup;
            }
            continue;
        }

        if (!ct_resolve_compile_time_decorator_binding(cs, decoratorNode, &binding)) {
            if (cs->hasError) {
                goto cleanup;
            }
            continue;
        }

        decoratorNames[compileIndex] = binding.name;
        compileTimeDecoratorNodes[compileIndex] = decoratorNode;
        if (binding.decoratorFunction != ZR_NULL &&
            !ct_execute_compile_time_decorator_function(cs,
                                                        binding.decoratorFunction,
                                                        binding.constructorCall,
                                                        &targetSnapshot,
                                                        "Function",
                                                        &patchValues[compileIndex],
                                                        decoratorNode->location)) {
            goto cleanup;
        }
        compileIndex++;
    }

    if (compileTimeDecoratorCount > 0) {
        function->decoratorNames = (SZrString **)ZrCore_Memory_RawMallocWithType(cs->state->global,
                                                                                  sizeof(SZrString *) * compileTimeDecoratorCount,
                                                                                  ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (function->decoratorNames == ZR_NULL) {
            goto cleanup;
        }

        ZrCore_Memory_RawSet(function->decoratorNames, 0, sizeof(SZrString *) * compileTimeDecoratorCount);
        function->decoratorCount = (TZrUInt32)compileTimeDecoratorCount;
        for (TZrSize index = 0; index < compileTimeDecoratorCount; index++) {
            function->decoratorNames[index] = decoratorNames[index];
        }
    }

    for (TZrSize index = compileTimeDecoratorCount; index > 0; index--) {
        if (!ct_apply_function_decorator_patch(cs,
                                               function,
                                               decoratorNames[index - 1],
                                               &targetSnapshot,
                                               &patchValues[index - 1],
                                               compileTimeDecoratorNodes[index - 1] != ZR_NULL
                                                       ? compileTimeDecoratorNodes[index - 1]->location
                                                       : location)) {
            goto cleanup;
        }
    }

    success = ZR_TRUE;

cleanup:
    if (!success && function != ZR_NULL && function->decoratorNames != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      function->decoratorNames,
                                      sizeof(SZrString *) * function->decoratorCount,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        function->decoratorNames = ZR_NULL;
        function->decoratorCount = 0;
    }
    if (patchValues != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      patchValues,
                                      sizeof(SZrTypeValue) * compileTimeDecoratorCount,
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    if (decoratorNames != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      decoratorNames,
                                      sizeof(SZrString *) * compileTimeDecoratorCount,
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    if (compileTimeDecoratorNodes != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      compileTimeDecoratorNodes,
                                      sizeof(SZrAstNode *) * compileTimeDecoratorCount,
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    return success;
}

TZrBool ZrParser_CompileTime_ApplyMemberDecorators(SZrCompilerState *cs,
                                                   SZrAstNode *memberNode,
                                                   SZrAstNodeArray *decorators,
                                                   SZrTypeMemberInfo *memberInfo) {
    SZrTypeValue targetSnapshot;
    const TZrChar *expectedTargetName;
    TZrSize compileTimeDecoratorCount = 0;
    SZrTypeValue *patchValues = ZR_NULL;
    SZrString **decoratorNames = ZR_NULL;
    SZrAstNode **compileTimeDecoratorNodes = ZR_NULL;
    TZrBool success = ZR_FALSE;

    if (cs == ZR_NULL || memberNode == ZR_NULL || memberInfo == ZR_NULL) {
        return ZR_FALSE;
    }

    if (decorators == ZR_NULL || decorators->count == 0) {
        return ZR_TRUE;
    }

    if (!ZrParser_DecoratorContract_ValidateNoRuntimeDecorators(
                cs, decorators, ZR_FALSE)) {
        return ZR_FALSE;
    }

    expectedTargetName = ct_expected_member_decorator_target_name(memberNode);
    if (expectedTargetName == ZR_NULL) {
        return ZR_TRUE;
    }

    for (TZrSize index = 0; index < decorators->count; index++) {
        SZrAstNode *decoratorNode = decorators->nodes[index];

        if (decoratorNode == ZR_NULL) {
            continue;
        }

        if (ZrParser_Compiler_IsCompileTimeDecorator(cs, decoratorNode)) {
            compileTimeDecoratorCount++;
        }
        if (cs->hasError) {
            goto cleanup;
        }
    }

    if (compileTimeDecoratorCount == 0) {
        return ZR_TRUE;
    }

    patchValues = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(cs->state->global,
                                                                  sizeof(SZrTypeValue) * compileTimeDecoratorCount,
                                                                  ZR_MEMORY_NATIVE_TYPE_ARRAY);
    decoratorNames = (SZrString **)ZrCore_Memory_RawMallocWithType(cs->state->global,
                                                                   sizeof(SZrString *) * compileTimeDecoratorCount,
                                                                   ZR_MEMORY_NATIVE_TYPE_ARRAY);
    compileTimeDecoratorNodes = (SZrAstNode **)ZrCore_Memory_RawMallocWithType(cs->state->global,
                                                                                sizeof(SZrAstNode *) * compileTimeDecoratorCount,
                                                                                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (patchValues == ZR_NULL || decoratorNames == ZR_NULL || compileTimeDecoratorNodes == ZR_NULL) {
        goto cleanup;
    }

    ZrCore_Memory_RawSet(patchValues, 0, sizeof(SZrTypeValue) * compileTimeDecoratorCount);
    ZrCore_Memory_RawSet(decoratorNames, 0, sizeof(SZrString *) * compileTimeDecoratorCount);
    ZrCore_Memory_RawSet(compileTimeDecoratorNodes, 0, sizeof(SZrAstNode *) * compileTimeDecoratorCount);

    if (!ct_build_member_decorator_snapshot(cs, memberNode, memberInfo, &targetSnapshot)) {
        goto cleanup;
    }

    for (TZrSize index = 0, compileIndex = 0; index < decorators->count; index++) {
        SZrAstNode *decoratorNode = decorators->nodes[index];
        SZrResolvedCompileTimeDecoratorBinding binding;

        if (decoratorNode == ZR_NULL) {
            continue;
        }

        if (!ZrParser_Compiler_IsCompileTimeDecorator(cs, decoratorNode)) {
            if (cs->hasError) {
                goto cleanup;
            }
            continue;
        }

        if (!ct_resolve_compile_time_decorator_binding(cs, decoratorNode, &binding)) {
            if (cs->hasError) {
                goto cleanup;
            }
            continue;
        }

        decoratorNames[compileIndex] = binding.name;
        compileTimeDecoratorNodes[compileIndex] = decoratorNode;
        if (binding.decoratorFunction != ZR_NULL &&
            !ct_execute_compile_time_decorator_function(cs,
                                                        binding.decoratorFunction,
                                                        binding.constructorCall,
                                                        &targetSnapshot,
                                                        expectedTargetName,
                                                        &patchValues[compileIndex],
                                                        decoratorNode->location)) {
            goto cleanup;
        }
        compileIndex++;
    }

    for (TZrSize index = compileTimeDecoratorCount; index > 0; index--) {
        if (!ct_apply_member_decorator_patch(cs,
                                             memberInfo,
                                             decoratorNames[index - 1],
                                             &targetSnapshot,
                                             &patchValues[index - 1],
                                             compileTimeDecoratorNodes[index - 1] != ZR_NULL
                                                     ? compileTimeDecoratorNodes[index - 1]->location
                                                     : memberNode->location)) {
            goto cleanup;
        }
    }

    success = ZR_TRUE;

cleanup:
    if (patchValues != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      patchValues,
                                      sizeof(SZrTypeValue) * compileTimeDecoratorCount,
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    if (decoratorNames != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      decoratorNames,
                                      sizeof(SZrString *) * compileTimeDecoratorCount,
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    if (compileTimeDecoratorNodes != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      compileTimeDecoratorNodes,
                                      sizeof(SZrAstNode *) * compileTimeDecoratorCount,
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    return success;
}

TZrBool ZrParser_CompileTime_ApplyParameterDecorators(SZrCompilerState *cs,
                                                      SZrAstNode *parameterNode,
                                                      TZrUInt32 position,
                                                      SZrFunctionMetadataParameter *parameterInfo) {
    SZrParameter *parameter;
    SZrAstNodeArray *decorators;
    SZrTypeValue targetSnapshot;
    TZrSize compileTimeDecoratorCount = 0;
    TZrSize appliedDecoratorCount = 0;
    SZrTypeValue *patchValues = ZR_NULL;
    SZrString **decoratorNames = ZR_NULL;
    SZrAstNode **compileTimeDecoratorNodes = ZR_NULL;
    TZrBool success = ZR_FALSE;

    if (cs == ZR_NULL || parameterNode == ZR_NULL || parameterInfo == ZR_NULL ||
        parameterNode->type != ZR_AST_PARAMETER) {
        return ZR_FALSE;
    }

    parameter = &parameterNode->data.parameter;
    decorators = parameter->decorators;
    if (decorators == ZR_NULL || decorators->count == 0) {
        return ZR_TRUE;
    }

    if (!ZrParser_DecoratorContract_ValidateNoRuntimeDecorators(
                cs, decorators, ZR_FALSE)) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < decorators->count; index++) {
        SZrAstNode *decoratorNode = decorators->nodes[index];

        if (decoratorNode == ZR_NULL) {
            continue;
        }

        if (ZrParser_Compiler_IsCompileTimeDecorator(cs, decoratorNode)) {
            compileTimeDecoratorCount++;
        }
        if (cs->hasError) {
            goto cleanup;
        }
    }

    if (compileTimeDecoratorCount == 0) {
        return ZR_TRUE;
    }

    patchValues = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(cs->state->global,
                                                                  sizeof(SZrTypeValue) * compileTimeDecoratorCount,
                                                                  ZR_MEMORY_NATIVE_TYPE_ARRAY);
    decoratorNames = (SZrString **)ZrCore_Memory_RawMallocWithType(cs->state->global,
                                                                   sizeof(SZrString *) * compileTimeDecoratorCount,
                                                                   ZR_MEMORY_NATIVE_TYPE_ARRAY);
    compileTimeDecoratorNodes = (SZrAstNode **)ZrCore_Memory_RawMallocWithType(cs->state->global,
                                                                                sizeof(SZrAstNode *) * compileTimeDecoratorCount,
                                                                                ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (patchValues == ZR_NULL || decoratorNames == ZR_NULL || compileTimeDecoratorNodes == ZR_NULL) {
        goto cleanup;
    }

    ZrCore_Memory_RawSet(patchValues, 0, sizeof(SZrTypeValue) * compileTimeDecoratorCount);
    ZrCore_Memory_RawSet(decoratorNames, 0, sizeof(SZrString *) * compileTimeDecoratorCount);
    ZrCore_Memory_RawSet(compileTimeDecoratorNodes, 0, sizeof(SZrAstNode *) * compileTimeDecoratorCount);

    if (!ct_build_parameter_decorator_snapshot(cs, parameterNode, position, parameterInfo, &targetSnapshot)) {
        goto cleanup;
    }

    for (TZrSize index = 0; index < decorators->count; index++) {
        SZrAstNode *decoratorNode = decorators->nodes[index];
        SZrResolvedCompileTimeDecoratorBinding binding;

        if (decoratorNode == ZR_NULL) {
            continue;
        }

        if (!ZrParser_Compiler_IsCompileTimeDecorator(cs, decoratorNode)) {
            if (cs->hasError) {
                goto cleanup;
            }
            continue;
        }

        if (!ct_resolve_compile_time_decorator_binding(cs, decoratorNode, &binding)) {
            if (cs->hasError) {
                goto cleanup;
            }
            continue;
        }

        decoratorNames[appliedDecoratorCount] = binding.name;
        compileTimeDecoratorNodes[appliedDecoratorCount] = decoratorNode;
        if (binding.decoratorFunction != ZR_NULL &&
            !ct_execute_compile_time_decorator_function(cs,
                                                        binding.decoratorFunction,
                                                        binding.constructorCall,
                                                        &targetSnapshot,
                                                        "Parameter",
                                                        &patchValues[appliedDecoratorCount],
                                                        decoratorNode->location)) {
            goto cleanup;
        }
        appliedDecoratorCount++;
    }

    if (appliedDecoratorCount > 0) {
        parameterInfo->decoratorNames = (SZrString **)ZrCore_Memory_RawMallocWithType(
                cs->state->global,
                sizeof(SZrString *) * appliedDecoratorCount,
                ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        if (parameterInfo->decoratorNames == ZR_NULL) {
            goto cleanup;
        }

        ZrCore_Memory_RawSet(parameterInfo->decoratorNames, 0, sizeof(SZrString *) * appliedDecoratorCount);
        parameterInfo->decoratorCount = (TZrUInt32)appliedDecoratorCount;
        for (TZrSize index = 0; index < appliedDecoratorCount; index++) {
            parameterInfo->decoratorNames[index] = decoratorNames[index];
        }
    }

    for (TZrSize index = appliedDecoratorCount; index > 0; index--) {
        if (!ct_apply_parameter_decorator_patch(cs,
                                                parameterInfo,
                                                &targetSnapshot,
                                                &patchValues[index - 1],
                                                compileTimeDecoratorNodes[index - 1] != ZR_NULL
                                                        ? compileTimeDecoratorNodes[index - 1]->location
                                                        : parameterNode->location)) {
            goto cleanup;
        }
    }

    success = ZR_TRUE;

cleanup:
    if (!success && parameterInfo->decoratorNames != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      parameterInfo->decoratorNames,
                                      sizeof(SZrString *) * parameterInfo->decoratorCount,
                                      ZR_MEMORY_NATIVE_TYPE_FUNCTION);
        parameterInfo->decoratorNames = ZR_NULL;
        parameterInfo->decoratorCount = 0;
    }
    if (patchValues != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      patchValues,
                                      sizeof(SZrTypeValue) * compileTimeDecoratorCount,
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    if (decoratorNames != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      decoratorNames,
                                      sizeof(SZrString *) * compileTimeDecoratorCount,
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    if (compileTimeDecoratorNodes != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      compileTimeDecoratorNodes,
                                      sizeof(SZrAstNode *) * compileTimeDecoratorCount,
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    return success;
}

static TZrBool ct_eval_binary(SZrCompilerState *cs, SZrAstNode *node, SZrCompileTimeFrame *frame, SZrTypeValue *result) {
    SZrBinaryExpression *expr = &node->data.binaryExpression;
    SZrTypeValue leftValue;
    SZrTypeValue rightValue;
    const TZrChar *op = expr->op.op;

    if (!evaluate_compile_time_expression_internal(cs, expr->left, frame, &leftValue) ||
        !evaluate_compile_time_expression_internal(cs, expr->right, frame, &rightValue) ||
        op == ZR_NULL) {
        return ZR_FALSE;
    }

    if (strcmp(op, "+") == 0 || strcmp(op, "-") == 0 || strcmp(op, "*") == 0 || strcmp(op, "/") == 0) {
        if (!ZR_VALUE_IS_TYPE_NUMBER(leftValue.type) || !ZR_VALUE_IS_TYPE_NUMBER(rightValue.type)) {
            ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR, "Compile-time arithmetic requires numeric operands", node->location);
            return ZR_FALSE;
        }
        if (strcmp(op, "/") == 0) {
            TZrBool isZero = ZR_VALUE_IS_TYPE_INT(rightValue.type)
                               ? (rightValue.value.nativeObject.nativeInt64 == 0)
                               : (rightValue.value.nativeObject.nativeDouble == 0.0);
            if (isZero) {
                ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR, "Division by zero in compile-time expression", node->location);
                return ZR_FALSE;
            }
        }
        if (ZR_VALUE_IS_TYPE_INT(leftValue.type) && ZR_VALUE_IS_TYPE_INT(rightValue.type)) {
            TZrInt64 left = leftValue.value.nativeObject.nativeInt64;
            TZrInt64 right = rightValue.value.nativeObject.nativeInt64;
            if (strcmp(op, "+") == 0) {
                ZrCore_Value_InitAsInt(cs->state, result, left + right);
            } else if (strcmp(op, "-") == 0) {
                ZrCore_Value_InitAsInt(cs->state, result, left - right);
            } else if (strcmp(op, "*") == 0) {
                ZrCore_Value_InitAsInt(cs->state, result, left * right);
            } else {
                ZrCore_Value_InitAsInt(cs->state, result, left / right);
            }
            return ZR_TRUE;
        }

        TZrDouble left = leftValue.value.nativeObject.nativeDouble;
        TZrDouble right = rightValue.value.nativeObject.nativeDouble;
        if (strcmp(op, "+") == 0) {
            ZrCore_Value_InitAsFloat(cs->state, result, left + right);
        } else if (strcmp(op, "-") == 0) {
            ZrCore_Value_InitAsFloat(cs->state, result, left - right);
        } else if (strcmp(op, "*") == 0) {
            ZrCore_Value_InitAsFloat(cs->state, result, left * right);
        } else {
            ZrCore_Value_InitAsFloat(cs->state, result, left / right);
        }
        return ZR_TRUE;
    }

    if (strcmp(op, "%") == 0) {
        if (!ZR_VALUE_IS_TYPE_INT(leftValue.type) || !ZR_VALUE_IS_TYPE_INT(rightValue.type)) {
            ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR, "Compile-time modulo requires integer operands", node->location);
            return ZR_FALSE;
        }
        if (rightValue.value.nativeObject.nativeInt64 == 0) {
            ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR, "Modulo by zero in compile-time expression", node->location);
            return ZR_FALSE;
        }
        ZrCore_Value_InitAsInt(cs->state, result, leftValue.value.nativeObject.nativeInt64 % rightValue.value.nativeObject.nativeInt64);
        return ZR_TRUE;
    }

    if (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0 ||
        strcmp(op, "<") == 0 || strcmp(op, "<=") == 0 ||
        strcmp(op, ">") == 0 || strcmp(op, ">=") == 0) {
        TZrBool value = ZR_FALSE;
        if (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0) {
            if (leftValue.type == rightValue.type) {
                if (ZR_VALUE_IS_TYPE_INT(leftValue.type)) {
                    value = leftValue.value.nativeObject.nativeInt64 == rightValue.value.nativeObject.nativeInt64;
                } else if (ZR_VALUE_IS_TYPE_FLOAT(leftValue.type)) {
                    value = leftValue.value.nativeObject.nativeDouble == rightValue.value.nativeObject.nativeDouble;
                } else if (leftValue.type == ZR_VALUE_TYPE_BOOL) {
                    value = leftValue.value.nativeObject.nativeBool == rightValue.value.nativeObject.nativeBool;
                } else if (leftValue.type == ZR_VALUE_TYPE_NULL) {
                    value = ZR_TRUE;
                }
            }
            if (strcmp(op, "!=") == 0) {
                value = !value;
            }
        } else if (ZR_VALUE_IS_TYPE_NUMBER(leftValue.type) && ZR_VALUE_IS_TYPE_NUMBER(rightValue.type)) {
            if (ZR_VALUE_IS_TYPE_INT(leftValue.type) && ZR_VALUE_IS_TYPE_INT(rightValue.type)) {
                TZrInt64 left = leftValue.value.nativeObject.nativeInt64;
                TZrInt64 right = rightValue.value.nativeObject.nativeInt64;
                value = strcmp(op, "<") == 0 ? left < right :
                        strcmp(op, "<=") == 0 ? left <= right :
                        strcmp(op, ">") == 0 ? left > right : left >= right;
            } else {
                TZrDouble left = leftValue.value.nativeObject.nativeDouble;
                TZrDouble right = rightValue.value.nativeObject.nativeDouble;
                value = strcmp(op, "<") == 0 ? left < right :
                        strcmp(op, "<=") == 0 ? left <= right :
                        strcmp(op, ">") == 0 ? left > right : left >= right;
            }
        } else {
            ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR, "Compile-time comparison requires compatible operands", node->location);
            return ZR_FALSE;
        }

        ZrCore_Value_InitAsUInt(cs->state, result, value ? 1 : 0);
        result->type = ZR_VALUE_TYPE_BOOL;
        return ZR_TRUE;
    }

    ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR, "Unsupported compile-time binary expression", node->location);
    return ZR_FALSE;
}

static TZrBool ct_eval_logical(SZrCompilerState *cs, SZrAstNode *node, SZrCompileTimeFrame *frame, SZrTypeValue *result) {
    SZrLogicalExpression *expr = &node->data.logicalExpression;
    SZrTypeValue leftValue;
    TZrBool value;

    if (!evaluate_compile_time_expression_internal(cs, expr->left, frame, &leftValue)) {
        return ZR_FALSE;
    }

    if (strcmp(expr->op, "&&") == 0) {
        value = ct_truthy(&leftValue);
        if (value) {
            SZrTypeValue rightValue;
            if (!evaluate_compile_time_expression_internal(cs, expr->right, frame, &rightValue)) {
                return ZR_FALSE;
            }
            value = ct_truthy(&rightValue);
        }
    } else if (strcmp(expr->op, "||") == 0) {
        value = ct_truthy(&leftValue);
        if (!value) {
            SZrTypeValue rightValue;
            if (!evaluate_compile_time_expression_internal(cs, expr->right, frame, &rightValue)) {
                return ZR_FALSE;
            }
            value = ct_truthy(&rightValue);
        }
    } else {
        ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR, "Unsupported compile-time logical expression", node->location);
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsUInt(cs->state, result, value ? 1 : 0);
    result->type = ZR_VALUE_TYPE_BOOL;
    return ZR_TRUE;
}

static TZrBool ct_eval_unary(SZrCompilerState *cs, SZrAstNode *node, SZrCompileTimeFrame *frame, SZrTypeValue *result) {
    SZrUnaryExpression *expr = &node->data.unaryExpression;
    SZrTypeValue argValue;

    if (!evaluate_compile_time_expression_internal(cs, expr->argument, frame, &argValue)) {
        return ZR_FALSE;
    }

    if (strcmp(expr->op.op, "!") == 0) {
        ZrCore_Value_InitAsUInt(cs->state, result, ct_truthy(&argValue) ? 0 : 1);
        result->type = ZR_VALUE_TYPE_BOOL;
        return ZR_TRUE;
    }
    if (strcmp(expr->op.op, "+") == 0) {
        *result = argValue;
        return ZR_TRUE;
    }
    if (strcmp(expr->op.op, "-") == 0) {
        if (ZR_VALUE_IS_TYPE_INT(argValue.type)) {
            ZrCore_Value_InitAsInt(cs->state, result, -argValue.value.nativeObject.nativeInt64);
            return ZR_TRUE;
        }
        if (ZR_VALUE_IS_TYPE_FLOAT(argValue.type)) {
            ZrCore_Value_InitAsFloat(cs->state, result, -argValue.value.nativeObject.nativeDouble);
            return ZR_TRUE;
        }
    }

    ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR, "Unsupported compile-time unary expression", node->location);
    return ZR_FALSE;
}

static TZrBool ct_eval_type_cast(SZrCompilerState *cs,
                                 SZrAstNode *node,
                                 SZrCompileTimeFrame *frame,
                                 SZrTypeValue *result) {
    SZrTypeCastExpression *expr;
    SZrTypeValue sourceValue;
    const TZrChar *targetName;

    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL || node->type != ZR_AST_TYPE_CAST_EXPRESSION) {
        return ZR_FALSE;
    }

    expr = &node->data.typeCastExpression;
    if (expr->targetType == ZR_NULL || expr->expression == ZR_NULL ||
        expr->targetType->name == ZR_NULL ||
        expr->targetType->name->type != ZR_AST_IDENTIFIER_LITERAL ||
        expr->targetType->name->data.identifier.name == ZR_NULL) {
        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                   "Unsupported compile-time type cast target",
                                   node->location);
        return ZR_FALSE;
    }

    if (!evaluate_compile_time_expression_internal(cs, expr->expression, frame, &sourceValue)) {
        return ZR_FALSE;
    }

    targetName = ct_name(expr->targetType->name->data.identifier.name);
    if (strcmp(targetName, "int") == 0) {
        if (ZR_VALUE_IS_TYPE_INT(sourceValue.type)) {
            ZrCore_Value_Copy(cs->state, result, &sourceValue);
        } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(sourceValue.type)) {
            ZrCore_Value_InitAsInt(cs->state, result, (TZrInt64)sourceValue.value.nativeObject.nativeUInt64);
        } else if (ZR_VALUE_IS_TYPE_FLOAT(sourceValue.type)) {
            ZrCore_Value_InitAsInt(cs->state, result, (TZrInt64)sourceValue.value.nativeObject.nativeDouble);
        } else if (sourceValue.type == ZR_VALUE_TYPE_BOOL) {
            ZrCore_Value_InitAsInt(cs->state, result, sourceValue.value.nativeObject.nativeBool ? ZR_TRUE : ZR_FALSE);
        } else {
            ZrCore_Value_InitAsInt(cs->state, result, 0);
        }
        return ZR_TRUE;
    }

    if (strcmp(targetName, "float") == 0) {
        if (ZR_VALUE_IS_TYPE_FLOAT(sourceValue.type)) {
            ZrCore_Value_Copy(cs->state, result, &sourceValue);
        } else if (ZR_VALUE_IS_TYPE_INT(sourceValue.type)) {
            ZrCore_Value_InitAsFloat(cs->state, result, (TZrFloat64)sourceValue.value.nativeObject.nativeInt64);
        } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(sourceValue.type)) {
            ZrCore_Value_InitAsFloat(cs->state, result, (TZrFloat64)sourceValue.value.nativeObject.nativeUInt64);
        } else if (sourceValue.type == ZR_VALUE_TYPE_BOOL) {
            ZrCore_Value_InitAsFloat(cs->state,
                                     result,
                                     sourceValue.value.nativeObject.nativeBool ? (TZrFloat64)ZR_TRUE : (TZrFloat64)ZR_FALSE);
        } else {
            ZrCore_Value_InitAsFloat(cs->state, result, 0.0);
        }
        return ZR_TRUE;
    }

    if (strcmp(targetName, "bool") == 0) {
        ZrCore_Value_InitAsUInt(cs->state, result, ct_truthy(&sourceValue) ? 1 : 0);
        result->type = ZR_VALUE_TYPE_BOOL;
        return ZR_TRUE;
    }

    if (strcmp(targetName, "string") == 0) {
        SZrString *stringValue = ZrCore_Value_ConvertToString(cs->state, &sourceValue);
        if (stringValue == ZR_NULL) {
            ZrCore_Value_ResetAsNull(result);
        } else {
            ZrCore_Value_InitAsRawObject(cs->state, result, ZR_CAST_RAW_OBJECT_AS_SUPER(stringValue));
            result->type = ZR_VALUE_TYPE_STRING;
        }
        return ZR_TRUE;
    }

    ZrParser_CompileTime_Error(cs,
                               ZR_COMPILE_TIME_ERROR_ERROR,
                               "Unsupported compile-time type cast target",
                               node->location);
    return ZR_FALSE;
}

static TZrBool ct_eval_call_arg(SZrCompilerState *cs,
                              SZrFunctionCall *call,
                              SZrParameter *param,
                              TZrSize paramIndex,
                              SZrCompileTimeFrame *frame,
                              SZrTypeValue *result) {
    TZrSize positionalCount = 0;

    if (call != ZR_NULL && call->hasNamedArgs && call->argNames != ZR_NULL &&
        param != ZR_NULL && param->name != ZR_NULL && param->name->name != ZR_NULL) {
        for (TZrSize i = 0; i < call->argNames->length && i < call->args->count; i++) {
            SZrString **argNamePtr = (SZrString **)ZrCore_Array_Get(call->argNames, i);
            if (argNamePtr != ZR_NULL && *argNamePtr == ZR_NULL) {
                positionalCount++;
                continue;
            }
            break;
        }

        for (TZrSize i = 0; i < call->argNames->length && i < call->args->count; i++) {
            SZrString **argNamePtr = (SZrString **)ZrCore_Array_Get(call->argNames, i);
            if (argNamePtr != ZR_NULL && *argNamePtr != ZR_NULL &&
                ZrCore_String_Equal(*argNamePtr, param->name->name)) {
                return evaluate_compile_time_expression_internal(cs, call->args->nodes[i], frame, result);
            }
        }

        if (paramIndex < positionalCount) {
            return evaluate_compile_time_expression_internal(cs, call->args->nodes[paramIndex], frame, result);
        }
    } else if (call != ZR_NULL && call->args != ZR_NULL && paramIndex < call->args->count) {
        return evaluate_compile_time_expression_internal(cs, call->args->nodes[paramIndex], frame, result);
    }

    if (param != ZR_NULL && param->defaultValue != ZR_NULL) {
        return evaluate_compile_time_expression_internal(cs, param->defaultValue, frame, result);
    }

    ct_error_name(cs,
                  param != ZR_NULL && param->name != ZR_NULL ? param->name->name : ZR_NULL,
                  "Missing compile-time argument for parameter: ",
                  (SZrFileRange){{0, 0, 0}, {0, 0, 0}, ZR_NULL});
    return ZR_FALSE;
}

static TZrBool ct_eval_runtime_projected_call_arg(SZrCompilerState *cs,
                                                  SZrCompileTimeFunction *func,
                                                  SZrFunctionCall *call,
                                                  SZrString *paramName,
                                                  TZrSize paramIndex,
                                                  SZrCompileTimeFrame *frame,
                                                  SZrTypeValue *result) {
    TZrSize positionalCount = 0;

    if (cs == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (call != ZR_NULL && call->hasNamedArgs && call->argNames != ZR_NULL) {
        for (TZrSize i = 0; i < call->argNames->length && call->args != ZR_NULL && i < call->args->count; i++) {
            SZrString **argNamePtr = (SZrString **)ZrCore_Array_Get(call->argNames, i);
            if (argNamePtr != ZR_NULL && *argNamePtr == ZR_NULL) {
                positionalCount++;
                continue;
            }
            break;
        }

        if (paramName != ZR_NULL) {
            for (TZrSize i = 0; i < call->argNames->length && call->args != ZR_NULL && i < call->args->count; i++) {
                SZrString **argNamePtr = (SZrString **)ZrCore_Array_Get(call->argNames, i);
                if (argNamePtr != ZR_NULL && *argNamePtr != ZR_NULL &&
                    ZrCore_String_Equal(*argNamePtr, paramName)) {
                    return evaluate_compile_time_expression_internal(cs, call->args->nodes[i], frame, result);
                }
            }
        }

        if (call->args != ZR_NULL && paramIndex < positionalCount) {
            return evaluate_compile_time_expression_internal(cs, call->args->nodes[paramIndex], frame, result);
        }
    } else if (call != ZR_NULL && call->args != ZR_NULL && paramIndex < call->args->count) {
        return evaluate_compile_time_expression_internal(cs, call->args->nodes[paramIndex], frame, result);
    }

    if (func != ZR_NULL &&
        paramIndex < func->paramHasDefaultValues.length &&
        paramIndex < func->paramDefaultValues.length) {
        TZrBool *hasDefaultPtr = (TZrBool *)ZrCore_Array_Get(&func->paramHasDefaultValues, paramIndex);
        SZrTypeValue *defaultValue = (SZrTypeValue *)ZrCore_Array_Get(&func->paramDefaultValues, paramIndex);
        if (hasDefaultPtr != ZR_NULL && *hasDefaultPtr && defaultValue != ZR_NULL) {
            ZrCore_Value_ResetAsNull(result);
            ZrCore_Value_Copy(cs->state, result, defaultValue);
            return ZR_TRUE;
        }
    }

    ct_error_name(cs, paramName, "Missing compile-time argument for parameter: ", (SZrFileRange){{0, 0, 0}, {0, 0, 0}, ZR_NULL});
    return ZR_FALSE;
}

static TZrBool ct_invoke_runtime_callable_with_values(SZrCompilerState *cs,
                                                      SZrAstNode *callSite,
                                                      const SZrTypeValue *callableValue,
                                                      TZrSize argCount,
                                                      const SZrTypeValue *argValues,
                                                      SZrTypeValue *result) {
    SZrState *state;
    TZrStackValuePointer base;
    SZrFunctionStackAnchor baseAnchor;

    if (cs == ZR_NULL || callableValue == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    state = cs->state;
    base = state->stackTop.valuePointer;
    base = ZrCore_Function_CheckStackAndAnchor(state, argCount + 1, base, base, &baseAnchor);
    state->stackTop.valuePointer = base;
    ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base));
    ZrCore_Value_Copy(state, ZrCore_Stack_GetValue(base), callableValue);
    state->stackTop.valuePointer = base + 1;

    for (TZrSize i = 0; i < argCount; i++) {
        ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(base + 1 + i));
        ZrCore_Value_Copy(state, ZrCore_Stack_GetValue(base + 1 + i), &argValues[i]);
        state->stackTop.valuePointer = base + 2 + i;
    }

    base = ZrCore_Function_CallAndRestoreAnchor(state, &baseAnchor, 1);
    if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                   "Runtime callable failed during compile-time evaluation",
                                   callSite != ZR_NULL ? callSite->location : (SZrFileRange){{0, 0, 0}, {0, 0, 0}, ZR_NULL});
        return ZR_FALSE;
    }

    ZrCore_Value_ResetAsNull(result);
    ZrCore_Value_Copy(state, result, ZrCore_Stack_GetValue(base));
    return ZR_TRUE;
}

static TZrBool ct_call_runtime_projected_compile_time_function(SZrCompilerState *cs,
                                                               SZrAstNode *callSite,
                                                               SZrCompileTimeFunction *func,
                                                               SZrFunctionCall *call,
                                                               SZrCompileTimeFrame *frame,
                                                               SZrTypeValue *result) {
    SZrTypeValue callableValue;
    SZrTypeValue *argValues = ZR_NULL;
    TZrSize parameterCount;
    TZrBool success = ZR_FALSE;

    if (cs == ZR_NULL || func == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    parameterCount = func->paramNames.length;
    if (!ct_value_from_compile_time_function(cs, func, &callableValue)) {
        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                   "Failed to resolve runtime projection for compile-time function",
                                   callSite != ZR_NULL ? callSite->location : (SZrFileRange){{0, 0, 0}, {0, 0, 0}, ZR_NULL});
        return ZR_FALSE;
    }

    if (call != ZR_NULL && call->args != ZR_NULL && call->args->count > parameterCount) {
        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                   "Too many arguments for compile-time function call",
                                   callSite != ZR_NULL ? callSite->location : (SZrFileRange){{0, 0, 0}, {0, 0, 0}, ZR_NULL});
        return ZR_FALSE;
    }

    if (parameterCount > 0) {
        argValues = (SZrTypeValue *)ZrCore_Memory_RawMallocWithType(cs->state->global,
                                                                    sizeof(SZrTypeValue) * parameterCount,
                                                                    ZR_MEMORY_NATIVE_TYPE_ARRAY);
        if (argValues == ZR_NULL) {
            return ZR_FALSE;
        }

        for (TZrSize index = 0; index < parameterCount; index++) {
            SZrString **paramNamePtr = (SZrString **)ZrCore_Array_Get(&func->paramNames, index);
            if (!ct_eval_runtime_projected_call_arg(cs,
                                                    func,
                                                    call,
                                                    paramNamePtr != ZR_NULL ? *paramNamePtr : ZR_NULL,
                                                    index,
                                                    frame,
                                                    &argValues[index])) {
                goto cleanup;
            }
        }
    }

    success = ct_invoke_runtime_callable_with_values(cs,
                                                     callSite,
                                                     &callableValue,
                                                     parameterCount,
                                                     argValues,
                                                     result);

cleanup:
    if (argValues != ZR_NULL) {
        ZrCore_Memory_RawFreeWithType(cs->state->global,
                                      argValues,
                                      sizeof(SZrTypeValue) * parameterCount,
                                      ZR_MEMORY_NATIVE_TYPE_ARRAY);
    }
    return success;
}

static TZrBool ct_call_function(SZrCompilerState *cs,
                              SZrAstNode *callSite,
                              SZrCompileTimeFunction *func,
                              SZrFunctionCall *call,
                              SZrCompileTimeFrame *parentFrame,
                              SZrTypeValue *result) {
    SZrFunctionDeclaration *decl;
    SZrCompileTimeFrame frame;
    TZrBool success = ZR_FALSE;
    TZrBool didReturn = ZR_FALSE;
    TZrBool enteredCall = ZR_FALSE;
    TZrBool cacheable;
    SZrComptimeCacheKey cacheKey;
    TZrUInt64 diagnosticCountBefore;
    SZrCompileToolExecutionScope executionScope;
    TZrBool enteredExecutionScope = ZR_FALSE;

    if (cs == ZR_NULL || func == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (func->isRuntimeProjection) {
        return ct_call_runtime_projected_compile_time_function(cs, callSite, func, call, parentFrame, result);
    }

    if (func->declaration == ZR_NULL || func->declaration->type != ZR_AST_FUNCTION_DECLARATION) {
        return ZR_FALSE;
    }

    if (!ZrParser_CompileToolExecutionScope_EnterFunction(
                cs, func, &executionScope)) {
        return ZR_FALSE;
    }
    enteredExecutionScope = ZR_TRUE;

    decl = &func->declaration->data.functionDeclaration;
    ct_frame_init(cs, &frame, parentFrame);
    cacheable = ZrParser_ComptimeCache_BeginKey(cs, func, &cacheKey);

    if (decl->params != ZR_NULL) {
        for (TZrSize i = 0; i < decl->params->count; i++) {
            SZrAstNode *paramNode = decl->params->nodes[i];
            SZrParameter *param;
            SZrTypeValue argValue;

            if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
                continue;
            }

            param = &paramNode->data.parameter;
            if (!ct_eval_call_arg(cs, call, param, i, &frame, &argValue)) {
                goto cleanup;
            }
            if (cacheable &&
                !ZrParser_ComptimeCache_MixValue(&cacheKey, &argValue)) {
                cacheable = ZR_FALSE;
            }
            if (param->name == ZR_NULL || param->name->name == ZR_NULL ||
                !ct_frame_set(cs, &frame, param->name->name, &argValue)) {
                goto cleanup;
            }
        }
    }

    if (call != ZR_NULL && call->args != ZR_NULL && !call->hasNamedArgs) {
        TZrSize expectedArgs = decl->params != ZR_NULL ? decl->params->count : 0;
        if (call->args->count > expectedArgs) {
            ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR, "Too many arguments for compile-time function call", callSite->location);
            goto cleanup;
        }
    }

    if (decl->body == ZR_NULL) {
        ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR, "Compile-time function body is null", callSite->location);
        goto cleanup;
    }

    if (cacheable && ZrParser_ComptimeCache_Lookup(
                             cs, &cacheKey, result)) {
        success = ZR_TRUE;
        goto cleanup;
    }
    if (!ZrParser_ComptimeRuntime_EnterCall(
                cs,
                callSite != ZR_NULL ? callSite->location : func->location)) {
        goto cleanup;
    }
    enteredCall = ZR_TRUE;
    diagnosticCountBefore = cs->comptimeBudget.usage.diagnosticCount;

    success = decl->body->type == ZR_AST_BLOCK
                  ? execute_compile_time_block(cs, decl->body, &frame, &didReturn, result)
                  : execute_compile_time_statement(cs, decl->body, &frame, &didReturn, result);
    if (success && !didReturn) {
        ZrCore_Value_ResetAsNull(result);
    }
    if (success && cacheable &&
        diagnosticCountBefore ==
                cs->comptimeBudget.usage.diagnosticCount) {
        ZrParser_ComptimeCache_Store(cs, &cacheKey, result);
    }

cleanup:
    if (enteredCall) {
        ZrParser_ComptimeRuntime_LeaveCall(cs);
    }
    ct_frame_free(cs, &frame);
    if (enteredExecutionScope) {
        ZrParser_CompileToolExecutionScope_Leave(cs, &executionScope);
    }
    return success;
}

static TZrBool ct_eval_object_key(SZrCompilerState *cs,
                                SZrAstNode *keyNode,
                                TZrBool keyIsComputed,
                                SZrCompileTimeFrame *frame,
                                SZrTypeValue *result) {
    if (cs == ZR_NULL || keyNode == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!keyIsComputed && keyNode->type == ZR_AST_IDENTIFIER_LITERAL) {
        ZrCore_Value_InitAsRawObject(cs->state, result, ZR_CAST_RAW_OBJECT_AS_SUPER(keyNode->data.identifier.name));
        result->type = ZR_VALUE_TYPE_STRING;
        return ZR_TRUE;
    }

    return evaluate_compile_time_expression_internal(cs, keyNode, frame, result);
}

static TZrBool ct_eval_object_literal(SZrCompilerState *cs,
                                    SZrAstNode *node,
                                    SZrCompileTimeFrame *frame,
                                    SZrTypeValue *result) {
    SZrObjectLiteral *objectLiteral;
    SZrObject *object;

    if (cs == ZR_NULL || node == ZR_NULL || node->type != ZR_AST_OBJECT_LITERAL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    objectLiteral = &node->data.objectLiteral;
    object = ct_new_object(cs);
    if (object == ZR_NULL) {
        return ZR_FALSE;
    }

    if (objectLiteral->properties != ZR_NULL) {
        for (TZrSize i = 0; i < objectLiteral->properties->count; i++) {
            SZrAstNode *propertyNode = objectLiteral->properties->nodes[i];
            SZrTypeValue keyValue;
            SZrTypeValue propertyValue;

            if (propertyNode == ZR_NULL) {
                continue;
            }

            if (propertyNode->type != ZR_AST_KEY_VALUE_PAIR) {
                ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR,
                                   "Unsupported compile-time object literal property",
                                   propertyNode->location);
                return ZR_FALSE;
            }
            if (!ZrParser_ComptimeRuntime_Consume(
                        cs,
                        ZR_PARSER_COMPTIME_BUDGET_AGGREGATE_COUNT,
                        1U,
                        propertyNode->location)) {
                return ZR_FALSE;
            }

            if (!ct_eval_object_key(cs,
                                    propertyNode->data.keyValuePair.key,
                                    propertyNode->data.keyValuePair.keyIsComputed,
                                    frame,
                                    &keyValue) ||
                !evaluate_compile_time_expression_internal(cs, propertyNode->data.keyValuePair.value, frame, &propertyValue)) {
                return ZR_FALSE;
            }

            ZrCore_Object_SetValue(cs->state, object, &keyValue, &propertyValue);
        }
    }

    ZrCore_Value_InitAsRawObject(cs->state, result, ZR_CAST_RAW_OBJECT_AS_SUPER(object));
    result->type = ZR_VALUE_TYPE_OBJECT;
    return ZR_TRUE;
}

static TZrBool ct_eval_array_literal(SZrCompilerState *cs,
                                    SZrAstNode *node,
                                    SZrCompileTimeFrame *frame,
                                    SZrTypeValue *result) {
    SZrArrayLiteral *arrayLiteral;
    SZrObject *array;

    if (cs == ZR_NULL || node == ZR_NULL || node->type != ZR_AST_ARRAY_LITERAL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    arrayLiteral = &node->data.arrayLiteral;
    array = ct_new_array(cs);
    if (array == ZR_NULL) {
        return ZR_FALSE;
    }

    if (arrayLiteral->elements != ZR_NULL) {
        for (TZrSize index = 0; index < arrayLiteral->elements->count; index++) {
            SZrAstNode *elementNode = arrayLiteral->elements->nodes[index];
            SZrTypeValue key;
            SZrTypeValue value;

            if (elementNode == ZR_NULL) {
                continue;
            }
            if (elementNode->type == ZR_AST_UNPACK_LITERAL) {
                ZrParser_CompileTime_Error(cs,
                                           ZR_COMPILE_TIME_ERROR_ERROR,
                                           "Compile-time array spread is not supported",
                                           elementNode->location);
                return ZR_FALSE;
            }
            if (!ZrParser_ComptimeRuntime_Consume(
                        cs,
                        ZR_PARSER_COMPTIME_BUDGET_AGGREGATE_COUNT,
                        1U,
                        elementNode->location) ||
                !evaluate_compile_time_expression_internal(cs, elementNode, frame, &value)) {
                return ZR_FALSE;
            }
            ZrCore_Value_InitAsInt(cs->state, &key, (TZrInt64)index);
            ZrCore_Object_SetValue(cs->state, array, &key, &value);
        }
    }

    ZrCore_Value_InitAsRawObject(cs->state, result, ZR_CAST_RAW_OBJECT_AS_SUPER(array));
    result->type = ZR_VALUE_TYPE_ARRAY;
    return ZR_TRUE;
}

static TZrBool ct_eval_member_key(SZrCompilerState *cs,
                                SZrMemberExpression *memberExpr,
                                SZrCompileTimeFrame *frame,
                                SZrTypeValue *result) {
    if (cs == ZR_NULL || memberExpr == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!memberExpr->computed && memberExpr->property != ZR_NULL &&
        memberExpr->property->type == ZR_AST_IDENTIFIER_LITERAL) {
        ZrCore_Value_InitAsRawObject(cs->state,
                                     result,
                                     ZR_CAST_RAW_OBJECT_AS_SUPER(memberExpr->property->data.identifier.name));
        result->type = ZR_VALUE_TYPE_STRING;
        return ZR_TRUE;
    }

    return evaluate_compile_time_expression_internal(cs, memberExpr->property, frame, result);
}

static TZrBool ct_eval_member_access(SZrCompilerState *cs,
                                   SZrAstNode *callSite,
                                   const SZrTypeValue *baseValue,
                                   SZrMemberExpression *memberExpr,
                                   SZrCompileTimeFrame *frame,
                                   SZrTypeValue *result) {
    SZrTypeValue keyValue;
    const SZrTypeValue *memberValue;

    if (cs == ZR_NULL || baseValue == ZR_NULL || memberExpr == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (baseValue->type != ZR_VALUE_TYPE_OBJECT && baseValue->type != ZR_VALUE_TYPE_ARRAY) {
        ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR,
                           "Compile-time member access requires object or array value",
                           callSite != ZR_NULL ? callSite->location : (SZrFileRange){{0, 0, 0}, {0, 0, 0}, ZR_NULL});
        return ZR_FALSE;
    }

    if (!ct_eval_member_key(cs, memberExpr, frame, &keyValue)) {
        return ZR_FALSE;
    }

    memberValue = ZrCore_Object_GetValue(cs->state, ZR_CAST_OBJECT(cs->state, baseValue->value.object), &keyValue);
    if (memberValue == ZR_NULL) {
        TZrChar message[ZR_PARSER_ERROR_BUFFER_LENGTH];
        snprintf(message, sizeof(message), "Unknown compile-time member: %s",
                 (!memberExpr->computed && memberExpr->property != ZR_NULL &&
                  memberExpr->property->type == ZR_AST_IDENTIFIER_LITERAL)
                         ? ct_name(memberExpr->property->data.identifier.name)
                         : "<computed>");
        ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR, message,
                           callSite != ZR_NULL ? callSite->location : (SZrFileRange){{0, 0, 0}, {0, 0, 0}, ZR_NULL});
        return ZR_FALSE;
    }

    *result = *memberValue;
    return ZR_TRUE;
}

static TZrBool ct_call_value(SZrCompilerState *cs,
                           SZrAstNode *callSite,
                           const SZrTypeValue *callableValue,
                           SZrFunctionCall *call,
                           SZrCompileTimeFrame *frame,
                           SZrTypeValue *result) {
    SZrCompileTimeFunction *compileTimeFunction = ZR_NULL;

    if (ct_value_try_get_compile_time_function(cs, callableValue, &compileTimeFunction)) {
        return ct_call_function(cs, callSite, compileTimeFunction, call, frame, result);
    }

    ZrParser_CompileTime_Error(
            cs,
            ZR_COMPILE_TIME_ERROR_ERROR,
            "comptime.effect_violation: runtime callable invocation is not allowed in compile-time evaluation",
            callSite != ZR_NULL
                    ? callSite->location
                    : (SZrFileRange){{0, 0, 0}, {0, 0, 0}, ZR_NULL});
    return ZR_FALSE;
}

static TZrBool ct_eval_primary(SZrCompilerState *cs, SZrAstNode *node, SZrCompileTimeFrame *frame, SZrTypeValue *result) {
    SZrPrimaryExpression *primary = &node->data.primaryExpression;
    SZrTypeValue currentValue;
    TZrSize startIndex = 0;
    TZrBool handledCompileToolCall = ZR_FALSE;

    if (!ZrParser_CompileToolEvaluator_TryEvaluate(
                cs, node, frame, result, &handledCompileToolCall)) {
        return ZR_FALSE;
    }
    if (handledCompileToolCall) {
        return ZR_TRUE;
    }

    if (primary->members == ZR_NULL || primary->members->count == 0) {
        return primary->property != ZR_NULL
                   ? evaluate_compile_time_expression_internal(cs, primary->property, frame, result)
                   : ZR_FALSE;
    }

    if (primary->property != ZR_NULL &&
        primary->property->type == ZR_AST_IMPORT_EXPRESSION &&
        primary->property->data.importExpression.modulePath != ZR_NULL &&
        primary->property->data.importExpression.modulePath->type == ZR_AST_STRING_LITERAL &&
        primary->members->count >= 2 &&
        primary->members->nodes[0] != ZR_NULL &&
        primary->members->nodes[0]->type == ZR_AST_MEMBER_EXPRESSION &&
        !primary->members->nodes[0]->data.memberExpression.computed &&
        primary->members->nodes[0]->data.memberExpression.property != ZR_NULL &&
        primary->members->nodes[0]->data.memberExpression.property->type == ZR_AST_IDENTIFIER_LITERAL &&
        primary->members->nodes[1] != ZR_NULL &&
        primary->members->nodes[1]->type == ZR_AST_FUNCTION_CALL) {
        SZrCompileTimeFunction *importedFunction =
                resolve_imported_compile_time_function(
                        cs,
                        primary->property->data.importExpression.modulePath->data.stringLiteral.value,
                        primary->members->nodes[0]->data.memberExpression.property->data.identifier.name);

        if (importedFunction != ZR_NULL) {
            if (!ct_call_function(
                        cs,
                        primary->members->nodes[1],
                        importedFunction,
                        &primary->members->nodes[1]->data.functionCall,
                        frame,
                        &currentValue)) {
                return ZR_FALSE;
            }
            startIndex = 2;
        }
    }

    if (startIndex == 0 && primary->property != ZR_NULL &&
        primary->property->type == ZR_AST_IDENTIFIER_LITERAL &&
        primary->members->count >= 2 &&
        primary->members->nodes[0] != ZR_NULL &&
        primary->members->nodes[0]->type == ZR_AST_MEMBER_EXPRESSION &&
        !primary->members->nodes[0]->data.memberExpression.computed &&
        primary->members->nodes[0]->data.memberExpression.property != ZR_NULL &&
        primary->members->nodes[0]->data.memberExpression.property->type ==
                ZR_AST_IDENTIFIER_LITERAL &&
        primary->members->nodes[1] != ZR_NULL &&
        primary->members->nodes[1]->type == ZR_AST_FUNCTION_CALL) {
        SZrImportedCompileTimeModule *importedModule =
                ct_find_imported_compile_time_module_alias(
                        cs, primary->property->data.identifier.name);
        SZrCompileTimeFunction *importedFunction =
                ct_find_imported_compile_time_function(
                        importedModule,
                        primary->members->nodes[0]
                                ->data.memberExpression.property
                                ->data.identifier.name);

        if (importedFunction != ZR_NULL) {
            if (!ct_call_function(
                        cs,
                        primary->members->nodes[1],
                        importedFunction,
                        &primary->members->nodes[1]->data.functionCall,
                        frame,
                        &currentValue)) {
                return ZR_FALSE;
            }
            startIndex = 2;
        }
    }

    if (startIndex == 0 && primary->property != ZR_NULL &&
        primary->property->type == ZR_AST_IDENTIFIER_LITERAL &&
        primary->members->nodes[0] != ZR_NULL &&
        primary->members->nodes[0]->type == ZR_AST_FUNCTION_CALL) {
        SZrString *funcName = primary->property->data.identifier.name;
        SZrFunctionCall *call = &primary->members->nodes[0]->data.functionCall;
        SZrCompileTimeFunction *compileTimeFunction = ZR_NULL;

        if ((compileTimeFunction = find_compile_time_function(cs, funcName)) != ZR_NULL) {
            if (!ct_call_function(cs, primary->members->nodes[0], compileTimeFunction, call, frame, &currentValue)) {
                return ZR_FALSE;
            }
            startIndex = 1;
        } else if (!evaluate_compile_time_expression_internal(cs, primary->property, frame, &currentValue)) {
            return ZR_FALSE;
        }
    } else if (startIndex == 0 &&
               !evaluate_compile_time_expression_internal(cs, primary->property, frame, &currentValue)) {
        return ZR_FALSE;
    }

    for (TZrSize i = startIndex; i < primary->members->count; i++) {
        SZrAstNode *memberNode = primary->members->nodes[i];

        if (memberNode == ZR_NULL) {
            continue;
        }

        if (memberNode->type == ZR_AST_MEMBER_EXPRESSION) {
            if (!ct_eval_member_access(cs, memberNode, &currentValue, &memberNode->data.memberExpression, frame,
                                       &currentValue)) {
                return ZR_FALSE;
            }
            continue;
        }

        if (memberNode->type == ZR_AST_FUNCTION_CALL) {
            if (!ct_call_value(cs, memberNode, &currentValue, &memberNode->data.functionCall, frame, &currentValue)) {
                return ZR_FALSE;
            }
            continue;
        }

        ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR,
                           "Unsupported compile-time primary expression member",
                           memberNode->location);
        return ZR_FALSE;
    }

    *result = currentValue;
    return ZR_TRUE;
}

static TZrBool ct_assign_identifier(SZrCompilerState *cs,
                                    SZrCompileTimeFrame *frame,
                                    SZrString *name,
                                    const SZrTypeValue *value,
                                    SZrFileRange location,
                                    SZrTypeValue *result) {
    SZrCompileTimeFrame *cursor = frame;

    if (cs == ZR_NULL || name == ZR_NULL || value == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    while (cursor != ZR_NULL) {
        for (TZrSize i = cursor->bindings.length; i > 0; i--) {
            SZrCompileTimeBinding *binding =
                    (SZrCompileTimeBinding *)ZrCore_Array_Get(&cursor->bindings, i - 1);
            if (binding != ZR_NULL && binding->name != ZR_NULL && ZrCore_String_Equal(binding->name, name)) {
                binding->value = *value;
                *result = *value;
                return ZR_TRUE;
            }
        }
        cursor = cursor->parent;
    }

    {
        SZrCompileTimeVariable *var = find_compile_time_variable(cs, name);
        if (var != ZR_NULL) {
            var->evaluatedValue = *value;
            var->hasEvaluatedValue = ZR_TRUE;
            var->isEvaluating = ZR_FALSE;
            *result = *value;
            return ZR_TRUE;
        }
    }

    ct_error_name(cs, name, "Unknown compile-time assignment target: ", location);
    return ZR_FALSE;
}

static TZrBool ct_resolve_primary_assignment_target(SZrCompilerState *cs,
                                                    SZrAstNode *node,
                                                    SZrCompileTimeFrame *frame,
                                                    SZrObject **targetObject,
                                                    SZrTypeValue *keyValue) {
    SZrPrimaryExpression *primary;
    SZrTypeValue currentValue;
    SZrAstNode *lastMemberNode;

    if (cs == ZR_NULL || node == ZR_NULL || node->type != ZR_AST_PRIMARY_EXPRESSION ||
        targetObject == ZR_NULL || keyValue == ZR_NULL) {
        return ZR_FALSE;
    }

    *targetObject = ZR_NULL;
    primary = &node->data.primaryExpression;
    if (primary->property == ZR_NULL || primary->members == ZR_NULL || primary->members->count == 0) {
        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                   "Compile-time assignment target must end in a member access",
                                   node->location);
        return ZR_FALSE;
    }

    if (!evaluate_compile_time_expression_internal(cs, primary->property, frame, &currentValue)) {
        return ZR_FALSE;
    }

    for (TZrSize i = 0; i + 1 < primary->members->count; i++) {
        SZrAstNode *memberNode = primary->members->nodes[i];

        if (memberNode == ZR_NULL) {
            continue;
        }

        if (memberNode->type == ZR_AST_MEMBER_EXPRESSION) {
            if (!ct_eval_member_access(cs,
                                       memberNode,
                                       &currentValue,
                                       &memberNode->data.memberExpression,
                                       frame,
                                       &currentValue)) {
                return ZR_FALSE;
            }
            continue;
        }

        if (memberNode->type == ZR_AST_FUNCTION_CALL) {
            if (!ct_call_value(cs, memberNode, &currentValue, &memberNode->data.functionCall, frame, &currentValue)) {
                return ZR_FALSE;
            }
            continue;
        }

        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                   "Unsupported compile-time assignment target member",
                                   memberNode->location);
        return ZR_FALSE;
    }

    lastMemberNode = primary->members->nodes[primary->members->count - 1];
    if (lastMemberNode == ZR_NULL || lastMemberNode->type != ZR_AST_MEMBER_EXPRESSION) {
        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                   "Compile-time assignment target must end in a member access",
                                   node->location);
        return ZR_FALSE;
    }

    if ((currentValue.type != ZR_VALUE_TYPE_OBJECT && currentValue.type != ZR_VALUE_TYPE_ARRAY) ||
        currentValue.value.object == ZR_NULL) {
        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                   "Compile-time assignment member target requires object or array value",
                                   node->location);
        return ZR_FALSE;
    }

    if (!ct_eval_member_key(cs, &lastMemberNode->data.memberExpression, frame, keyValue)) {
        return ZR_FALSE;
    }

    *targetObject = ZR_CAST_OBJECT(cs->state, currentValue.value.object);
    return *targetObject != ZR_NULL;
}

static TZrBool ct_eval_assignment(SZrCompilerState *cs,
                                SZrAstNode *node,
                                SZrCompileTimeFrame *frame,
                                SZrTypeValue *result) {
    SZrAssignmentExpression *expr;
    SZrTypeValue assignedValue;

    if (cs == ZR_NULL || node == ZR_NULL || node->type != ZR_AST_ASSIGNMENT_EXPRESSION || result == ZR_NULL) {
        return ZR_FALSE;
    }

    expr = &node->data.assignmentExpression;
    if (expr->op.op == ZR_NULL || strcmp(expr->op.op, "=") != 0) {
        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                   "Only '=' assignments are supported in compile-time expressions",
                                   node->location);
        return ZR_FALSE;
    }

    if (!evaluate_compile_time_expression_internal(cs, expr->right, frame, &assignedValue)) {
        return ZR_FALSE;
    }

    if (expr->left == ZR_NULL) {
        ZrParser_CompileTime_Error(cs,
                                   ZR_COMPILE_TIME_ERROR_ERROR,
                                   "Compile-time assignment target is missing",
                                   node->location);
        return ZR_FALSE;
    }

    if (expr->left->type == ZR_AST_IDENTIFIER_LITERAL) {
        return ct_assign_identifier(cs,
                                    frame,
                                    expr->left->data.identifier.name,
                                    &assignedValue,
                                    expr->left->location,
                                    result);
    }

    if (expr->left->type == ZR_AST_PRIMARY_EXPRESSION) {
        SZrPrimaryExpression *primary = &expr->left->data.primaryExpression;

        if (primary->members == ZR_NULL || primary->members->count == 0) {
            if (primary->property != ZR_NULL && primary->property->type == ZR_AST_IDENTIFIER_LITERAL) {
                return ct_assign_identifier(cs,
                                            frame,
                                            primary->property->data.identifier.name,
                                            &assignedValue,
                                            primary->property->location,
                                            result);
            }

            ZrParser_CompileTime_Error(cs,
                                       ZR_COMPILE_TIME_ERROR_ERROR,
                                       "Compile-time assignment target must be an identifier or member access",
                                       expr->left->location);
            return ZR_FALSE;
        }

        {
            SZrObject *targetObject = ZR_NULL;
            SZrTypeValue keyValue;

            if (!ct_resolve_primary_assignment_target(cs, expr->left, frame, &targetObject, &keyValue)) {
                return ZR_FALSE;
            }

            ZrCore_Object_SetValue(cs->state, targetObject, &keyValue, &assignedValue);
            *result = assignedValue;
            return ZR_TRUE;
        }
    }

    ZrParser_CompileTime_Error(cs,
                               ZR_COMPILE_TIME_ERROR_ERROR,
                               "Compile-time assignment target must be an identifier or member access",
                               expr->left->location);
    return ZR_FALSE;
}

TZrBool evaluate_compile_time_expression_internal(SZrCompilerState *cs,
                                                       SZrAstNode *node,
                                                       SZrCompileTimeFrame *frame,
                                                       SZrTypeValue *result) {
    TZrBool oldContext;

    if (cs == ZR_NULL || node == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!ZrParser_ComptimeRuntime_Consume(
                cs,
                ZR_PARSER_COMPTIME_BUDGET_FUEL,
                1U,
                node->location)) {
        return ZR_FALSE;
    }

    oldContext = cs->isInCompileTimeContext;
    cs->isInCompileTimeContext = ZR_TRUE;

    switch (node->type) {
        case ZR_AST_INTEGER_LITERAL:
            ZrCore_Value_InitAsInt(cs->state, result, node->data.integerLiteral.value);
            break;
        case ZR_AST_FLOAT_LITERAL:
            ZrCore_Value_InitAsFloat(cs->state, result, node->data.floatLiteral.value);
            break;
        case ZR_AST_BOOLEAN_LITERAL:
            ZrCore_Value_InitAsUInt(cs->state, result, node->data.booleanLiteral.value ? 1 : 0);
            result->type = ZR_VALUE_TYPE_BOOL;
            break;
        case ZR_AST_STRING_LITERAL:
            ZrCore_Value_InitAsRawObject(cs->state, result, ZR_CAST_RAW_OBJECT_AS_SUPER(node->data.stringLiteral.value));
            result->type = ZR_VALUE_TYPE_STRING;
            break;
        case ZR_AST_NULL_LITERAL:
            ZrCore_Value_ResetAsNull(result);
            break;
        case ZR_AST_IDENTIFIER_LITERAL:
            if (!ct_frame_get(frame, node->data.identifier.name, result) &&
                !ZrParser_Compiler_TryGetCompileTimeValue(cs, node->data.identifier.name, result)) {
                SZrCompileTimeFunction *func = find_compile_time_function(cs, node->data.identifier.name);
                if (func != ZR_NULL && ct_value_from_compile_time_function(cs, func, result)) {
                    break;
                }
                ct_error_name(cs, node->data.identifier.name, "Unknown compile-time identifier: ", node->location);
                cs->isInCompileTimeContext = oldContext;
                return ZR_FALSE;
            }
            break;
        case ZR_AST_OBJECT_LITERAL:
            cs->isInCompileTimeContext = oldContext;
            return ct_eval_object_literal(cs, node, frame, result);
        case ZR_AST_ARRAY_LITERAL:
            cs->isInCompileTimeContext = oldContext;
            return ct_eval_array_literal(cs, node, frame, result);
        case ZR_AST_STRUCT_INIT_EXPRESSION: {
            TZrBool handledCompileToolValue = ZR_FALSE;
            TZrBool evaluated;
            cs->isInCompileTimeContext = oldContext;
            evaluated = ZrParser_CompileToolEvaluator_TryEvaluate(
                    cs, node, frame, result, &handledCompileToolValue);
            if (!evaluated || handledCompileToolValue) {
                return evaluated;
            }
            ZrParser_CompileTime_Error(
                    cs,
                    ZR_COMPILE_TIME_ERROR_ERROR,
                    "Only compiler-owned typed data may be constructed in compile-time expressions",
                    node->location);
            return ZR_FALSE;
        }
        case ZR_AST_ASSIGNMENT_EXPRESSION:
            cs->isInCompileTimeContext = oldContext;
            return ct_eval_assignment(cs, node, frame, result);
        case ZR_AST_BINARY_EXPRESSION:
            cs->isInCompileTimeContext = oldContext;
            return ct_eval_binary(cs, node, frame, result);
        case ZR_AST_LOGICAL_EXPRESSION:
            cs->isInCompileTimeContext = oldContext;
            return ct_eval_logical(cs, node, frame, result);
        case ZR_AST_UNARY_EXPRESSION:
            cs->isInCompileTimeContext = oldContext;
            return ct_eval_unary(cs, node, frame, result);
        case ZR_AST_TYPE_CAST_EXPRESSION:
            cs->isInCompileTimeContext = oldContext;
            return ct_eval_type_cast(cs, node, frame, result);
        case ZR_AST_IMPORT_EXPRESSION:
            cs->isInCompileTimeContext = oldContext;
            return ct_eval_import_expression(cs, node, result);
        case ZR_AST_TYPE_QUERY_EXPRESSION:
            if (node->data.typeQueryExpression.kind == ZR_TYPE_QUERY_CANONICAL_IDENTITY) {
                if (node->data.typeQueryExpression.typeOperand == ZR_NULL) {
                    ZrParser_CompileTime_Error(cs,
                                               ZR_COMPILE_TIME_ERROR_ERROR,
                                               "typeid requires a TypeRef operand",
                                               node->location);
                    cs->isInCompileTimeContext = oldContext;
                    return ZR_FALSE;
                }
                cs->isInCompileTimeContext = oldContext;
                return compiler_build_type_identity_value(
                        cs,
                        node->data.typeQueryExpression.typeOperand,
                        node->location,
                        result);
            }
            ZrParser_CompileTime_Error(cs,
                                       ZR_COMPILE_TIME_ERROR_ERROR,
                                       "typeof is not supported in compile-time expressions",
                                       node->location);
            cs->isInCompileTimeContext = oldContext;
            return ZR_FALSE;
        case ZR_AST_CONDITIONAL_EXPRESSION: {
            SZrConditionalExpression *expr = &node->data.conditionalExpression;
            SZrTypeValue condValue;
            if (!evaluate_compile_time_expression_internal(cs, expr->test, frame, &condValue)) {
                cs->isInCompileTimeContext = oldContext;
                return ZR_FALSE;
            }
            cs->isInCompileTimeContext = oldContext;
            return evaluate_compile_time_expression_internal(cs, ct_truthy(&condValue) ? expr->consequent : expr->alternate, frame, result);
        }
        case ZR_AST_PRIMARY_EXPRESSION:
            cs->isInCompileTimeContext = oldContext;
            return ct_eval_primary(cs, node, frame, result);
        case ZR_AST_PROTOTYPE_REFERENCE_EXPRESSION:
            ZrParser_CompileTime_Error(cs,
                               ZR_COMPILE_TIME_ERROR_ERROR,
                               "Prototype references are not supported in compile-time expressions",
                               node->location);
            cs->isInCompileTimeContext = oldContext;
            return ZR_FALSE;
        case ZR_AST_CONSTRUCT_EXPRESSION:
            ZrParser_CompileTime_Error(cs,
                               ZR_COMPILE_TIME_ERROR_ERROR,
                               "Prototype construction is not supported in compile-time expressions",
                               node->location);
            cs->isInCompileTimeContext = oldContext;
            return ZR_FALSE;
        case ZR_AST_EXPRESSION_STATEMENT:
            cs->isInCompileTimeContext = oldContext;
            return evaluate_compile_time_expression_internal(cs, node->data.expressionStatement.expr, frame, result);
        default:
            ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR, "Unsupported compile-time expression node", node->location);
            cs->isInCompileTimeContext = oldContext;
            return ZR_FALSE;
    }

    cs->isInCompileTimeContext = oldContext;
    return ZR_TRUE;
}

ZR_PARSER_API TZrBool ZrParser_Compiler_EvaluateCompileTimeExpression(SZrCompilerState *cs,
                                                            SZrAstNode *node,
                                                            SZrTypeValue *result) {
    return evaluate_compile_time_expression_internal(cs, node, ZR_NULL, result);
}

TZrBool execute_compile_time_statement(SZrCompilerState *cs,
                                            SZrAstNode *node,
                                            SZrCompileTimeFrame *frame,
                                            TZrBool *didReturn,
                                            SZrTypeValue *result) {
    if (didReturn != ZR_NULL) {
        *didReturn = ZR_FALSE;
    }
    if (cs == ZR_NULL || node == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrParser_ComptimeRuntime_Consume(
                cs,
                ZR_PARSER_COMPTIME_BUDGET_FUEL,
                1U,
                node->location)) {
        return ZR_FALSE;
    }

    switch (node->type) {
        case ZR_AST_BLOCK:
            return execute_compile_time_block(cs, node, frame, didReturn, result);
        case ZR_AST_RETURN_STATEMENT: {
            SZrReturnStatement *stmt = &node->data.returnStatement;
            if (result == ZR_NULL) {
                return ZR_FALSE;
            }
            if (stmt->expr == ZR_NULL) {
                ZrCore_Value_ResetAsNull(result);
            } else if (!evaluate_compile_time_expression_internal(cs, stmt->expr, frame, result)) {
                return ZR_FALSE;
            }
            if (didReturn != ZR_NULL) {
                *didReturn = ZR_TRUE;
            }
            return ZR_TRUE;
        }
        case ZR_AST_VARIABLE_DECLARATION: {
            SZrVariableDeclaration *decl = &node->data.variableDeclaration;
            SZrTypeValue value;
            if (frame == ZR_NULL) {
                return register_compile_time_variable_declaration(cs, node, node->location);
            }
            if (decl->pattern == ZR_NULL || decl->pattern->type != ZR_AST_IDENTIFIER_LITERAL) {
                return ZR_TRUE;
            }
            if (decl->value != ZR_NULL && !evaluate_compile_time_expression_internal(cs, decl->value, frame, &value)) {
                return ZR_FALSE;
            }
            if (decl->value == ZR_NULL) {
                ZrCore_Value_ResetAsNull(&value);
            }
            return ct_frame_set(cs, frame, decl->pattern->data.identifier.name, &value);
        }
        case ZR_AST_IF_EXPRESSION: {
            SZrIfExpression *expr = &node->data.ifExpression;
            SZrTypeValue condValue;
            SZrAstNode *branch;
            if (!evaluate_compile_time_expression_internal(cs, expr->condition, frame, &condValue)) {
                return ZR_FALSE;
            }
            branch = ct_truthy(&condValue) ? expr->thenExpr : expr->elseExpr;
            if (branch == ZR_NULL) {
                return ZR_TRUE;
            }
            if (branch->type == ZR_AST_BLOCK || branch->type == ZR_AST_RETURN_STATEMENT ||
                branch->type == ZR_AST_IF_EXPRESSION || branch->type == ZR_AST_VARIABLE_DECLARATION ||
                branch->type == ZR_AST_FUNCTION_DECLARATION ||
                branch->type == ZR_AST_EXPRESSION_STATEMENT) {
                return execute_compile_time_statement(cs, branch, frame, didReturn, result);
            }
            return result != ZR_NULL ? evaluate_compile_time_expression_internal(cs, branch, frame, result) : ZR_TRUE;
        }
        case ZR_AST_FUNCTION_DECLARATION:
            if (frame == ZR_NULL) {
                return register_compile_time_function_declaration(cs, node, node->location);
            }
            ZrParser_CompileTime_Error(cs, ZR_COMPILE_TIME_ERROR_ERROR,
                               "Nested compile-time function declarations are not supported in local frames",
                               node->location);
            return ZR_FALSE;
        case ZR_AST_COMPILE_TIME_DECLARATION:
            return ZrParser_CompileTimeDeclaration_Execute(cs, node);
        default: {
            SZrTypeValue ignored;
            return evaluate_compile_time_expression_internal(cs, node, frame, result != ZR_NULL ? result : &ignored);
        }
    }
}

TZrBool execute_compile_time_block(SZrCompilerState *cs,
                                        SZrAstNode *node,
                                        SZrCompileTimeFrame *frame,
                                        TZrBool *didReturn,
                                        SZrTypeValue *result) {
    SZrBlock *block;
    if (cs == ZR_NULL || node == ZR_NULL || node->type != ZR_AST_BLOCK) {
        return ZR_FALSE;
    }
    block = &node->data.block;
    if (block->body == ZR_NULL) {
        return ZR_TRUE;
    }
    for (TZrSize i = 0; i < block->body->count; i++) {
        TZrBool returned = ZR_FALSE;
        if (!execute_compile_time_statement(cs, block->body->nodes[i], frame, &returned, result)) {
            return ZR_FALSE;
        }
        if (returned) {
            if (didReturn != ZR_NULL) {
                *didReturn = ZR_TRUE;
            }
            return ZR_TRUE;
        }
    }
    return ZR_TRUE;
}

ZR_PARSER_API TZrBool ZrParser_CompileTimeDeclaration_Execute(SZrCompilerState *cs, SZrAstNode *node) {
    SZrCompileTimeDeclaration *decl;
    SZrAstNode *body;
    TZrBool oldContext;
    EZrParserComptimeContext oldComptimeContext;

    if (cs == ZR_NULL || node == ZR_NULL || node->type != ZR_AST_COMPILE_TIME_DECLARATION) {
        return ZR_FALSE;
    }

    decl = &node->data.compileTimeDeclaration;
    body = decl->declaration;
    if (body == ZR_NULL) {
        return ZR_FALSE;
    }

    oldContext = cs->isInCompileTimeContext;
    cs->isInCompileTimeContext = ZR_TRUE;

    if (decl->isConditionalPruning) {
        SZrIfExpression *ifExpression;
        SZrTypeValue conditionValue;

        if (body->type != ZR_AST_IF_EXPRESSION) {
            ZrParser_CompileTime_Error(cs,
                                       ZR_COMPILE_TIME_ERROR_ERROR,
                                       "comptime if requires an if expression",
                                       node->location);
            cs->isInCompileTimeContext = oldContext;
            return ZR_FALSE;
        }

        ifExpression = &body->data.ifExpression;
        oldComptimeContext = cs->comptimeContext;
        cs->comptimeContext = ZR_PARSER_COMPTIME_CONTEXT_PURE_VALUE;
        if (!evaluate_compile_time_expression_internal(
                    cs, ifExpression->condition, ZR_NULL, &conditionValue)) {
            cs->comptimeContext = oldComptimeContext;
            cs->isInCompileTimeContext = oldContext;
            return ZR_FALSE;
        }
        cs->comptimeContext = oldComptimeContext;
        decl->selectedBranch = ct_truthy(&conditionValue) ? ifExpression->thenExpr : ifExpression->elseExpr;
        cs->isInCompileTimeContext = oldContext;
        return ZR_TRUE;
    }

    switch (decl->declarationType) {
        case ZR_COMPILE_TIME_VARIABLE: {
            if (!register_compile_time_variable_declaration(cs, body, node->location)) {
                cs->isInCompileTimeContext = oldContext;
                return ZR_FALSE;
            }
            cs->isInCompileTimeContext = oldContext;
            return ZR_TRUE;
        }

        case ZR_COMPILE_TIME_FUNCTION: {
            if (!ZrParser_Metadata_ValidateFunctionAttributes(cs, body) ||
                !ZrParser_CompileTime_RegisterDecoratorFunctionIfAvailable(
                        cs, body, node->location)) {
                cs->isInCompileTimeContext = oldContext;
                return ZR_FALSE;
            }
            if (find_compile_time_function(
                        cs, body->data.functionDeclaration.name->name) == ZR_NULL &&
                !register_compile_time_function_declaration(
                        cs, body, node->location)) {
                cs->isInCompileTimeContext = oldContext;
                return ZR_FALSE;
            }
            cs->isInCompileTimeContext = oldContext;
            return ZR_TRUE;
        }

        case ZR_COMPILE_TIME_STATEMENT: {
            TZrBool didReturn = ZR_FALSE;
            SZrTypeValue ignored;
            oldComptimeContext = cs->comptimeContext;
            cs->comptimeContext = ZR_PARSER_COMPTIME_CONTEXT_CHECK;
            TZrBool ok = body->type == ZR_AST_BLOCK
                           ? execute_compile_time_block(cs, body, ZR_NULL, &didReturn, &ignored)
                           : execute_compile_time_statement(cs, body, ZR_NULL, &didReturn, &ignored);
            cs->comptimeContext = oldComptimeContext;
            cs->isInCompileTimeContext = oldContext;
            return ok;
        }

        case ZR_COMPILE_TIME_EXPRESSION: {
            SZrTypeValue ignored;
            oldComptimeContext = cs->comptimeContext;
            cs->comptimeContext = ZR_PARSER_COMPTIME_CONTEXT_CHECK;
            TZrBool ok = evaluate_compile_time_expression_internal(cs, body, ZR_NULL, &ignored);
            cs->comptimeContext = oldComptimeContext;
            cs->isInCompileTimeContext = oldContext;
            return ok;
        }
    }

    cs->isInCompileTimeContext = oldContext;
    return ZR_FALSE;
}
