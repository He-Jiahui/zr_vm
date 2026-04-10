//
// Created by Auto on 2025/01/XX.
//

#include "semantic_analyzer_internal.h"

SZrTypePrototypeInfo *find_compiler_type_prototype_inference(SZrCompilerState *cs, SZrString *typeName);
void free_resolved_call_signature(SZrState *state, SZrResolvedCallSignature *signature);
static TZrBool semantic_type_from_ast(SZrState *state,
                                      SZrSemanticAnalyzer *analyzer,
                                      const SZrType *typeNode,
                                      SZrInferredType *result);
static TZrBool semantic_infer_node_type(SZrState *state,
                                        SZrSemanticAnalyzer *analyzer,
                                        SZrAstNode *node,
                                        SZrInferredType *result);

static const TZrChar *semantic_identifier_node_text(SZrAstNode *node) {
    if (node == ZR_NULL || node->type != ZR_AST_IDENTIFIER_LITERAL || node->data.identifier.name == ZR_NULL) {
        return ZR_NULL;
    }

    return semantic_string_native(node->data.identifier.name);
}

static const TZrChar *semantic_member_property_text(SZrAstNode *node) {
    if (node == ZR_NULL || node->type != ZR_AST_MEMBER_EXPRESSION ||
        node->data.memberExpression.computed) {
        return ZR_NULL;
    }

    return semantic_identifier_node_text(node->data.memberExpression.property);
}

static void semantic_add_cannot_infer_exact_type_diagnostic(SZrState *state,
                                                            SZrSemanticAnalyzer *analyzer,
                                                            SZrFileRange location) {
    ZrLanguageServer_SemanticAnalyzer_AddDiagnostic(state,
                                                    analyzer,
                                                    ZR_DIAGNOSTIC_ERROR,
                                                    location,
                                                    "cannot infer exact type",
                                                    "cannot_infer_exact_type");
}

static void semantic_add_initializer_requires_annotation_diagnostic(SZrState *state,
                                                                    SZrSemanticAnalyzer *analyzer,
                                                                    SZrFileRange location) {
    ZrLanguageServer_SemanticAnalyzer_AddDiagnostic(state,
                                                    analyzer,
                                                    ZR_DIAGNOSTIC_ERROR,
                                                    location,
                                                    "initializer requires annotation",
                                                    "initializer_requires_annotation");
}

static TZrBool semantic_text_equals(const TZrChar *value, const TZrChar *expected) {
    return value != ZR_NULL && expected != ZR_NULL && strcmp(value, expected) == 0;
}

typedef struct SZrSemanticTypecheckContextSnapshot {
    SZrTypePrototypeInfo *typePrototype;
    SZrAstNode *typeNode;
    SZrString *typeName;
    SZrAstNode *functionNode;
} SZrSemanticTypecheckContextSnapshot;

static SZrTypeEnvironment *semantic_typecheck_push_runtime_type_binding_scope(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer) {
    SZrTypeEnvironment *savedEnv;
    SZrTypeEnvironment *newEnv;

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL) {
        return ZR_NULL;
    }

    savedEnv = analyzer->compilerState->typeEnv;
    newEnv = ZrParser_TypeEnvironment_New(state);
    if (newEnv == ZR_NULL) {
        return savedEnv;
    }

    newEnv->parent = savedEnv;
    newEnv->semanticContext =
        savedEnv != ZR_NULL ? savedEnv->semanticContext : analyzer->compilerState->semanticContext;
    analyzer->compilerState->typeEnv = newEnv;
    return savedEnv;
}

static void semantic_typecheck_pop_runtime_type_binding_scope(SZrState *state,
                                                              SZrSemanticAnalyzer *analyzer,
                                                              SZrTypeEnvironment *savedEnv) {
    SZrTypeEnvironment *currentEnv;

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL) {
        return;
    }

    currentEnv = analyzer->compilerState->typeEnv;
    if (currentEnv == savedEnv) {
        return;
    }

    analyzer->compilerState->typeEnv = savedEnv;
    if (currentEnv != ZR_NULL) {
        ZrParser_TypeEnvironment_Free(state, currentEnv);
    }
}

static void semantic_typecheck_register_variable_binding(SZrState *state,
                                                         SZrSemanticAnalyzer *analyzer,
                                                         SZrString *name,
                                                         const SZrType *typeNode,
                                                         SZrAstNode *valueNode) {
    SZrInferredType bindingType;

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL ||
        analyzer->compilerState->typeEnv == ZR_NULL || name == ZR_NULL) {
        return;
    }

    ZrParser_InferredType_Init(state, &bindingType, ZR_VALUE_TYPE_OBJECT);
    if (typeNode != ZR_NULL) {
        semantic_type_from_ast(state, analyzer, typeNode, &bindingType);
    } else if (valueNode != ZR_NULL) {
        if (!semantic_infer_node_type(state, analyzer, valueNode, &bindingType)) {
            if (analyzer->compilerState != ZR_NULL && analyzer->compilerState->hasError) {
                ZrLanguageServer_SemanticAnalyzer_ConsumeCompilerErrorDiagnostic(state, analyzer, valueNode->location);
            }
            semantic_add_initializer_requires_annotation_diagnostic(state, analyzer, valueNode->location);
            ZrParser_InferredType_Free(state, &bindingType);
            return;
        }
    }

    ZrParser_TypeEnvironment_RegisterVariable(state, analyzer->compilerState->typeEnv, name, &bindingType);
    ZrParser_InferredType_Free(state, &bindingType);
}

static void semantic_typecheck_register_parameter_bindings(SZrState *state,
                                                           SZrSemanticAnalyzer *analyzer,
                                                           SZrAstNodeArray *params) {
    if (state == ZR_NULL || analyzer == ZR_NULL || params == ZR_NULL || params->nodes == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < params->count; index++) {
        SZrAstNode *paramNode = params->nodes[index];
        SZrParameter *param;

        if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
            continue;
        }

        param = &paramNode->data.parameter;
        semantic_typecheck_register_variable_binding(state,
                                                     analyzer,
                                                     param->name != ZR_NULL ? param->name->name : ZR_NULL,
                                                     param->typeInfo,
                                                     ZR_NULL);
    }
}

static SZrString *semantic_typecheck_extract_owner_type_name(SZrAstNode *ownerTypeNode) {
    if (ownerTypeNode == ZR_NULL) {
        return ZR_NULL;
    }

    switch (ownerTypeNode->type) {
        case ZR_AST_CLASS_DECLARATION:
            return ownerTypeNode->data.classDeclaration.name != ZR_NULL
                       ? ownerTypeNode->data.classDeclaration.name->name
                       : ZR_NULL;

        case ZR_AST_STRUCT_DECLARATION:
            return ownerTypeNode->data.structDeclaration.name != ZR_NULL
                       ? ownerTypeNode->data.structDeclaration.name->name
                       : ZR_NULL;

        case ZR_AST_INTERFACE_DECLARATION:
            return ownerTypeNode->data.interfaceDeclaration.name != ZR_NULL
                       ? ownerTypeNode->data.interfaceDeclaration.name->name
                       : ZR_NULL;

        default:
            return ZR_NULL;
    }
}

static void semantic_typecheck_push_compiler_context(SZrSemanticAnalyzer *analyzer,
                                                     SZrAstNode *ownerTypeNode,
                                                     SZrAstNode *functionNode,
                                                     SZrSemanticTypecheckContextSnapshot *snapshot) {
    SZrCompilerState *compilerState;
    SZrString *typeName;

    if (snapshot != ZR_NULL) {
        memset(snapshot, 0, sizeof(*snapshot));
    }

    if (analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL || snapshot == ZR_NULL) {
        return;
    }

    compilerState = analyzer->compilerState;
    snapshot->typePrototype = compilerState->currentTypePrototypeInfo;
    snapshot->typeNode = compilerState->currentTypeNode;
    snapshot->typeName = compilerState->currentTypeName;
    snapshot->functionNode = compilerState->currentFunctionNode;

    typeName = semantic_typecheck_extract_owner_type_name(ownerTypeNode);
    if (typeName != ZR_NULL) {
        compilerState->currentTypeNode = ownerTypeNode;
        compilerState->currentTypeName = typeName;
        compilerState->currentTypePrototypeInfo =
            find_compiler_type_prototype_inference(compilerState, typeName);
    }

    if (functionNode != ZR_NULL) {
        compilerState->currentFunctionNode = functionNode;
    }
}

static void semantic_typecheck_pop_compiler_context(
        SZrSemanticAnalyzer *analyzer,
        const SZrSemanticTypecheckContextSnapshot *snapshot) {
    SZrCompilerState *compilerState;

    if (analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL || snapshot == ZR_NULL) {
        return;
    }

    compilerState = analyzer->compilerState;
    compilerState->currentTypePrototypeInfo = snapshot->typePrototype;
    compilerState->currentTypeNode = snapshot->typeNode;
    compilerState->currentTypeName = snapshot->typeName;
    compilerState->currentFunctionNode = snapshot->functionNode;
}

