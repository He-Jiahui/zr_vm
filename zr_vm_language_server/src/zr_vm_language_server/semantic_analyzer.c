//
// Created by Auto on 2025/01/XX.
//

#include "zr_vm_language_server/semantic_analyzer.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/array.h"
#include "zr_vm_core/string.h"
#include "zr_vm_library/native_registry.h"
#include "zr_vm_library/native_binding.h"
#include "zr_vm_parser/type_inference.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/type_system.h"

#include <string.h>
#include <stdio.h>

// 前向声明编译器状态初始化函数
extern void ZrParser_CompilerState_Init(SZrCompilerState *cs, SZrState *state);
extern void ZrParser_CompilerState_Free(SZrCompilerState *cs);

// 前向声明类型推断函数
extern TZrBool ZrParser_ExpressionType_Infer(SZrCompilerState *cs, SZrAstNode *node, SZrInferredType *result);
extern TZrBool ZrParser_TypeCompatibility_Check(SZrCompilerState *cs, const SZrInferredType *fromType, 
                                       const SZrInferredType *toType, SZrFileRange location);
extern TZrBool ZrParser_AssignmentCompatibility_Check(SZrCompilerState *cs, const SZrInferredType *leftType, 
                                            const SZrInferredType *rightType, SZrFileRange location);

static SZrSymbol *lookup_symbol_at_node(SZrSemanticAnalyzer *analyzer, SZrAstNode *node);
static void collect_references_from_ast(SZrState *state, SZrSemanticAnalyzer *analyzer, SZrAstNode *node);
static void perform_control_flow_analysis(SZrState *state, SZrSemanticAnalyzer *analyzer, SZrAstNode *node);
static SZrInferredType *create_declared_symbol_type(SZrState *state,
                                                    SZrSemanticAnalyzer *analyzer,
                                                    const SZrType *declaredType);
static void semantic_enter_type_scope(SZrState *state,
                                      SZrSemanticAnalyzer *analyzer);
static void semantic_exit_type_scope(SZrState *state,
                                     SZrSemanticAnalyzer *analyzer);
static void register_parameter_types_in_current_environment(SZrState *state,
                                                            SZrSemanticAnalyzer *analyzer,
                                                            SZrAstNodeArray *params);
static void perform_type_checking_with_context(SZrState *state,
                                               SZrSemanticAnalyzer *analyzer,
                                               SZrAstNode *node,
                                               const SZrInferredType *currentReturnType);

static void free_inferred_type_pointer(SZrState *state, SZrInferredType *typeInfo) {
    if (state == ZR_NULL || typeInfo == ZR_NULL) {
        return;
    }

    ZrParser_InferredType_Free(state, typeInfo);
    ZrCore_Memory_RawFree(state->global, typeInfo, sizeof(SZrInferredType));
}

static void add_type_mismatch_diagnostic(SZrState *state,
                                         SZrSemanticAnalyzer *analyzer,
                                         SZrFileRange location,
                                         const TZrChar *message) {
    if (state == ZR_NULL || analyzer == ZR_NULL || message == ZR_NULL) {
        return;
    }

    ZrLanguageServer_SemanticAnalyzer_AddDiagnostic(state,
                                                    analyzer,
                                                    ZR_DIAGNOSTIC_ERROR,
                                                    location,
                                                    message,
                                                    "type_mismatch");
}

typedef struct ZrSemanticCompilerErrorState {
    TZrBool hasError;
    const TZrChar *errorMessage;
    SZrFileRange errorLocation;
} ZrSemanticCompilerErrorState;

typedef enum EZrSemanticDecoratorTargetKind {
    ZR_SEMANTIC_DECORATOR_TARGET_FUNCTION = 0,
    ZR_SEMANTIC_DECORATOR_TARGET_EXTERN_FUNCTION,
    ZR_SEMANTIC_DECORATOR_TARGET_EXTERN_DELEGATE,
    ZR_SEMANTIC_DECORATOR_TARGET_PARAMETER,
    ZR_SEMANTIC_DECORATOR_TARGET_STRUCT,
    ZR_SEMANTIC_DECORATOR_TARGET_STRUCT_FIELD,
    ZR_SEMANTIC_DECORATOR_TARGET_ENUM
} EZrSemanticDecoratorTargetKind;

static void add_invalid_decorator_diagnostic(SZrState *state,
                                             SZrSemanticAnalyzer *analyzer,
                                             SZrFileRange location,
                                             const TZrChar *message) {
    if (state == ZR_NULL || analyzer == ZR_NULL || message == ZR_NULL) {
        return;
    }

    ZrLanguageServer_SemanticAnalyzer_AddDiagnostic(state,
                                                    analyzer,
                                                    ZR_DIAGNOSTIC_ERROR,
                                                    location,
                                                    message,
                                                    "invalid_decorator");
}

static TZrBool semantic_string_equals_cstr(SZrString *value, const TZrChar *literal) {
    TZrNativeString text;

    if (value == ZR_NULL || literal == ZR_NULL) {
        return ZR_FALSE;
    }

    text = ZrCore_String_GetNativeString(value);
    return text != ZR_NULL && strcmp(text, literal) == 0;
}

static TZrBool semantic_identifier_equals(SZrAstNode *node, const TZrChar *literal) {
    if (node == ZR_NULL || node->type != ZR_AST_IDENTIFIER_LITERAL || literal == ZR_NULL) {
        return ZR_FALSE;
    }

    return semantic_string_equals_cstr(node->data.identifier.name, literal);
}

static void semantic_save_compiler_error_state(SZrCompilerState *cs,
                                               ZrSemanticCompilerErrorState *snapshot) {
    if (cs == ZR_NULL || snapshot == ZR_NULL) {
        return;
    }

    snapshot->hasError = cs->hasError;
    snapshot->errorMessage = cs->errorMessage;
    snapshot->errorLocation = cs->errorLocation;
}

static void semantic_restore_compiler_error_state(SZrCompilerState *cs,
                                                  const ZrSemanticCompilerErrorState *snapshot) {
    if (cs == ZR_NULL || snapshot == ZR_NULL) {
        return;
    }

    cs->hasError = snapshot->hasError;
    cs->errorMessage = snapshot->errorMessage;
    cs->errorLocation = snapshot->errorLocation;
}

static TZrBool semantic_function_candidates_exist(SZrState *state,
                                                  SZrTypeEnvironment *env,
                                                  SZrString *funcName) {
    SZrArray candidates;
    TZrBool found = ZR_FALSE;

    if (state == ZR_NULL || env == ZR_NULL || funcName == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Construct(&candidates);
    if (ZrParser_TypeEnvironment_LookupFunctions(state, env, funcName, &candidates)) {
        found = candidates.length > 0;
    }
    if (candidates.isValid) {
        ZrCore_Array_Free(state, &candidates);
    }

    return found;
}

static TZrBool semantic_resolve_overload(SZrSemanticAnalyzer *analyzer,
                                         SZrTypeEnvironment *env,
                                         SZrString *funcName,
                                         SZrFunctionCall *call,
                                         SZrFileRange location,
                                         SZrFunctionTypeInfo **resolvedFunction) {
    ZrSemanticCompilerErrorState snapshot;
    TZrBool resolved;

    if (analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL) {
        return ZR_FALSE;
    }

    semantic_save_compiler_error_state(analyzer->compilerState, &snapshot);
    resolved = ZrParser_FunctionCallOverload_Resolve(analyzer->compilerState,
                                                     env,
                                                     funcName,
                                                     call,
                                                     location,
                                                     resolvedFunction);
    semantic_restore_compiler_error_state(analyzer->compilerState, &snapshot);
    return resolved;
}

static TZrBool semantic_check_call_compatibility(SZrSemanticAnalyzer *analyzer,
                                                 SZrTypeEnvironment *env,
                                                 SZrString *funcName,
                                                 SZrFunctionCall *call,
                                                 SZrFunctionTypeInfo *funcType,
                                                 SZrFileRange location) {
    ZrSemanticCompilerErrorState snapshot;
    TZrBool compatible;

    if (analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL) {
        return ZR_FALSE;
    }

    semantic_save_compiler_error_state(analyzer->compilerState, &snapshot);
    compatible = ZrParser_FunctionCallCompatibility_Check(analyzer->compilerState,
                                                          env,
                                                          funcName,
                                                          call,
                                                          funcType,
                                                          location);
    semantic_restore_compiler_error_state(analyzer->compilerState, &snapshot);
    return compatible;
}

static TZrBool semantic_try_check_call_in_environment(SZrState *state,
                                                      SZrSemanticAnalyzer *analyzer,
                                                      SZrTypeEnvironment *env,
                                                      SZrString *funcName,
                                                      SZrFunctionCall *call,
                                                      SZrFileRange location,
                                                      TZrBool *outCompatible) {
    SZrFunctionTypeInfo *resolvedFunction = ZR_NULL;

    if (outCompatible != ZR_NULL) {
        *outCompatible = ZR_FALSE;
    }
    if (state == ZR_NULL || analyzer == ZR_NULL || env == ZR_NULL || funcName == ZR_NULL || call == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!semantic_function_candidates_exist(state, env, funcName)) {
        return ZR_FALSE;
    }

    if (!semantic_resolve_overload(analyzer, env, funcName, call, location, &resolvedFunction)) {
        return ZR_TRUE;
    }

    if (outCompatible != ZR_NULL) {
        *outCompatible = semantic_check_call_compatibility(analyzer,
                                                           env,
                                                           funcName,
                                                           call,
                                                           resolvedFunction,
                                                           location);
    }
    return ZR_TRUE;
}

static SZrString *semantic_build_method_function_name(SZrState *state,
                                                      SZrString *typeName,
                                                      SZrString *methodName) {
    TZrNativeString typeNameText;
    TZrNativeString methodNameText;
    TZrSize typeNameLength;
    TZrSize methodNameLength;
    TZrSize qualifiedLength;
    TZrChar *qualifiedText;
    SZrString *qualifiedName;

    if (state == ZR_NULL || typeName == ZR_NULL || methodName == ZR_NULL) {
        return ZR_NULL;
    }

    typeNameText = ZrCore_String_GetNativeString(typeName);
    methodNameText = ZrCore_String_GetNativeString(methodName);
    if (typeNameText == ZR_NULL || methodNameText == ZR_NULL) {
        return ZR_NULL;
    }

    typeNameLength = strlen(typeNameText);
    methodNameLength = strlen(methodNameText);
    qualifiedLength = typeNameLength + 1 + methodNameLength;
    qualifiedText = (TZrChar *)ZrCore_Memory_RawMalloc(state->global, qualifiedLength + 1);
    if (qualifiedText == ZR_NULL) {
        return ZR_NULL;
    }

    snprintf(qualifiedText, qualifiedLength + 1, "%s.%s", typeNameText, methodNameText);
    qualifiedName = ZrCore_String_Create(state, qualifiedText, qualifiedLength);
    ZrCore_Memory_RawFree(state->global, qualifiedText, qualifiedLength + 1);
    return qualifiedName;
}

static SZrString *semantic_infer_receiver_type_name(SZrState *state,
                                                    SZrSemanticAnalyzer *analyzer,
                                                    SZrAstNode *receiverNode) {
    SZrInferredType receiverType;
    SZrString *typeName = ZR_NULL;

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL || receiverNode == ZR_NULL) {
        return ZR_NULL;
    }

    ZrParser_InferredType_Init(state, &receiverType, ZR_VALUE_TYPE_OBJECT);
    if (ZrParser_ExpressionType_Infer(analyzer->compilerState, receiverNode, &receiverType) &&
        receiverType.typeName != ZR_NULL) {
        typeName = receiverType.typeName;
    }
    ZrParser_InferredType_Free(state, &receiverType);

    if (typeName != ZR_NULL) {
        return typeName;
    }

    if (receiverNode->type == ZR_AST_IDENTIFIER_LITERAL &&
        receiverNode->data.identifier.name != ZR_NULL &&
        analyzer->compilerState->typeEnv != ZR_NULL &&
        ZrParser_TypeEnvironment_LookupType(analyzer->compilerState->typeEnv,
                                            receiverNode->data.identifier.name)) {
        return receiverNode->data.identifier.name;
    }

    return ZR_NULL;
}

static SZrString *semantic_resolve_primary_call_name(SZrState *state,
                                                     SZrSemanticAnalyzer *analyzer,
                                                     SZrPrimaryExpression *primary,
                                                     SZrFunctionCall **outCall) {
    SZrAstNode *firstMember;
    SZrAstNode *secondMember;
    SZrString *receiverTypeName;

    if (outCall != ZR_NULL) {
        *outCall = ZR_NULL;
    }
    if (state == ZR_NULL || analyzer == ZR_NULL || primary == ZR_NULL ||
        primary->members == ZR_NULL || primary->members->count == 0 || primary->members->nodes == ZR_NULL) {
        return ZR_NULL;
    }

    firstMember = primary->members->nodes[0];
    if (firstMember != ZR_NULL &&
        firstMember->type == ZR_AST_FUNCTION_CALL &&
        primary->property != ZR_NULL &&
        primary->property->type == ZR_AST_IDENTIFIER_LITERAL) {
        if (outCall != ZR_NULL) {
            *outCall = &firstMember->data.functionCall;
        }
        return primary->property->data.identifier.name;
    }

    if (primary->members->count < 2) {
        return ZR_NULL;
    }

    secondMember = primary->members->nodes[1];
    if (primary->property == ZR_NULL ||
        firstMember == ZR_NULL ||
        firstMember->type != ZR_AST_MEMBER_EXPRESSION ||
        secondMember == ZR_NULL ||
        secondMember->type != ZR_AST_FUNCTION_CALL ||
        firstMember->data.memberExpression.computed ||
        firstMember->data.memberExpression.property == ZR_NULL ||
        firstMember->data.memberExpression.property->type != ZR_AST_IDENTIFIER_LITERAL) {
        return ZR_NULL;
    }

    receiverTypeName = semantic_infer_receiver_type_name(state, analyzer, primary->property);
    if (receiverTypeName == ZR_NULL) {
        return ZR_NULL;
    }

    if (outCall != ZR_NULL) {
        *outCall = &secondMember->data.functionCall;
    }
    return semantic_build_method_function_name(state,
                                               receiverTypeName,
                                               firstMember->data.memberExpression.property->data.identifier.name);
}

static void check_primary_expression_call_compatibility(SZrState *state,
                                                        SZrSemanticAnalyzer *analyzer,
                                                        SZrAstNode *node) {
    SZrPrimaryExpression *primary;
    SZrFunctionCall *call = ZR_NULL;
    SZrString *callName;
    TZrBool compatible = ZR_FALSE;
    TZrBool hadCandidates;

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL ||
        node == ZR_NULL || node->type != ZR_AST_PRIMARY_EXPRESSION) {
        return;
    }

    primary = &node->data.primaryExpression;
    callName = semantic_resolve_primary_call_name(state, analyzer, primary, &call);
    if (callName == ZR_NULL || call == ZR_NULL) {
        return;
    }

    hadCandidates = semantic_try_check_call_in_environment(state,
                                                           analyzer,
                                                           analyzer->compilerState->typeEnv,
                                                           callName,
                                                           call,
                                                           node->location,
                                                           &compatible);
    if (!hadCandidates) {
        hadCandidates = semantic_try_check_call_in_environment(state,
                                                               analyzer,
                                                               analyzer->compilerState->compileTimeTypeEnv,
                                                               callName,
                                                               call,
                                                               node->location,
                                                               &compatible);
    }

    if (hadCandidates && !compatible) {
        add_type_mismatch_diagnostic(state,
                                     analyzer,
                                     node->location,
                                     "Type mismatch in call: incompatible argument types");
    }
}

static TZrBool semantic_extract_ffi_decorator(SZrAstNode *decoratorNode,
                                              SZrString **outLeafName,
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
    if (!semantic_identifier_equals(primary->property, "zr") ||
        primary->members == ZR_NULL ||
        primary->members->nodes == ZR_NULL ||
        primary->members->count < 2 ||
        primary->members->count > 3) {
        return ZR_FALSE;
    }

    ffiMember = primary->members->nodes[0];
    leafMember = primary->members->nodes[1];
    if (ffiMember == ZR_NULL ||
        ffiMember->type != ZR_AST_MEMBER_EXPRESSION ||
        leafMember == ZR_NULL ||
        leafMember->type != ZR_AST_MEMBER_EXPRESSION ||
        ffiMember->data.memberExpression.property == ZR_NULL ||
        leafMember->data.memberExpression.property == ZR_NULL ||
        ffiMember->data.memberExpression.computed ||
        leafMember->data.memberExpression.computed ||
        ffiMember->data.memberExpression.property->type != ZR_AST_IDENTIFIER_LITERAL ||
        leafMember->data.memberExpression.property->type != ZR_AST_IDENTIFIER_LITERAL ||
        !semantic_identifier_equals(ffiMember->data.memberExpression.property, "ffi")) {
        return ZR_FALSE;
    }

    if (outLeafName != ZR_NULL) {
        *outLeafName = leafMember->data.memberExpression.property->data.identifier.name;
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

static TZrBool semantic_extract_string_argument(SZrFunctionCall *call, SZrString **outString) {
    if (outString != ZR_NULL) {
        *outString = ZR_NULL;
    }
    if (call == ZR_NULL || outString == ZR_NULL || call->args == ZR_NULL || call->args->count != 1) {
        return ZR_FALSE;
    }
    if (call->args->nodes[0] == ZR_NULL || call->args->nodes[0]->type != ZR_AST_STRING_LITERAL) {
        return ZR_FALSE;
    }

    *outString = call->args->nodes[0]->data.stringLiteral.value;
    return *outString != ZR_NULL;
}

static TZrBool semantic_extract_int_argument(SZrFunctionCall *call, TZrInt64 *outValue) {
    if (outValue != ZR_NULL) {
        *outValue = 0;
    }
    if (call == ZR_NULL || outValue == ZR_NULL || call->args == ZR_NULL || call->args->count != 1) {
        return ZR_FALSE;
    }
    if (call->args->nodes[0] == ZR_NULL || call->args->nodes[0]->type != ZR_AST_INTEGER_LITERAL) {
        return ZR_FALSE;
    }

    *outValue = call->args->nodes[0]->data.integerLiteral.value;
    return ZR_TRUE;
}

static void semantic_validate_ffi_decorators(SZrState *state,
                                             SZrSemanticAnalyzer *analyzer,
                                             SZrAstNodeArray *decorators,
                                             EZrSemanticDecoratorTargetKind targetKind,
                                             SZrFileRange targetLocation) {
    TZrBool hasIn = ZR_FALSE;
    TZrBool hasOut = ZR_FALSE;
    TZrBool hasInout = ZR_FALSE;

    if (state == ZR_NULL || analyzer == ZR_NULL || decorators == ZR_NULL || decorators->nodes == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < decorators->count; index++) {
        SZrAstNode *decorator = decorators->nodes[index];
        SZrString *leafName = ZR_NULL;
        SZrFunctionCall *call = ZR_NULL;
        TZrBool hasCall = ZR_FALSE;
        SZrFileRange diagnosticLocation;

        if (!semantic_extract_ffi_decorator(decorator, &leafName, &hasCall, &call) || leafName == ZR_NULL) {
            continue;
        }

        diagnosticLocation = decorator != ZR_NULL ? decorator->location : targetLocation;

        if (semantic_string_equals_cstr(leafName, "entry")) {
            SZrString *stringArg = ZR_NULL;
            if (targetKind != ZR_SEMANTIC_DECORATOR_TARGET_EXTERN_FUNCTION &&
                targetKind != ZR_SEMANTIC_DECORATOR_TARGET_EXTERN_DELEGATE) {
                add_invalid_decorator_diagnostic(state,
                                                 analyzer,
                                                 diagnosticLocation,
                                                 "zr.ffi.entry is only valid on extern functions or delegates");
            } else if (!hasCall || !semantic_extract_string_argument(call, &stringArg)) {
                add_invalid_decorator_diagnostic(state,
                                                 analyzer,
                                                 diagnosticLocation,
                                                 "zr.ffi.entry requires a single string literal argument");
            }
            continue;
        }

        if (semantic_string_equals_cstr(leafName, "callconv")) {
            SZrString *stringArg = ZR_NULL;
            if (targetKind != ZR_SEMANTIC_DECORATOR_TARGET_EXTERN_FUNCTION &&
                targetKind != ZR_SEMANTIC_DECORATOR_TARGET_EXTERN_DELEGATE) {
                add_invalid_decorator_diagnostic(state,
                                                 analyzer,
                                                 diagnosticLocation,
                                                 "zr.ffi.callconv is only valid on extern functions or delegates");
            } else if (!hasCall || !semantic_extract_string_argument(call, &stringArg)) {
                add_invalid_decorator_diagnostic(state,
                                                 analyzer,
                                                 diagnosticLocation,
                                                 "zr.ffi.callconv requires a single string literal argument");
            }
            continue;
        }

        if (semantic_string_equals_cstr(leafName, "pack") || semantic_string_equals_cstr(leafName, "align")) {
            TZrInt64 ignoredValue = 0;
            if (targetKind != ZR_SEMANTIC_DECORATOR_TARGET_STRUCT) {
                add_invalid_decorator_diagnostic(state,
                                                 analyzer,
                                                 diagnosticLocation,
                                                 "zr.ffi.pack/align are only valid on extern structs");
            } else if (!hasCall || !semantic_extract_int_argument(call, &ignoredValue)) {
                add_invalid_decorator_diagnostic(state,
                                                 analyzer,
                                                 diagnosticLocation,
                                                 "zr.ffi.pack/align require a single integer literal argument");
            }
            continue;
        }

        if (semantic_string_equals_cstr(leafName, "offset")) {
            TZrInt64 ignoredValue = 0;
            if (targetKind != ZR_SEMANTIC_DECORATOR_TARGET_STRUCT_FIELD) {
                add_invalid_decorator_diagnostic(state,
                                                 analyzer,
                                                 diagnosticLocation,
                                                 "zr.ffi.offset is only valid on extern struct fields");
            } else if (!hasCall || !semantic_extract_int_argument(call, &ignoredValue)) {
                add_invalid_decorator_diagnostic(state,
                                                 analyzer,
                                                 diagnosticLocation,
                                                 "zr.ffi.offset requires a single integer literal argument");
            }
            continue;
        }

        if (semantic_string_equals_cstr(leafName, "underlying")) {
            SZrString *stringArg = ZR_NULL;
            if (targetKind != ZR_SEMANTIC_DECORATOR_TARGET_ENUM) {
                add_invalid_decorator_diagnostic(state,
                                                 analyzer,
                                                 diagnosticLocation,
                                                 "zr.ffi.underlying is only valid on extern enums");
            } else if (!hasCall || !semantic_extract_string_argument(call, &stringArg)) {
                add_invalid_decorator_diagnostic(state,
                                                 analyzer,
                                                 diagnosticLocation,
                                                 "zr.ffi.underlying requires a single string literal argument");
            }
            continue;
        }

        if (semantic_string_equals_cstr(leafName, "in") ||
            semantic_string_equals_cstr(leafName, "out") ||
            semantic_string_equals_cstr(leafName, "inout")) {
            if (targetKind != ZR_SEMANTIC_DECORATOR_TARGET_PARAMETER || hasCall) {
                add_invalid_decorator_diagnostic(state,
                                                 analyzer,
                                                 diagnosticLocation,
                                                 "zr.ffi.in/out/inout are only valid as parameter flags");
            } else if (semantic_string_equals_cstr(leafName, "in")) {
                hasIn = ZR_TRUE;
            } else if (semantic_string_equals_cstr(leafName, "out")) {
                hasOut = ZR_TRUE;
            } else {
                hasInout = ZR_TRUE;
            }
            continue;
        }

        add_invalid_decorator_diagnostic(state,
                                         analyzer,
                                         diagnosticLocation,
                                         "Unknown zr.ffi decorator");
    }

    if (targetKind == ZR_SEMANTIC_DECORATOR_TARGET_PARAMETER &&
        ((hasIn ? 1 : 0) + (hasOut ? 1 : 0) + (hasInout ? 1 : 0) > 1)) {
        add_invalid_decorator_diagnostic(state,
                                         analyzer,
                                         targetLocation,
                                         "Conflicting zr.ffi parameter direction decorators");
    }
}

// 辅助函数：执行类型检查
static void perform_type_checking(SZrState *state, SZrSemanticAnalyzer *analyzer, SZrAstNode *node) {
    perform_type_checking_with_context(state, analyzer, node, ZR_NULL);
}

static void perform_type_checking_with_context(SZrState *state,
                                               SZrSemanticAnalyzer *analyzer,
                                               SZrAstNode *node,
                                               const SZrInferredType *currentReturnType) {
    if (state == ZR_NULL || analyzer == ZR_NULL || node == ZR_NULL) {
        return;
    }
    
    // 根据节点类型进行类型检查
    switch (node->type) {
        case ZR_AST_BINARY_EXPRESSION: {
            SZrBinaryExpression *binExpr = &node->data.binaryExpression;
            if (binExpr->left != ZR_NULL && binExpr->right != ZR_NULL) {
                SZrInferredType expressionType;
                TZrBool inferredType;
                ZrParser_InferredType_Init(state, &expressionType, ZR_VALUE_TYPE_OBJECT);
                inferredType = ZrParser_ExpressionType_Infer(analyzer->compilerState, node, &expressionType);
                if (!inferredType) {
                    const TZrChar *op = binExpr->op.op;
                    TZrChar buffer[256];
                    snprintf(buffer, sizeof(buffer),
                             "Type mismatch in binary expression: incompatible types for operator '%s'",
                             op != ZR_NULL ? op : "?");
                    ZrLanguageServer_SemanticAnalyzer_AddDiagnostic(state, analyzer,
                                                    ZR_DIAGNOSTIC_ERROR,
                                                    node->location,
                                                    buffer,
                                                    "type_mismatch");
                }
                ZrParser_InferredType_Free(state, &expressionType);
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
                    SZrSymbol *symbol = lookup_symbol_at_node(analyzer, assignExpr->left);
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
            break;
        }
        
        case ZR_AST_FUNCTION_CALL: {
            break;
        }
        
        case ZR_AST_VARIABLE_DECLARATION: {
            SZrVariableDeclaration *varDecl = &node->data.variableDeclaration;
            if (varDecl->typeInfo != ZR_NULL && varDecl->value != ZR_NULL) {
                SZrInferredType valueType;
                SZrInferredType *declaredType;
                ZrParser_InferredType_Init(state, &valueType, ZR_VALUE_TYPE_OBJECT);
                declaredType = create_declared_symbol_type(state, analyzer, varDecl->typeInfo);
                if (ZrParser_ExpressionType_Infer(analyzer->compilerState, varDecl->value, &valueType)) {
                    if (declaredType != ZR_NULL &&
                        !ZrParser_AssignmentCompatibility_Check(analyzer->compilerState,
                                                                declaredType,
                                                                &valueType,
                                                                node->location)) {
                        add_type_mismatch_diagnostic(state,
                                                     analyzer,
                                                     node->location,
                                                     "Type mismatch in variable declaration: incompatible initializer type");
                    }
                }
                ZrParser_InferredType_Free(state, &valueType);
                free_inferred_type_pointer(state, declaredType);
            }
            break;
        }
        
        case ZR_AST_RETURN_STATEMENT: {
            SZrReturnStatement *returnStmt = &node->data.returnStatement;
            if (currentReturnType != ZR_NULL && returnStmt->expr != ZR_NULL) {
                SZrInferredType valueType;
                ZrParser_InferredType_Init(state, &valueType, ZR_VALUE_TYPE_OBJECT);
                if (ZrParser_ExpressionType_Infer(analyzer->compilerState, returnStmt->expr, &valueType) &&
                    !ZrParser_TypeCompatibility_Check(analyzer->compilerState,
                                                      &valueType,
                                                      currentReturnType,
                                                      node->location)) {
                    add_type_mismatch_diagnostic(state,
                                                 analyzer,
                                                 node->location,
                                                 "Type mismatch in return statement: incompatible return type");
                }
                ZrParser_InferredType_Free(state, &valueType);
            }
            break;
        }

        case ZR_AST_PRIMARY_EXPRESSION:
            check_primary_expression_call_compatibility(state, analyzer, node);
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
                        perform_type_checking_with_context(state,
                                                           analyzer,
                                                           script->statements->nodes[i],
                                                           currentReturnType);
                    }
                }
            }
            break;
        }
        
        case ZR_AST_BLOCK: {
            SZrBlock *block = &node->data.block;
            if (block->body != ZR_NULL && block->body->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < block->body->count; i++) {
                    if (block->body->nodes[i] != ZR_NULL) {
                        perform_type_checking_with_context(state,
                                                           analyzer,
                                                           block->body->nodes[i],
                                                           currentReturnType);
                    }
                }
            }
            break;
        }
        
        case ZR_AST_BINARY_EXPRESSION: {
            SZrBinaryExpression *binExpr = &node->data.binaryExpression;
            perform_type_checking_with_context(state, analyzer, binExpr->left, currentReturnType);
            perform_type_checking_with_context(state, analyzer, binExpr->right, currentReturnType);
            break;
        }
        
        case ZR_AST_UNARY_EXPRESSION: {
            SZrUnaryExpression *unaryExpr = &node->data.unaryExpression;
            perform_type_checking_with_context(state, analyzer, unaryExpr->argument, currentReturnType);
            break;
        }
        
        case ZR_AST_ASSIGNMENT_EXPRESSION: {
            SZrAssignmentExpression *assignExpr = &node->data.assignmentExpression;
            perform_type_checking_with_context(state, analyzer, assignExpr->left, currentReturnType);
            perform_type_checking_with_context(state, analyzer, assignExpr->right, currentReturnType);
            break;
        }
        
        case ZR_AST_FUNCTION_CALL: {
            SZrFunctionCall *funcCall = &node->data.functionCall;
            if (funcCall->args != ZR_NULL && funcCall->args->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < funcCall->args->count; i++) {
                    if (funcCall->args->nodes[i] != ZR_NULL) {
                        perform_type_checking_with_context(state,
                                                           analyzer,
                                                           funcCall->args->nodes[i],
                                                           currentReturnType);
                    }
                }
            }
            break;
        }
        
        case ZR_AST_PRIMARY_EXPRESSION: {
            SZrPrimaryExpression *primaryExpr = &node->data.primaryExpression;
            perform_type_checking_with_context(state, analyzer, primaryExpr->property, currentReturnType);
            if (primaryExpr->members != ZR_NULL && primaryExpr->members->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < primaryExpr->members->count; i++) {
                    if (primaryExpr->members->nodes[i] != ZR_NULL) {
                        perform_type_checking_with_context(state,
                                                           analyzer,
                                                           primaryExpr->members->nodes[i],
                                                           currentReturnType);
                    }
                }
            }
            break;
        }
        
        case ZR_AST_VARIABLE_DECLARATION: {
            SZrVariableDeclaration *varDecl = &node->data.variableDeclaration;
            perform_type_checking_with_context(state, analyzer, varDecl->pattern, currentReturnType);
            perform_type_checking_with_context(state, analyzer, varDecl->value, currentReturnType);
            break;
        }

        case ZR_AST_EXPRESSION_STATEMENT: {
            SZrExpressionStatement *exprStmt = &node->data.expressionStatement;
            perform_type_checking_with_context(state, analyzer, exprStmt->expr, currentReturnType);
            break;
        }

        case ZR_AST_USING_STATEMENT: {
            SZrUsingStatement *usingStmt = &node->data.usingStatement;
            perform_type_checking_with_context(state, analyzer, usingStmt->resource, currentReturnType);
            perform_type_checking_with_context(state, analyzer, usingStmt->body, currentReturnType);
            break;
        }

        case ZR_AST_TEMPLATE_STRING_LITERAL: {
            SZrTemplateStringLiteral *templateLiteral = &node->data.templateStringLiteral;
            if (templateLiteral->segments != ZR_NULL && templateLiteral->segments->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < templateLiteral->segments->count; i++) {
                    if (templateLiteral->segments->nodes[i] != ZR_NULL) {
                        perform_type_checking(state, analyzer, templateLiteral->segments->nodes[i]);
                    }
                }
            }
            break;
        }

        case ZR_AST_INTERPOLATED_SEGMENT: {
            perform_type_checking_with_context(state,
                                               analyzer,
                                               node->data.interpolatedSegment.expression,
                                               currentReturnType);
            break;
        }
        
        case ZR_AST_FUNCTION_DECLARATION: {
            SZrFunctionDeclaration *funcDecl = &node->data.functionDeclaration;
            SZrInferredType *returnType = funcDecl->returnType != ZR_NULL
                                              ? create_declared_symbol_type(state, analyzer, funcDecl->returnType)
                                              : ZR_NULL;
            semantic_enter_type_scope(state, analyzer);
            register_parameter_types_in_current_environment(state, analyzer, funcDecl->params);
            perform_type_checking_with_context(state, analyzer, funcDecl->body, returnType);
            semantic_exit_type_scope(state, analyzer);
            free_inferred_type_pointer(state, returnType);
            break;
        }
        
        case ZR_AST_IF_EXPRESSION: {
            SZrIfExpression *ifExpr = &node->data.ifExpression;
            perform_type_checking_with_context(state, analyzer, ifExpr->condition, currentReturnType);
            perform_type_checking_with_context(state, analyzer, ifExpr->thenExpr, currentReturnType);
            perform_type_checking_with_context(state, analyzer, ifExpr->elseExpr, currentReturnType);
            break;
        }
        
        case ZR_AST_WHILE_LOOP: {
            SZrWhileLoop *whileLoop = &node->data.whileLoop;
            perform_type_checking_with_context(state, analyzer, whileLoop->cond, currentReturnType);
            perform_type_checking_with_context(state, analyzer, whileLoop->block, currentReturnType);
            break;
        }
        
        case ZR_AST_FOR_LOOP: {
            SZrForLoop *forLoop = &node->data.forLoop;
            perform_type_checking_with_context(state, analyzer, forLoop->init, currentReturnType);
            perform_type_checking_with_context(state, analyzer, forLoop->cond, currentReturnType);
            perform_type_checking_with_context(state, analyzer, forLoop->step, currentReturnType);
            perform_type_checking_with_context(state, analyzer, forLoop->block, currentReturnType);
            break;
        }

        case ZR_AST_STRUCT_METHOD: {
            SZrStructMethod *method = &node->data.structMethod;
            SZrInferredType *returnType = method->returnType != ZR_NULL
                                              ? create_declared_symbol_type(state, analyzer, method->returnType)
                                              : ZR_NULL;
            semantic_enter_type_scope(state, analyzer);
            register_parameter_types_in_current_environment(state, analyzer, method->params);
            perform_type_checking_with_context(state, analyzer, method->body, returnType);
            semantic_exit_type_scope(state, analyzer);
            free_inferred_type_pointer(state, returnType);
            break;
        }

        case ZR_AST_STRUCT_META_FUNCTION: {
            SZrStructMetaFunction *metaFunc = &node->data.structMetaFunction;
            SZrInferredType *returnType = metaFunc->returnType != ZR_NULL
                                              ? create_declared_symbol_type(state, analyzer, metaFunc->returnType)
                                              : ZR_NULL;
            perform_type_checking_with_context(state, analyzer, metaFunc->body, returnType);
            free_inferred_type_pointer(state, returnType);
            break;
        }

        case ZR_AST_CLASS_METHOD: {
            SZrClassMethod *method = &node->data.classMethod;
            SZrInferredType *returnType = method->returnType != ZR_NULL
                                              ? create_declared_symbol_type(state, analyzer, method->returnType)
                                              : ZR_NULL;
            semantic_enter_type_scope(state, analyzer);
            register_parameter_types_in_current_environment(state, analyzer, method->params);
            perform_type_checking_with_context(state, analyzer, method->body, returnType);
            semantic_exit_type_scope(state, analyzer);
            free_inferred_type_pointer(state, returnType);
            break;
        }

        case ZR_AST_CLASS_META_FUNCTION: {
            SZrClassMetaFunction *metaFunc = &node->data.classMetaFunction;
            SZrInferredType *returnType = metaFunc->returnType != ZR_NULL
                                              ? create_declared_symbol_type(state, analyzer, metaFunc->returnType)
                                              : ZR_NULL;
            perform_type_checking_with_context(state, analyzer, metaFunc->body, returnType);
            free_inferred_type_pointer(state, returnType);
            break;
        }
        
        default:
            break;
    }
}

enum EZrDeterministicTruthValue {
    ZR_DETERMINISTIC_TRUTH_UNKNOWN = 0,
    ZR_DETERMINISTIC_TRUTH_FALSE,
    ZR_DETERMINISTIC_TRUTH_TRUE,
};

typedef enum EZrDeterministicTruthValue EZrDeterministicTruthValue;

static EZrDeterministicTruthValue invert_deterministic_truth(EZrDeterministicTruthValue value) {
    switch (value) {
        case ZR_DETERMINISTIC_TRUTH_TRUE:
            return ZR_DETERMINISTIC_TRUTH_FALSE;
        case ZR_DETERMINISTIC_TRUTH_FALSE:
            return ZR_DETERMINISTIC_TRUTH_TRUE;
        default:
            return ZR_DETERMINISTIC_TRUTH_UNKNOWN;
    }
}

static EZrDeterministicTruthValue evaluate_deterministic_truth(SZrAstNode *node) {
    if (node == ZR_NULL) {
        return ZR_DETERMINISTIC_TRUTH_UNKNOWN;
    }

    switch (node->type) {
        case ZR_AST_BOOLEAN_LITERAL:
            return node->data.booleanLiteral.value
                       ? ZR_DETERMINISTIC_TRUTH_TRUE
                       : ZR_DETERMINISTIC_TRUTH_FALSE;

        case ZR_AST_UNARY_EXPRESSION: {
            SZrUnaryExpression *unaryExpr = &node->data.unaryExpression;
            if (unaryExpr->op.op != ZR_NULL && strcmp(unaryExpr->op.op, "!") == 0) {
                return invert_deterministic_truth(
                    evaluate_deterministic_truth(unaryExpr->argument));
            }
            return ZR_DETERMINISTIC_TRUTH_UNKNOWN;
        }

        case ZR_AST_LOGICAL_EXPRESSION: {
            SZrLogicalExpression *logicalExpr = &node->data.logicalExpression;
            EZrDeterministicTruthValue leftValue = evaluate_deterministic_truth(logicalExpr->left);
            EZrDeterministicTruthValue rightValue;

            if (logicalExpr->op == ZR_NULL) {
                return ZR_DETERMINISTIC_TRUTH_UNKNOWN;
            }

            if (strcmp(logicalExpr->op, "&&") == 0) {
                if (leftValue == ZR_DETERMINISTIC_TRUTH_FALSE) {
                    return ZR_DETERMINISTIC_TRUTH_FALSE;
                }
                if (leftValue == ZR_DETERMINISTIC_TRUTH_TRUE) {
                    return evaluate_deterministic_truth(logicalExpr->right);
                }

                rightValue = evaluate_deterministic_truth(logicalExpr->right);
                if (rightValue == ZR_DETERMINISTIC_TRUTH_FALSE) {
                    return ZR_DETERMINISTIC_TRUTH_FALSE;
                }
                return ZR_DETERMINISTIC_TRUTH_UNKNOWN;
            }

            if (strcmp(logicalExpr->op, "||") == 0) {
                if (leftValue == ZR_DETERMINISTIC_TRUTH_TRUE) {
                    return ZR_DETERMINISTIC_TRUTH_TRUE;
                }
                if (leftValue == ZR_DETERMINISTIC_TRUTH_FALSE) {
                    return evaluate_deterministic_truth(logicalExpr->right);
                }

                rightValue = evaluate_deterministic_truth(logicalExpr->right);
                if (rightValue == ZR_DETERMINISTIC_TRUTH_TRUE) {
                    return ZR_DETERMINISTIC_TRUTH_TRUE;
                }
                return ZR_DETERMINISTIC_TRUTH_UNKNOWN;
            }

            return ZR_DETERMINISTIC_TRUTH_UNKNOWN;
        }

        default:
            return ZR_DETERMINISTIC_TRUTH_UNKNOWN;
    }
}