static TZrBool semantic_extract_ffi_decorator(SZrAstNode *decoratorNode,
                                              const TZrChar **outLeafName,
                                              TZrBool *outHasCall,
                                              SZrFunctionCall **outCall) {
    SZrAstNode *expr;
    SZrPrimaryExpression *primary;
    SZrAstNode *ffiMember;
    SZrAstNode *leafMember;

    if (outLeafName != ZR_NULL) {
        *outLeafName = ZR_NULL;
    }
    if (outHasCall != ZR_NULL) {
        *outHasCall = ZR_FALSE;
    }
    if (outCall != ZR_NULL) {
        *outCall = ZR_NULL;
    }

    if (decoratorNode == ZR_NULL || decoratorNode->type != ZR_AST_DECORATOR_EXPRESSION) {
        return ZR_FALSE;
    }

    expr = decoratorNode->data.decoratorExpression.expr;
    if (expr == ZR_NULL || expr->type != ZR_AST_PRIMARY_EXPRESSION) {
        return ZR_FALSE;
    }

    primary = &expr->data.primaryExpression;
    if (!semantic_text_equals(semantic_identifier_node_text(primary->property), "zr") ||
        primary->members == ZR_NULL || primary->members->count < 2 || primary->members->count > 3) {
        return ZR_FALSE;
    }

    ffiMember = primary->members->nodes[0];
    leafMember = primary->members->nodes[1];
    if (!semantic_text_equals(semantic_member_property_text(ffiMember), "ffi")) {
        return ZR_FALSE;
    }

    if (outLeafName != ZR_NULL) {
        *outLeafName = semantic_member_property_text(leafMember);
    }
    if (outLeafName != ZR_NULL && *outLeafName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (primary->members->count == 3) {
        SZrAstNode *callNode = primary->members->nodes[2];
        if (callNode == ZR_NULL || callNode->type != ZR_AST_FUNCTION_CALL) {
            return ZR_FALSE;
        }
        if (outHasCall != ZR_NULL) {
            *outHasCall = ZR_TRUE;
        }
        if (outCall != ZR_NULL) {
            *outCall = &callNode->data.functionCall;
        }
    }

    return ZR_TRUE;
}

static TZrBool semantic_call_has_single_string_arg(SZrFunctionCall *call, const TZrChar **outValue) {
    SZrAstNode *arg;

    if (outValue != ZR_NULL) {
        *outValue = ZR_NULL;
    }
    if (call == ZR_NULL || call->args == ZR_NULL || call->args->count != 1) {
        return ZR_FALSE;
    }

    arg = call->args->nodes[0];
    if (arg == ZR_NULL || arg->type != ZR_AST_STRING_LITERAL || arg->data.stringLiteral.value == ZR_NULL) {
        return ZR_FALSE;
    }

    if (outValue != ZR_NULL) {
        *outValue = semantic_string_native(arg->data.stringLiteral.value);
    }
    return ZR_TRUE;
}

static TZrBool semantic_call_has_single_integer_arg(SZrFunctionCall *call) {
    return call != ZR_NULL && call->args != ZR_NULL && call->args->count == 1 &&
           call->args->nodes[0] != ZR_NULL &&
           call->args->nodes[0]->type == ZR_AST_INTEGER_LITERAL;
}

static TZrBool semantic_text_in_set(const TZrChar *value, const TZrChar *const *allowedValues, TZrSize count) {
    TZrSize index;

    if (value == ZR_NULL || allowedValues == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < count; index++) {
        if (semantic_text_equals(value, allowedValues[index])) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void semantic_add_invalid_decorator(SZrState *state,
                                           SZrSemanticAnalyzer *analyzer,
                                           SZrAstNode *decoratorNode,
                                           const TZrChar *message) {
    if (state == ZR_NULL || analyzer == ZR_NULL || decoratorNode == ZR_NULL) {
        return;
    }

    ZrLanguageServer_SemanticAnalyzer_AddDiagnostic(state,
                                                    analyzer,
                                                    ZR_DIAGNOSTIC_ERROR,
                                                    decoratorNode->location,
                                                    message,
                                                    "invalid_decorator");
}

static void semantic_validate_extern_callable_decorators(SZrState *state,
                                                         SZrSemanticAnalyzer *analyzer,
                                                         SZrAstNodeArray *decorators,
                                                         const TZrChar *targetName) {
    static const TZrChar *const allowedCallconvs[] = {"cdecl", "stdcall", "system"};
    static const TZrChar *const allowedCharsets[] = {"utf8", "utf16", "ansi"};
    TZrSize index;

    if (decorators == ZR_NULL) {
        return;
    }

    for (index = 0; index < decorators->count; index++) {
        const TZrChar *leafName = ZR_NULL;
        TZrBool hasCall = ZR_FALSE;
        SZrFunctionCall *call = ZR_NULL;
        const TZrChar *stringArg = ZR_NULL;
        SZrAstNode *decoratorNode = decorators->nodes[index];

        if (!semantic_extract_ffi_decorator(decoratorNode, &leafName, &hasCall, &call) || leafName == ZR_NULL) {
            continue;
        }

        if (semantic_text_equals(leafName, "entry")) {
            if (!hasCall || !semantic_call_has_single_string_arg(call, ZR_NULL)) {
                semantic_add_invalid_decorator(state, analyzer, decoratorNode,
                                               "zr.ffi.entry requires a single string argument");
            }
        } else if (semantic_text_equals(leafName, "callconv")) {
            if (!hasCall || !semantic_call_has_single_string_arg(call, &stringArg) ||
                !semantic_text_in_set(stringArg, allowedCallconvs, 3)) {
                semantic_add_invalid_decorator(state, analyzer, decoratorNode,
                                               "zr.ffi.callconv requires one of: cdecl, stdcall, system");
            }
        } else if (semantic_text_equals(leafName, "charset")) {
            if (!hasCall || !semantic_call_has_single_string_arg(call, &stringArg) ||
                !semantic_text_in_set(stringArg, allowedCharsets, 3)) {
                semantic_add_invalid_decorator(state, analyzer, decoratorNode,
                                               "zr.ffi.charset requires one of: utf8, utf16, ansi");
            }
        } else {
            TZrChar buffer[ZR_LSP_TYPE_BUFFER_LENGTH];
            snprintf(buffer, sizeof(buffer), "zr.ffi.%s is not valid on %s", leafName, targetName);
            semantic_add_invalid_decorator(state, analyzer, decoratorNode, buffer);
        }
    }
}

static void semantic_validate_extern_struct_decorators(SZrState *state,
                                                       SZrSemanticAnalyzer *analyzer,
                                                       SZrAstNodeArray *decorators) {
    TZrSize index;

    if (decorators == ZR_NULL) {
        return;
    }

    for (index = 0; index < decorators->count; index++) {
        const TZrChar *leafName = ZR_NULL;
        TZrBool hasCall = ZR_FALSE;
        SZrFunctionCall *call = ZR_NULL;
        SZrAstNode *decoratorNode = decorators->nodes[index];

        if (!semantic_extract_ffi_decorator(decoratorNode, &leafName, &hasCall, &call) || leafName == ZR_NULL) {
            continue;
        }

        if ((semantic_text_equals(leafName, "pack") || semantic_text_equals(leafName, "align")) &&
            hasCall && semantic_call_has_single_integer_arg(call)) {
            continue;
        }

        if (semantic_text_equals(leafName, "pack") || semantic_text_equals(leafName, "align")) {
            semantic_add_invalid_decorator(state, analyzer, decoratorNode,
                                           "zr.ffi.pack/align require a single integer argument");
        } else {
            TZrChar buffer[ZR_LSP_TYPE_BUFFER_LENGTH];
            snprintf(buffer, sizeof(buffer), "zr.ffi.%s is not valid on extern struct declarations", leafName);
            semantic_add_invalid_decorator(state, analyzer, decoratorNode, buffer);
        }
    }
}

static void semantic_validate_extern_struct_field_decorators(SZrState *state,
                                                             SZrSemanticAnalyzer *analyzer,
                                                             SZrAstNodeArray *decorators) {
    TZrSize index;

    if (decorators == ZR_NULL) {
        return;
    }

    for (index = 0; index < decorators->count; index++) {
        const TZrChar *leafName = ZR_NULL;
        TZrBool hasCall = ZR_FALSE;
        SZrFunctionCall *call = ZR_NULL;
        SZrAstNode *decoratorNode = decorators->nodes[index];

        if (!semantic_extract_ffi_decorator(decoratorNode, &leafName, &hasCall, &call) || leafName == ZR_NULL) {
            continue;
        }

        if (semantic_text_equals(leafName, "offset") && hasCall && semantic_call_has_single_integer_arg(call)) {
            continue;
        }

        if (semantic_text_equals(leafName, "offset")) {
            semantic_add_invalid_decorator(state, analyzer, decoratorNode,
                                           "zr.ffi.offset requires a single integer argument");
        } else {
            TZrChar buffer[ZR_LSP_TYPE_BUFFER_LENGTH];
            snprintf(buffer, sizeof(buffer), "zr.ffi.%s is not valid on extern struct fields", leafName);
            semantic_add_invalid_decorator(state, analyzer, decoratorNode, buffer);
        }
    }
}

static void semantic_validate_extern_enum_decorators(SZrState *state,
                                                     SZrSemanticAnalyzer *analyzer,
                                                     SZrAstNodeArray *decorators) {
    TZrSize index;

    if (decorators == ZR_NULL) {
        return;
    }

    for (index = 0; index < decorators->count; index++) {
        const TZrChar *leafName = ZR_NULL;
        TZrBool hasCall = ZR_FALSE;
        SZrFunctionCall *call = ZR_NULL;
        SZrAstNode *decoratorNode = decorators->nodes[index];

        if (!semantic_extract_ffi_decorator(decoratorNode, &leafName, &hasCall, &call) || leafName == ZR_NULL) {
            continue;
        }

        if (semantic_text_equals(leafName, "underlying") && hasCall &&
            semantic_call_has_single_string_arg(call, ZR_NULL)) {
            continue;
        }

        if (semantic_text_equals(leafName, "underlying")) {
            semantic_add_invalid_decorator(state, analyzer, decoratorNode,
                                           "zr.ffi.underlying requires a single string argument");
        } else {
            TZrChar buffer[ZR_LSP_TYPE_BUFFER_LENGTH];
            snprintf(buffer, sizeof(buffer), "zr.ffi.%s is not valid on extern enum declarations", leafName);
            semantic_add_invalid_decorator(state, analyzer, decoratorNode, buffer);
        }
    }
}

static void semantic_validate_extern_enum_member_decorators(SZrState *state,
                                                            SZrSemanticAnalyzer *analyzer,
                                                            SZrAstNodeArray *decorators) {
    TZrSize index;

    if (decorators == ZR_NULL) {
        return;
    }

    for (index = 0; index < decorators->count; index++) {
        const TZrChar *leafName = ZR_NULL;
        TZrBool hasCall = ZR_FALSE;
        SZrFunctionCall *call = ZR_NULL;
        SZrAstNode *decoratorNode = decorators->nodes[index];

        if (!semantic_extract_ffi_decorator(decoratorNode, &leafName, &hasCall, &call) || leafName == ZR_NULL) {
            continue;
        }

        if (semantic_text_equals(leafName, "value") && hasCall && semantic_call_has_single_integer_arg(call)) {
            continue;
        }

        if (semantic_text_equals(leafName, "value")) {
            semantic_add_invalid_decorator(state, analyzer, decoratorNode,
                                           "zr.ffi.value requires a single integer argument");
        } else {
            TZrChar buffer[ZR_LSP_TYPE_BUFFER_LENGTH];
            snprintf(buffer, sizeof(buffer), "zr.ffi.%s is not valid on extern enum members", leafName);
            semantic_add_invalid_decorator(state, analyzer, decoratorNode, buffer);
        }
    }
}

static TZrBool semantic_ffi_integer_type_name_supported(const TZrChar *typeName) {
    static const TZrChar *const kSupportedIntegerTypeNames[] = {
            "i8", "u8", "i16", "u16", "i32", "u32", "i64", "u64",
    };

    if (typeName == ZR_NULL) {
        return ZR_FALSE;
    }

    return semantic_text_in_set(typeName, kSupportedIntegerTypeNames, ZR_ARRAY_COUNT(kSupportedIntegerTypeNames));
}

static TZrBool semantic_view_type_is_source_extern_struct(SZrSemanticAnalyzer *analyzer, const TZrChar *typeName) {
    SZrScript *script;

    if (analyzer == ZR_NULL || analyzer->ast == ZR_NULL || typeName == ZR_NULL ||
        analyzer->ast->type != ZR_AST_SCRIPT) {
        return ZR_FALSE;
    }

    script = &analyzer->ast->data.script;
    if (script->statements == ZR_NULL || script->statements->nodes == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize statementIndex = 0; statementIndex < script->statements->count; statementIndex++) {
        SZrAstNode *statement = script->statements->nodes[statementIndex];

        if (statement == ZR_NULL || statement->type != ZR_AST_EXTERN_BLOCK ||
            statement->data.externBlock.declarations == ZR_NULL ||
            statement->data.externBlock.declarations->nodes == ZR_NULL) {
            continue;
        }

        for (TZrSize declarationIndex = 0;
             declarationIndex < statement->data.externBlock.declarations->count;
             declarationIndex++) {
            SZrAstNode *declaration = statement->data.externBlock.declarations->nodes[declarationIndex];

            if (declaration == ZR_NULL || declaration->type != ZR_AST_STRUCT_DECLARATION ||
                declaration->data.structDeclaration.name == ZR_NULL ||
                declaration->data.structDeclaration.name->name == ZR_NULL) {
                continue;
            }

            if (semantic_text_equals(semantic_string_native(declaration->data.structDeclaration.name->name), typeName)) {
                return ZR_TRUE;
            }
        }
    }

    return ZR_FALSE;
}

static void semantic_validate_class_wrapper_decorators(SZrState *state,
                                                       SZrSemanticAnalyzer *analyzer,
                                                       SZrAstNodeArray *decorators) {
    static const TZrChar *const allowedLowerings[] = {"value", "pointer", "handle_id"};
    static const TZrChar *const allowedOwnerModes[] = {"borrowed", "owned"};
    SZrAstNode *loweringDecoratorNode = ZR_NULL;
    SZrAstNode *viewTypeDecoratorNode = ZR_NULL;
    SZrAstNode *underlyingDecoratorNode = ZR_NULL;
    const TZrChar *viewTypeName = ZR_NULL;
    const TZrChar *underlyingTypeName = ZR_NULL;
    TZrBool loweringIsHandleId = ZR_FALSE;
    TZrBool loweringWasValid = ZR_FALSE;
    TZrBool viewTypeWasValid = ZR_FALSE;
    TZrBool underlyingWasValid = ZR_FALSE;
    TZrSize index;

    if (decorators == ZR_NULL) {
        return;
    }

    for (index = 0; index < decorators->count; index++) {
        const TZrChar *leafName = ZR_NULL;
        TZrBool hasCall = ZR_FALSE;
        SZrFunctionCall *call = ZR_NULL;
        const TZrChar *stringArg = ZR_NULL;
        SZrAstNode *decoratorNode = decorators->nodes[index];

        if (!semantic_extract_ffi_decorator(decoratorNode, &leafName, &hasCall, &call) || leafName == ZR_NULL) {
            continue;
        }

        if (semantic_text_equals(leafName, "lowering")) {
            loweringDecoratorNode = decoratorNode;
            if (!hasCall || !semantic_call_has_single_string_arg(call, &stringArg) ||
                !semantic_text_in_set(stringArg, allowedLowerings, ZR_ARRAY_COUNT(allowedLowerings))) {
                semantic_add_invalid_decorator(state,
                                               analyzer,
                                               decoratorNode,
                                               "zr.ffi.lowering requires one of: value, pointer, handle_id");
            } else {
                loweringWasValid = ZR_TRUE;
                loweringIsHandleId = semantic_text_equals(stringArg, "handle_id");
            }
        } else if (semantic_text_equals(leafName, "viewType")) {
            viewTypeDecoratorNode = decoratorNode;
            if (!hasCall || !semantic_call_has_single_string_arg(call, &stringArg)) {
                semantic_add_invalid_decorator(state, analyzer, decoratorNode,
                                               "zr.ffi.viewType requires a single string argument");
            } else {
                viewTypeWasValid = ZR_TRUE;
                viewTypeName = stringArg;
            }
        } else if (semantic_text_equals(leafName, "underlying")) {
            underlyingDecoratorNode = decoratorNode;
            if (!hasCall || !semantic_call_has_single_string_arg(call, &stringArg)) {
                semantic_add_invalid_decorator(state, analyzer, decoratorNode,
                                               "zr.ffi.underlying requires a single string argument");
            } else {
                underlyingWasValid = ZR_TRUE;
                underlyingTypeName = stringArg;
            }
        } else if (semantic_text_equals(leafName, "ownerMode")) {
            if (!hasCall || !semantic_call_has_single_string_arg(call, &stringArg) ||
                !semantic_text_in_set(stringArg, allowedOwnerModes, ZR_ARRAY_COUNT(allowedOwnerModes))) {
                semantic_add_invalid_decorator(state, analyzer, decoratorNode,
                                               "zr.ffi.ownerMode requires one of: borrowed, owned");
            }
        } else if (semantic_text_equals(leafName, "releaseHook")) {
            if (!hasCall || !semantic_call_has_single_string_arg(call, ZR_NULL)) {
                semantic_add_invalid_decorator(state, analyzer, decoratorNode,
                                               "zr.ffi.releaseHook requires a single string argument");
            }
        } else {
            TZrChar buffer[ZR_LSP_TYPE_BUFFER_LENGTH];
            snprintf(buffer, sizeof(buffer), "zr.ffi.%s is not valid on class declarations", leafName);
            semantic_add_invalid_decorator(state, analyzer, decoratorNode, buffer);
        }
    }

    if (underlyingWasValid && (!loweringWasValid || !loweringIsHandleId) && underlyingDecoratorNode != ZR_NULL) {
        semantic_add_invalid_decorator(state,
                                       analyzer,
                                       underlyingDecoratorNode,
                                       "zr.ffi.underlying on class wrappers requires zr.ffi.lowering(\"handle_id\")");
    }

    if (loweringWasValid && loweringIsHandleId && !underlyingWasValid && loweringDecoratorNode != ZR_NULL) {
        semantic_add_invalid_decorator(state,
                                       analyzer,
                                       loweringDecoratorNode,
                                       "zr.ffi.lowering(\"handle_id\") requires zr.ffi.underlying(...)");
    }

    if (loweringWasValid && loweringIsHandleId && underlyingWasValid &&
        !semantic_ffi_integer_type_name_supported(underlyingTypeName) && underlyingDecoratorNode != ZR_NULL) {
        semantic_add_invalid_decorator(state,
                                       analyzer,
                                       underlyingDecoratorNode,
                                       "zr.ffi.underlying on class wrappers requires a supported integer type name: i8, u8, i16, u16, i32, u32, i64, u64");
    }

    if (viewTypeWasValid && !semantic_view_type_is_source_extern_struct(analyzer, viewTypeName) &&
        viewTypeDecoratorNode != ZR_NULL) {
        semantic_add_invalid_decorator(state,
                                       analyzer,
                                       viewTypeDecoratorNode,
                                       "zr.ffi.viewType on class wrappers requires a source extern struct name");
    }
}

static void semantic_validate_extern_parameter_decorators(SZrState *state,
                                                          SZrSemanticAnalyzer *analyzer,
                                                          SZrAstNode *parameterNode) {
    SZrAstNodeArray *decorators;
    TZrSize index;
    TZrSize directionCount = 0;

    if (parameterNode == ZR_NULL || parameterNode->type != ZR_AST_PARAMETER) {
        return;
    }

    decorators = parameterNode->data.parameter.decorators;
    if (decorators == ZR_NULL) {
        return;
    }

    for (index = 0; index < decorators->count; index++) {
        const TZrChar *leafName = ZR_NULL;
        TZrBool hasCall = ZR_FALSE;
        SZrFunctionCall *call = ZR_NULL;
        SZrAstNode *decoratorNode = decorators->nodes[index];

        if (!semantic_extract_ffi_decorator(decoratorNode, &leafName, &hasCall, &call) || leafName == ZR_NULL) {
            continue;
        }

        if (semantic_text_equals(leafName, "in") ||
            semantic_text_equals(leafName, "out") ||
            semantic_text_equals(leafName, "inout")) {
            if (hasCall || call != ZR_NULL) {
                semantic_add_invalid_decorator(state, analyzer, decoratorNode,
                                               "FFI direction decorators do not take arguments");
            } else {
                directionCount++;
            }
        } else {
            TZrChar buffer[ZR_LSP_TYPE_BUFFER_LENGTH];
            snprintf(buffer, sizeof(buffer), "zr.ffi.%s is not valid on extern parameters", leafName);
            semantic_add_invalid_decorator(state, analyzer, decoratorNode, buffer);
        }
    }

    if (directionCount > 1) {
        ZrLanguageServer_SemanticAnalyzer_AddDiagnostic(state,
                                                        analyzer,
                                                        ZR_DIAGNOSTIC_ERROR,
                                                        parameterNode->location,
                                                        "Extern parameters may specify only one of zr.ffi.in/out/inout",
                                                        "invalid_decorator");
    }
}

static void semantic_add_type_mismatch_diagnostic(SZrState *state,
                                                  SZrSemanticAnalyzer *analyzer,
                                                  SZrFileRange location,
                                                  const TZrChar *message) {
    ZrLanguageServer_SemanticAnalyzer_AddDiagnostic(state,
                                                    analyzer,
                                                    ZR_DIAGNOSTIC_ERROR,
                                                    location,
                                                    message != ZR_NULL ? message : "Type mismatch",
                                                    "type_mismatch");
}

typedef enum EZrSemanticVariancePosition {
    ZR_SEMANTIC_VARIANCE_POSITION_INVARIANT = 0,
    ZR_SEMANTIC_VARIANCE_POSITION_OUT = 1,
    ZR_SEMANTIC_VARIANCE_POSITION_IN = -1,
} EZrSemanticVariancePosition;

static EZrGenericVariance semantic_interface_generic_variance_for_name(SZrAstNodeArray *params, SZrString *name) {
    if (params == ZR_NULL || name == ZR_NULL) {
        return ZR_GENERIC_VARIANCE_NONE;
    }

    for (TZrSize index = 0; index < params->count; index++) {
        SZrAstNode *paramNode = params->nodes[index];
        if (paramNode == ZR_NULL ||
            paramNode->type != ZR_AST_PARAMETER ||
            paramNode->data.parameter.name == ZR_NULL ||
            paramNode->data.parameter.name->name == ZR_NULL) {
            continue;
        }

        if (ZrCore_String_Equal(paramNode->data.parameter.name->name, name)) {
            return paramNode->data.parameter.variance;
        }
    }

    return ZR_GENERIC_VARIANCE_NONE;
}

static SZrInterfaceDeclaration *semantic_lookup_interface_declaration(SZrSemanticAnalyzer *analyzer,
                                                                      SZrString *typeName) {
    SZrSymbol *symbol;

    if (analyzer == ZR_NULL || analyzer->symbolTable == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    symbol = ZrLanguageServer_SymbolTable_Lookup(analyzer->symbolTable, typeName, ZR_NULL);
    if (symbol == ZR_NULL ||
        symbol->type != ZR_SYMBOL_INTERFACE ||
        symbol->astNode == ZR_NULL ||
        symbol->astNode->type != ZR_AST_INTERFACE_DECLARATION) {
        return ZR_NULL;
    }

    return &symbol->astNode->data.interfaceDeclaration;
}

static EZrGenericVariance semantic_interface_decl_variance_at(SZrSemanticAnalyzer *analyzer,
                                                              SZrString *typeName,
                                                              TZrSize index) {
    SZrInterfaceDeclaration *interfaceDecl = semantic_lookup_interface_declaration(analyzer, typeName);
    SZrAstNode *paramNode;

    if (interfaceDecl == ZR_NULL ||
        interfaceDecl->generic == ZR_NULL ||
        interfaceDecl->generic->params == ZR_NULL ||
        index >= interfaceDecl->generic->params->count) {
        return ZR_GENERIC_VARIANCE_NONE;
    }

    paramNode = interfaceDecl->generic->params->nodes[index];
    if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
        return ZR_GENERIC_VARIANCE_NONE;
    }

    return paramNode->data.parameter.variance;
}

static EZrSemanticVariancePosition semantic_combine_variance_position(EZrSemanticVariancePosition outerPosition,
                                                                      EZrGenericVariance declaredVariance) {
    if (outerPosition == ZR_SEMANTIC_VARIANCE_POSITION_INVARIANT ||
        declaredVariance == ZR_GENERIC_VARIANCE_NONE) {
        return ZR_SEMANTIC_VARIANCE_POSITION_INVARIANT;
    }

    if (declaredVariance == ZR_GENERIC_VARIANCE_OUT) {
        return outerPosition;
    }

    return outerPosition == ZR_SEMANTIC_VARIANCE_POSITION_OUT
           ? ZR_SEMANTIC_VARIANCE_POSITION_IN
           : ZR_SEMANTIC_VARIANCE_POSITION_OUT;
}

static void semantic_add_invalid_variance_diagnostic(SZrState *state,
                                                     SZrSemanticAnalyzer *analyzer,
                                                     SZrString *parameterName,
                                                     EZrGenericVariance declaredVariance,
                                                     const TZrChar *context,
                                                     TZrBool nestedUsage,
                                                     SZrFileRange location) {
    TZrChar buffer[ZR_LSP_TEXT_BUFFER_LENGTH];
    const TZrChar *nameText = semantic_string_native(parameterName);

    if (state == ZR_NULL || analyzer == ZR_NULL || parameterName == ZR_NULL || context == ZR_NULL) {
        return;
    }

    snprintf(buffer,
             sizeof(buffer),
             "%s generic parameter '%s' cannot be used in %s%s position",
             declaredVariance == ZR_GENERIC_VARIANCE_OUT ? "covariant" : "contravariant",
             nameText != ZR_NULL ? nameText : "<unknown>",
             nestedUsage ? "nested " : "",
             context);
    ZrLanguageServer_SemanticAnalyzer_AddDiagnostic(state,
                                                    analyzer,
                                                    ZR_DIAGNOSTIC_ERROR,
                                                    location,
                                                    buffer,
                                                    "invalid_variance");
}

static void semantic_validate_interface_type_variance(SZrState *state,
                                                      SZrSemanticAnalyzer *analyzer,
                                                      SZrAstNodeArray *interfaceGenericParams,
                                                      SZrType *typeNode,
                                                      EZrSemanticVariancePosition position,
                                                      const TZrChar *context,
                                                      TZrBool nestedUsage,
                                                      SZrFileRange location) {
    SZrString *typeName = ZR_NULL;
    EZrGenericVariance declaredVariance;

    if (state == ZR_NULL || analyzer == ZR_NULL || interfaceGenericParams == ZR_NULL || typeNode == ZR_NULL) {
        return;
    }

    if (typeNode->subType != ZR_NULL) {
        semantic_validate_interface_type_variance(state,
                                                 analyzer,
                                                 interfaceGenericParams,
                                                 typeNode->subType,
                                                 ZR_SEMANTIC_VARIANCE_POSITION_INVARIANT,
                                                 context,
                                                 nestedUsage,
                                                 location);
    }

    if (typeNode->name == ZR_NULL) {
        return;
    }

    if (typeNode->name->type == ZR_AST_IDENTIFIER_LITERAL) {
        typeName = typeNode->name->data.identifier.name;
        declaredVariance = semantic_interface_generic_variance_for_name(interfaceGenericParams, typeName);
        if (declaredVariance == ZR_GENERIC_VARIANCE_NONE) {
            return;
        }

        if (position == ZR_SEMANTIC_VARIANCE_POSITION_INVARIANT ||
            (declaredVariance == ZR_GENERIC_VARIANCE_OUT && position == ZR_SEMANTIC_VARIANCE_POSITION_IN) ||
            (declaredVariance == ZR_GENERIC_VARIANCE_IN && position == ZR_SEMANTIC_VARIANCE_POSITION_OUT)) {
            semantic_add_invalid_variance_diagnostic(state,
                                                     analyzer,
                                                     typeName,
                                                     declaredVariance,
                                                     context,
                                                     nestedUsage,
                                                     location);
        }
        return;
    }

    if (typeNode->name->type == ZR_AST_GENERIC_TYPE) {
        SZrGenericType *genericType = &typeNode->name->data.genericType;
        SZrString *outerTypeName = genericType->name != ZR_NULL ? genericType->name->name : ZR_NULL;

        if (genericType->params == ZR_NULL) {
            return;
        }

        for (TZrSize index = 0; index < genericType->params->count; index++) {
            SZrAstNode *argNode = genericType->params->nodes[index];
            EZrGenericVariance declaredOuterVariance =
                    semantic_interface_decl_variance_at(analyzer, outerTypeName, index);
            EZrSemanticVariancePosition childPosition =
                    semantic_combine_variance_position(position, declaredOuterVariance);

            if (argNode != ZR_NULL && argNode->type == ZR_AST_TYPE) {
                semantic_validate_interface_type_variance(state,
                                                         analyzer,
                                                         interfaceGenericParams,
                                                         &argNode->data.type,
                                                         childPosition,
                                                         context,
                                                         ZR_TRUE,
                                                         location);
            }
        }
    }
}

static void semantic_validate_interface_variance_rules(SZrState *state,
                                                       SZrSemanticAnalyzer *analyzer,
                                                       SZrAstNode *interfaceNode) {
    SZrInterfaceDeclaration *interfaceDecl;

    if (state == ZR_NULL ||
        analyzer == ZR_NULL ||
        interfaceNode == ZR_NULL ||
        interfaceNode->type != ZR_AST_INTERFACE_DECLARATION) {
        return;
    }

    interfaceDecl = &interfaceNode->data.interfaceDeclaration;
    if (interfaceDecl->generic == ZR_NULL ||
        interfaceDecl->generic->params == ZR_NULL ||
        interfaceDecl->members == ZR_NULL) {
        return;
    }

    for (TZrSize memberIndex = 0; memberIndex < interfaceDecl->members->count; memberIndex++) {
        SZrAstNode *member = interfaceDecl->members->nodes[memberIndex];

        if (member == ZR_NULL) {
            continue;
        }

        switch (member->type) {
            case ZR_AST_INTERFACE_FIELD_DECLARATION:
                semantic_validate_interface_type_variance(state,
                                                         analyzer,
                                                         interfaceDecl->generic->params,
                                                         member->data.interfaceFieldDeclaration.typeInfo,
                                                         ZR_SEMANTIC_VARIANCE_POSITION_INVARIANT,
                                                         "field",
                                                         ZR_FALSE,
                                                         member->location);
                break;

            case ZR_AST_INTERFACE_METHOD_SIGNATURE:
                if (member->data.interfaceMethodSignature.params != ZR_NULL) {
                    for (TZrSize paramIndex = 0;
                         paramIndex < member->data.interfaceMethodSignature.params->count;
                         paramIndex++) {
                        SZrAstNode *paramNode = member->data.interfaceMethodSignature.params->nodes[paramIndex];
                        if (paramNode != ZR_NULL && paramNode->type == ZR_AST_PARAMETER) {
                            semantic_validate_interface_type_variance(state,
                                                                     analyzer,
                                                                     interfaceDecl->generic->params,
                                                                     paramNode->data.parameter.typeInfo,
                                                                     ZR_SEMANTIC_VARIANCE_POSITION_IN,
                                                                     "contravariant parameter",
                                                                     ZR_FALSE,
                                                                     paramNode->location);
                        }
                    }
                }
                semantic_validate_interface_type_variance(state,
                                                         analyzer,
                                                         interfaceDecl->generic->params,
                                                         member->data.interfaceMethodSignature.returnType,
                                                         ZR_SEMANTIC_VARIANCE_POSITION_OUT,
                                                         "covariant return",
                                                         ZR_FALSE,
                                                         member->location);
                break;

            case ZR_AST_INTERFACE_PROPERTY_SIGNATURE: {
                SZrInterfacePropertySignature *property = &member->data.interfacePropertySignature;
                EZrSemanticVariancePosition propertyPosition = ZR_SEMANTIC_VARIANCE_POSITION_INVARIANT;
                const TZrChar *context = "property";

                if (property->hasGet && !property->hasSet) {
                    propertyPosition = ZR_SEMANTIC_VARIANCE_POSITION_OUT;
                    context = "getter";
                } else if (property->hasSet && !property->hasGet) {
                    propertyPosition = ZR_SEMANTIC_VARIANCE_POSITION_IN;
                    context = "setter";
                }

                semantic_validate_interface_type_variance(state,
                                                         analyzer,
                                                         interfaceDecl->generic->params,
                                                         property->typeInfo,
                                                         propertyPosition,
                                                         context,
                                                         ZR_FALSE,
                                                         member->location);
                break;
            }

            case ZR_AST_INTERFACE_META_SIGNATURE:
                if (member->data.interfaceMetaSignature.params != ZR_NULL) {
                    for (TZrSize paramIndex = 0;
                         paramIndex < member->data.interfaceMetaSignature.params->count;
                         paramIndex++) {
                        SZrAstNode *paramNode = member->data.interfaceMetaSignature.params->nodes[paramIndex];
                        if (paramNode != ZR_NULL && paramNode->type == ZR_AST_PARAMETER) {
                            semantic_validate_interface_type_variance(state,
                                                                     analyzer,
                                                                     interfaceDecl->generic->params,
                                                                     paramNode->data.parameter.typeInfo,
                                                                     ZR_SEMANTIC_VARIANCE_POSITION_IN,
                                                                     "contravariant parameter",
                                                                     ZR_FALSE,
                                                                     paramNode->location);
                        }
                    }
                }
                semantic_validate_interface_type_variance(state,
                                                         analyzer,
                                                         interfaceDecl->generic->params,
                                                         member->data.interfaceMetaSignature.returnType,
                                                         ZR_SEMANTIC_VARIANCE_POSITION_OUT,
                                                         "covariant return",
                                                         ZR_FALSE,
                                                         member->location);
                break;

            default:
                break;
        }
    }
}

static TZrBool semantic_type_from_ast(SZrState *state,
                                      SZrSemanticAnalyzer *analyzer,
                                      const SZrType *typeNode,
                                      SZrInferredType *result) {
    if (state == ZR_NULL || analyzer == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (typeNode != ZR_NULL &&
        analyzer->compilerState != ZR_NULL &&
        ZrParser_AstTypeToInferredType_Convert(analyzer->compilerState, typeNode, result)) {
        return ZR_TRUE;
    }

    if (typeNode != ZR_NULL) {
        ZrLanguageServer_SemanticAnalyzer_ConsumeCompilerErrorDiagnostic(state,
                                                                         analyzer,
                                                                         typeNode->name != ZR_NULL
                                                                                 ? typeNode->name->location
                                                                                 : ZrParser_FileRange_Create(
                                                                                           ZrParser_FilePosition_Create(0, 0, 0),
                                                                                           ZrParser_FilePosition_Create(0, 0, 0),
                                                                                           ZR_NULL));
    }

    ZrParser_InferredType_Init(state, result, ZR_VALUE_TYPE_OBJECT);
    if (typeNode != ZR_NULL) {
        result->ownershipQualifier = typeNode->ownershipQualifier;
    }
    return ZR_TRUE;
}

static TZrBool semantic_infer_node_type(SZrState *state,
                                        SZrSemanticAnalyzer *analyzer,
                                        SZrAstNode *node,
                                        SZrInferredType *result) {
    if (state == ZR_NULL || analyzer == ZR_NULL || node == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZrLanguageServer_SemanticAnalyzer_InferExactExpressionType(state, analyzer, node, result)) {
        return ZR_TRUE;
    }

    if (analyzer->compilerState != ZR_NULL && analyzer->compilerState->hasError) {
        ZrLanguageServer_SemanticAnalyzer_ConsumeCompilerErrorDiagnostic(state, analyzer, node->location);
    }
    return ZR_FALSE;
}

static TZrBool semantic_call_matches_parameters(SZrState *state,
                                                SZrSemanticAnalyzer *analyzer,
                                                SZrAstNodeArray *params,
                                                SZrFunctionCall *call) {
    if (state == ZR_NULL || analyzer == ZR_NULL || call == ZR_NULL) {
        return ZR_FALSE;
    }

    if (params == ZR_NULL) {
        return call->args == ZR_NULL || call->args->count == 0;
    }

    if ((call->args != ZR_NULL ? call->args->count : 0) != params->count) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < params->count; index++) {
        SZrAstNode *paramNode = params->nodes[index];
        SZrAstNode *argNode = call->args != ZR_NULL ? call->args->nodes[index] : ZR_NULL;
        SZrInferredType expectedType;
        SZrInferredType actualType;
        TZrBool compatible;

        if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER || argNode == ZR_NULL) {
            return ZR_FALSE;
        }

        ZrParser_InferredType_Init(state, &expectedType, ZR_VALUE_TYPE_OBJECT);
        ZrParser_InferredType_Init(state, &actualType, ZR_VALUE_TYPE_OBJECT);
        semantic_type_from_ast(state, analyzer, paramNode->data.parameter.typeInfo, &expectedType);
        if (!semantic_infer_node_type(state, analyzer, argNode, &actualType)) {
            ZrParser_InferredType_Free(state, &actualType);
            ZrParser_InferredType_Free(state, &expectedType);
            return ZR_FALSE;
        }
        compatible = analyzer->compilerState != ZR_NULL &&
                     ZrParser_AssignmentCompatibility_Check(analyzer->compilerState,
                                                            &expectedType,
                                                            &actualType,
                                                            argNode->location);
        ZrParser_InferredType_Free(state, &actualType);
        ZrParser_InferredType_Free(state, &expectedType);
        if (!compatible) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool semantic_resolve_named_function_call_in_env(SZrState *state,
                                                           SZrCompilerState *compilerState,
                                                           SZrTypeEnvironment *typeEnv,
                                                           SZrString *name,
                                                           SZrFunctionCall *call,
                                                           SZrFileRange location,
                                                           TZrBool *outHasCandidate) {
    SZrFunctionTypeInfo *resolvedFunction = ZR_NULL;
    SZrResolvedCallSignature resolvedSignature;
    TZrBool compatible;

    if (outHasCandidate != ZR_NULL) {
        *outHasCandidate = ZR_FALSE;
    }

    if (state == ZR_NULL || compilerState == ZR_NULL || typeEnv == ZR_NULL ||
        name == ZR_NULL || call == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrParser_TypeEnvironment_LookupFunction(typeEnv, name, &resolvedFunction)) {
        return ZR_FALSE;
    }

    if (outHasCandidate != ZR_NULL) {
        *outHasCandidate = ZR_TRUE;
    }

    memset(&resolvedSignature, 0, sizeof(resolvedSignature));
    ZrParser_InferredType_Init(state, &resolvedSignature.returnType, ZR_VALUE_TYPE_OBJECT);
    ZrCore_Array_Construct(&resolvedSignature.parameterTypes);
    ZrCore_Array_Construct(&resolvedSignature.parameterPassingModes);

    if (!ZrParser_FunctionCallOverload_Resolve(compilerState,
                                               typeEnv,
                                               name,
                                               call,
                                               location,
                                               &resolvedFunction,
                                               &resolvedSignature)) {
        compilerState->hasError = ZR_FALSE;
        free_resolved_call_signature(state, &resolvedSignature);
        return ZR_FALSE;
    }

    compatible = ZrParser_FunctionCallCompatibility_Check(compilerState,
                                                          typeEnv,
                                                          name,
                                                          call,
                                                          resolvedFunction,
                                                          &resolvedSignature,
                                                          location);
    compilerState->hasError = ZR_FALSE;
    free_resolved_call_signature(state, &resolvedSignature);
    return compatible;
}

static void semantic_check_named_function_call(SZrState *state,
                                               SZrSemanticAnalyzer *analyzer,
                                               SZrString *name,
                                               SZrFunctionCall *call,
                                               SZrFileRange location) {
    SZrCompilerState *compilerState;
    TZrBool hasRuntimeFunction = ZR_FALSE;
    TZrBool hasCompileTimeFunction = ZR_FALSE;

    if (state == ZR_NULL || analyzer == ZR_NULL || name == ZR_NULL || call == ZR_NULL) {
        return;
    }

    compilerState = analyzer->compilerState;
    if (compilerState == ZR_NULL) {
        return;
    }

    if (semantic_resolve_named_function_call_in_env(state,
                                                    compilerState,
                                                    compilerState->typeEnv,
                                                    name,
                                                    call,
                                                    location,
                                                    &hasRuntimeFunction)) {
        return;
    }
    if (hasRuntimeFunction) {
        semantic_add_type_mismatch_diagnostic(state, analyzer, location, "Type mismatch in function call");
        return;
    }

    if (semantic_resolve_named_function_call_in_env(state,
                                                    compilerState,
                                                    compilerState->compileTimeTypeEnv,
                                                    name,
                                                    call,
                                                    location,
                                                    &hasCompileTimeFunction)) {
        return;
    }
    if (hasCompileTimeFunction) {
        semantic_add_type_mismatch_diagnostic(state, analyzer, location, "Type mismatch in function call");
    }
}

static void semantic_check_method_call(SZrState *state,
                                       SZrSemanticAnalyzer *analyzer,
                                       SZrAstNode *receiverNode,
                                       SZrAstNode *memberNode,
                                       SZrFunctionCall *call,
                                       SZrFileRange location) {
    SZrInferredType receiverType;
    SZrSymbol *classSymbol;
    TZrBool sawCandidate = ZR_FALSE;
    TZrBool matchedCandidate = ZR_FALSE;
    const TZrChar *memberName;

    if (state == ZR_NULL || analyzer == ZR_NULL || receiverNode == ZR_NULL ||
        memberNode == ZR_NULL || call == ZR_NULL) {
        return;
    }

    memberName = semantic_member_property_text(memberNode);
    if (memberName == ZR_NULL) {
        return;
    }

    ZrParser_InferredType_Init(state, &receiverType, ZR_VALUE_TYPE_OBJECT);
    if (!semantic_infer_node_type(state, analyzer, receiverNode, &receiverType)) {
        ZrParser_InferredType_Free(state, &receiverType);
        return;
    }
    if (receiverType.typeName == ZR_NULL) {
        ZrParser_InferredType_Free(state, &receiverType);
        return;
    }

    classSymbol = ZrLanguageServer_SymbolTable_Lookup(analyzer->symbolTable, receiverType.typeName, ZR_NULL);
    if (classSymbol != ZR_NULL && classSymbol->astNode != ZR_NULL &&
        classSymbol->astNode->type == ZR_AST_CLASS_DECLARATION &&
        classSymbol->astNode->data.classDeclaration.members != ZR_NULL) {
        for (TZrSize index = 0; index < classSymbol->astNode->data.classDeclaration.members->count; index++) {
            SZrAstNode *candidateNode = classSymbol->astNode->data.classDeclaration.members->nodes[index];
            if (candidateNode == ZR_NULL || candidateNode->type != ZR_AST_CLASS_METHOD ||
                candidateNode->data.classMethod.name == ZR_NULL ||
                !semantic_text_equals(semantic_string_native(candidateNode->data.classMethod.name->name), memberName)) {
                continue;
            }

            sawCandidate = ZR_TRUE;
            if (semantic_call_matches_parameters(state,
                                                 analyzer,
                                                 candidateNode->data.classMethod.params,
                                                 call)) {
                matchedCandidate = ZR_TRUE;
                break;
            }
        }
    }

    ZrParser_InferredType_Free(state, &receiverType);
    if (sawCandidate && !matchedCandidate) {
        semantic_add_type_mismatch_diagnostic(state, analyzer, location, "Type mismatch in method call");
    }
}

static TZrBool semantic_is_boolean_literal(SZrAstNode *node, TZrBool *outValue) {
    if (outValue != ZR_NULL) {
        *outValue = ZR_FALSE;
    }

    if (node == ZR_NULL || node->type != ZR_AST_BOOLEAN_LITERAL) {
        return ZR_FALSE;
    }

    if (outValue != ZR_NULL) {
        *outValue = node->data.booleanLiteral.value;
    }
    return ZR_TRUE;
}

static TZrBool semantic_statement_definitely_exits(SZrAstNode *node) {
    return node != ZR_NULL &&
           (node->type == ZR_AST_RETURN_STATEMENT || node->type == ZR_AST_THROW_STATEMENT);
}

static const SZrType *semantic_find_enclosing_return_type(SZrSymbolTable *table, SZrFileRange position) {
    SZrSymbol *bestSymbol = ZR_NULL;

    if (table == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize scopeIndex = 0; scopeIndex < table->allScopes.length; scopeIndex++) {
        SZrSymbolScope **scopePtr = (SZrSymbolScope **)ZrCore_Array_Get(&table->allScopes, scopeIndex);
        if (scopePtr == ZR_NULL || *scopePtr == ZR_NULL) {
            continue;
        }

        for (TZrSize symbolIndex = 0; symbolIndex < (*scopePtr)->symbols.length; symbolIndex++) {
            SZrSymbol **symbolPtr = (SZrSymbol **)ZrCore_Array_Get(&(*scopePtr)->symbols, symbolIndex);
            if (symbolPtr == ZR_NULL || *symbolPtr == ZR_NULL || (*symbolPtr)->astNode == ZR_NULL) {
                continue;
            }

            if ((*symbolPtr)->type != ZR_SYMBOL_FUNCTION && (*symbolPtr)->type != ZR_SYMBOL_METHOD) {
                continue;
            }

            if ((*symbolPtr)->location.start.offset <= position.start.offset &&
                position.end.offset <= (*symbolPtr)->location.end.offset &&
                (bestSymbol == ZR_NULL ||
                 (*symbolPtr)->location.start.offset >= bestSymbol->location.start.offset)) {
                bestSymbol = *symbolPtr;
            }
        }
    }

    if (bestSymbol == ZR_NULL || bestSymbol->astNode == ZR_NULL) {
        return ZR_NULL;
    }

    if (bestSymbol->astNode->type == ZR_AST_FUNCTION_DECLARATION) {
        return bestSymbol->astNode->data.functionDeclaration.returnType;
    }
    if (bestSymbol->astNode->type == ZR_AST_CLASS_METHOD) {
        return bestSymbol->astNode->data.classMethod.returnType;
    }

    return ZR_NULL;
}

void ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(SZrState *state, SZrSemanticAnalyzer *analyzer, SZrAstNode *node) {
    if (state == ZR_NULL || analyzer == ZR_NULL || node == ZR_NULL) {
        return;
    }
    
    // 根据节点类型进行类型检查
    switch (node->type) {
        case ZR_AST_BINARY_EXPRESSION: {
            SZrBinaryExpression *binExpr = &node->data.binaryExpression;
            if (binExpr->left != ZR_NULL && binExpr->right != ZR_NULL) {
                ZR_UNUSED_PARAMETER(binExpr);
            }
            break;
        }
        
        case ZR_AST_ASSIGNMENT_EXPRESSION: {
            SZrAssignmentExpression *assignExpr = &node->data.assignmentExpression;
            if (assignExpr->left != ZR_NULL && assignExpr->right != ZR_NULL) {
                SZrInferredType leftType, rightType;
                TZrBool hasLeftType;
                TZrBool hasRightType;
                ZrParser_InferredType_Init(state, &leftType, ZR_VALUE_TYPE_OBJECT);
                ZrParser_InferredType_Init(state, &rightType, ZR_VALUE_TYPE_OBJECT);
                hasLeftType = ZrParser_ExpressionType_Infer(analyzer->compilerState, assignExpr->left, &leftType);
                hasRightType = hasLeftType
                               ? ZrParser_ExpressionType_Infer(analyzer->compilerState, assignExpr->right, &rightType)
                               : ZR_FALSE;
                if (hasLeftType && hasRightType) {
                    // 检查赋值类型兼容性
                    if (!ZrParser_AssignmentCompatibility_Check(analyzer->compilerState, &leftType, &rightType, node->location)) {
                        ZrLanguageServer_SemanticAnalyzer_AddDiagnostic(state, analyzer,
                                                        ZR_DIAGNOSTIC_ERROR,
                                                        node->location,
                                                        "Type mismatch in assignment: incompatible types",
                                                        "type_mismatch");
                    }
                }
                if (hasRightType) {
                    ZrParser_InferredType_Free(state, &rightType);
                }
                if (hasLeftType) {
                    ZrParser_InferredType_Free(state, &leftType);
                }
                
                // 检查 const 变量赋值限制
                if (assignExpr->left != ZR_NULL && assignExpr->left->type == ZR_AST_IDENTIFIER_LITERAL) {
                    SZrString *varName = assignExpr->left->data.identifier.name;
                    if (varName != ZR_NULL) {
                        // 查找符号
                        SZrSymbol *symbol = ZrLanguageServer_SymbolTable_Lookup(analyzer->symbolTable, varName, ZR_NULL);
                        if (symbol != ZR_NULL && symbol->isConst) {
                            // 检查是否是 const 变量
                            if (symbol->type == ZR_SYMBOL_VARIABLE) {
                                // const 局部变量：只能在声明时赋值
                                // TODO: 需要检查是否在声明语句中（通过 AST 上下文判断）
                                // 暂时报告错误，后续完善
                                ZrLanguageServer_SemanticAnalyzer_AddDiagnostic(state, analyzer,
                                                                ZR_DIAGNOSTIC_ERROR,
                                                                node->location,
                                                                "Cannot assign to const variable after declaration",
                                                                "const_assignment");
                            } else if (symbol->type == ZR_SYMBOL_PARAMETER) {
                                // const 函数参数：不能在函数体内修改
                                ZrLanguageServer_SemanticAnalyzer_AddDiagnostic(state, analyzer,
                                                                ZR_DIAGNOSTIC_ERROR,
                                                                node->location,
                                                                "Cannot assign to const parameter",
                                                                "const_assignment");
                            } else if (symbol->type == ZR_SYMBOL_FIELD) {
                                // const 成员字段：只能在构造函数中赋值
                                // TODO: 需要检查是否在构造函数中（通过检查当前函数是否为 @constructor 元方法）
                                // 暂时报告错误，后续完善
                                ZrLanguageServer_SemanticAnalyzer_AddDiagnostic(state, analyzer,
                                                                ZR_DIAGNOSTIC_ERROR,
                                                                node->location,
                                                                "Cannot assign to const field outside constructor",
                                                                "const_assignment");
                            }
                        }
                    }
                }
            }
            break;
        }
        
        case ZR_AST_FUNCTION_CALL: {
            ZR_UNUSED_PARAMETER(&node->data.functionCall);
            // 检查函数调用的参数类型
            // TODO: 注意：这里需要查找函数定义并检查参数类型，简化实现暂时跳过
            // 完整实现需要使用 ZrParser_FunctionCallCompatibility_Check
            break;
        }
        
        case ZR_AST_VARIABLE_DECLARATION: {
            SZrVariableDeclaration *varDecl = &node->data.variableDeclaration;
            if (varDecl->typeInfo != ZR_NULL && varDecl->value != ZR_NULL) {
                SZrInferredType expectedType;
                SZrInferredType valueType;
                TZrBool hasValueType;
                TZrBool compatible;

                ZrParser_InferredType_Init(state, &expectedType, ZR_VALUE_TYPE_OBJECT);
                ZrParser_InferredType_Init(state, &valueType, ZR_VALUE_TYPE_OBJECT);
                semantic_type_from_ast(state, analyzer, varDecl->typeInfo, &expectedType);
                hasValueType = semantic_infer_node_type(state, analyzer, varDecl->value, &valueType);
                if (!hasValueType) {
                    semantic_add_cannot_infer_exact_type_diagnostic(state, analyzer, varDecl->value->location);
                } else {
                    compatible = analyzer->compilerState != ZR_NULL &&
                                 ZrParser_AssignmentCompatibility_Check(analyzer->compilerState,
                                                                        &expectedType,
                                                                        &valueType,
                                                                        varDecl->value->location);
                }
                if (hasValueType && !compatible) {
                    TZrChar message[ZR_LSP_TYPE_BUFFER_LENGTH];
                    TZrChar expectedBuffer[ZR_LSP_TYPE_BUFFER_LENGTH];
                    TZrChar actualBuffer[ZR_LSP_TYPE_BUFFER_LENGTH];
                    const TZrChar *expectedText =
                            ZrParser_TypeNameString_Get(state, &expectedType, expectedBuffer, sizeof(expectedBuffer));
                    const TZrChar *actualText =
                            ZrParser_TypeNameString_Get(state, &valueType, actualBuffer, sizeof(actualBuffer));

                    snprintf(message,
                             sizeof(message),
                             "Type mismatch in variable initializer (expected: %s, actual: %s)",
                             expectedText != ZR_NULL ? expectedText : "unknown",
                             actualText != ZR_NULL ? actualText : "unknown");
                    semantic_add_type_mismatch_diagnostic(state, analyzer, node->location, message);
                }
                ZrParser_InferredType_Free(state, &valueType);
                ZrParser_InferredType_Free(state, &expectedType);
            }
            break;
        }
        
        case ZR_AST_RETURN_STATEMENT: {
            SZrReturnStatement *returnStmt = &node->data.returnStatement;
            if (returnStmt->expr != ZR_NULL) {
                const SZrType *returnTypeNode = semantic_find_enclosing_return_type(analyzer->symbolTable, node->location);
                if (returnTypeNode != ZR_NULL) {
                    SZrInferredType expectedType;
                    SZrInferredType actualType;
                    TZrBool compatible;

                    ZrParser_InferredType_Init(state, &expectedType, ZR_VALUE_TYPE_OBJECT);
                    ZrParser_InferredType_Init(state, &actualType, ZR_VALUE_TYPE_OBJECT);
                    semantic_type_from_ast(state, analyzer, returnTypeNode, &expectedType);
                    compatible = semantic_infer_node_type(state, analyzer, returnStmt->expr, &actualType) &&
                                 analyzer->compilerState != ZR_NULL &&
                                 ZrParser_AssignmentCompatibility_Check(analyzer->compilerState,
                                                                        &expectedType,
                                                                        &actualType,
                                                                        returnStmt->expr->location);
                    ZrParser_InferredType_Free(state, &actualType);
                    ZrParser_InferredType_Free(state, &expectedType);
                    if (!compatible) {
                        semantic_add_type_mismatch_diagnostic(state,
                                                              analyzer,
                                                              node->location,
                                                              "Type mismatch in return statement");
                    }
                } else {
                    SZrInferredType actualType;
                    TZrBool hasActualType;

                    ZrParser_InferredType_Init(state, &actualType, ZR_VALUE_TYPE_OBJECT);
                    hasActualType = semantic_infer_node_type(state, analyzer, returnStmt->expr, &actualType);
                    ZrParser_InferredType_Free(state, &actualType);
                    if (!hasActualType) {
                        semantic_add_cannot_infer_exact_type_diagnostic(state,
                                                                        analyzer,
                                                                        returnStmt->expr->location);
                    }
                }
            }
            break;
        }

        case ZR_AST_LOGICAL_EXPRESSION: {
            TZrBool leftValue = ZR_FALSE;
            if (semantic_is_boolean_literal(node->data.logicalExpression.left, &leftValue) &&
                ((semantic_text_equals(node->data.logicalExpression.op, "||") && leftValue) ||
                 (semantic_text_equals(node->data.logicalExpression.op, "&&") && !leftValue))) {
                ZrLanguageServer_SemanticAnalyzer_AddDiagnostic(state,
                                                                analyzer,
                                                                ZR_DIAGNOSTIC_WARNING,
                                                                node->data.logicalExpression.right->location,
                                                                "Right-hand branch is unreachable due to deterministic short-circuit",
                                                                "short_circuit_unreachable");
            }
            break;
        }

        case ZR_AST_IF_EXPRESSION: {
            TZrBool conditionValue = ZR_FALSE;
            if (semantic_is_boolean_literal(node->data.ifExpression.condition, &conditionValue)) {
                SZrAstNode *unreachableBranch = conditionValue
                                                ? node->data.ifExpression.elseExpr
                                                : node->data.ifExpression.thenExpr;
                if (unreachableBranch != ZR_NULL) {
                    ZrLanguageServer_SemanticAnalyzer_AddDiagnostic(state,
                                                                    analyzer,
                                                                    ZR_DIAGNOSTIC_WARNING,
                                                                    unreachableBranch->location,
                                                                    "Branch is statically unreachable",
                                                                    "unreachable_branch");
                }
            }
            break;
        }

        case ZR_AST_PRIMARY_EXPRESSION: {
            SZrPrimaryExpression *primaryExpr = &node->data.primaryExpression;
            if (primaryExpr->property != ZR_NULL &&
                primaryExpr->property->type == ZR_AST_IDENTIFIER_LITERAL &&
                primaryExpr->members != ZR_NULL &&
                primaryExpr->members->count > 0 &&
                primaryExpr->members->nodes[0] != ZR_NULL &&
                primaryExpr->members->nodes[0]->type == ZR_AST_FUNCTION_CALL) {
                semantic_check_named_function_call(state,
                                                  analyzer,
                                                  primaryExpr->property->data.identifier.name,
                                                  &primaryExpr->members->nodes[0]->data.functionCall,
                                                  node->location);
            } else if (primaryExpr->property != ZR_NULL &&
                       primaryExpr->members != ZR_NULL &&
                       primaryExpr->members->count > 1 &&
                       primaryExpr->members->nodes[0] != ZR_NULL &&
                       primaryExpr->members->nodes[0]->type == ZR_AST_MEMBER_EXPRESSION &&
                       primaryExpr->members->nodes[1] != ZR_NULL &&
                       primaryExpr->members->nodes[1]->type == ZR_AST_FUNCTION_CALL) {
                semantic_check_method_call(state,
                                           analyzer,
                                           primaryExpr->property,
                                           primaryExpr->members->nodes[0],
                                           &primaryExpr->members->nodes[1]->data.functionCall,
                                           node->location);
            }
            break;
        }

        case ZR_AST_EXTERN_FUNCTION_DECLARATION:
            semantic_validate_extern_callable_decorators(state,
                                                         analyzer,
                                                         node->data.externFunctionDeclaration.decorators,
                                                         "extern functions");
            break;

        case ZR_AST_EXTERN_DELEGATE_DECLARATION:
            semantic_validate_extern_callable_decorators(state,
                                                         analyzer,
                                                         node->data.externDelegateDeclaration.decorators,
                                                         "extern delegates");
            break;

        case ZR_AST_CLASS_DECLARATION:
            semantic_validate_class_wrapper_decorators(state,
                                                       analyzer,
                                                       node->data.classDeclaration.decorators);
            break;

        case ZR_AST_STRUCT_DECLARATION:
            semantic_validate_extern_struct_decorators(state,
                                                       analyzer,
                                                       node->data.structDeclaration.decorators);
            break;

        case ZR_AST_STRUCT_FIELD:
            semantic_validate_extern_struct_field_decorators(state,
                                                             analyzer,
                                                             node->data.structField.decorators);
            break;

        case ZR_AST_ENUM_DECLARATION:
            semantic_validate_extern_enum_decorators(state,
                                                     analyzer,
                                                     node->data.enumDeclaration.decorators);
            break;

        case ZR_AST_ENUM_MEMBER:
            semantic_validate_extern_enum_member_decorators(state,
                                                            analyzer,
                                                            node->data.enumMember.decorators);
            break;

        case ZR_AST_PARAMETER:
            semantic_validate_extern_parameter_decorators(state, analyzer, node);
            break;

        case ZR_AST_INTERFACE_DECLARATION:
            semantic_validate_interface_variance_rules(state, analyzer, node);
            break;
        
        default:
            break;
    }
    
    // 递归检查子节点
    switch (node->type) {
        case ZR_AST_SCRIPT: {
            SZrScript *script = &node->data.script;
            if (script->statements != ZR_NULL && script->statements->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < script->statements->count; i++) {
                    if (script->statements->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, script->statements->nodes[i]);
                    }
                }
            }
            break;
        }

        case ZR_AST_EXTERN_BLOCK: {
            SZrExternBlock *externBlock = &node->data.externBlock;
            if (externBlock->declarations != ZR_NULL && externBlock->declarations->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < externBlock->declarations->count; i++) {
                    if (externBlock->declarations->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state,
                                                                              analyzer,
                                                                              externBlock->declarations->nodes[i]);
                    }
                }
            }
            break;
        }
        
        case ZR_AST_BLOCK: {
            SZrBlock *block = &node->data.block;
            SZrTypeEnvironment *savedTypeEnv =
                semantic_typecheck_push_runtime_type_binding_scope(state, analyzer);
            TZrBool terminated = ZR_FALSE;
            if (block->body != ZR_NULL && block->body->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < block->body->count; i++) {
                    if (block->body->nodes[i] != ZR_NULL) {
                        if (terminated) {
                            ZrLanguageServer_SemanticAnalyzer_AddDiagnostic(state,
                                                                            analyzer,
                                                                            ZR_DIAGNOSTIC_WARNING,
                                                                            block->body->nodes[i]->location,
                                                                            "Statement is unreachable",
                                                                            "unreachable_code");
                        }
                        ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, block->body->nodes[i]);
                        if (!terminated && semantic_statement_definitely_exits(block->body->nodes[i])) {
                            terminated = ZR_TRUE;
                        }
                    }
                }
            }
            semantic_typecheck_pop_runtime_type_binding_scope(state, analyzer, savedTypeEnv);
            break;
        }
        
        case ZR_AST_BINARY_EXPRESSION: {
            SZrBinaryExpression *binExpr = &node->data.binaryExpression;
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, binExpr->left);
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, binExpr->right);
            break;
        }
        
        case ZR_AST_UNARY_EXPRESSION: {
            SZrUnaryExpression *unaryExpr = &node->data.unaryExpression;
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, unaryExpr->argument);
            break;
        }
        
        case ZR_AST_ASSIGNMENT_EXPRESSION: {
            SZrAssignmentExpression *assignExpr = &node->data.assignmentExpression;
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, assignExpr->left);
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, assignExpr->right);
            break;
        }
        
        case ZR_AST_FUNCTION_CALL: {
            SZrFunctionCall *funcCall = &node->data.functionCall;
            if (funcCall->args != ZR_NULL && funcCall->args->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < funcCall->args->count; i++) {
                    if (funcCall->args->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, funcCall->args->nodes[i]);
                    }
                }
            }
            break;
        }
        
        case ZR_AST_PRIMARY_EXPRESSION: {
            SZrPrimaryExpression *primaryExpr = &node->data.primaryExpression;
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, primaryExpr->property);
            if (primaryExpr->members != ZR_NULL && primaryExpr->members->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < primaryExpr->members->count; i++) {
                    if (primaryExpr->members->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, primaryExpr->members->nodes[i]);
                    }
                }
            }
            break;
        }
        
        case ZR_AST_VARIABLE_DECLARATION: {
            SZrVariableDeclaration *varDecl = &node->data.variableDeclaration;
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, varDecl->pattern);
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, varDecl->value);
            semantic_typecheck_register_variable_binding(
                    state,
                    analyzer,
                    ZrLanguageServer_SemanticAnalyzer_ExtractIdentifierName(state, varDecl->pattern),
                    varDecl->typeInfo,
                    varDecl->value);
            break;
        }

        case ZR_AST_EXPRESSION_STATEMENT: {
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state,
                                                                  analyzer,
                                                                  node->data.expressionStatement.expr);
            break;
        }

        case ZR_AST_RETURN_STATEMENT: {
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state,
                                                                  analyzer,
                                                                  node->data.returnStatement.expr);
            break;
        }

        case ZR_AST_THROW_STATEMENT: {
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state,
                                                                  analyzer,
                                                                  node->data.throwStatement.expr);
            break;
        }

        case ZR_AST_USING_STATEMENT: {
            SZrUsingStatement *usingStmt = &node->data.usingStatement;
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, usingStmt->resource);
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, usingStmt->body);
            break;
        }

        case ZR_AST_TEMPLATE_STRING_LITERAL: {
            SZrTemplateStringLiteral *templateLiteral = &node->data.templateStringLiteral;
            if (templateLiteral->segments != ZR_NULL && templateLiteral->segments->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < templateLiteral->segments->count; i++) {
                    if (templateLiteral->segments->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, templateLiteral->segments->nodes[i]);
                    }
                }
            }
            break;
        }

        case ZR_AST_INTERPOLATED_SEGMENT: {
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, node->data.interpolatedSegment.expression);
            break;
        }
        
        case ZR_AST_FUNCTION_DECLARATION: {
            SZrFunctionDeclaration *funcDecl = &node->data.functionDeclaration;
            SZrSemanticTypecheckContextSnapshot contextSnapshot;
            SZrTypeEnvironment *savedTypeEnv;

            semantic_typecheck_push_compiler_context(analyzer, ZR_NULL, node, &contextSnapshot);
            savedTypeEnv = semantic_typecheck_push_runtime_type_binding_scope(state, analyzer);
            semantic_typecheck_register_parameter_bindings(state, analyzer, funcDecl->params);
            if (funcDecl->params != ZR_NULL && funcDecl->params->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < funcDecl->params->count; i++) {
                    if (funcDecl->params->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, funcDecl->params->nodes[i]);
                    }
                }
            }
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, funcDecl->body);
            semantic_typecheck_pop_runtime_type_binding_scope(state, analyzer, savedTypeEnv);
            semantic_typecheck_pop_compiler_context(analyzer, &contextSnapshot);
            break;
        }

        case ZR_AST_EXTERN_FUNCTION_DECLARATION: {
            SZrExternFunctionDeclaration *funcDecl = &node->data.externFunctionDeclaration;
            SZrSemanticTypecheckContextSnapshot contextSnapshot;
            SZrTypeEnvironment *savedTypeEnv;

            semantic_typecheck_push_compiler_context(analyzer, ZR_NULL, node, &contextSnapshot);
            savedTypeEnv = semantic_typecheck_push_runtime_type_binding_scope(state, analyzer);
            semantic_typecheck_register_parameter_bindings(state, analyzer, funcDecl->params);
            if (funcDecl->params != ZR_NULL && funcDecl->params->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < funcDecl->params->count; i++) {
                    if (funcDecl->params->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, funcDecl->params->nodes[i]);
                    }
                }
            }
            semantic_typecheck_pop_runtime_type_binding_scope(state, analyzer, savedTypeEnv);
            semantic_typecheck_pop_compiler_context(analyzer, &contextSnapshot);
            break;
        }

        case ZR_AST_LOGICAL_EXPRESSION: {
            SZrLogicalExpression *logicalExpr = &node->data.logicalExpression;
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, logicalExpr->left);
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, logicalExpr->right);
            break;
        }

        case ZR_AST_EXTERN_DELEGATE_DECLARATION: {
            SZrExternDelegateDeclaration *delegateDecl = &node->data.externDelegateDeclaration;
            if (delegateDecl->params != ZR_NULL && delegateDecl->params->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < delegateDecl->params->count; i++) {
                    if (delegateDecl->params->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, delegateDecl->params->nodes[i]);
                    }
                }
            }
            break;
        }

        case ZR_AST_STRUCT_DECLARATION: {
            SZrStructDeclaration *structDecl = &node->data.structDeclaration;
            if (structDecl->members != ZR_NULL && structDecl->members->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < structDecl->members->count; i++) {
                    if (structDecl->members->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, structDecl->members->nodes[i]);
                    }
                }
            }
            break;
        }

        case ZR_AST_ENUM_DECLARATION: {
            SZrEnumDeclaration *enumDecl = &node->data.enumDeclaration;
            if (enumDecl->members != ZR_NULL && enumDecl->members->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < enumDecl->members->count; i++) {
                    if (enumDecl->members->nodes[i] != ZR_NULL) {
                        ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, enumDecl->members->nodes[i]);
                    }
                }
            }
            break;
        }
        
        case ZR_AST_IF_EXPRESSION: {
            SZrIfExpression *ifExpr = &node->data.ifExpression;
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, ifExpr->condition);
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, ifExpr->thenExpr);
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, ifExpr->elseExpr);
            break;
        }
        
        case ZR_AST_WHILE_LOOP: {
            SZrWhileLoop *whileLoop = &node->data.whileLoop;
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, whileLoop->cond);
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, whileLoop->block);
            break;
        }
        
        case ZR_AST_FOR_LOOP: {
            SZrForLoop *forLoop = &node->data.forLoop;
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, forLoop->init);
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, forLoop->cond);
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, forLoop->step);
            ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, forLoop->block);
            break;
        }
        
        default:
            break;
    }
}

// 创建语义分析器