static TZrBool node_definitely_terminates(SZrAstNode *node);

static TZrBool statement_array_definitely_terminates(SZrAstNodeArray *nodes) {
    TZrSize i;

    if (nodes == ZR_NULL || nodes->nodes == ZR_NULL) {
        return ZR_FALSE;
    }

    for (i = 0; i < nodes->count; i++) {
        if (node_definitely_terminates(nodes->nodes[i])) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool node_definitely_terminates(SZrAstNode *node) {
    EZrDeterministicTruthValue conditionValue;

    if (node == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (node->type) {
        case ZR_AST_RETURN_STATEMENT:
        case ZR_AST_THROW_STATEMENT:
            return ZR_TRUE;

        case ZR_AST_BLOCK:
            return statement_array_definitely_terminates(node->data.block.body);

        case ZR_AST_IF_EXPRESSION:
            conditionValue = evaluate_deterministic_truth(node->data.ifExpression.condition);
            if (conditionValue == ZR_DETERMINISTIC_TRUTH_TRUE) {
                return node_definitely_terminates(node->data.ifExpression.thenExpr);
            }
            if (conditionValue == ZR_DETERMINISTIC_TRUTH_FALSE) {
                return node_definitely_terminates(node->data.ifExpression.elseExpr);
            }
            return node->data.ifExpression.elseExpr != ZR_NULL &&
                   node_definitely_terminates(node->data.ifExpression.thenExpr) &&
                   node_definitely_terminates(node->data.ifExpression.elseExpr);

        default:
            return ZR_FALSE;
    }
}

static void diagnose_unreachable_statement_sequence(SZrState *state,
                                                    SZrSemanticAnalyzer *analyzer,
                                                    SZrAstNodeArray *nodes) {
    TZrBool terminated = ZR_FALSE;
    TZrSize i;

    if (state == ZR_NULL || analyzer == ZR_NULL || nodes == ZR_NULL || nodes->nodes == ZR_NULL) {
        return;
    }

    for (i = 0; i < nodes->count; i++) {
        SZrAstNode *statement = nodes->nodes[i];
        if (statement == ZR_NULL) {
            continue;
        }

        if (terminated) {
            ZrLanguageServer_SemanticAnalyzer_AddDiagnostic(state,
                                                            analyzer,
                                                            ZR_DIAGNOSTIC_WARNING,
                                                            statement->location,
                                                            "Unreachable code: statement will never execute",
                                                            "unreachable_code");
        }

        perform_control_flow_analysis(state, analyzer, statement);

        if (!terminated && node_definitely_terminates(statement)) {
            terminated = ZR_TRUE;
        }
    }
}

static void perform_control_flow_analysis(SZrState *state, SZrSemanticAnalyzer *analyzer, SZrAstNode *node) {
    if (state == ZR_NULL || analyzer == ZR_NULL || node == ZR_NULL) {
        return;
    }

    switch (node->type) {
        case ZR_AST_SCRIPT:
            diagnose_unreachable_statement_sequence(state, analyzer, node->data.script.statements);
            break;

        case ZR_AST_BLOCK:
            diagnose_unreachable_statement_sequence(state, analyzer, node->data.block.body);
            break;

        case ZR_AST_FUNCTION_DECLARATION:
            perform_control_flow_analysis(state, analyzer, node->data.functionDeclaration.body);
            break;

        case ZR_AST_STRUCT_METHOD:
            perform_control_flow_analysis(state, analyzer, node->data.structMethod.body);
            break;

        case ZR_AST_STRUCT_META_FUNCTION:
            perform_control_flow_analysis(state, analyzer, node->data.structMetaFunction.body);
            break;

        case ZR_AST_CLASS_METHOD:
            perform_control_flow_analysis(state, analyzer, node->data.classMethod.body);
            break;

        case ZR_AST_CLASS_META_FUNCTION:
            perform_control_flow_analysis(state, analyzer, node->data.classMetaFunction.body);
            break;

        case ZR_AST_CLASS_DECLARATION:
            if (node->data.classDeclaration.members != ZR_NULL &&
                node->data.classDeclaration.members->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < node->data.classDeclaration.members->count; i++) {
                    perform_control_flow_analysis(state,
                                                  analyzer,
                                                  node->data.classDeclaration.members->nodes[i]);
                }
            }
            break;

        case ZR_AST_STRUCT_DECLARATION:
            if (node->data.structDeclaration.members != ZR_NULL &&
                node->data.structDeclaration.members->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < node->data.structDeclaration.members->count; i++) {
                    perform_control_flow_analysis(state,
                                                  analyzer,
                                                  node->data.structDeclaration.members->nodes[i]);
                }
            }
            break;

        case ZR_AST_IF_EXPRESSION: {
            SZrIfExpression *ifExpr = &node->data.ifExpression;
            EZrDeterministicTruthValue conditionValue = evaluate_deterministic_truth(ifExpr->condition);

            perform_control_flow_analysis(state, analyzer, ifExpr->condition);

            if (conditionValue == ZR_DETERMINISTIC_TRUTH_TRUE && ifExpr->elseExpr != ZR_NULL) {
                ZrLanguageServer_SemanticAnalyzer_AddDiagnostic(state,
                                                                analyzer,
                                                                ZR_DIAGNOSTIC_WARNING,
                                                                ifExpr->elseExpr->location,
                                                                "Unreachable branch: condition is always true",
                                                                "unreachable_branch");
            } else if (conditionValue == ZR_DETERMINISTIC_TRUTH_FALSE && ifExpr->thenExpr != ZR_NULL) {
                ZrLanguageServer_SemanticAnalyzer_AddDiagnostic(state,
                                                                analyzer,
                                                                ZR_DIAGNOSTIC_WARNING,
                                                                ifExpr->thenExpr->location,
                                                                "Unreachable branch: condition is always false",
                                                                "unreachable_branch");
            }

            perform_control_flow_analysis(state, analyzer, ifExpr->thenExpr);
            perform_control_flow_analysis(state, analyzer, ifExpr->elseExpr);
            break;
        }

        case ZR_AST_LOGICAL_EXPRESSION: {
            SZrLogicalExpression *logicalExpr = &node->data.logicalExpression;
            EZrDeterministicTruthValue leftValue;

            perform_control_flow_analysis(state, analyzer, logicalExpr->left);
            leftValue = evaluate_deterministic_truth(logicalExpr->left);

            if (logicalExpr->op != ZR_NULL && logicalExpr->right != ZR_NULL) {
                if ((strcmp(logicalExpr->op, "&&") == 0 &&
                     leftValue == ZR_DETERMINISTIC_TRUTH_FALSE) ||
                    (strcmp(logicalExpr->op, "||") == 0 &&
                     leftValue == ZR_DETERMINISTIC_TRUTH_TRUE)) {
                    ZrLanguageServer_SemanticAnalyzer_AddDiagnostic(state,
                                                                    analyzer,
                                                                    ZR_DIAGNOSTIC_WARNING,
                                                                    logicalExpr->right->location,
                                                                    "Unreachable branch: right-hand side is skipped by deterministic short-circuiting",
                                                                    "short_circuit_unreachable");
                }
            }

            perform_control_flow_analysis(state, analyzer, logicalExpr->right);
            break;
        }

        case ZR_AST_VARIABLE_DECLARATION:
            perform_control_flow_analysis(state, analyzer, node->data.variableDeclaration.value);
            break;

        case ZR_AST_EXPRESSION_STATEMENT:
            perform_control_flow_analysis(state, analyzer, node->data.expressionStatement.expr);
            break;

        case ZR_AST_RETURN_STATEMENT:
            perform_control_flow_analysis(state, analyzer, node->data.returnStatement.expr);
            break;

        case ZR_AST_THROW_STATEMENT:
            perform_control_flow_analysis(state, analyzer, node->data.throwStatement.expr);
            break;

        case ZR_AST_ASSIGNMENT_EXPRESSION:
            perform_control_flow_analysis(state, analyzer, node->data.assignmentExpression.left);
            perform_control_flow_analysis(state, analyzer, node->data.assignmentExpression.right);
            break;

        case ZR_AST_BINARY_EXPRESSION:
            perform_control_flow_analysis(state, analyzer, node->data.binaryExpression.left);
            perform_control_flow_analysis(state, analyzer, node->data.binaryExpression.right);
            break;

        case ZR_AST_UNARY_EXPRESSION:
            perform_control_flow_analysis(state, analyzer, node->data.unaryExpression.argument);
            break;

        case ZR_AST_PRIMARY_EXPRESSION: {
            SZrPrimaryExpression *primaryExpr = &node->data.primaryExpression;
            perform_control_flow_analysis(state, analyzer, primaryExpr->property);
            if (primaryExpr->members != ZR_NULL && primaryExpr->members->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < primaryExpr->members->count; i++) {
                    perform_control_flow_analysis(state, analyzer, primaryExpr->members->nodes[i]);
                }
            }
            break;
        }

        case ZR_AST_FUNCTION_CALL:
            if (node->data.functionCall.args != ZR_NULL && node->data.functionCall.args->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < node->data.functionCall.args->count; i++) {
                    perform_control_flow_analysis(state, analyzer, node->data.functionCall.args->nodes[i]);
                }
            }
            break;

        case ZR_AST_USING_STATEMENT:
            perform_control_flow_analysis(state, analyzer, node->data.usingStatement.resource);
            perform_control_flow_analysis(state, analyzer, node->data.usingStatement.body);
            break;

        case ZR_AST_WHILE_LOOP:
            perform_control_flow_analysis(state, analyzer, node->data.whileLoop.cond);
            perform_control_flow_analysis(state, analyzer, node->data.whileLoop.block);
            break;

        case ZR_AST_FOR_LOOP:
            perform_control_flow_analysis(state, analyzer, node->data.forLoop.init);
            perform_control_flow_analysis(state, analyzer, node->data.forLoop.cond);
            perform_control_flow_analysis(state, analyzer, node->data.forLoop.step);
            perform_control_flow_analysis(state, analyzer, node->data.forLoop.block);
            break;

        default:
            break;
    }
}

// 创建语义分析器
SZrSemanticAnalyzer *ZrLanguageServer_SemanticAnalyzer_New(SZrState *state) {
    if (state == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrSemanticAnalyzer *analyzer = (SZrSemanticAnalyzer *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrSemanticAnalyzer));
    if (analyzer == ZR_NULL) {
        return ZR_NULL;
    }
    
    analyzer->state = state;
    analyzer->symbolTable = ZrLanguageServer_SymbolTable_New(state);
    analyzer->referenceTracker = ZR_NULL;
    analyzer->ast = ZR_NULL;
    analyzer->cache = ZR_NULL;
    analyzer->enableCache = ZR_TRUE; // 默认启用缓存
    analyzer->compilerState = ZR_NULL; // 延迟创建
    analyzer->semanticContext = ZR_NULL;
    analyzer->hirModule = ZR_NULL;
    
    if (analyzer->symbolTable == ZR_NULL) {
        ZrCore_Memory_RawFree(state->global, analyzer, sizeof(SZrSemanticAnalyzer));
        return ZR_NULL;
    }
    
    analyzer->referenceTracker = ZrLanguageServer_ReferenceTracker_New(state, analyzer->symbolTable);
    if (analyzer->referenceTracker == ZR_NULL) {
        ZrLanguageServer_SymbolTable_Free(state, analyzer->symbolTable);
        ZrCore_Memory_RawFree(state->global, analyzer, sizeof(SZrSemanticAnalyzer));
        return ZR_NULL;
    }
    
    ZrCore_Array_Init(state, &analyzer->diagnostics, sizeof(SZrDiagnostic *), 8);
    
    // 创建缓存
    analyzer->cache = (SZrAnalysisCache *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrAnalysisCache));
    if (analyzer->cache != ZR_NULL) {
        analyzer->cache->isValid = ZR_FALSE;
        analyzer->cache->astHash = 0;
        ZrCore_Array_Init(state, &analyzer->cache->cachedDiagnostics, sizeof(SZrDiagnostic *), 8);
        ZrCore_Array_Init(state, &analyzer->cache->cachedSymbols, sizeof(SZrSymbol *), 8);
    }
    
    return analyzer;
}

// 释放语义分析器
void ZrLanguageServer_SemanticAnalyzer_Free(SZrState *state, SZrSemanticAnalyzer *analyzer) {
    if (state == ZR_NULL || analyzer == ZR_NULL) {
        return;
    }
    
    // 释放所有诊断
    for (TZrSize i = 0; i < analyzer->diagnostics.length; i++) {
        SZrDiagnostic **diagPtr = (SZrDiagnostic **)ZrCore_Array_Get(&analyzer->diagnostics, i);
        if (diagPtr != ZR_NULL && *diagPtr != ZR_NULL) {
            ZrLanguageServer_Diagnostic_Free(state, *diagPtr);
        }
    }
    
    ZrCore_Array_Free(state, &analyzer->diagnostics);
    
    // 释放缓存
    if (analyzer->cache != ZR_NULL) {
        // 释放缓存的诊断信息
        for (TZrSize i = 0; i < analyzer->cache->cachedDiagnostics.length; i++) {
            SZrDiagnostic **diagPtr = (SZrDiagnostic **)ZrCore_Array_Get(&analyzer->cache->cachedDiagnostics, i);
            if (diagPtr != ZR_NULL && *diagPtr != ZR_NULL) {
                // 注意：诊断可能在 analyzer->diagnostics 中，避免重复释放
            }
        }
        ZrCore_Array_Free(state, &analyzer->cache->cachedDiagnostics);
        ZrCore_Array_Free(state, &analyzer->cache->cachedSymbols);
        ZrCore_Memory_RawFree(state->global, analyzer->cache, sizeof(SZrAnalysisCache));
    }
    
    if (analyzer->referenceTracker != ZR_NULL) {
        ZrLanguageServer_ReferenceTracker_Free(state, analyzer->referenceTracker);
    }
    
    if (analyzer->symbolTable != ZR_NULL) {
        ZrLanguageServer_SymbolTable_Free(state, analyzer->symbolTable);
    }
    
    // 释放编译器状态
    if (analyzer->compilerState != ZR_NULL) {
        ZrParser_CompilerState_Free(analyzer->compilerState);
        ZrCore_Memory_RawFree(state->global, analyzer->compilerState, sizeof(SZrCompilerState));
    }

    analyzer->semanticContext = ZR_NULL;
    analyzer->hirModule = ZR_NULL;
    
    ZrCore_Memory_RawFree(state->global, analyzer, sizeof(SZrSemanticAnalyzer));
}

// 辅助函数：从 AST 节点提取标识符名称
static SZrString *extract_identifier_name(SZrState *state, SZrAstNode *node) {
    if (node == ZR_NULL || state == ZR_NULL) {
        return ZR_NULL;
    }
    
    if (node->type == ZR_AST_IDENTIFIER_LITERAL) {
        SZrIdentifier *identifier = &node->data.identifier;
        if (identifier != ZR_NULL && identifier->name != ZR_NULL) {
            return identifier->name;
        }
    }
    
    return ZR_NULL;
}

static SZrSymbol *lookup_symbol_at_node(SZrSemanticAnalyzer *analyzer, SZrAstNode *node) {
    SZrString *name;

    if (analyzer == ZR_NULL || analyzer->symbolTable == ZR_NULL || node == ZR_NULL) {
        return ZR_NULL;
    }

    name = extract_identifier_name(analyzer->state, node);
    if (name == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrLanguageServer_SymbolTable_LookupAtPosition(analyzer->symbolTable,
                                                         name,
                                                         node->location);
}

static SZrInferredType *create_declared_symbol_type(SZrState *state,
                                                    SZrSemanticAnalyzer *analyzer,
                                                    const SZrType *declaredType) {
    SZrInferredType *typeInfo;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    typeInfo = (SZrInferredType *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrInferredType));
    if (typeInfo == ZR_NULL) {
        return ZR_NULL;
    }

    ZrParser_InferredType_Init(state, typeInfo, ZR_VALUE_TYPE_OBJECT);
    if (declaredType == ZR_NULL) {
        return typeInfo;
    }

    if (analyzer != ZR_NULL &&
        analyzer->compilerState != ZR_NULL &&
        ZrParser_AstTypeToInferredType_Convert(analyzer->compilerState, declaredType, typeInfo)) {
        return typeInfo;
    }

    typeInfo->ownershipQualifier = declaredType->ownershipQualifier;
    return typeInfo;
}

// 辅助函数：递归计算 AST 节点的哈希值
static TZrUInt64 compute_node_hash_recursive(SZrAstNode *node, TZrSize depth) {
    if (node == ZR_NULL || depth > 32) { // 限制递归深度避免栈溢出
        return 0;
    }
    
    TZrUInt64 hash = (TZrUInt64)node->type;
    hash = hash * 31 + (TZrUInt64)node->location.start.offset;
    hash = hash * 31 + (TZrUInt64)node->location.end.offset;
    
    // 根据节点类型访问不同的子节点
    switch (node->type) {
        case ZR_AST_SCRIPT: {
            SZrScript *script = &node->data.script;
            if (script->statements != ZR_NULL && script->statements->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < script->statements->count; i++) {
                    hash = hash * 31 + compute_node_hash_recursive(script->statements->nodes[i], depth + 1);
                }
            }
            if (script->moduleName != ZR_NULL) {
                hash = hash * 31 + compute_node_hash_recursive(script->moduleName, depth + 1);
            }
            break;
        }
        
        case ZR_AST_BLOCK: {
            SZrBlock *block = &node->data.block;
            if (block->body != ZR_NULL && block->body->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < block->body->count; i++) {
                    hash = hash * 31 + compute_node_hash_recursive(block->body->nodes[i], depth + 1);
                }
            }
            break;
        }
        
        case ZR_AST_BINARY_EXPRESSION: {
            SZrBinaryExpression *binExpr = &node->data.binaryExpression;
            hash = hash * 31 + compute_node_hash_recursive(binExpr->left, depth + 1);
            hash = hash * 31 + compute_node_hash_recursive(binExpr->right, depth + 1);
            break;
        }
        
        case ZR_AST_UNARY_EXPRESSION: {
            SZrUnaryExpression *unaryExpr = &node->data.unaryExpression;
            hash = hash * 31 + compute_node_hash_recursive(unaryExpr->argument, depth + 1);
            break;
        }
        
        case ZR_AST_ASSIGNMENT_EXPRESSION: {
            SZrAssignmentExpression *assignExpr = &node->data.assignmentExpression;
            hash = hash * 31 + compute_node_hash_recursive(assignExpr->left, depth + 1);
            hash = hash * 31 + compute_node_hash_recursive(assignExpr->right, depth + 1);
            break;
        }
        
        case ZR_AST_CONDITIONAL_EXPRESSION: {
            SZrConditionalExpression *condExpr = &node->data.conditionalExpression;
            hash = hash * 31 + compute_node_hash_recursive(condExpr->test, depth + 1);
            hash = hash * 31 + compute_node_hash_recursive(condExpr->consequent, depth + 1);
            hash = hash * 31 + compute_node_hash_recursive(condExpr->alternate, depth + 1);
            break;
        }
        
        case ZR_AST_FUNCTION_CALL: {
            SZrFunctionCall *funcCall = &node->data.functionCall;
            if (funcCall->args != ZR_NULL && funcCall->args->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < funcCall->args->count; i++) {
                    hash = hash * 31 + compute_node_hash_recursive(funcCall->args->nodes[i], depth + 1);
                }
            }
            break;
        }
        
        case ZR_AST_PRIMARY_EXPRESSION: {
            SZrPrimaryExpression *primaryExpr = &node->data.primaryExpression;
            hash = hash * 31 + compute_node_hash_recursive(primaryExpr->property, depth + 1);
            if (primaryExpr->members != ZR_NULL && primaryExpr->members->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < primaryExpr->members->count; i++) {
                    hash = hash * 31 + compute_node_hash_recursive(primaryExpr->members->nodes[i], depth + 1);
                }
            }
            break;
        }
        
        case ZR_AST_VARIABLE_DECLARATION: {
            SZrVariableDeclaration *varDecl = &node->data.variableDeclaration;
            hash = hash * 31 + compute_node_hash_recursive(varDecl->pattern, depth + 1);
            hash = hash * 31 + compute_node_hash_recursive(varDecl->value, depth + 1);
            break;
        }
        
        case ZR_AST_FUNCTION_DECLARATION: {
            SZrFunctionDeclaration *funcDecl = &node->data.functionDeclaration;
            if (funcDecl->params != ZR_NULL && funcDecl->params->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < funcDecl->params->count; i++) {
                    hash = hash * 31 + compute_node_hash_recursive(funcDecl->params->nodes[i], depth + 1);
                }
            }
            hash = hash * 31 + compute_node_hash_recursive(funcDecl->body, depth + 1);
            break;
        }
        
        case ZR_AST_ARRAY_LITERAL: {
            SZrArrayLiteral *arrayLit = &node->data.arrayLiteral;
            if (arrayLit->elements != ZR_NULL && arrayLit->elements->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < arrayLit->elements->count; i++) {
                    hash = hash * 31 + compute_node_hash_recursive(arrayLit->elements->nodes[i], depth + 1);
                }
            }
            break;
        }
        
        case ZR_AST_OBJECT_LITERAL: {
            SZrObjectLiteral *objLit = &node->data.objectLiteral;
            if (objLit->properties != ZR_NULL && objLit->properties->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < objLit->properties->count; i++) {
                    hash = hash * 31 + compute_node_hash_recursive(objLit->properties->nodes[i], depth + 1);
                }
            }
            break;
        }
        
        case ZR_AST_IF_EXPRESSION: {
            SZrIfExpression *ifExpr = &node->data.ifExpression;
            hash = hash * 31 + compute_node_hash_recursive(ifExpr->condition, depth + 1);
            hash = hash * 31 + compute_node_hash_recursive(ifExpr->thenExpr, depth + 1);
            hash = hash * 31 + compute_node_hash_recursive(ifExpr->elseExpr, depth + 1);
            break;
        }
        
        case ZR_AST_WHILE_LOOP: {
            SZrWhileLoop *whileLoop = &node->data.whileLoop;
            hash = hash * 31 + compute_node_hash_recursive(whileLoop->cond, depth + 1);
            hash = hash * 31 + compute_node_hash_recursive(whileLoop->block, depth + 1);
            break;
        }
        
        case ZR_AST_FOR_LOOP: {
            SZrForLoop *forLoop = &node->data.forLoop;
            hash = hash * 31 + compute_node_hash_recursive(forLoop->init, depth + 1);
            hash = hash * 31 + compute_node_hash_recursive(forLoop->cond, depth + 1);
            hash = hash * 31 + compute_node_hash_recursive(forLoop->step, depth + 1);
            hash = hash * 31 + compute_node_hash_recursive(forLoop->block, depth + 1);
            break;
        }
        
        case ZR_AST_CLASS_DECLARATION: {
            SZrClassDeclaration *classDecl = &node->data.classDeclaration;
            if (classDecl->members != ZR_NULL && classDecl->members->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < classDecl->members->count; i++) {
                    hash = hash * 31 + compute_node_hash_recursive(classDecl->members->nodes[i], depth + 1);
                }
            }
            break;
        }
        
        case ZR_AST_STRUCT_DECLARATION: {
            SZrStructDeclaration *structDecl = &node->data.structDeclaration;
            if (structDecl->members != ZR_NULL && structDecl->members->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < structDecl->members->count; i++) {
                    hash = hash * 31 + compute_node_hash_recursive(structDecl->members->nodes[i], depth + 1);
                }
            }
            break;
        }
        
        default:
            // 对于其他节点类型，只使用基础哈希值
            break;
    }
    
    return hash;
}

// 辅助函数：计算 AST 哈希（递归实现）
static TZrSize compute_ast_hash(SZrAstNode *ast) {
    if (ast == ZR_NULL) {
        return 0;
    }
    
    return (TZrSize)compute_node_hash_recursive(ast, 0);
}

static TZrBool reset_symbol_tracking(SZrState *state, SZrSemanticAnalyzer *analyzer) {
    if (state == ZR_NULL || analyzer == ZR_NULL) {
        return ZR_FALSE;
    }

    if (analyzer->referenceTracker != ZR_NULL) {
        ZrLanguageServer_ReferenceTracker_Free(state, analyzer->referenceTracker);
        analyzer->referenceTracker = ZR_NULL;
    }
    if (analyzer->symbolTable != ZR_NULL) {
        ZrLanguageServer_SymbolTable_Free(state, analyzer->symbolTable);
        analyzer->symbolTable = ZR_NULL;
    }

    analyzer->symbolTable = ZrLanguageServer_SymbolTable_New(state);
    if (analyzer->symbolTable == ZR_NULL) {
        return ZR_FALSE;
    }

    analyzer->referenceTracker = ZrLanguageServer_ReferenceTracker_New(state, analyzer->symbolTable);
    if (analyzer->referenceTracker == ZR_NULL) {
        ZrLanguageServer_SymbolTable_Free(state, analyzer->symbolTable);
        analyzer->symbolTable = ZR_NULL;
        return ZR_FALSE;
    }

    return ZR_TRUE;
}

static TZrBool prepare_semantic_state(SZrState *state,
                                    SZrSemanticAnalyzer *analyzer,
                                    SZrAstNode *ast) {
    if (state == ZR_NULL || analyzer == ZR_NULL || ast == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!reset_symbol_tracking(state, analyzer)) {
        return ZR_FALSE;
    }

    if (analyzer->compilerState == ZR_NULL) {
        analyzer->compilerState =
            (SZrCompilerState *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrCompilerState));
        if (analyzer->compilerState == ZR_NULL) {
            return ZR_FALSE;
        }
    } else {
        ZrParser_CompilerState_Free(analyzer->compilerState);
    }

    ZrParser_CompilerState_Init(analyzer->compilerState, state);
    analyzer->compilerState->scriptAst = ast;
    if (analyzer->compilerState->typeEnv != ZR_NULL) {
        analyzer->compilerState->typeEnv->semanticContext =
            analyzer->compilerState->semanticContext;
    }
    if (analyzer->compilerState->compileTimeTypeEnv != ZR_NULL) {
        analyzer->compilerState->compileTimeTypeEnv->semanticContext =
            analyzer->compilerState->semanticContext;
    }

    if (analyzer->compilerState->hirModule != ZR_NULL) {
        ZrParser_HirModule_Free(state, analyzer->compilerState->hirModule);
        analyzer->compilerState->hirModule = ZR_NULL;
    }
    analyzer->compilerState->hirModule =
        ZrParser_HirModule_New(state, analyzer->compilerState->semanticContext, ast);
    analyzer->semanticContext = analyzer->compilerState->semanticContext;
    analyzer->hirModule = analyzer->compilerState->hirModule;

    return analyzer->semanticContext != ZR_NULL;
}

static TZrBool register_symbol_semantics(SZrSemanticAnalyzer *analyzer,
                                       SZrSymbol *symbol,
                                       EZrSemanticSymbolKind semanticKind,
                                       const SZrInferredType *typeInfo,
                                       EZrSemanticTypeKind typeKind) {
    TZrTypeId typeId = 0;
    TZrOverloadSetId overloadSetId = 0;
    TZrSymbolId symbolId;

    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL || symbol == ZR_NULL) {
        return ZR_FALSE;
    }

    if (semanticKind == ZR_SEMANTIC_SYMBOL_KIND_TYPE && typeInfo == ZR_NULL) {
        typeId = ZrParser_Semantic_RegisterNamedType(analyzer->semanticContext,
                                             symbol->name,
                                             typeKind,
                                             symbol->astNode);
    } else if (typeInfo != ZR_NULL) {
        typeId = ZrParser_Semantic_RegisterInferredType(analyzer->semanticContext,
                                                typeInfo,
                                                typeKind,
                                                typeInfo->typeName,
                                                symbol->astNode);
    }

    if (semanticKind == ZR_SEMANTIC_SYMBOL_KIND_FUNCTION) {
        overloadSetId = ZrParser_Semantic_GetOrCreateOverloadSet(analyzer->semanticContext, symbol->name);
    }

    symbolId = ZrParser_Semantic_RegisterSymbol(analyzer->semanticContext,
                                        symbol->name,
                                        semanticKind,
                                        typeId,
                                        overloadSetId,
                                        symbol->astNode,
                                        symbol->location);
    if (overloadSetId != 0) {
        ZrParser_Semantic_AddOverloadMember(analyzer->semanticContext, overloadSetId, symbolId);
    }

    symbol->semanticId = symbolId;
    symbol->semanticTypeId = typeId;
    symbol->overloadSetId = overloadSetId;
    return symbolId != 0;
}

static TZrSymbolId resolve_semantic_symbol_id_for_node(SZrSemanticAnalyzer *analyzer,
                                                       SZrAstNode *node) {
    SZrSymbol *symbol;

    if (analyzer == ZR_NULL || analyzer->symbolTable == ZR_NULL || node == ZR_NULL) {
        return 0;
    }

    symbol = lookup_symbol_at_node(analyzer, node);
    if (symbol == ZR_NULL) {
        return 0;
    }

    return symbol->semanticId;
}

static void record_template_string_segments(SZrSemanticAnalyzer *analyzer,
                                            SZrAstNode *node) {
    SZrTemplateStringLiteral *templateLiteral;

    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL || node == ZR_NULL ||
        node->type != ZR_AST_TEMPLATE_STRING_LITERAL) {
        return;
    }

    templateLiteral = &node->data.templateStringLiteral;
    if (templateLiteral->segments == ZR_NULL || templateLiteral->segments->nodes == ZR_NULL) {
        return;
    }

    for (TZrSize i = 0; i < templateLiteral->segments->count; i++) {
        SZrAstNode *segmentNode = templateLiteral->segments->nodes[i];
        SZrTemplateSegment segment;

        if (segmentNode == ZR_NULL) {
            continue;
        }

        segment.isInterpolation = ZR_FALSE;
        segment.staticText = ZR_NULL;
        segment.expression = ZR_NULL;

        if (segmentNode->type == ZR_AST_STRING_LITERAL) {
            segment.staticText = segmentNode->data.stringLiteral.value;
        } else if (segmentNode->type == ZR_AST_INTERPOLATED_SEGMENT) {
            segment.isInterpolation = ZR_TRUE;
            segment.expression = segmentNode->data.interpolatedSegment.expression;
        } else {
            continue;
        }

        ZrParser_Semantic_AppendTemplateSegment(analyzer->semanticContext, &segment);
    }
}

static void record_using_cleanup_step(SZrSemanticAnalyzer *analyzer,
                                      SZrAstNode *resource) {
    SZrDeterministicCleanupStep step;

    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL || resource == ZR_NULL) {
        return;
    }

    step.kind = ZR_DETERMINISTIC_CLEANUP_KIND_BLOCK_SCOPE;
    step.regionId = ZrParser_Semantic_ReserveLifetimeRegionId(analyzer->semanticContext);
    step.ownerRegionId = step.regionId;
    step.symbolId = resolve_semantic_symbol_id_for_node(analyzer, resource);
    step.declarationOrder = (TZrInt32)analyzer->semanticContext->cleanupPlan.length;
    step.callsClose = ZR_TRUE;
    step.callsDestructor = ZR_TRUE;
    ZrParser_Semantic_AppendCleanupStep(analyzer->semanticContext, &step);
}

static SZrInferredType *create_field_symbol_type(SZrState *state,
                                                 SZrSemanticAnalyzer *analyzer,
                                                 const SZrType *fieldType) {
    SZrInferredType *typeInfo;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    typeInfo = (SZrInferredType *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrInferredType));
    if (typeInfo == ZR_NULL) {
        return ZR_NULL;
    }

    ZrParser_InferredType_Init(state, typeInfo, ZR_VALUE_TYPE_OBJECT);
    if (fieldType == ZR_NULL) {
        return typeInfo;
    }

    if (analyzer != ZR_NULL && analyzer->compilerState != ZR_NULL &&
        ZrParser_AstTypeToInferredType_Convert(analyzer->compilerState, fieldType, typeInfo)) {
        return typeInfo;
    }

    ZrParser_InferredType_Free(state, typeInfo);
    ZrParser_InferredType_Init(state, typeInfo, ZR_VALUE_TYPE_OBJECT);
    typeInfo->ownershipQualifier = fieldType->ownershipQualifier;
    return typeInfo;
}

static void record_field_cleanup_step(SZrSemanticAnalyzer *analyzer,
                                      SZrSymbol *symbol,
                                      EZrDeterministicCleanupKind kind,
                                      TZrLifetimeRegionId ownerRegionId,
                                      TZrInt32 declarationOrder) {
    SZrDeterministicCleanupStep step;

    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL || symbol == ZR_NULL ||
        symbol->semanticId == 0) {
        return;
    }

    step.kind = kind;
    step.regionId = ZrParser_Semantic_ReserveLifetimeRegionId(analyzer->semanticContext);
    step.ownerRegionId = ownerRegionId;
    step.symbolId = symbol->semanticId;
    step.declarationOrder = declarationOrder;
    step.callsClose = ZR_TRUE;
    step.callsDestructor = ZR_TRUE;
    ZrParser_Semantic_AppendCleanupStep(analyzer->semanticContext, &step);
}

static void register_field_symbol_from_ast(SZrState *state,
                                           SZrSemanticAnalyzer *analyzer,
                                           SZrAstNode *fieldNode,
                                           TZrLifetimeRegionId ownerRegionId,
                                           EZrDeterministicCleanupKind cleanupKind,
                                           TZrInt32 declarationOrder) {
    SZrSymbol *symbol = ZR_NULL;
    SZrString *name = ZR_NULL;
    SZrInferredType *typeInfo = ZR_NULL;
    EZrAccessModifier accessModifier = ZR_ACCESS_PRIVATE;
    TZrBool isUsingManaged = ZR_FALSE;
    TZrBool isStatic = ZR_FALSE;
    const SZrType *fieldType = ZR_NULL;

    if (state == ZR_NULL || analyzer == ZR_NULL || fieldNode == ZR_NULL) {
        return;
    }

    if (fieldNode->type == ZR_AST_STRUCT_FIELD) {
        SZrStructField *field = &fieldNode->data.structField;
        name = field->name != ZR_NULL ? field->name->name : ZR_NULL;
        accessModifier = field->access;
        isUsingManaged = field->isUsingManaged;
        isStatic = field->isStatic;
        fieldType = field->typeInfo;
    } else if (fieldNode->type == ZR_AST_CLASS_FIELD) {
        SZrClassField *field = &fieldNode->data.classField;
        name = field->name != ZR_NULL ? field->name->name : ZR_NULL;
        accessModifier = field->access;
        isUsingManaged = field->isUsingManaged;
        isStatic = field->isStatic;
        fieldType = field->typeInfo;
    } else {
        return;
    }

    if (name == ZR_NULL) {
        return;
    }

    typeInfo = create_field_symbol_type(state, analyzer, fieldType);
    ZrLanguageServer_SymbolTable_AddSymbolEx(state,
                             analyzer->symbolTable,
                             ZR_SYMBOL_FIELD,
                             name,
                             fieldNode->location,
                             typeInfo,
                             accessModifier,
                             fieldNode,
                             &symbol);

    if (symbol != ZR_NULL) {
        register_symbol_semantics(analyzer,
                                  symbol,
                                  ZR_SEMANTIC_SYMBOL_KIND_FIELD,
                                  typeInfo,
                                  ZR_SEMANTIC_TYPE_KIND_UNKNOWN);
    }

    if (!isUsingManaged) {
        return;
    }

    if (isStatic) {
        ZrLanguageServer_SemanticAnalyzer_AddDiagnostic(state,
                                        analyzer,
                                        ZR_DIAGNOSTIC_ERROR,
                                        fieldNode->location,
                                        "Field-scoped `using` only supports instance fields",
                                        "static_using_field");
        return;
    }

    record_field_cleanup_step(analyzer,
                              symbol,
                              cleanupKind,
                              ownerRegionId,
                              declarationOrder);
}

static void collect_symbols_from_ast(SZrState *state, SZrSemanticAnalyzer *analyzer, SZrAstNode *node);

static void collect_symbols_from_node_array(SZrState *state,
                                            SZrSemanticAnalyzer *analyzer,
                                            SZrAstNodeArray *nodes) {
    if (nodes == ZR_NULL || nodes->nodes == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < nodes->count; index++) {
        if (nodes->nodes[index] != ZR_NULL) {
            collect_symbols_from_ast(state, analyzer, nodes->nodes[index]);
        }
    }
}

static void collect_parameter_symbols(SZrState *state,
                                      SZrSemanticAnalyzer *analyzer,
                                      SZrAstNodeArray *params,
                                      SZrArray *paramTypes) {
    if (state == ZR_NULL || analyzer == ZR_NULL || params == ZR_NULL || params->nodes == ZR_NULL) {
        return;
    }

    if (paramTypes != ZR_NULL && !paramTypes->isValid) {
        ZrCore_Array_Init(state, paramTypes, sizeof(SZrInferredType), 4);
    }

    for (TZrSize index = 0; index < params->count; index++) {
        SZrAstNode *paramNode = params->nodes[index];
        SZrParameter *parameter;
        SZrInferredType *typeInfo;
        SZrSymbol *symbol = ZR_NULL;

        if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
            continue;
        }

        parameter = &paramNode->data.parameter;
        if (parameter->name == ZR_NULL || parameter->name->name == ZR_NULL) {
            continue;
        }

        typeInfo = create_declared_symbol_type(state, analyzer, parameter->typeInfo);
        ZrLanguageServer_SymbolTable_AddSymbolEx(state,
                                 analyzer->symbolTable,
                                 ZR_SYMBOL_PARAMETER,
                                 parameter->name->name,
                                 paramNode->location,
                                 typeInfo,
                                 ZR_ACCESS_PRIVATE,
                                 paramNode,
                                 &symbol);
        if (analyzer->compilerState != ZR_NULL &&
            analyzer->compilerState->typeEnv != ZR_NULL &&
            typeInfo != ZR_NULL) {
            ZrParser_TypeEnvironment_RegisterVariable(state,
                                             analyzer->compilerState->typeEnv,
                                             parameter->name->name,
                                             typeInfo);
        }
        if (paramTypes != ZR_NULL && paramTypes->isValid && typeInfo != ZR_NULL) {
            SZrInferredType copiedType;
            ZrParser_InferredType_Copy(state, &copiedType, typeInfo);
            ZrCore_Array_Push(state, paramTypes, &copiedType);
        }
        register_symbol_semantics(analyzer,
                                  symbol,
                                  ZR_SEMANTIC_SYMBOL_KIND_PARAMETER,
                                  typeInfo,
                                  ZR_SEMANTIC_TYPE_KIND_UNKNOWN);
    }
}

static void free_inferred_type_value_array(SZrState *state, SZrArray *types) {
    TZrSize i;

    if (state == ZR_NULL || types == ZR_NULL || !types->isValid) {
        return;
    }

    for (i = 0; i < types->length; i++) {
        SZrInferredType *typeInfo = (SZrInferredType *)ZrCore_Array_Get(types, i);
        if (typeInfo != ZR_NULL) {
            ZrParser_InferredType_Free(state, typeInfo);
        }
    }
    ZrCore_Array_Free(state, types);
}

static void collect_parameter_type_records(SZrState *state,
                                           SZrSemanticAnalyzer *analyzer,
                                           SZrAstNodeArray *params,
                                           SZrArray *paramTypes) {
    TZrSize i;

    if (state == ZR_NULL || analyzer == ZR_NULL || params == ZR_NULL || paramTypes == ZR_NULL ||
        params->nodes == ZR_NULL) {
        return;
    }

    for (i = 0; i < params->count; i++) {
        SZrAstNode *paramNode = params->nodes[i];
        SZrParameter *parameter;
        SZrInferredType *typeInfo;
        SZrInferredType copiedType;

        if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
            continue;
        }

        parameter = &paramNode->data.parameter;
        typeInfo = create_declared_symbol_type(state, analyzer, parameter->typeInfo);
        if (typeInfo == ZR_NULL) {
            continue;
        }

        ZrParser_InferredType_Copy(state, &copiedType, typeInfo);
        ZrCore_Array_Push(state, paramTypes, &copiedType);
        free_inferred_type_pointer(state, typeInfo);
    }
}

static void register_method_function_type(SZrState *state,
                                          SZrSemanticAnalyzer *analyzer,
                                          SZrString *ownerTypeName,
                                          SZrString *methodName,
                                          SZrAstNodeArray *params,
                                          const SZrType *returnType) {
    SZrArray paramTypes;
    SZrInferredType *methodReturnType;
    SZrString *qualifiedName;

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL ||
        analyzer->compilerState->typeEnv == ZR_NULL || ownerTypeName == ZR_NULL || methodName == ZR_NULL) {
        return;
    }

    ZrCore_Array_Construct(&paramTypes);
    ZrCore_Array_Init(state, &paramTypes, sizeof(SZrInferredType), 4);
    collect_parameter_type_records(state, analyzer, params, &paramTypes);

    methodReturnType = create_declared_symbol_type(state, analyzer, returnType);
    qualifiedName = semantic_build_method_function_name(state, ownerTypeName, methodName);
    if (qualifiedName != ZR_NULL && methodReturnType != ZR_NULL) {
        ZrParser_TypeEnvironment_RegisterFunction(state,
                                                  analyzer->compilerState->typeEnv,
                                                  qualifiedName,
                                                  methodReturnType,
                                                  &paramTypes);
    }

    free_inferred_type_pointer(state, methodReturnType);
    free_inferred_type_value_array(state, &paramTypes);
}

static void register_methods_from_members(SZrState *state,
                                          SZrSemanticAnalyzer *analyzer,
                                          SZrString *ownerTypeName,
                                          SZrAstNodeArray *members) {
    if (state == ZR_NULL || analyzer == ZR_NULL || ownerTypeName == ZR_NULL ||
        members == ZR_NULL || members->nodes == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < members->count; index++) {
        SZrAstNode *member = members->nodes[index];

        if (member == ZR_NULL) {
            continue;
        }

        if (member->type == ZR_AST_CLASS_METHOD) {
            SZrClassMethod *method = &member->data.classMethod;
            register_method_function_type(state,
                                          analyzer,
                                          ownerTypeName,
                                          method->name != ZR_NULL ? method->name->name : ZR_NULL,
                                          method->params,
                                          method->returnType);
        } else if (member->type == ZR_AST_STRUCT_METHOD) {
            SZrStructMethod *method = &member->data.structMethod;
            register_method_function_type(state,
                                          analyzer,
                                          ownerTypeName,
                                          method->name != ZR_NULL ? method->name->name : ZR_NULL,
                                          method->params,
                                          method->returnType);
        }
    }
}

static void semantic_enter_type_scope(SZrState *state,
                                      SZrSemanticAnalyzer *analyzer) {
    SZrCompilerState *cs;
    SZrTypeEnvironment *newEnv;

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL) {
        return;
    }

    cs = analyzer->compilerState;
    newEnv = ZrParser_TypeEnvironment_New(state);
    if (newEnv == ZR_NULL) {
        return;
    }

    newEnv->parent = cs->typeEnv;
    newEnv->semanticContext = cs->typeEnv != ZR_NULL ? cs->typeEnv->semanticContext : cs->semanticContext;
    if (cs->typeEnv != ZR_NULL) {
        ZrCore_Array_Push(state, &cs->typeEnvStack, &cs->typeEnv);
    }
    cs->typeEnv = newEnv;
}

static void semantic_exit_type_scope(SZrState *state,
                                     SZrSemanticAnalyzer *analyzer) {
    SZrCompilerState *cs;

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL) {
        return;
    }

    cs = analyzer->compilerState;
    if (cs->typeEnv != ZR_NULL) {
        ZrParser_TypeEnvironment_Free(state, cs->typeEnv);
    }

    if (cs->typeEnvStack.length > 0) {
        SZrTypeEnvironment **parentEnvPtr = (SZrTypeEnvironment **)ZrCore_Array_Pop(&cs->typeEnvStack);
        cs->typeEnv = parentEnvPtr != ZR_NULL ? *parentEnvPtr : ZR_NULL;
    } else {
        cs->typeEnv = ZR_NULL;
    }
}

static void register_parameter_types_in_current_environment(SZrState *state,
                                                            SZrSemanticAnalyzer *analyzer,
                                                            SZrAstNodeArray *params) {
    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL ||
        analyzer->compilerState->typeEnv == ZR_NULL || params == ZR_NULL || params->nodes == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < params->count; index++) {
        SZrAstNode *paramNode = params->nodes[index];
        SZrParameter *parameter;
        SZrInferredType *typeInfo;

        if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
            continue;
        }

        parameter = &paramNode->data.parameter;
        if (parameter->name == ZR_NULL || parameter->name->name == ZR_NULL) {
            continue;
        }

        typeInfo = create_declared_symbol_type(state, analyzer, parameter->typeInfo);
        if (typeInfo == ZR_NULL) {
            continue;
        }

        ZrParser_TypeEnvironment_RegisterVariable(state,
                                                  analyzer->compilerState->typeEnv,
                                                  parameter->name->name,
                                                  typeInfo);
        free_inferred_type_pointer(state, typeInfo);
    }
}

static void collect_function_scope_symbols(SZrState *state,
                                           SZrSemanticAnalyzer *analyzer,
                                           SZrFileRange scopeRange,
                                           SZrAstNodeArray *params,
                                           SZrAstNode *body,
                                           SZrArray *paramTypes) {
    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->symbolTable == ZR_NULL) {
        return;
    }

    ZrLanguageServer_SymbolTable_EnterScope(state,
                                            analyzer->symbolTable,
                                            scopeRange,
                                            ZR_TRUE,
                                            ZR_FALSE,
                                            ZR_FALSE);
    collect_parameter_symbols(state, analyzer, params, paramTypes);
    if (body != ZR_NULL) {
        collect_symbols_from_ast(state, analyzer, body);
    }
    ZrLanguageServer_SymbolTable_ExitScope(analyzer->symbolTable);
}

// 辅助函数：遍历 AST 收集符号定义
static void collect_symbols_from_ast(SZrState *state, SZrSemanticAnalyzer *analyzer, SZrAstNode *node) {
    if (state == ZR_NULL || analyzer == ZR_NULL || node == ZR_NULL) {
        return;
    }
    
    switch (node->type) {
        case ZR_AST_SCRIPT: {
            // 脚本节点：遍历 statements 数组
            SZrScript *script = &node->data.script;
            if (script->statements != ZR_NULL && script->statements->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < script->statements->count; i++) {
                    if (script->statements->nodes[i] != ZR_NULL) {
                        collect_symbols_from_ast(state, analyzer, script->statements->nodes[i]);
                    }
                }
            }
            // 处理 moduleName（如果有）
            if (script->moduleName != ZR_NULL) {
                collect_symbols_from_ast(state, analyzer, script->moduleName);
            }
            return; // 已经递归处理，不需要继续
        }
        
        case ZR_AST_BLOCK: {
            // 块节点：遍历 body 数组
            SZrBlock *block = &node->data.block;
            if (block->body != ZR_NULL && block->body->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < block->body->count; i++) {
                    if (block->body->nodes[i] != ZR_NULL) {
                        collect_symbols_from_ast(state, analyzer, block->body->nodes[i]);
                    }
                }
            }
            return; // 已经递归处理，不需要继续
        }
        
        case ZR_AST_VARIABLE_DECLARATION: {
            SZrVariableDeclaration *varDecl = &node->data.variableDeclaration;
            SZrSymbol *symbol = ZR_NULL;
            // pattern 可能是 Identifier, DestructuringPattern, 或 DestructuringArrayPattern
            SZrString *name = extract_identifier_name(state, varDecl->pattern);
            if (name != ZR_NULL) {
                // 推断类型（集成类型推断系统）
                SZrInferredType *typeInfo = (SZrInferredType *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrInferredType));
                if (typeInfo != ZR_NULL) {
                    if (varDecl->typeInfo != ZR_NULL) {
                        // 转换 AST 类型到推断类型
                        // TODO: 简化实现：根据类型名称推断基础类型
                        // 完整实现需要使用 ZrParser_AstTypeToInferredType_Convert
                        ZrParser_InferredType_Init(state, typeInfo, ZR_VALUE_TYPE_OBJECT);
                    } else if (varDecl->value != ZR_NULL) {
                        // 从值推断类型
                        // 使用类型推断系统
                        if (analyzer->compilerState != ZR_NULL) {
                            SZrInferredType inferredType;
                            ZrParser_InferredType_Init(state, &inferredType, ZR_VALUE_TYPE_OBJECT);
                            if (ZrParser_ExpressionType_Infer(analyzer->compilerState, varDecl->value, &inferredType)) {
                                // 复制推断类型
                                *typeInfo = inferredType;
                            } else {
                                ZrParser_InferredType_Free(state, &inferredType);
                                // TODO: 回退到简化实现
                                if (varDecl->value->type == ZR_AST_INTEGER_LITERAL) {
                                    ZrParser_InferredType_Init(state, typeInfo, ZR_VALUE_TYPE_INT64);
                                } else if (varDecl->value->type == ZR_AST_FLOAT_LITERAL) {
                                    ZrParser_InferredType_Init(state, typeInfo, ZR_VALUE_TYPE_DOUBLE);
                                } else if (varDecl->value->type == ZR_AST_STRING_LITERAL) {
                                    ZrParser_InferredType_Init(state, typeInfo, ZR_VALUE_TYPE_STRING);
                                } else if (varDecl->value->type == ZR_AST_BOOLEAN_LITERAL) {
                                    ZrParser_InferredType_Init(state, typeInfo, ZR_VALUE_TYPE_BOOL);
                                } else {
                                    ZrParser_InferredType_Init(state, typeInfo, ZR_VALUE_TYPE_OBJECT);
                                }
                            }
                        } else {
                            // TODO: 简化实现：根据字面量类型推断
                            if (varDecl->value->type == ZR_AST_INTEGER_LITERAL) {
                                ZrParser_InferredType_Init(state, typeInfo, ZR_VALUE_TYPE_INT64);
                            } else if (varDecl->value->type == ZR_AST_FLOAT_LITERAL) {
                                ZrParser_InferredType_Init(state, typeInfo, ZR_VALUE_TYPE_DOUBLE);
                            } else if (varDecl->value->type == ZR_AST_STRING_LITERAL) {
                                ZrParser_InferredType_Init(state, typeInfo, ZR_VALUE_TYPE_STRING);
                            } else if (varDecl->value->type == ZR_AST_BOOLEAN_LITERAL) {
                                ZrParser_InferredType_Init(state, typeInfo, ZR_VALUE_TYPE_BOOL);
                            } else {
                                ZrParser_InferredType_Init(state, typeInfo, ZR_VALUE_TYPE_OBJECT);
                            }
                        }
                    } else {
                        // 默认类型
                        ZrParser_InferredType_Init(state, typeInfo, ZR_VALUE_TYPE_OBJECT);
                    }
                }
                
                ZrLanguageServer_SymbolTable_AddSymbolEx(state, analyzer->symbolTable,
                                         ZR_SYMBOL_VARIABLE, name,
                                         node->location, typeInfo,
                                         varDecl->accessModifier, node,
                                         &symbol);
                if (analyzer->compilerState != ZR_NULL &&
                    analyzer->compilerState->typeEnv != ZR_NULL &&
                    typeInfo != ZR_NULL) {
                    ZrParser_TypeEnvironment_RegisterVariable(state,
                                                     analyzer->compilerState->typeEnv,
                                                     name,
                                                     typeInfo);
                }
                register_symbol_semantics(analyzer,
                                          symbol,
                                          ZR_SEMANTIC_SYMBOL_KIND_VARIABLE,
                                          typeInfo,
                                          ZR_SEMANTIC_TYPE_KIND_UNKNOWN);
            }
            break;
        }

        case ZR_AST_USING_STATEMENT: {
            SZrUsingStatement *usingStmt = &node->data.usingStatement;
            if (usingStmt->body != ZR_NULL) {
                collect_symbols_from_ast(state, analyzer, usingStmt->body);
            }
            break;
        }
        
        case ZR_AST_FUNCTION_DECLARATION: {
            SZrFunctionDeclaration *funcDecl = &node->data.functionDeclaration;
            SZrSymbol *symbol = ZR_NULL;
            SZrArray paramTypes;
            SZrString *name = funcDecl->name != ZR_NULL ? funcDecl->name->name : ZR_NULL;
            ZrCore_Array_Construct(&paramTypes);
            ZrCore_Array_Init(state, &paramTypes, sizeof(SZrInferredType), 4);
            collect_parameter_type_records(state, analyzer, funcDecl->params, &paramTypes);
            if (name != ZR_NULL) {
                // 推断返回类型（集成类型推断系统）
                SZrInferredType *returnType =
                    create_declared_symbol_type(state, analyzer, funcDecl->returnType);
                
                // SZrFunctionDeclaration 没有 accessModifier 成员，使用默认值
                ZrLanguageServer_SymbolTable_AddSymbolEx(state, analyzer->symbolTable,
                                         ZR_SYMBOL_FUNCTION, name,
                                         node->location, returnType,
                                         ZR_ACCESS_PUBLIC, node,
                                         &symbol);
                if (analyzer->compilerState != ZR_NULL &&
                    analyzer->compilerState->typeEnv != ZR_NULL &&
                    returnType != ZR_NULL) {
                    ZrParser_TypeEnvironment_RegisterFunction(state,
                                                     analyzer->compilerState->typeEnv,
                                                     name,
                                                     returnType,
                                                     &paramTypes);
                }
                register_symbol_semantics(analyzer,
                                          symbol,
                                          ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
                                         returnType,
                                         ZR_SEMANTIC_TYPE_KIND_UNKNOWN);
            }
            collect_function_scope_symbols(state,
                                           analyzer,
                                           node->location,
                                           funcDecl->params,
                                           funcDecl->body,
                                           ZR_NULL);
            free_inferred_type_value_array(state, &paramTypes);
            break;
        }
        
        case ZR_AST_CLASS_DECLARATION: {
            SZrClassDeclaration *classDecl = &node->data.classDeclaration;
            SZrSymbol *symbol = ZR_NULL;
            SZrString *name = classDecl->name != ZR_NULL ? classDecl->name->name : ZR_NULL;
            TZrLifetimeRegionId ownerRegionId = 0;
            if (name != ZR_NULL) {
                ZrLanguageServer_SymbolTable_AddSymbolEx(state, analyzer->symbolTable,
                                         ZR_SYMBOL_CLASS, name,
                                         node->location, ZR_NULL,
                                         classDecl->accessModifier, node,
                                         &symbol);
                if (analyzer->compilerState != ZR_NULL &&
                    analyzer->compilerState->typeEnv != ZR_NULL) {
                    ZrParser_TypeEnvironment_RegisterType(state, analyzer->compilerState->typeEnv, name);
                }
                register_symbol_semantics(analyzer,
                                          symbol,
                                          ZR_SEMANTIC_SYMBOL_KIND_TYPE,
                                          ZR_NULL,
                                          ZR_SEMANTIC_TYPE_KIND_REFERENCE);
                
                // 检查类实现的接口，验证 const 字段匹配
                if (classDecl->inherits != ZR_NULL && classDecl->inherits->count > 0) {
                    for (TZrSize i = 0; i < classDecl->inherits->count; i++) {
                        SZrAstNode *inheritNode = classDecl->inherits->nodes[i];
                        if (inheritNode != ZR_NULL && inheritNode->type == ZR_AST_TYPE) {
                            // 查找接口定义
                            SZrType *inheritType = &inheritNode->data.type;
                            if (inheritType->name != ZR_NULL && 
                                inheritType->name->type == ZR_AST_IDENTIFIER_LITERAL) {
                                SZrString *interfaceName = inheritType->name->data.identifier.name;
                                if (interfaceName != ZR_NULL) {
                                    SZrSymbol *interfaceSymbol = ZrLanguageServer_SymbolTable_Lookup(analyzer->symbolTable, interfaceName, ZR_NULL);
                                    if (interfaceSymbol != ZR_NULL && 
                                        interfaceSymbol->type == ZR_SYMBOL_INTERFACE &&
                                        interfaceSymbol->astNode != ZR_NULL &&
                                        interfaceSymbol->astNode->type == ZR_AST_INTERFACE_DECLARATION) {
                                        // 检查接口中的 const 字段是否在类中也标记为 const
                                        SZrInterfaceDeclaration *interfaceDecl = &interfaceSymbol->astNode->data.interfaceDeclaration;
                                        if (interfaceDecl->members != ZR_NULL) {
                                            for (TZrSize j = 0; j < interfaceDecl->members->count; j++) {
                                                SZrAstNode *interfaceMember = interfaceDecl->members->nodes[j];
                                                if (interfaceMember != ZR_NULL && 
                                                    interfaceMember->type == ZR_AST_INTERFACE_FIELD_DECLARATION) {
                                                    SZrInterfaceFieldDeclaration *interfaceField = &interfaceMember->data.interfaceFieldDeclaration;
                                                    if (interfaceField->isConst && interfaceField->name != ZR_NULL) {
                                                        SZrString *fieldName = interfaceField->name->name;
                                                        // 在类中查找对应的字段
                                                        if (classDecl->members != ZR_NULL) {
                                                            TZrBool found = ZR_FALSE;
                                                            for (TZrSize k = 0; k < classDecl->members->count; k++) {
                                                                SZrAstNode *classMember = classDecl->members->nodes[k];
                                                                if (classMember != ZR_NULL && 
                                                                    classMember->type == ZR_AST_CLASS_FIELD) {
                                                                    SZrClassField *classField = &classMember->data.classField;
                                                                    if (classField->name != ZR_NULL && 
                                                                        ZrCore_String_Equal(classField->name->name, fieldName)) {
                                                                        found = ZR_TRUE;
                                                                        // 检查类字段是否也是 const
                                                                        if (!classField->isConst) {
                                                                            TZrChar errorMsg[256];
                                                                            TZrNativeString fieldNameStr = ZrCore_String_GetNativeStringShort(fieldName);
                                                                            if (fieldNameStr != ZR_NULL) {
                                                                                snprintf(errorMsg, sizeof(errorMsg), 
                                                                                        "Interface field '%s' is const, but implementation field is not const", 
                                                                                        fieldNameStr);
                                                                            } else {
                                                                                snprintf(errorMsg, sizeof(errorMsg), 
                                                                                        "Interface field is const, but implementation field is not const");
                                                                            }
                                                                            ZrLanguageServer_SemanticAnalyzer_AddDiagnostic(state, analyzer,
                                                                                                            ZR_DIAGNOSTIC_ERROR,
                                                                                                            classMember->location,
                                                                                                            errorMsg,
                                            "const_interface_mismatch");
                                                                        }
                                                                        break;
                                                                    }
                                                                }
                                                            }
                                                            // TODO: 如果字段未找到，也应该报告错误（字段缺失）
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            if (classDecl->members != ZR_NULL) {
                ownerRegionId = analyzer->semanticContext != ZR_NULL
                                    ? ZrParser_Semantic_ReserveLifetimeRegionId(analyzer->semanticContext)
                                    : 0;
                for (TZrSize memberIndex = 0; memberIndex < classDecl->members->count; memberIndex++) {
                    SZrAstNode *classMember = classDecl->members->nodes[memberIndex];
                    if (classMember != ZR_NULL && classMember->type == ZR_AST_CLASS_FIELD) {
                        register_field_symbol_from_ast(state,
                                                       analyzer,
                                                       classMember,
                                                       ownerRegionId,
                                                       ZR_DETERMINISTIC_CLEANUP_KIND_INSTANCE_FIELD,
                                                       (TZrInt32)memberIndex);
                    }
                }
                register_methods_from_members(state, analyzer, name, classDecl->members);
                collect_symbols_from_node_array(state, analyzer, classDecl->members);
            }
            break;
        }

        case ZR_AST_STRUCT_DECLARATION: {
            SZrStructDeclaration *structDecl = &node->data.structDeclaration;
            SZrSymbol *symbol = ZR_NULL;
            SZrString *name = structDecl->name != ZR_NULL ? structDecl->name->name : ZR_NULL;
            TZrLifetimeRegionId ownerRegionId = 0;
            if (name != ZR_NULL) {
                ZrLanguageServer_SymbolTable_AddSymbolEx(state, analyzer->symbolTable,
                                         ZR_SYMBOL_STRUCT, name,
                                         node->location, ZR_NULL,
                                         structDecl->accessModifier, node,
                                         &symbol);
                if (analyzer->compilerState != ZR_NULL &&
                    analyzer->compilerState->typeEnv != ZR_NULL) {
                    ZrParser_TypeEnvironment_RegisterType(state, analyzer->compilerState->typeEnv, name);
                }
                register_symbol_semantics(analyzer,
                                          symbol,
                                          ZR_SEMANTIC_SYMBOL_KIND_TYPE,
                                          ZR_NULL,
                                          ZR_SEMANTIC_TYPE_KIND_VALUE);
            }

            if (structDecl->members != ZR_NULL) {
                ownerRegionId = analyzer->semanticContext != ZR_NULL
                                    ? ZrParser_Semantic_ReserveLifetimeRegionId(analyzer->semanticContext)
                                    : 0;
                for (TZrSize memberIndex = 0; memberIndex < structDecl->members->count; memberIndex++) {
                    SZrAstNode *structMember = structDecl->members->nodes[memberIndex];
                    if (structMember != ZR_NULL && structMember->type == ZR_AST_STRUCT_FIELD) {
                        register_field_symbol_from_ast(state,
                                                       analyzer,
                                                       structMember,
                                                       ownerRegionId,
                                                       ZR_DETERMINISTIC_CLEANUP_KIND_STRUCT_VALUE_FIELD,
                                                       (TZrInt32)memberIndex);
                    }
                }
                register_methods_from_members(state, analyzer, name, structDecl->members);
                collect_symbols_from_node_array(state, analyzer, structDecl->members);
            }
            break;
        }

        case ZR_AST_STRUCT_METHOD: {
            SZrStructMethod *method = &node->data.structMethod;
            collect_function_scope_symbols(state,
                                           analyzer,
                                           node->location,
                                           method->params,
                                           method->body,
                                           ZR_NULL);
            break;
        }

        case ZR_AST_STRUCT_META_FUNCTION: {
            SZrStructMetaFunction *metaFunc = &node->data.structMetaFunction;
            collect_function_scope_symbols(state,
                                           analyzer,
                                           node->location,
                                           metaFunc->params,
                                           metaFunc->body,
                                           ZR_NULL);
            break;
        }

        case ZR_AST_CLASS_METHOD: {
            SZrClassMethod *method = &node->data.classMethod;
            collect_function_scope_symbols(state,
                                           analyzer,
                                           node->location,
                                           method->params,
                                           method->body,
                                           ZR_NULL);
            break;
        }

        case ZR_AST_CLASS_META_FUNCTION: {
            SZrClassMetaFunction *metaFunc = &node->data.classMetaFunction;
            collect_function_scope_symbols(state,
                                           analyzer,
                                           node->location,
                                           metaFunc->params,
                                           metaFunc->body,
                                           ZR_NULL);
            break;
        }
        
        default:
            // 对于其他节点类型，继续递归处理可能的子节点
            break;
    }
    
    // 递归处理子节点（根据不同节点类型访问不同的子节点数组）
    // 对于已处理的节点类型（如 SCRIPT, BLOCK），已经在 switch 中处理并返回
    // 对于其他节点类型，需要根据具体情况递归处理子节点
    switch (node->type) {
        case ZR_AST_USING_STATEMENT: {
            SZrUsingStatement *usingStmt = &node->data.usingStatement;
            if (usingStmt->body != ZR_NULL) {
                collect_symbols_from_ast(state, analyzer, usingStmt->body);
            }
            break;
        }

        case ZR_AST_IF_EXPRESSION: {
            SZrIfExpression *ifExpr = &node->data.ifExpression;
            collect_symbols_from_ast(state, analyzer, ifExpr->thenExpr);
            collect_symbols_from_ast(state, analyzer, ifExpr->elseExpr);
            break;
        }

        case ZR_AST_WHILE_LOOP: {
            collect_symbols_from_ast(state, analyzer, node->data.whileLoop.block);
            break;
        }

        case ZR_AST_FOR_LOOP: {
            SZrForLoop *forLoop = &node->data.forLoop;
            collect_symbols_from_ast(state, analyzer, forLoop->init);
            collect_symbols_from_ast(state, analyzer, forLoop->block);
            break;
        }

        default:
            break;
    }
}

static void perform_decorator_validation_with_context(SZrState *state,
                                                      SZrSemanticAnalyzer *analyzer,
                                                      SZrAstNode *node,
                                                      TZrBool insideExternBlock);

static void perform_decorator_validation_on_nodes(SZrState *state,
                                                  SZrSemanticAnalyzer *analyzer,
                                                  SZrAstNodeArray *nodes,
                                                  TZrBool insideExternBlock) {
    if (nodes == ZR_NULL || nodes->nodes == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < nodes->count; index++) {
        if (nodes->nodes[index] != ZR_NULL) {
            perform_decorator_validation_with_context(state,
                                                      analyzer,
                                                      nodes->nodes[index],
                                                      insideExternBlock);
        }
    }
}

static void perform_decorator_validation_with_context(SZrState *state,
                                                      SZrSemanticAnalyzer *analyzer,
                                                      SZrAstNode *node,
                                                      TZrBool insideExternBlock) {
    if (state == ZR_NULL || analyzer == ZR_NULL || node == ZR_NULL) {
        return;
    }

    switch (node->type) {
        case ZR_AST_SCRIPT: {
            SZrScript *script = &node->data.script;
            perform_decorator_validation_on_nodes(state, analyzer, script->statements, ZR_FALSE);
            break;
        }

        case ZR_AST_BLOCK: {
            SZrBlock *block = &node->data.block;
            perform_decorator_validation_on_nodes(state, analyzer, block->body, insideExternBlock);
            break;
        }

        case ZR_AST_FUNCTION_DECLARATION: {
            SZrFunctionDeclaration *funcDecl = &node->data.functionDeclaration;
            semantic_validate_ffi_decorators(state,
                                             analyzer,
                                             funcDecl->decorators,
                                             ZR_SEMANTIC_DECORATOR_TARGET_FUNCTION,
                                             node->location);
            perform_decorator_validation_on_nodes(state, analyzer, funcDecl->params, insideExternBlock);
            perform_decorator_validation_with_context(state, analyzer, funcDecl->body, insideExternBlock);
            break;
        }

        case ZR_AST_EXTERN_BLOCK: {
            SZrExternBlock *externBlock = &node->data.externBlock;
            perform_decorator_validation_on_nodes(state, analyzer, externBlock->declarations, ZR_TRUE);
            break;
        }

        case ZR_AST_EXTERN_FUNCTION_DECLARATION: {
            SZrExternFunctionDeclaration *externFunction = &node->data.externFunctionDeclaration;
            semantic_validate_ffi_decorators(state,
                                             analyzer,
                                             externFunction->decorators,
                                             ZR_SEMANTIC_DECORATOR_TARGET_EXTERN_FUNCTION,
                                             node->location);
            perform_decorator_validation_on_nodes(state, analyzer, externFunction->params, ZR_TRUE);
            if (externFunction->args != ZR_NULL) {
                semantic_validate_ffi_decorators(state,
                                                 analyzer,
                                                 externFunction->args->decorators,
                                                 ZR_SEMANTIC_DECORATOR_TARGET_PARAMETER,
                                                 node->location);
            }
            break;
        }

        case ZR_AST_EXTERN_DELEGATE_DECLARATION: {
            SZrExternDelegateDeclaration *externDelegate = &node->data.externDelegateDeclaration;
            semantic_validate_ffi_decorators(state,
                                             analyzer,
                                             externDelegate->decorators,
                                             ZR_SEMANTIC_DECORATOR_TARGET_EXTERN_DELEGATE,
                                             node->location);
            perform_decorator_validation_on_nodes(state, analyzer, externDelegate->params, ZR_TRUE);
            if (externDelegate->args != ZR_NULL) {
                semantic_validate_ffi_decorators(state,
                                                 analyzer,
                                                 externDelegate->args->decorators,
                                                 ZR_SEMANTIC_DECORATOR_TARGET_PARAMETER,
                                                 node->location);
            }
            break;
        }

        case ZR_AST_PARAMETER:
            semantic_validate_ffi_decorators(state,
                                             analyzer,
                                             node->data.parameter.decorators,
                                             ZR_SEMANTIC_DECORATOR_TARGET_PARAMETER,
                                             node->location);
            break;

        case ZR_AST_STRUCT_DECLARATION: {
            SZrStructDeclaration *structDecl = &node->data.structDeclaration;
            if (insideExternBlock) {
                semantic_validate_ffi_decorators(state,
                                                 analyzer,
                                                 structDecl->decorators,
                                                 ZR_SEMANTIC_DECORATOR_TARGET_STRUCT,
                                                 node->location);
            }
            perform_decorator_validation_on_nodes(state, analyzer, structDecl->members, insideExternBlock);
            break;
        }

        case ZR_AST_STRUCT_FIELD:
            if (insideExternBlock) {
                semantic_validate_ffi_decorators(state,
                                                 analyzer,
                                                 node->data.structField.decorators,
                                                 ZR_SEMANTIC_DECORATOR_TARGET_STRUCT_FIELD,
                                                 node->location);
            }
            break;

        case ZR_AST_STRUCT_METHOD: {
            SZrStructMethod *method = &node->data.structMethod;
            semantic_validate_ffi_decorators(state,
                                             analyzer,
                                             method->decorators,
                                             ZR_SEMANTIC_DECORATOR_TARGET_FUNCTION,
                                             node->location);
            perform_decorator_validation_on_nodes(state, analyzer, method->params, insideExternBlock);
            perform_decorator_validation_with_context(state, analyzer, method->body, insideExternBlock);
            break;
        }

        case ZR_AST_CLASS_DECLARATION: {
            SZrClassDeclaration *classDecl = &node->data.classDeclaration;
            perform_decorator_validation_on_nodes(state, analyzer, classDecl->members, insideExternBlock);
            break;
        }

        case ZR_AST_CLASS_METHOD: {
            SZrClassMethod *method = &node->data.classMethod;
            semantic_validate_ffi_decorators(state,
                                             analyzer,
                                             method->decorators,
                                             ZR_SEMANTIC_DECORATOR_TARGET_FUNCTION,
                                             node->location);
            perform_decorator_validation_on_nodes(state, analyzer, method->params, insideExternBlock);
            perform_decorator_validation_with_context(state, analyzer, method->body, insideExternBlock);
            break;
        }

        case ZR_AST_ENUM_DECLARATION: {
            SZrEnumDeclaration *enumDecl = &node->data.enumDeclaration;
            if (insideExternBlock) {
                semantic_validate_ffi_decorators(state,
                                                 analyzer,
                                                 enumDecl->decorators,
                                                 ZR_SEMANTIC_DECORATOR_TARGET_ENUM,
                                                 node->location);
            }
            break;
        }

        default:
            break;
    }
}

static void perform_decorator_validation(SZrState *state,
                                         SZrSemanticAnalyzer *analyzer,
                                         SZrAstNode *node) {
    perform_decorator_validation_with_context(state, analyzer, node, ZR_FALSE);
}

// 辅助函数：遍历 AST 收集引用
static void collect_references_from_ast(SZrState *state, SZrSemanticAnalyzer *analyzer, SZrAstNode *node) {
    if (state == ZR_NULL || analyzer == ZR_NULL || node == ZR_NULL) {
        return;
    }
    
    // 检查是否是标识符引用
    if (node->type == ZR_AST_IDENTIFIER_LITERAL) {
        SZrSymbol *symbol = lookup_symbol_at_node(analyzer, node);
        if (symbol != ZR_NULL) {
                // 根据上下文判断引用类型
                EZrReferenceType refType = ZR_REFERENCE_READ; // 默认是读引用
                
                // 检查父节点以确定引用类型
                // 注意：这里需要从AST节点中获取父节点信息，但AST节点可能没有父节点指针
                // TODO: 简化实现：根据节点类型推断
                // 如果是赋值表达式的左值，则是写引用
                // 如果是函数调用的callee，则是调用引用
                // 其他情况是读引用
                
                // 由于AST节点没有父节点指针，我们使用简化策略：
                // 在collect_references_from_ast中，我们已经知道当前处理的节点类型
                // 可以通过检查当前处理的节点类型来判断
                // TODO: 这里暂时使用默认的读引用，实际实现需要更复杂的上下文分析
                
            ZrLanguageServer_ReferenceTracker_AddReference(state, analyzer->referenceTracker,
                                           symbol, node->location, refType);
        } else if (extract_identifier_name(state, node) != ZR_NULL) {
            // 未定义的标识符，添加诊断
            ZrLanguageServer_SemanticAnalyzer_AddDiagnostic(state, analyzer,
                                            ZR_DIAGNOSTIC_ERROR,
                                            node->location,
                                            "Undefined identifier",
                                            "undefined_identifier");
        }
    }
    
    // 递归处理子节点（根据不同节点类型访问不同的子节点数组）
    // 对于标识符引用，已经处理了引用关系
    // 对于其他节点类型，需要根据具体情况递归处理子节点
    switch (node->type) {
        case ZR_AST_SCRIPT: {
            // 脚本节点：遍历 statements 数组
            SZrScript *script = &node->data.script;
            if (script->statements != ZR_NULL && script->statements->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < script->statements->count; i++) {
                    if (script->statements->nodes[i] != ZR_NULL) {
                        collect_references_from_ast(state, analyzer, script->statements->nodes[i]);
                    }
                }
            }
            if (script->moduleName != ZR_NULL) {
                collect_references_from_ast(state, analyzer, script->moduleName);
            }
            break;
        }
        
        case ZR_AST_BLOCK: {
            // 块节点：遍历 body 数组
            SZrBlock *block = &node->data.block;
            if (block->body != ZR_NULL && block->body->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < block->body->count; i++) {
                    if (block->body->nodes[i] != ZR_NULL) {
                        collect_references_from_ast(state, analyzer, block->body->nodes[i]);
                    }
                }
            }
            break;
        }

        case ZR_AST_FUNCTION_DECLARATION: {
            SZrFunctionDeclaration *funcDecl = &node->data.functionDeclaration;
            collect_references_from_ast(state, analyzer, funcDecl->body);
            break;
        }

        case ZR_AST_STRUCT_METHOD: {
            SZrStructMethod *method = &node->data.structMethod;
            collect_references_from_ast(state, analyzer, method->body);
            break;
        }

        case ZR_AST_STRUCT_META_FUNCTION: {
            SZrStructMetaFunction *metaFunc = &node->data.structMetaFunction;
            collect_references_from_ast(state, analyzer, metaFunc->body);
            break;
        }

        case ZR_AST_CLASS_METHOD: {
            SZrClassMethod *method = &node->data.classMethod;
            collect_references_from_ast(state, analyzer, method->body);
            break;
        }

        case ZR_AST_CLASS_META_FUNCTION: {
            SZrClassMetaFunction *metaFunc = &node->data.classMetaFunction;
            collect_references_from_ast(state, analyzer, metaFunc->body);
            break;
        }
        
        case ZR_AST_VARIABLE_DECLARATION: {
            // 变量声明：递归处理 value（表达式可能包含引用）
            SZrVariableDeclaration *varDecl = &node->data.variableDeclaration;
            if (varDecl->value != ZR_NULL) {
                collect_references_from_ast(state, analyzer, varDecl->value);
            }
            break;
        }

        case ZR_AST_USING_STATEMENT: {
            SZrUsingStatement *usingStmt = &node->data.usingStatement;
            record_using_cleanup_step(analyzer, usingStmt->resource);
            if (usingStmt->resource != ZR_NULL) {
                collect_references_from_ast(state, analyzer, usingStmt->resource);
            }
            if (usingStmt->body != ZR_NULL) {
                collect_references_from_ast(state, analyzer, usingStmt->body);
            }
            break;
        }
        
        case ZR_AST_EXPRESSION_STATEMENT: {
            // 表达式语句：递归处理表达式
            SZrExpressionStatement *exprStmt = &node->data.expressionStatement;
            if (exprStmt->expr != ZR_NULL) {
                collect_references_from_ast(state, analyzer, exprStmt->expr);
            }
            break;
        }

        case ZR_AST_RETURN_STATEMENT: {
            SZrReturnStatement *returnStmt = &node->data.returnStatement;
            if (returnStmt->expr != ZR_NULL) {
                collect_references_from_ast(state, analyzer, returnStmt->expr);
            }
            break;
        }
        
        case ZR_AST_FUNCTION_CALL: {
            // 函数调用：递归处理参数
            // 注意：函数调用本身不包含 callee，callee 通过 SZrPrimaryExpression 的 property 和 members 组织
            SZrFunctionCall *funcCall = &node->data.functionCall;
            if (funcCall->args != ZR_NULL && funcCall->args->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < funcCall->args->count; i++) {
                    if (funcCall->args->nodes[i] != ZR_NULL) {
                        collect_references_from_ast(state, analyzer, funcCall->args->nodes[i]);
                    }
                }
            }
            break;
        }
        
        case ZR_AST_PRIMARY_EXPRESSION: {
            // 主表达式：可能包含 property 和 members（包括函数调用）
            SZrPrimaryExpression *primaryExpr = &node->data.primaryExpression;
            if (primaryExpr->property != ZR_NULL) {
                // property 可能是标识符（函数名），需要标记为调用引用
                if (primaryExpr->property->type == ZR_AST_IDENTIFIER_LITERAL) {
                    SZrSymbol *symbol = lookup_symbol_at_node(analyzer, primaryExpr->property);
                    if (symbol != ZR_NULL && symbol->type == ZR_SYMBOL_FUNCTION) {
                            // 如果后面有函数调用，则是调用引用
                            TZrBool isCall = (primaryExpr->members != ZR_NULL && 
                                           primaryExpr->members->count > 0);
                            EZrReferenceType refType = isCall ? ZR_REFERENCE_CALL : ZR_REFERENCE_READ;
                            ZrLanguageServer_ReferenceTracker_AddReference(state, analyzer->referenceTracker,
                                                           symbol, primaryExpr->property->location, 
                                                           refType);
                    } else {
                        collect_references_from_ast(state, analyzer, primaryExpr->property);
                    }
                } else {
                    collect_references_from_ast(state, analyzer, primaryExpr->property);
                }
            }
            if (primaryExpr->members != ZR_NULL && primaryExpr->members->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < primaryExpr->members->count; i++) {
                    if (primaryExpr->members->nodes[i] != ZR_NULL) {
                        collect_references_from_ast(state, analyzer, primaryExpr->members->nodes[i]);
                    }
                }
            }
            break;
        }
        
        case ZR_AST_MEMBER_EXPRESSION: {
            // 成员表达式：递归处理 property
            SZrMemberExpression *memberExpr = &node->data.memberExpression;
            if (memberExpr->property != ZR_NULL) {
                collect_references_from_ast(state, analyzer, memberExpr->property);
            }
            break;
        }

        case ZR_AST_TEMPLATE_STRING_LITERAL: {
            SZrTemplateStringLiteral *templateLiteral = &node->data.templateStringLiteral;
            record_template_string_segments(analyzer, node);
            if (templateLiteral->segments != ZR_NULL && templateLiteral->segments->nodes != ZR_NULL) {
                for (TZrSize i = 0; i < templateLiteral->segments->count; i++) {
                    if (templateLiteral->segments->nodes[i] != ZR_NULL) {
                        collect_references_from_ast(state, analyzer, templateLiteral->segments->nodes[i]);
                    }
                }
            }
            break;
        }

        case ZR_AST_INTERPOLATED_SEGMENT: {
            if (node->data.interpolatedSegment.expression != ZR_NULL) {
                collect_references_from_ast(state,
                                            analyzer,
                                            node->data.interpolatedSegment.expression);
            }
            break;
        }
        
        case ZR_AST_BINARY_EXPRESSION: {
            // 二元表达式：递归处理左右操作数
            SZrBinaryExpression *binExpr = &node->data.binaryExpression;
            if (binExpr->left != ZR_NULL) {
                collect_references_from_ast(state, analyzer, binExpr->left);
            }
            if (binExpr->right != ZR_NULL) {
                collect_references_from_ast(state, analyzer, binExpr->right);
            }
            break;
        }
        
        case ZR_AST_UNARY_EXPRESSION: {
            // 一元表达式：递归处理参数
            SZrUnaryExpression *unaryExpr = &node->data.unaryExpression;
            if (unaryExpr->argument != ZR_NULL) {
                collect_references_from_ast(state, analyzer, unaryExpr->argument);
            }
            break;
        }
        
        case ZR_AST_ASSIGNMENT_EXPRESSION: {
            // 赋值表达式：递归处理左右操作数
            SZrAssignmentExpression *assignExpr = &node->data.assignmentExpression;
            if (assignExpr->left != ZR_NULL) {
                // 左值是写引用
                if (assignExpr->left->type == ZR_AST_IDENTIFIER_LITERAL) {
                    SZrSymbol *symbol = lookup_symbol_at_node(analyzer, assignExpr->left);
                    if (symbol != ZR_NULL) {
                        ZrLanguageServer_ReferenceTracker_AddReference(state, analyzer->referenceTracker,
                                                       symbol, assignExpr->left->location, 
                                                       ZR_REFERENCE_WRITE);
                    }
                } else {
                    collect_references_from_ast(state, analyzer, assignExpr->left);
                }
            }
            if (assignExpr->right != ZR_NULL) {
                collect_references_from_ast(state, analyzer, assignExpr->right);
            }
            break;
        }

        case ZR_AST_IF_EXPRESSION: {
            SZrIfExpression *ifExpr = &node->data.ifExpression;
            collect_references_from_ast(state, analyzer, ifExpr->condition);
            collect_references_from_ast(state, analyzer, ifExpr->thenExpr);
            collect_references_from_ast(state, analyzer, ifExpr->elseExpr);
            break;
        }

        case ZR_AST_WHILE_LOOP: {
            SZrWhileLoop *whileLoop = &node->data.whileLoop;
            collect_references_from_ast(state, analyzer, whileLoop->cond);
            collect_references_from_ast(state, analyzer, whileLoop->block);
            break;
        }

        case ZR_AST_FOR_LOOP: {
            SZrForLoop *forLoop = &node->data.forLoop;
            collect_references_from_ast(state, analyzer, forLoop->init);
            collect_references_from_ast(state, analyzer, forLoop->cond);
            collect_references_from_ast(state, analyzer, forLoop->step);
            collect_references_from_ast(state, analyzer, forLoop->block);
            break;
        }
        
        default:
            // TODO: 其他节点类型暂时跳过
            break;
    }
}

// 分析 AST
TZrBool ZrLanguageServer_SemanticAnalyzer_Analyze(SZrState *state, 
                                 SZrSemanticAnalyzer *analyzer,
                                 SZrAstNode *ast) {
    if (state == ZR_NULL || analyzer == ZR_NULL || ast == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 检查缓存
    TZrSize astHash = 0;
    if (analyzer->enableCache && analyzer->cache != ZR_NULL) {
        astHash = compute_ast_hash(ast);
        if (analyzer->cache->isValid && analyzer->cache->astHash == astHash) {
            // 缓存有效，使用缓存结果
            // 复制缓存的诊断信息
            for (TZrSize i = 0; i < analyzer->cache->cachedDiagnostics.length; i++) {
                SZrDiagnostic **diagPtr = (SZrDiagnostic **)ZrCore_Array_Get(&analyzer->cache->cachedDiagnostics, i);
                if (diagPtr != ZR_NULL && *diagPtr != ZR_NULL) {
                    ZrCore_Array_Push(state, &analyzer->diagnostics, diagPtr);
                }
            }
            return ZR_TRUE;
        }
    }
    
    analyzer->ast = ast;
    
    // 清除旧的诊断信息
    for (TZrSize i = 0; i < analyzer->diagnostics.length; i++) {
        SZrDiagnostic **diagPtr = (SZrDiagnostic **)ZrCore_Array_Get(&analyzer->diagnostics, i);
        if (diagPtr != ZR_NULL && *diagPtr != ZR_NULL) {
            ZrLanguageServer_Diagnostic_Free(state, *diagPtr);
        }
    }
    analyzer->diagnostics.length = 0;
    
    if (!prepare_semantic_state(state, analyzer, ast)) {
        return ZR_FALSE;
    }
    
    // 第一阶段：收集符号定义
    collect_symbols_from_ast(state, analyzer, ast);

    // 第二阶段：decorator 语义校验
    perform_decorator_validation(state, analyzer, ast);
    
    // 第三阶段：收集引用
    collect_references_from_ast(state, analyzer, ast);
    
    // 第四阶段：类型检查（集成类型推断系统）
    // 遍历 AST 进行类型检查
    if (analyzer->compilerState != ZR_NULL) {
        perform_type_checking(state, analyzer, ast);
    }

    // 第五阶段：确定性控制流诊断（不可达代码/分支/短路）
    perform_control_flow_analysis(state, analyzer, ast);
    
    // 更新缓存
    if (analyzer->enableCache && analyzer->cache != ZR_NULL) {
        analyzer->cache->astHash = astHash;
        analyzer->cache->isValid = ZR_TRUE;
        
        // 复制诊断信息到缓存
        analyzer->cache->cachedDiagnostics.length = 0;
        for (TZrSize i = 0; i < analyzer->diagnostics.length; i++) {
            SZrDiagnostic **diagPtr = (SZrDiagnostic **)ZrCore_Array_Get(&analyzer->diagnostics, i);
            if (diagPtr != ZR_NULL && *diagPtr != ZR_NULL) {
                ZrCore_Array_Push(state, &analyzer->cache->cachedDiagnostics, diagPtr);
            }
        }
    }
    
    return ZR_TRUE;
}

// 获取诊断信息
TZrBool ZrLanguageServer_SemanticAnalyzer_GetDiagnostics(SZrState *state,
                                        SZrSemanticAnalyzer *analyzer,
                                        SZrArray *result) {
    if (state == ZR_NULL || analyzer == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 初始化结果数组
    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrDiagnostic *), analyzer->diagnostics.length);
    }
    
    // 复制所有诊断
    for (TZrSize i = 0; i < analyzer->diagnostics.length; i++) {
        SZrDiagnostic **diagPtr = (SZrDiagnostic **)ZrCore_Array_Get(&analyzer->diagnostics, i);
        if (diagPtr != ZR_NULL && *diagPtr != ZR_NULL) {
            ZrCore_Array_Push(state, result, diagPtr);
        }
    }
    
    return ZR_TRUE;
}

// 获取位置的符号
SZrSymbol *ZrLanguageServer_SemanticAnalyzer_GetSymbolAt(SZrSemanticAnalyzer *analyzer,
                                         SZrFileRange position) {
    SZrReference *reference;

    if (analyzer == ZR_NULL) {
        return ZR_NULL;
    }

    reference = ZrLanguageServer_ReferenceTracker_FindReferenceAt(analyzer->referenceTracker, position);
    if (reference != ZR_NULL) {
        return reference->symbol;
    }

    return ZrLanguageServer_SymbolTable_FindDefinition(analyzer->symbolTable, position);
}

static const TZrChar *symbol_kind_display_name(EZrSymbolType symbolType) {
    switch (symbolType) {
        case ZR_SYMBOL_FUNCTION:
        case ZR_SYMBOL_METHOD:
            return "function";
        case ZR_SYMBOL_CLASS:
            return "class";
        case ZR_SYMBOL_STRUCT:
            return "struct";
        case ZR_SYMBOL_INTERFACE:
            return "interface";
        case ZR_SYMBOL_ENUM:
            return "enum";
        case ZR_SYMBOL_MODULE:
            return "module";
        case ZR_SYMBOL_PARAMETER:
            return "parameter";
        case ZR_SYMBOL_FIELD:
            return "field";
        default:
            return "variable";
    }
}

static const TZrChar *value_type_display_name(EZrValueType baseType) {
    switch (baseType) {
        case ZR_VALUE_TYPE_NULL: return "null";
        case ZR_VALUE_TYPE_BOOL: return "bool";
        case ZR_VALUE_TYPE_INT8:
        case ZR_VALUE_TYPE_INT16:
        case ZR_VALUE_TYPE_INT32:
        case ZR_VALUE_TYPE_INT64: return "int";
        case ZR_VALUE_TYPE_UINT8:
        case ZR_VALUE_TYPE_UINT16:
        case ZR_VALUE_TYPE_UINT32:
        case ZR_VALUE_TYPE_UINT64: return "uint";
        case ZR_VALUE_TYPE_FLOAT: return "float";
        case ZR_VALUE_TYPE_DOUBLE: return "double";
        case ZR_VALUE_TYPE_STRING: return "string";
        case ZR_VALUE_TYPE_BUFFER: return "buffer";
        case ZR_VALUE_TYPE_ARRAY: return "array";
        case ZR_VALUE_TYPE_FUNCTION: return "function";
        case ZR_VALUE_TYPE_CLOSURE_VALUE: return "closure-value";
        case ZR_VALUE_TYPE_CLOSURE: return "closure";
        case ZR_VALUE_TYPE_OBJECT: return "object";
        case ZR_VALUE_TYPE_THREAD: return "thread";
        case ZR_VALUE_TYPE_NATIVE_POINTER: return "native-pointer";
        case ZR_VALUE_TYPE_NATIVE_DATA: return "native-data";
        case ZR_VALUE_TYPE_VM_MEMORY: return "vm-memory";
        case ZR_VALUE_TYPE_UNKNOWN: return "unknown";
        default: return "object";
    }
}

static const TZrChar *access_modifier_display_name(EZrAccessModifier accessModifier) {
    switch (accessModifier) {
        case ZR_ACCESS_PUBLIC: return "public";
        case ZR_ACCESS_PROTECTED: return "protected";
        case ZR_ACCESS_PRIVATE:
        default:
            return "private";
    }
}

static const TZrChar *ownership_qualifier_display_name(EZrOwnershipQualifier ownershipQualifier) {
    switch (ownershipQualifier) {
        case ZR_OWNERSHIP_QUALIFIER_UNIQUE: return "unique";
        case ZR_OWNERSHIP_QUALIFIER_SHARED: return "shared";
        case ZR_OWNERSHIP_QUALIFIER_WEAK: return "weak";
        case ZR_OWNERSHIP_QUALIFIER_NONE:
        default:
            return ZR_NULL;
    }
}

static void build_inferred_type_display(char *buffer,
                                        size_t bufferSize,
                                        const SZrInferredType *typeInfo) {
    const TZrChar *baseName;
    const TZrChar *ownershipName;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }

    buffer[0] = '\0';
    if (typeInfo == ZR_NULL) {
        return;
    }

    baseName = typeInfo->typeName != ZR_NULL
               ? ZrCore_String_GetNativeString(typeInfo->typeName)
               : value_type_display_name(typeInfo->baseType);
    ownershipName = ownership_qualifier_display_name(typeInfo->ownershipQualifier);

    snprintf(buffer,
             bufferSize,
             "%s%s%s%s",
             ownershipName != ZR_NULL ? ownershipName : "",
             ownershipName != ZR_NULL ? " " : "",
             baseName != ZR_NULL ? baseName : "object",
             typeInfo->isNullable ? "?" : "");
}

static TZrBool inferred_type_is_placeholder(const SZrInferredType *typeInfo) {
    return typeInfo == ZR_NULL ||
           (typeInfo->baseType == ZR_VALUE_TYPE_OBJECT &&
            typeInfo->typeName == ZR_NULL &&
            typeInfo->ownershipQualifier == ZR_OWNERSHIP_QUALIFIER_NONE &&
            !typeInfo->isNullable);
}

static const SZrType *symbol_declared_type_from_ast(SZrSymbol *symbol) {
    if (symbol == ZR_NULL || symbol->astNode == ZR_NULL) {
        return ZR_NULL;
    }

    switch (symbol->astNode->type) {
        case ZR_AST_PARAMETER:
            return symbol->astNode->data.parameter.typeInfo;
        case ZR_AST_VARIABLE_DECLARATION:
            return symbol->astNode->data.variableDeclaration.typeInfo;
        case ZR_AST_FUNCTION_DECLARATION:
            return symbol->astNode->data.functionDeclaration.returnType;
        case ZR_AST_STRUCT_FIELD:
            return symbol->astNode->data.structField.typeInfo;
        case ZR_AST_CLASS_FIELD:
            return symbol->astNode->data.classField.typeInfo;
        case ZR_AST_STRUCT_METHOD:
            return symbol->astNode->data.structMethod.returnType;
        case ZR_AST_CLASS_METHOD:
            return symbol->astNode->data.classMethod.returnType;
        default:
            return ZR_NULL;
    }
}

static void build_ast_type_display(char *buffer,
                                   size_t bufferSize,
                                   const SZrType *declaredType) {
    const TZrChar *typeName = ZR_NULL;
    const TZrChar *ownershipName;
    TZrInt32 dimensions;
    TZrSize offset = 0;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }

    buffer[0] = '\0';
    if (declaredType == ZR_NULL) {
        return;
    }

    if (declaredType->name != ZR_NULL &&
        declaredType->name->type == ZR_AST_IDENTIFIER_LITERAL &&
        declaredType->name->data.identifier.name != ZR_NULL) {
        typeName = ZrCore_String_GetNativeString(declaredType->name->data.identifier.name);
    }
    if (typeName == ZR_NULL) {
        typeName = "object";
    }

    ownershipName = ownership_qualifier_display_name(declaredType->ownershipQualifier);
    offset = (TZrSize)snprintf(buffer,
                               bufferSize,
                               "%s%s%s",
                               ownershipName != ZR_NULL ? ownershipName : "",
                               ownershipName != ZR_NULL ? " " : "",
                               typeName);
    if (offset >= bufferSize) {
        buffer[bufferSize - 1] = '\0';
        return;
    }

    dimensions = declaredType->dimensions;
    while (dimensions > 0 && offset + 2 < bufferSize) {
        buffer[offset++] = '[';
        buffer[offset++] = ']';
        dimensions--;
    }
    buffer[offset] = '\0';
}

static void build_symbol_type_display(char *buffer,
                                      size_t bufferSize,
                                      SZrSymbol *symbol) {
    const SZrType *declaredType;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }

    buffer[0] = '\0';
    if (symbol == ZR_NULL) {
        return;
    }

    declaredType = symbol_declared_type_from_ast(symbol);
    if (declaredType != ZR_NULL) {
        build_ast_type_display(buffer, bufferSize, declaredType);
        if (buffer[0] != '\0') {
            return;
        }
    }

    build_inferred_type_display(buffer, bufferSize, symbol->typeInfo);
    if (!inferred_type_is_placeholder(symbol->typeInfo) && buffer[0] != '\0') {
        return;
    }
}

static void build_symbol_completion_detail(char *buffer,
                                           size_t bufferSize,
                                           SZrSymbol *symbol) {
    char typeBuffer[256];
    const TZrChar *accessName;
    const TZrChar *kindName;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }

    buffer[0] = '\0';
    if (symbol == ZR_NULL) {
        return;
    }

    build_symbol_type_display(typeBuffer, sizeof(typeBuffer), symbol);
    accessName = access_modifier_display_name(symbol->accessModifier);
    kindName = symbol_kind_display_name(symbol->type);

    if (typeBuffer[0] != '\0') {
        if (symbol->type == ZR_SYMBOL_FUNCTION || symbol->type == ZR_SYMBOL_METHOD) {
            snprintf(buffer, bufferSize, "returns %s (%s)", typeBuffer, accessName);
        } else {
            snprintf(buffer, bufferSize, "%s (%s)", typeBuffer, accessName);
        }
        return;
    }

    snprintf(buffer, bufferSize, "%s (%s)", kindName, accessName);
}

// 获取悬停信息
TZrBool ZrLanguageServer_SemanticAnalyzer_GetHoverInfo(SZrState *state,
                                     SZrSemanticAnalyzer *analyzer,
                                     SZrFileRange position,
                                     SZrHoverInfo **result) {
    SZrSymbol *symbol;
    TZrNativeString nameStr = ZR_NULL;
    TZrSize nameLen = 0;
    const TZrChar *kindName;
    const TZrChar *accessName;
    char buffer[512];
    char typeBuffer[256];

    if (state == ZR_NULL || analyzer == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 查找位置的符号
    symbol = ZrLanguageServer_SemanticAnalyzer_GetSymbolAt(analyzer, position);
    if (symbol == ZR_NULL) {
        return ZR_FALSE;
    }

    if (symbol->name != ZR_NULL && symbol->name->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        nameStr = ZrCore_String_GetNativeStringShort(symbol->name);
        nameLen = symbol->name->shortStringLength;
    } else if (symbol->name != ZR_NULL) {
        nameStr = ZrCore_String_GetNativeString(symbol->name);
        nameLen = symbol->name->longStringLength;
    }
    if (nameStr == ZR_NULL) {
        nameStr = "<unnamed>";
        nameLen = strlen(nameStr);
    }

    kindName = symbol_kind_display_name(symbol->type);
    accessName = access_modifier_display_name(symbol->accessModifier);
    build_symbol_type_display(typeBuffer, sizeof(typeBuffer), symbol);

    if (typeBuffer[0] != '\0') {
        if (symbol->type == ZR_SYMBOL_FUNCTION || symbol->type == ZR_SYMBOL_METHOD) {
            snprintf(buffer,
                     sizeof(buffer),
                     "**%s** `%.*s`\n\nReturns: %s\nAccess: %s",
                     kindName,
                     (int)nameLen,
                     nameStr,
                     typeBuffer,
                     accessName);
        } else {
            snprintf(buffer,
                     sizeof(buffer),
                     "**%s** `%.*s`\n\nType: %s\nAccess: %s",
                     kindName,
                     (int)nameLen,
                     nameStr,
                     typeBuffer,
                     accessName);
        }
    } else {
        snprintf(buffer,
                 sizeof(buffer),
                 "**%s** `%.*s`\n\nKind: %s\nAccess: %s",
                 kindName,
                 (int)nameLen,
                 nameStr,
                 kindName,
                 accessName);
    }

    *result = ZrLanguageServer_HoverInfo_New(state, buffer, symbol->location, symbol->typeInfo);
    return *result != ZR_NULL;
}

static const TZrChar *completion_kind_for_symbol(EZrSymbolType symbolType) {
    return symbol_kind_display_name(symbolType);
}

static const TZrChar *completion_kind_for_prototype(EZrObjectPrototypeType prototypeType) {
    switch (prototypeType) {
        case ZR_OBJECT_PROTOTYPE_TYPE_CLASS:
            return "class";
        case ZR_OBJECT_PROTOTYPE_TYPE_STRUCT:
            return "struct";
        case ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE:
            return "interface";
        case ZR_OBJECT_PROTOTYPE_TYPE_ENUM:
            return "enum";
        case ZR_OBJECT_PROTOTYPE_TYPE_MODULE:
            return "module";
        default:
            return "type";
    }
}

static TZrBool completion_array_has_label(SZrArray *items, const TZrChar *label) {
    if (items == ZR_NULL || label == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < items->length; index++) {
        SZrCompletionItem **itemPtr = (SZrCompletionItem **)ZrCore_Array_Get(items, index);
        const TZrChar *existingLabel;
        if (itemPtr == ZR_NULL || *itemPtr == ZR_NULL || (*itemPtr)->label == ZR_NULL) {
            continue;
        }

        existingLabel = ZrCore_String_GetNativeString((*itemPtr)->label);
        if (existingLabel != ZR_NULL && strcmp(existingLabel, label) == 0) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void append_completion_item_if_missing(SZrState *state,
                                              SZrArray *result,
                                              const TZrChar *label,
                                              const TZrChar *kind,
                                              const TZrChar *detail,
                                              const TZrChar *documentation,
                                              SZrInferredType *typeInfo) {
    SZrCompletionItem *item;

    if (state == ZR_NULL || result == ZR_NULL || label == ZR_NULL || completion_array_has_label(result, label)) {
        return;
    }

    item = ZrLanguageServer_CompletionItem_New(state, label, kind, detail, documentation, typeInfo);
    if (item != ZR_NULL) {
        ZrCore_Array_Push(state, result, &item);
    }
}

static SZrString *extract_imported_module_name_from_call(SZrFunctionCall *call) {
    if (call == ZR_NULL || call->args == ZR_NULL || call->args->count == 0) {
        return ZR_NULL;
    }

    if (call->args->nodes[0] != ZR_NULL && call->args->nodes[0]->type == ZR_AST_STRING_LITERAL) {
        return call->args->nodes[0]->data.stringLiteral.value;
    }

    return ZR_NULL;
}

static SZrString *extract_imported_module_name_from_expression(SZrAstNode *node) {
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    if (node->type == ZR_AST_IMPORT_EXPRESSION &&
        node->data.importExpression.modulePath != ZR_NULL &&
        node->data.importExpression.modulePath->type == ZR_AST_STRING_LITERAL) {
        return node->data.importExpression.modulePath->data.stringLiteral.value;
    }

    if (node->type == ZR_AST_PRIMARY_EXPRESSION) {
        SZrPrimaryExpression *primary = &node->data.primaryExpression;
        if (primary->property != ZR_NULL &&
            primary->property->type == ZR_AST_IDENTIFIER_LITERAL &&
            primary->property->data.identifier.name != ZR_NULL &&
            strcmp(ZrCore_String_GetNativeString(primary->property->data.identifier.name), "import") == 0 &&
            primary->members != ZR_NULL &&
            primary->members->count > 0 &&
            primary->members->nodes[0] != ZR_NULL &&
            primary->members->nodes[0]->type == ZR_AST_FUNCTION_CALL) {
            return extract_imported_module_name_from_call(&primary->members->nodes[0]->data.functionCall);
        }
    }

    return ZR_NULL;
}

static const TZrChar *native_module_name_from_symbol(SZrSymbol *symbol) {
    if (symbol == ZR_NULL) {
        return ZR_NULL;
    }

    if (symbol->typeInfo != ZR_NULL && symbol->typeInfo->typeName != ZR_NULL) {
        return ZrCore_String_GetNativeString(symbol->typeInfo->typeName);
    }

    if (symbol->astNode != ZR_NULL && symbol->astNode->type == ZR_AST_VARIABLE_DECLARATION) {
        SZrAstNode *value = symbol->astNode->data.variableDeclaration.value;
        SZrString *moduleName = extract_imported_module_name_from_expression(value);
        if (moduleName != ZR_NULL) {
            return ZrCore_String_GetNativeString(moduleName);
        }
    }

    return ZR_NULL;
}

static const ZrLibTypeHintDescriptor *find_native_hint_descriptor(const ZrLibModuleDescriptor *descriptor,
                                                                  const TZrChar *label) {
    if (descriptor == ZR_NULL || label == ZR_NULL || descriptor->typeHints == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < descriptor->typeHintCount; index++) {
        const ZrLibTypeHintDescriptor *hint = &descriptor->typeHints[index];
        if (hint->symbolName != ZR_NULL && strcmp(hint->symbolName, label) == 0) {
            return hint;
        }
    }

    return ZR_NULL;
}

static TZrBool native_module_name_seen(SZrArray *visitedModules, const TZrChar *moduleName) {
    if (visitedModules == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < visitedModules->length; index++) {
        const TZrChar **existingName = (const TZrChar **)ZrCore_Array_Get(visitedModules, index);
        if (existingName != ZR_NULL && *existingName != ZR_NULL && strcmp(*existingName, moduleName) == 0) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void append_native_descriptor_completions(SZrState *state,
                                                 SZrArray *result,
                                                 SZrGlobalState *global,
                                                 const TZrChar *moduleName,
                                                 SZrArray *visitedModules) {
    ZrLibRegisteredModuleInfo moduleInfo;
    const ZrLibModuleDescriptor *descriptor;

    if (state == ZR_NULL || result == ZR_NULL || global == ZR_NULL || moduleName == ZR_NULL ||
        visitedModules == ZR_NULL || native_module_name_seen(visitedModules, moduleName)) {
        return;
    }

    ZrCore_Array_Push(state, visitedModules, &moduleName);
    if (!ZrLibrary_NativeRegistry_GetModuleInfo(global, moduleName, &moduleInfo) ||
        moduleInfo.descriptor == ZR_NULL) {
        return;
    }

    descriptor = moduleInfo.descriptor;

    for (TZrSize index = 0; index < descriptor->typeHintCount; index++) {
        const ZrLibTypeHintDescriptor *hint = &descriptor->typeHints[index];
        append_completion_item_if_missing(state,
                                          result,
                                          hint->symbolName,
                                          hint->symbolKind != ZR_NULL ? hint->symbolKind : "symbol",
                                          hint->signature,
                                          hint->documentation,
                                          ZR_NULL);
    }

    for (TZrSize index = 0; index < descriptor->constantCount; index++) {
        const ZrLibConstantDescriptor *constant = &descriptor->constants[index];
        append_completion_item_if_missing(state,
                                          result,
                                          constant->name,
                                          "constant",
                                          constant->typeName,
                                          constant->documentation,
                                          ZR_NULL);
    }

    for (TZrSize index = 0; index < descriptor->functionCount; index++) {
        const ZrLibFunctionDescriptor *functionDescriptor = &descriptor->functions[index];
        const ZrLibTypeHintDescriptor *hint = find_native_hint_descriptor(descriptor, functionDescriptor->name);
        TZrChar detail[256];

        detail[0] = '\0';
        if (hint == ZR_NULL && functionDescriptor->name != ZR_NULL) {
            snprintf(detail,
                     sizeof(detail),
                     "%s(...): %s",
                     functionDescriptor->name,
                     functionDescriptor->returnTypeName != ZR_NULL ? functionDescriptor->returnTypeName : "object");
        }

        append_completion_item_if_missing(state,
                                          result,
                                          functionDescriptor->name,
                                          "function",
                                          hint != ZR_NULL ? hint->signature : (detail[0] != '\0' ? detail : ZR_NULL),
                                          hint != ZR_NULL && hint->documentation != ZR_NULL
                                              ? hint->documentation
                                              : functionDescriptor->documentation,
                                          ZR_NULL);
    }

    for (TZrSize index = 0; index < descriptor->typeCount; index++) {
        const ZrLibTypeDescriptor *typeDescriptor = &descriptor->types[index];
        const ZrLibTypeHintDescriptor *hint = find_native_hint_descriptor(descriptor, typeDescriptor->name);
        TZrChar detail[256];

        detail[0] = '\0';
        if (hint == ZR_NULL && typeDescriptor->name != ZR_NULL) {
            snprintf(detail,
                     sizeof(detail),
                     "%s %s",
                     completion_kind_for_prototype(typeDescriptor->prototypeType),
                     typeDescriptor->name);
        }

        append_completion_item_if_missing(state,
                                          result,
                                          typeDescriptor->name,
                                          completion_kind_for_prototype(typeDescriptor->prototypeType),
                                          hint != ZR_NULL ? hint->signature : (detail[0] != '\0' ? detail : ZR_NULL),
                                          hint != ZR_NULL && hint->documentation != ZR_NULL
                                              ? hint->documentation
                                              : typeDescriptor->documentation,
                                          ZR_NULL);

        for (TZrSize methodIndex = 0; methodIndex < typeDescriptor->methodCount; methodIndex++) {
            const ZrLibMethodDescriptor *method = &typeDescriptor->methods[methodIndex];
            TZrChar methodDetail[256];

            if (!method->isStatic || method->name == ZR_NULL) {
                continue;
            }

            snprintf(methodDetail,
                     sizeof(methodDetail),
                     "%s.%s(...): %s",
                     typeDescriptor->name != ZR_NULL ? typeDescriptor->name : "type",
                     method->name,
                     method->returnTypeName != ZR_NULL ? method->returnTypeName : "object");
            append_completion_item_if_missing(state,
                                              result,
                                              method->name,
                                              "function",
                                              methodDetail,
                                              method->documentation,
                                              ZR_NULL);
        }
    }

    for (TZrSize index = 0; index < descriptor->moduleLinkCount; index++) {
        const ZrLibModuleLinkDescriptor *moduleLink = &descriptor->moduleLinks[index];
        append_completion_item_if_missing(state,
                                          result,
                                          moduleLink->name,
                                          "module",
                                          moduleLink->moduleName,
                                          moduleLink->documentation,
                                          ZR_NULL);
        append_native_descriptor_completions(state,
                                             result,
                                             global,
                                             moduleLink->moduleName,
                                             visitedModules);
    }
}

// 获取代码补全
TZrBool ZrLanguageServer_SemanticAnalyzer_GetCompletions(SZrState *state,
                                       SZrSemanticAnalyzer *analyzer,
                                       SZrFileRange position,
                                       SZrArray *result) {
    SZrArray visibleSymbols;
    SZrArray visitedModules;

    if (state == ZR_NULL || analyzer == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 初始化结果数组
    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrCompletionItem *), 8);
    }

    ZrCore_Array_Construct(&visibleSymbols);
    ZrCore_Array_Construct(&visitedModules);
    ZrLanguageServer_SymbolTable_GetVisibleSymbolsAtPosition(state,
                                             analyzer->symbolTable,
                                             position,
                                             &visibleSymbols);
    ZrCore_Array_Init(state, &visitedModules, sizeof(const TZrChar *), 4);

    for (TZrSize i = 0; i < visibleSymbols.length; i++) {
        SZrSymbol **symbolPtr = (SZrSymbol **)ZrCore_Array_Get(&visibleSymbols, i);
        SZrSymbol *symbol;
        const TZrChar *label;
        char detailBuffer[256];

        if (symbolPtr == ZR_NULL || *symbolPtr == ZR_NULL || (*symbolPtr)->name == ZR_NULL) {
            continue;
        }

        symbol = *symbolPtr;
        label = ZrCore_String_GetNativeString(symbol->name);
        build_symbol_completion_detail(detailBuffer, sizeof(detailBuffer), symbol);
        append_completion_item_if_missing(state,
                                          result,
                                          label,
                                          completion_kind_for_symbol(symbol->type),
                                          detailBuffer[0] != '\0' ? detailBuffer : ZR_NULL,
                                          ZR_NULL,
                                          symbol->typeInfo);

        {
            const TZrChar *moduleName = native_module_name_from_symbol(symbol);
            append_native_descriptor_completions(state,
                                                 result,
                                                 state->global,
                                                 moduleName,
                                                 &visitedModules);
        }
    }

    ZrCore_Array_Free(state, &visitedModules);
    ZrCore_Array_Free(state, &visibleSymbols);
    return ZR_TRUE;
}

// 添加诊断
TZrBool ZrLanguageServer_SemanticAnalyzer_AddDiagnostic(SZrState *state,
                                     SZrSemanticAnalyzer *analyzer,
                                     EZrDiagnosticSeverity severity,
                                     SZrFileRange location,
                                     const TZrChar *message,
                                     const TZrChar *code) {
    if (state == ZR_NULL || analyzer == ZR_NULL || message == ZR_NULL) {
        return ZR_FALSE;
    }
    
    SZrDiagnostic *diagnostic = ZrLanguageServer_Diagnostic_New(state, severity, location, message, code);
    if (diagnostic == ZR_NULL) {
        return ZR_FALSE;
    }
    
    ZrCore_Array_Push(state, &analyzer->diagnostics, &diagnostic);
    
    return ZR_TRUE;
}

// 创建诊断
SZrDiagnostic *ZrLanguageServer_Diagnostic_New(SZrState *state,
                                EZrDiagnosticSeverity severity,
                                SZrFileRange location,
                                const TZrChar *message,
                                const TZrChar *code) {
    if (state == ZR_NULL || message == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrDiagnostic *diagnostic = (SZrDiagnostic *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrDiagnostic));
    if (diagnostic == ZR_NULL) {
        return ZR_NULL;
    }
    
    diagnostic->severity = severity;
    diagnostic->location = location;
    diagnostic->message = ZrCore_String_Create(state, (TZrNativeString)message, strlen(message));
    diagnostic->code = code != ZR_NULL ? ZrCore_String_Create(state, (TZrNativeString)code, strlen(code)) : ZR_NULL;
    
    if (diagnostic->message == ZR_NULL) {
        ZrCore_Memory_RawFree(state->global, diagnostic, sizeof(SZrDiagnostic));
        return ZR_NULL;
    }
    
    return diagnostic;
}

// 释放诊断
void ZrLanguageServer_Diagnostic_Free(SZrState *state, SZrDiagnostic *diagnostic) {
    if (state == ZR_NULL || diagnostic == ZR_NULL) {
        return;
    }
    
    if (diagnostic->message != ZR_NULL) {
        // SZrString 由 GC 管理，不需要手动释放
    }
    if (diagnostic->code != ZR_NULL) {
        // SZrString 由 GC 管理，不需要手动释放
    }
    ZrCore_Memory_RawFree(state->global, diagnostic, sizeof(SZrDiagnostic));
}

// 创建补全项
SZrCompletionItem *ZrLanguageServer_CompletionItem_New(SZrState *state,
                                       const TZrChar *label,
                                       const TZrChar *kind,
                                       const TZrChar *detail,
                                       const TZrChar *documentation,
                                       SZrInferredType *typeInfo) {
    if (state == ZR_NULL || label == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrCompletionItem *item = (SZrCompletionItem *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrCompletionItem));
    if (item == ZR_NULL) {
        return ZR_NULL;
    }
    
    item->label = ZrCore_String_Create(state, (TZrNativeString)label, strlen(label));
    item->kind = kind != ZR_NULL ? ZrCore_String_Create(state, (TZrNativeString)kind, strlen(kind)) : ZR_NULL;
    item->detail = detail != ZR_NULL ? ZrCore_String_Create(state, (TZrNativeString)detail, strlen(detail)) : ZR_NULL;
    item->documentation = documentation != ZR_NULL ? ZrCore_String_Create(state, (TZrNativeString)documentation, strlen(documentation)) : ZR_NULL;
    item->typeInfo = typeInfo; // 不复制，只是引用
    
    if (item->label == ZR_NULL) {
        ZrCore_Memory_RawFree(state->global, item, sizeof(SZrCompletionItem));
        return ZR_NULL;
    }
    
    return item;
}

// 释放补全项
void ZrLanguageServer_CompletionItem_Free(SZrState *state, SZrCompletionItem *item) {
    if (state == ZR_NULL || item == ZR_NULL) {
        return;
    }
    
    if (item->label != ZR_NULL) {
        // SZrString 由 GC 管理，不需要手动释放
    }
    if (item->kind != ZR_NULL) {
        // SZrString 由 GC 管理，不需要手动释放
    }
    if (item->detail != ZR_NULL) {
        // SZrString 由 GC 管理，不需要手动释放
    }
    if (item->documentation != ZR_NULL) {
        // SZrString 由 GC 管理，不需要手动释放
    }
    ZrCore_Memory_RawFree(state->global, item, sizeof(SZrCompletionItem));
}

// 创建悬停信息
SZrHoverInfo *ZrLanguageServer_HoverInfo_New(SZrState *state,
                              const TZrChar *contents,
                              SZrFileRange range,
                              SZrInferredType *typeInfo) {
    if (state == ZR_NULL || contents == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrHoverInfo *info = (SZrHoverInfo *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrHoverInfo));
    if (info == ZR_NULL) {
        return ZR_NULL;
    }
    
    info->contents = ZrCore_String_Create(state, (TZrNativeString)contents, strlen(contents));
    info->range = range;
    info->typeInfo = typeInfo; // 不复制，只是引用
    
    if (info->contents == ZR_NULL) {
        ZrCore_Memory_RawFree(state->global, info, sizeof(SZrHoverInfo));
        return ZR_NULL;
    }
    
    return info;
}

// 释放悬停信息
void ZrLanguageServer_HoverInfo_Free(SZrState *state, SZrHoverInfo *info) {
    if (state == ZR_NULL || info == ZR_NULL) {
        return;
    }
    
    if (info->contents != ZR_NULL) {
        // SZrString 由 GC 管理，不需要手动释放
    }
    ZrCore_Memory_RawFree(state->global, info, sizeof(SZrHoverInfo));
}

// 启用/禁用缓存
void ZrLanguageServer_SemanticAnalyzer_SetCacheEnabled(SZrSemanticAnalyzer *analyzer, TZrBool enabled) {
    if (analyzer == ZR_NULL) {
        return;
    }
    analyzer->enableCache = enabled;
}

// 清除缓存
void ZrLanguageServer_SemanticAnalyzer_ClearCache(SZrState *state, SZrSemanticAnalyzer *analyzer) {
    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->cache == ZR_NULL) {
        return;
    }
    
    analyzer->cache->isValid = ZR_FALSE;
    analyzer->cache->astHash = 0;
    
    // 清除缓存的诊断信息
    for (TZrSize i = 0; i < analyzer->cache->cachedDiagnostics.length; i++) {
        SZrDiagnostic **diagPtr = (SZrDiagnostic **)ZrCore_Array_Get(&analyzer->cache->cachedDiagnostics, i);
        if (diagPtr != ZR_NULL && *diagPtr != ZR_NULL) {
            // 注意：诊断可能在 analyzer->diagnostics 中，避免重复释放
        }
    }
    analyzer->cache->cachedDiagnostics.length = 0;
    analyzer->cache->cachedSymbols.length = 0;
}
