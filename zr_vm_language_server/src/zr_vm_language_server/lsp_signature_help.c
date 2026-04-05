#include "lsp_interface_internal.h"

#include "../../../zr_vm_parser/src/zr_vm_parser/type_inference_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define ZR_LSP_SIGNATURE_BINDING_INDEX_NONE ((TZrInt32)-1)

typedef enum EZrLspCallContextKind {
    ZR_LSP_CALL_CONTEXT_NONE = 0,
    ZR_LSP_CALL_CONTEXT_FUNCTION_CALL,
    ZR_LSP_CALL_CONTEXT_SUPER_CONSTRUCTOR_CALL
} EZrLspCallContextKind;

typedef struct SZrLspCallContext {
    EZrLspCallContextKind kind;
    SZrAstNode *ownerTypeNode;
    SZrAstNode *primaryNode;
    SZrAstNode *callNode;
    SZrAstNode *metaFunctionNode;
    SZrAstNodeArray *argumentNodes;
    TZrSize callMemberIndex;
    TZrSize span;
} SZrLspCallContext;

typedef struct SZrLspGenericBinding {
    const SZrTypeGenericParameterInfo *parameterInfo;
    TZrBool isBound;
    SZrInferredType inferredType;
} SZrLspGenericBinding;

static TZrBool signature_range_contains_position(SZrFileRange range, SZrFileRange position) {
    if (!ZrLanguageServer_Lsp_StringsEqual(range.source, position.source) &&
        range.source != ZR_NULL &&
        position.source != ZR_NULL) {
        return ZR_FALSE;
    }

    if (range.start.offset > 0 && range.end.offset > 0 &&
        position.start.offset > 0 && position.end.offset > 0) {
        return range.start.offset <= position.start.offset &&
               position.end.offset <= range.end.offset;
    }

    return (range.start.line < position.start.line ||
            (range.start.line == position.start.line &&
             range.start.column <= position.start.column)) &&
           (position.end.line < range.end.line ||
            (position.end.line == range.end.line &&
             position.end.column <= range.end.column));
}

static SZrFileRange signature_call_context_range(SZrAstNode *callNode) {
    SZrFunctionCall *call;
    SZrFileRange range;

    memset(&range, 0, sizeof(range));
    if (callNode == ZR_NULL || callNode->type != ZR_AST_FUNCTION_CALL) {
        return range;
    }

    call = &callNode->data.functionCall;
    range = callNode->location;
    if (call->args != ZR_NULL && call->args->count > 0) {
        SZrAstNode *lastArg = call->args->nodes[call->args->count - 1];
        if (lastArg != ZR_NULL) {
            range.end = lastArg->location.end;
        }
    } else if (call->genericArguments != ZR_NULL && call->genericArguments->count > 0) {
        SZrAstNode *lastGenericArg = call->genericArguments->nodes[call->genericArguments->count - 1];
        if (lastGenericArg != ZR_NULL) {
            range.end = lastGenericArg->location.end;
        }
    }

    return range;
}

static TZrSize signature_range_span(SZrFileRange range) {
    if (range.end.offset > range.start.offset) {
        return range.end.offset - range.start.offset;
    }

    return ((TZrSize)range.end.line * ZR_LSP_SIGNATURE_RANGE_PACK_BASE + (TZrSize)range.end.column) -
           ((TZrSize)range.start.line * ZR_LSP_SIGNATURE_RANGE_PACK_BASE + (TZrSize)range.start.column);
}

static TZrSize signature_call_context_span(SZrAstNode *callNode) {
    return signature_range_span(signature_call_context_range(callNode));
}

static TZrBool signature_call_matches_position(SZrAstNode *callNode, SZrFileRange position) {
    SZrFunctionCall *call;

    if (callNode == ZR_NULL || callNode->type != ZR_AST_FUNCTION_CALL) {
        return ZR_FALSE;
    }

    if (signature_range_contains_position(signature_call_context_range(callNode), position)) {
        return ZR_TRUE;
    }

    call = &callNode->data.functionCall;
    if (call->args != ZR_NULL) {
        for (TZrSize index = 0; index < call->args->count; index++) {
            SZrAstNode *argNode = call->args->nodes[index];
            if (argNode != ZR_NULL && signature_range_contains_position(argNode->location, position)) {
                return ZR_TRUE;
            }
        }
    }

    if (call->genericArguments != ZR_NULL) {
        for (TZrSize index = 0; index < call->genericArguments->count; index++) {
            SZrAstNode *argNode = call->genericArguments->nodes[index];
            if (argNode != ZR_NULL && signature_range_contains_position(argNode->location, position)) {
                return ZR_TRUE;
            }
        }
    }

    return ZR_FALSE;
}

static void signature_buffer_append(TZrChar *buffer,
                                    TZrSize bufferSize,
                                    TZrSize *offset,
                                    const TZrChar *format,
                                    ...) {
    va_list args;
    TZrInt32 written;

    if (buffer == ZR_NULL || offset == ZR_NULL || format == ZR_NULL || *offset >= bufferSize) {
        return;
    }

    va_start(args, format);
    written = vsnprintf(buffer + *offset, bufferSize - *offset, format, args);
    va_end(args);
    if (written <= 0) {
        return;
    }

    if ((TZrSize)written >= bufferSize - *offset) {
        *offset = bufferSize - 1;
        buffer[*offset] = '\0';
        return;
    }

    *offset += (TZrSize)written;
}

static const TZrChar *signature_string_native(SZrString *value) {
    if (value == ZR_NULL) {
        return "";
    }

    if (value->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        return ZrCore_String_GetNativeStringShort(value);
    }

    return ZrCore_String_GetNativeString(value);
}

static const TZrChar *signature_parameter_passing_mode_text(EZrParameterPassingMode passingMode) {
    switch (passingMode) {
        case ZR_PARAMETER_PASSING_MODE_IN: return "%in";
        case ZR_PARAMETER_PASSING_MODE_OUT: return "%out";
        case ZR_PARAMETER_PASSING_MODE_REF: return "%ref";
        case ZR_PARAMETER_PASSING_MODE_VALUE:
        default:
            return ZR_NULL;
    }
}

static void signature_append_ast_type(SZrType *typeInfo,
                                      TZrChar *buffer,
                                      TZrSize bufferSize,
                                      TZrSize *offset) {
    if (typeInfo == ZR_NULL || typeInfo->name == ZR_NULL) {
        signature_buffer_append(buffer, bufferSize, offset, "object");
        return;
    }

    switch (typeInfo->name->type) {
        case ZR_AST_IDENTIFIER_LITERAL:
            signature_buffer_append(buffer,
                                    bufferSize,
                                    offset,
                                    "%s",
                                    signature_string_native(typeInfo->name->data.identifier.name));
            break;

        case ZR_AST_GENERIC_TYPE: {
            SZrGenericType *genericType = &typeInfo->name->data.genericType;

            signature_buffer_append(buffer,
                                    bufferSize,
                                    offset,
                                    "%s<",
                                    signature_string_native(genericType->name != ZR_NULL ? genericType->name->name
                                                                                          : ZR_NULL));
            if (genericType->params != ZR_NULL) {
                for (TZrSize index = 0; index < genericType->params->count; index++) {
                    SZrAstNode *paramNode = genericType->params->nodes[index];
                    if (index > 0) {
                        signature_buffer_append(buffer, bufferSize, offset, ", ");
                    }
                    if (paramNode != ZR_NULL && paramNode->type == ZR_AST_TYPE) {
                        signature_append_ast_type(&paramNode->data.type, buffer, bufferSize, offset);
                    } else if (paramNode != ZR_NULL && paramNode->type == ZR_AST_INTEGER_LITERAL) {
                        signature_buffer_append(buffer,
                                                bufferSize,
                                                offset,
                                                "%lld",
                                                (long long)paramNode->data.integerLiteral.value);
                    } else {
                        signature_buffer_append(buffer, bufferSize, offset, "?");
                    }
                }
            }
            signature_buffer_append(buffer, bufferSize, offset, ">");
            break;
        }

        case ZR_AST_TUPLE_TYPE: {
            SZrTupleType *tupleType = &typeInfo->name->data.tupleType;
            signature_buffer_append(buffer, bufferSize, offset, "(");
            if (tupleType->elements != ZR_NULL) {
                for (TZrSize index = 0; index < tupleType->elements->count; index++) {
                    SZrAstNode *elementNode = tupleType->elements->nodes[index];
                    if (index > 0) {
                        signature_buffer_append(buffer, bufferSize, offset, ", ");
                    }
                    if (elementNode != ZR_NULL && elementNode->type == ZR_AST_TYPE) {
                        signature_append_ast_type(&elementNode->data.type, buffer, bufferSize, offset);
                    }
                }
            }
            signature_buffer_append(buffer, bufferSize, offset, ")");
            break;
        }

        default:
            signature_buffer_append(buffer, bufferSize, offset, "object");
            break;
    }

    for (TZrInt32 dimension = 0; dimension < typeInfo->dimensions; dimension++) {
        signature_buffer_append(buffer, bufferSize, offset, "[]");
    }
}

static void signature_format_type(SZrState *state,
                                  const SZrInferredType *typeInfo,
                                  TZrChar *buffer,
                                  TZrSize bufferSize) {
    const TZrChar *text;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }

    buffer[0] = '\0';
    if (state == ZR_NULL || typeInfo == ZR_NULL) {
        snprintf(buffer, bufferSize, "object");
        return;
    }

    text = ZrParser_TypeNameString_Get(state, typeInfo, buffer, bufferSize);
    if (text == ZR_NULL || text[0] == '\0') {
        snprintf(buffer, bufferSize, "object");
    }
}

static void signature_append_generic_parameter_decl(SZrState *state,
                                                    SZrParameter *parameter,
                                                    TZrChar *buffer,
                                                    TZrSize bufferSize,
                                                    TZrSize *offset) {
    TZrChar typeBuffer[ZR_LSP_TYPE_BUFFER_LENGTH];

    ZR_UNUSED_PARAMETER(state);

    if (parameter == ZR_NULL || parameter->name == ZR_NULL) {
        return;
    }

    if (parameter->genericKind == ZR_GENERIC_PARAMETER_CONST_INT) {
        TZrSize typeOffset = 0;
        typeBuffer[0] = '\0';
        signature_append_ast_type(parameter->typeInfo, typeBuffer, sizeof(typeBuffer), &typeOffset);
        signature_buffer_append(buffer,
                                bufferSize,
                                offset,
                                "const %s: %s",
                                signature_string_native(parameter->name->name),
                                typeBuffer[0] != '\0' ? typeBuffer : "int");
        return;
    }

    if (parameter->variance == ZR_GENERIC_VARIANCE_IN) {
        signature_buffer_append(buffer, bufferSize, offset, "in ");
    } else if (parameter->variance == ZR_GENERIC_VARIANCE_OUT) {
        signature_buffer_append(buffer, bufferSize, offset, "out ");
    }

    signature_buffer_append(buffer,
                            bufferSize,
                            offset,
                            "%s",
                            signature_string_native(parameter->name->name));
}

static void signature_append_generic_declaration(SZrState *state,
                                                 SZrGenericDeclaration *generic,
                                                 TZrChar *buffer,
                                                 TZrSize bufferSize,
                                                 TZrSize *offset) {
    if (generic == ZR_NULL || generic->params == ZR_NULL || generic->params->count == 0) {
        return;
    }

    signature_buffer_append(buffer, bufferSize, offset, "<");
    for (TZrSize index = 0; index < generic->params->count; index++) {
        SZrAstNode *paramNode = generic->params->nodes[index];
        if (index > 0) {
            signature_buffer_append(buffer, bufferSize, offset, ", ");
        }
        if (paramNode != ZR_NULL && paramNode->type == ZR_AST_PARAMETER) {
            signature_append_generic_parameter_decl(state,
                                                    &paramNode->data.parameter,
                                                    buffer,
                                                    bufferSize,
                                                    offset);
        }
    }
    signature_buffer_append(buffer, bufferSize, offset, ">");
}

static void signature_append_where_clauses(SZrState *state,
                                           SZrGenericDeclaration *generic,
                                           TZrChar *buffer,
                                           TZrSize bufferSize,
                                           TZrSize *offset) {
    TZrChar typeBuffer[ZR_LSP_TYPE_BUFFER_LENGTH];

    ZR_UNUSED_PARAMETER(state);

    if (generic == ZR_NULL || generic->params == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < generic->params->count; index++) {
        SZrAstNode *paramNode = generic->params->nodes[index];
        SZrParameter *parameter;
        TZrBool firstConstraint = ZR_TRUE;

        if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
            continue;
        }

        parameter = &paramNode->data.parameter;
        if (!parameter->genericRequiresClass &&
            !parameter->genericRequiresStruct &&
            !parameter->genericRequiresNew &&
            (parameter->genericTypeConstraints == ZR_NULL ||
             parameter->genericTypeConstraints->count == 0)) {
            continue;
        }

        signature_buffer_append(buffer,
                                bufferSize,
                                offset,
                                " where %s: ",
                                signature_string_native(parameter->name != ZR_NULL ? parameter->name->name : ZR_NULL));
        if (parameter->genericRequiresClass) {
            signature_buffer_append(buffer, bufferSize, offset, "class");
            firstConstraint = ZR_FALSE;
        }
        if (parameter->genericRequiresStruct) {
            signature_buffer_append(buffer,
                                    bufferSize,
                                    offset,
                                    "%sstruct",
                                    firstConstraint ? "" : ", ");
            firstConstraint = ZR_FALSE;
        }
        if (parameter->genericTypeConstraints != ZR_NULL) {
            for (TZrSize constraintIndex = 0; constraintIndex < parameter->genericTypeConstraints->count; constraintIndex++) {
                SZrAstNode *constraintNode = parameter->genericTypeConstraints->nodes[constraintIndex];
                if (constraintNode == ZR_NULL || constraintNode->type != ZR_AST_TYPE) {
                    continue;
                }
                {
                    TZrSize typeOffset = 0;
                    typeBuffer[0] = '\0';
                    signature_append_ast_type(&constraintNode->data.type,
                                              typeBuffer,
                                              sizeof(typeBuffer),
                                              &typeOffset);
                }
                signature_buffer_append(buffer,
                                        bufferSize,
                                        offset,
                                        "%s%s",
                                        firstConstraint ? "" : ", ",
                                        typeBuffer[0] != '\0' ? typeBuffer : "object");
                firstConstraint = ZR_FALSE;
            }
        }
        if (parameter->genericRequiresNew) {
            signature_buffer_append(buffer,
                                    bufferSize,
                                    offset,
                                    "%snew()",
                                    firstConstraint ? "" : ", ");
        }
    }
}

static void signature_append_parameter_label(SZrState *state,
                                             SZrAstNode *paramNode,
                                             const SZrInferredType *resolvedType,
                                             EZrParameterPassingMode passingMode,
                                             TZrChar *buffer,
                                             TZrSize bufferSize,
                                             TZrSize *offset) {
    SZrParameter *parameter;
    const TZrChar *passingModeText;
    TZrChar typeBuffer[ZR_LSP_TYPE_BUFFER_LENGTH];

    if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
        return;
    }

    parameter = &paramNode->data.parameter;
    passingModeText = signature_parameter_passing_mode_text(passingMode);
    if (passingModeText != ZR_NULL) {
        signature_buffer_append(buffer, bufferSize, offset, "%s ", passingModeText);
    }

    if (resolvedType != ZR_NULL) {
        signature_format_type(state, resolvedType, typeBuffer, sizeof(typeBuffer));
    } else {
        TZrSize typeOffset = 0;
        typeBuffer[0] = '\0';
        signature_append_ast_type(parameter->typeInfo, typeBuffer, sizeof(typeBuffer), &typeOffset);
    }

    signature_buffer_append(buffer,
                            bufferSize,
                            offset,
                            "%s: %s",
                            signature_string_native(parameter->name != ZR_NULL ? parameter->name->name : ZR_NULL),
                            typeBuffer[0] != '\0' ? typeBuffer : "object");
}

static SZrGenericDeclaration *signature_method_generic_declaration(SZrAstNode *declarationNode) {
    if (declarationNode == ZR_NULL) {
        return ZR_NULL;
    }

    switch (declarationNode->type) {
        case ZR_AST_CLASS_METHOD:
            return declarationNode->data.classMethod.generic;

        case ZR_AST_STRUCT_METHOD:
            return declarationNode->data.structMethod.generic;

        default:
            return ZR_NULL;
    }
}

static SZrAstNodeArray *signature_method_parameter_nodes(SZrAstNode *declarationNode) {
    if (declarationNode == ZR_NULL) {
        return ZR_NULL;
    }

    switch (declarationNode->type) {
        case ZR_AST_CLASS_METHOD:
            return declarationNode->data.classMethod.params;

        case ZR_AST_STRUCT_METHOD:
            return declarationNode->data.structMethod.params;

        case ZR_AST_CLASS_META_FUNCTION:
            return declarationNode->data.classMetaFunction.params;

        case ZR_AST_STRUCT_META_FUNCTION:
            return declarationNode->data.structMetaFunction.params;

        default:
            return ZR_NULL;
    }
}

static TZrBool signature_build_label_from_method(SZrState *state,
                                                 SZrTypeMemberInfo *memberInfo,
                                                 const SZrResolvedCallSignature *resolvedSignature,
                                                 TZrChar *buffer,
                                                 TZrSize bufferSize) {
    SZrAstNode *declarationNode;
    SZrGenericDeclaration *genericDecl;
    SZrAstNodeArray *params;
    TZrChar typeBuffer[ZR_LSP_TYPE_BUFFER_LENGTH];
    TZrSize offset = 0;

    if (buffer == ZR_NULL || bufferSize == 0 || memberInfo == ZR_NULL) {
        return ZR_FALSE;
    }

    buffer[0] = '\0';
    declarationNode = memberInfo->declarationNode;
    genericDecl = signature_method_generic_declaration(declarationNode);
    params = signature_method_parameter_nodes(declarationNode);

    signature_buffer_append(buffer,
                            bufferSize,
                            &offset,
                            memberInfo->isMetaMethod ? "@%s" : "%s",
                            signature_string_native(memberInfo->name));
    signature_append_generic_declaration(state, genericDecl, buffer, bufferSize, &offset);
    signature_buffer_append(buffer, bufferSize, &offset, "(");
    if (params != ZR_NULL) {
        for (TZrSize index = 0; index < params->count; index++) {
            SZrInferredType *resolvedType = ZR_NULL;
            EZrParameterPassingMode passingMode = ZR_PARAMETER_PASSING_MODE_VALUE;

            if (index > 0) {
                signature_buffer_append(buffer, bufferSize, &offset, ", ");
            }
            if (resolvedSignature != ZR_NULL && index < resolvedSignature->parameterTypes.length) {
                resolvedType = (SZrInferredType *)ZrCore_Array_Get((SZrArray *)&resolvedSignature->parameterTypes, index);
            }
            if (resolvedSignature != ZR_NULL && index < resolvedSignature->parameterPassingModes.length) {
                EZrParameterPassingMode *mode =
                    (EZrParameterPassingMode *)ZrCore_Array_Get((SZrArray *)&resolvedSignature->parameterPassingModes, index);
                if (mode != ZR_NULL) {
                    passingMode = *mode;
                }
            }
            signature_append_parameter_label(state,
                                             params->nodes[index],
                                             resolvedType,
                                             passingMode,
                                             buffer,
                                             bufferSize,
                                             &offset);
        }
    }
    signature_buffer_append(buffer, bufferSize, &offset, "): ");
    signature_format_type(state,
                          resolvedSignature != ZR_NULL ? &resolvedSignature->returnType : ZR_NULL,
                          typeBuffer,
                          sizeof(typeBuffer));
    signature_buffer_append(buffer, bufferSize, &offset, "%s", typeBuffer);
    signature_append_where_clauses(state, genericDecl, buffer, bufferSize, &offset);
    return ZR_TRUE;
}

static TZrBool signature_build_label_from_function(SZrState *state,
                                                   SZrFunctionTypeInfo *funcType,
                                                   const SZrResolvedCallSignature *resolvedSignature,
                                                   TZrChar *buffer,
                                                   TZrSize bufferSize) {
    SZrAstNode *declarationNode;
    SZrGenericDeclaration *genericDecl = ZR_NULL;
    SZrAstNodeArray *params = ZR_NULL;
    SZrType *declaredReturnType = ZR_NULL;
    TZrBool useDeclaredAstTypes = ZR_FALSE;
    TZrChar typeBuffer[ZR_LSP_TYPE_BUFFER_LENGTH];
    TZrSize offset = 0;

    if (buffer == ZR_NULL || bufferSize == 0 || funcType == ZR_NULL) {
        return ZR_FALSE;
    }

    buffer[0] = '\0';
    declarationNode = funcType->declarationNode;
    if (declarationNode != ZR_NULL) {
        switch (declarationNode->type) {
            case ZR_AST_FUNCTION_DECLARATION:
                genericDecl = declarationNode->data.functionDeclaration.generic;
                params = declarationNode->data.functionDeclaration.params;
                declaredReturnType = declarationNode->data.functionDeclaration.returnType;
                break;
            case ZR_AST_EXTERN_FUNCTION_DECLARATION:
                params = declarationNode->data.externFunctionDeclaration.params;
                declaredReturnType = declarationNode->data.externFunctionDeclaration.returnType;
                useDeclaredAstTypes = ZR_TRUE;
                break;
            default:
                break;
        }
    }

    signature_buffer_append(buffer,
                            bufferSize,
                            &offset,
                            "%s",
                            signature_string_native(funcType->name));
    signature_append_generic_declaration(state, genericDecl, buffer, bufferSize, &offset);
    signature_buffer_append(buffer, bufferSize, &offset, "(");
    if (params != ZR_NULL) {
        for (TZrSize index = 0; index < params->count; index++) {
            SZrInferredType *resolvedType = ZR_NULL;
            EZrParameterPassingMode passingMode = ZR_PARAMETER_PASSING_MODE_VALUE;

            if (index > 0) {
                signature_buffer_append(buffer, bufferSize, &offset, ", ");
            }
            if (!useDeclaredAstTypes && resolvedSignature != ZR_NULL && index < resolvedSignature->parameterTypes.length) {
                resolvedType = (SZrInferredType *)ZrCore_Array_Get((SZrArray *)&resolvedSignature->parameterTypes, index);
            }
            if (resolvedSignature != ZR_NULL && index < resolvedSignature->parameterPassingModes.length) {
                EZrParameterPassingMode *mode =
                    (EZrParameterPassingMode *)ZrCore_Array_Get((SZrArray *)&resolvedSignature->parameterPassingModes, index);
                if (mode != ZR_NULL) {
                    passingMode = *mode;
                }
            }
            signature_append_parameter_label(state,
                                             params->nodes[index],
                                             resolvedType,
                                             passingMode,
                                             buffer,
                                             bufferSize,
                                             &offset);
        }
    }
    signature_buffer_append(buffer, bufferSize, &offset, "): ");
    if (useDeclaredAstTypes && declaredReturnType != ZR_NULL) {
        TZrSize typeOffset = 0;
        typeBuffer[0] = '\0';
        signature_append_ast_type(declaredReturnType, typeBuffer, sizeof(typeBuffer), &typeOffset);
    } else {
        signature_format_type(state,
                              resolvedSignature != ZR_NULL ? &resolvedSignature->returnType : &funcType->returnType,
                              typeBuffer,
                              sizeof(typeBuffer));
    }
    signature_buffer_append(buffer, bufferSize, &offset, "%s", typeBuffer);
    signature_append_where_clauses(state, genericDecl, buffer, bufferSize, &offset);
    return ZR_TRUE;
}

static TZrInt32 signature_active_parameter_index(SZrFunctionCall *call, SZrFilePosition position) {
    TZrInt32 activeIndex = 0;

    if (call == ZR_NULL || call->args == ZR_NULL || call->args->count == 0) {
        return 0;
    }

    for (TZrSize index = 0; index < call->args->count; index++) {
        SZrAstNode *argNode = call->args->nodes[index];
        if (argNode == ZR_NULL) {
            continue;
        }

        if (position.offset >= argNode->location.start.offset &&
            position.offset <= argNode->location.end.offset) {
            return (TZrInt32)index;
        }

        if (position.offset > argNode->location.end.offset) {
            activeIndex = (TZrInt32)index;
        }
    }

    return activeIndex;
}

static TZrInt32 signature_active_parameter_index_for_arguments(SZrAstNodeArray *args, SZrFilePosition position) {
    TZrInt32 activeIndex = 0;

    if (args == ZR_NULL || args->count == 0) {
        return 0;
    }

    for (TZrSize index = 0; index < args->count; index++) {
        SZrAstNode *argNode = args->nodes[index];
        if (argNode == ZR_NULL) {
            continue;
        }

        if (position.offset >= argNode->location.start.offset &&
            position.offset <= argNode->location.end.offset) {
            return (TZrInt32)index;
        }

        if (position.offset > argNode->location.end.offset) {
            activeIndex = (TZrInt32)index + 1;
        }
    }

    return activeIndex;
}

static void signature_update_best_context(SZrLspCallContext *best,
                                          SZrAstNode *primaryNode,
                                          SZrAstNode *callNode,
                                          TZrSize callMemberIndex) {
    TZrSize span;

    if (best == ZR_NULL || primaryNode == ZR_NULL || callNode == ZR_NULL) {
        return;
    }

    span = signature_call_context_span(callNode);
    if (best->kind == ZR_LSP_CALL_CONTEXT_NONE || span <= best->span) {
        best->kind = ZR_LSP_CALL_CONTEXT_FUNCTION_CALL;
        best->ownerTypeNode = ZR_NULL;
        best->primaryNode = primaryNode;
        best->callNode = callNode;
        best->metaFunctionNode = ZR_NULL;
        best->argumentNodes = callNode->data.functionCall.args;
        best->callMemberIndex = callMemberIndex;
        best->span = span;
    }
}

static SZrFileRange signature_super_call_context_range(SZrAstNode *metaFunctionNode) {
    SZrFileRange range;

    memset(&range, 0, sizeof(range));
    if (metaFunctionNode != ZR_NULL) {
        range = metaFunctionNode->location;
    }
    if (metaFunctionNode == ZR_NULL || metaFunctionNode->type != ZR_AST_CLASS_META_FUNCTION) {
        return range;
    }

    if (metaFunctionNode->data.classMetaFunction.superArgs != ZR_NULL &&
        metaFunctionNode->data.classMetaFunction.superArgs->count > 0 &&
        metaFunctionNode->data.classMetaFunction.superArgs->nodes != ZR_NULL &&
        metaFunctionNode->data.classMetaFunction.superArgs->nodes[0] != ZR_NULL) {
        SZrAstNode *lastArgNode =
            metaFunctionNode->data.classMetaFunction.superArgs
                ->nodes[metaFunctionNode->data.classMetaFunction.superArgs->count - 1];
        range.start = metaFunctionNode->data.classMetaFunction.superArgs->nodes[0]->location.start;
        if (range.start.offset >= 6) {
            range.start.offset -= 6;
        }
        if (range.start.column >= 6) {
            range.start.column -= 6;
        }
        range.end = lastArgNode != ZR_NULL ? lastArgNode->location.end : range.end;
    } else if (metaFunctionNode->data.classMetaFunction.body != ZR_NULL) {
        range.end = metaFunctionNode->data.classMetaFunction.body->location.start;
    }

    return range;
}

static TZrBool signature_super_call_matches_position(SZrAstNode *metaFunctionNode, SZrFileRange position) {
    SZrClassMetaFunction *metaFunction;
    SZrFileRange superRange;

    if (metaFunctionNode == ZR_NULL || metaFunctionNode->type != ZR_AST_CLASS_META_FUNCTION) {
        return ZR_FALSE;
    }

    metaFunction = &metaFunctionNode->data.classMetaFunction;
    if (!metaFunction->hasSuperCall) {
        return ZR_FALSE;
    }

    superRange = signature_super_call_context_range(metaFunctionNode);
    if (signature_range_contains_position(superRange, position)) {
        return ZR_TRUE;
    }

    if (metaFunction->superArgs != ZR_NULL) {
        for (TZrSize index = 0; index < metaFunction->superArgs->count; index++) {
            SZrAstNode *argNode = metaFunction->superArgs->nodes[index];
            if (argNode != ZR_NULL && signature_range_contains_position(argNode->location, position)) {
                return ZR_TRUE;
            }
        }
    }

    return ZR_FALSE;
}

static void signature_update_best_super_context(SZrLspCallContext *best,
                                                SZrAstNode *ownerTypeNode,
                                                SZrAstNode *metaFunctionNode) {
    TZrSize span;

    if (best == ZR_NULL || ownerTypeNode == ZR_NULL || metaFunctionNode == ZR_NULL) {
        return;
    }

    span = signature_range_span(signature_super_call_context_range(metaFunctionNode));
    if (best->kind == ZR_LSP_CALL_CONTEXT_NONE || span <= best->span) {
        best->kind = ZR_LSP_CALL_CONTEXT_SUPER_CONSTRUCTOR_CALL;
        best->ownerTypeNode = ownerTypeNode;
        best->primaryNode = ZR_NULL;
        best->callNode = ZR_NULL;
        best->metaFunctionNode = metaFunctionNode;
        best->argumentNodes = metaFunctionNode->data.classMetaFunction.superArgs;
        best->callMemberIndex = 0;
        best->span = span;
    }
}

static void signature_find_call_context_in_node(SZrAstNode *node,
                                                SZrFileRange position,
                                                SZrLspCallContext *best);

static void signature_find_call_context_in_array(SZrAstNodeArray *nodes,
                                                 SZrFileRange position,
                                                 SZrLspCallContext *best) {
    if (nodes == ZR_NULL || nodes->nodes == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < nodes->count; index++) {
        signature_find_call_context_in_node(nodes->nodes[index], position, best);
    }
}

static void signature_find_call_context_in_primary(SZrAstNode *node,
                                                   SZrFileRange position,
                                                   SZrLspCallContext *best) {
    SZrPrimaryExpression *primary;

    if (node == ZR_NULL || node->type != ZR_AST_PRIMARY_EXPRESSION) {
        return;
    }

    primary = &node->data.primaryExpression;
    signature_find_call_context_in_node(primary->property, position, best);
    if (primary->members == ZR_NULL || primary->members->nodes == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < primary->members->count; index++) {
        SZrAstNode *memberNode = primary->members->nodes[index];
        if (memberNode == ZR_NULL) {
            continue;
        }

        if (memberNode->type == ZR_AST_FUNCTION_CALL &&
            signature_call_matches_position(memberNode, position)) {
            signature_update_best_context(best, node, memberNode, index);
        }
        signature_find_call_context_in_node(memberNode, position, best);
    }
}

static void signature_find_call_context_in_node(SZrAstNode *node,
                                                SZrFileRange position,
                                                SZrLspCallContext *best) {
    if (node == ZR_NULL) {
        return;
    }

    switch (node->type) {
        case ZR_AST_SCRIPT:
            signature_find_call_context_in_array(node->data.script.statements, position, best);
            break;

        case ZR_AST_BLOCK:
            signature_find_call_context_in_array(node->data.block.body, position, best);
            break;

        case ZR_AST_TEST_DECLARATION:
            signature_find_call_context_in_node(node->data.testDeclaration.body, position, best);
            break;

        case ZR_AST_VARIABLE_DECLARATION:
            signature_find_call_context_in_node(node->data.variableDeclaration.value, position, best);
            break;

        case ZR_AST_FUNCTION_DECLARATION:
            signature_find_call_context_in_array(node->data.functionDeclaration.params, position, best);
            signature_find_call_context_in_node(node->data.functionDeclaration.body, position, best);
            break;

        case ZR_AST_CLASS_DECLARATION:
            if (node->data.classDeclaration.members != ZR_NULL &&
                node->data.classDeclaration.members->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.classDeclaration.members->count; index++) {
                    SZrAstNode *memberNode = node->data.classDeclaration.members->nodes[index];
                    if (memberNode == ZR_NULL) {
                        continue;
                    }

                    if (memberNode->type == ZR_AST_CLASS_META_FUNCTION &&
                        signature_super_call_matches_position(memberNode, position)) {
                        signature_update_best_super_context(best, node, memberNode);
                    }
                    signature_find_call_context_in_node(memberNode, position, best);
                }
            }
            break;

        case ZR_AST_STRUCT_DECLARATION:
            signature_find_call_context_in_array(node->data.structDeclaration.members, position, best);
            break;

        case ZR_AST_CLASS_METHOD:
            signature_find_call_context_in_array(node->data.classMethod.params, position, best);
            signature_find_call_context_in_node(node->data.classMethod.body, position, best);
            break;

        case ZR_AST_STRUCT_METHOD:
            signature_find_call_context_in_array(node->data.structMethod.params, position, best);
            signature_find_call_context_in_node(node->data.structMethod.body, position, best);
            break;

        case ZR_AST_CLASS_META_FUNCTION:
            signature_find_call_context_in_array(node->data.classMetaFunction.params, position, best);
            signature_find_call_context_in_array(node->data.classMetaFunction.superArgs, position, best);
            signature_find_call_context_in_node(node->data.classMetaFunction.body, position, best);
            break;

        case ZR_AST_STRUCT_META_FUNCTION:
            signature_find_call_context_in_array(node->data.structMetaFunction.params, position, best);
            signature_find_call_context_in_node(node->data.structMetaFunction.body, position, best);
            break;

        case ZR_AST_EXPRESSION_STATEMENT:
            signature_find_call_context_in_node(node->data.expressionStatement.expr, position, best);
            break;

        case ZR_AST_RETURN_STATEMENT:
            signature_find_call_context_in_node(node->data.returnStatement.expr, position, best);
            break;

        case ZR_AST_PRIMARY_EXPRESSION:
            signature_find_call_context_in_primary(node, position, best);
            break;

        case ZR_AST_FUNCTION_CALL:
            signature_find_call_context_in_array(node->data.functionCall.args, position, best);
            signature_find_call_context_in_array(node->data.functionCall.genericArguments, position, best);
            break;

        case ZR_AST_MEMBER_EXPRESSION:
            signature_find_call_context_in_node(node->data.memberExpression.property, position, best);
            break;

        case ZR_AST_ASSIGNMENT_EXPRESSION:
            signature_find_call_context_in_node(node->data.assignmentExpression.left, position, best);
            signature_find_call_context_in_node(node->data.assignmentExpression.right, position, best);
            break;

        case ZR_AST_BINARY_EXPRESSION:
            signature_find_call_context_in_node(node->data.binaryExpression.left, position, best);
            signature_find_call_context_in_node(node->data.binaryExpression.right, position, best);
            break;

        case ZR_AST_UNARY_EXPRESSION:
            signature_find_call_context_in_node(node->data.unaryExpression.argument, position, best);
            break;

        case ZR_AST_CONDITIONAL_EXPRESSION:
            signature_find_call_context_in_node(node->data.conditionalExpression.test, position, best);
            signature_find_call_context_in_node(node->data.conditionalExpression.consequent, position, best);
            signature_find_call_context_in_node(node->data.conditionalExpression.alternate, position, best);
            break;

        case ZR_AST_LOGICAL_EXPRESSION:
            signature_find_call_context_in_node(node->data.logicalExpression.left, position, best);
            signature_find_call_context_in_node(node->data.logicalExpression.right, position, best);
            break;

        case ZR_AST_ARRAY_LITERAL:
            signature_find_call_context_in_array(node->data.arrayLiteral.elements, position, best);
            break;

        case ZR_AST_OBJECT_LITERAL:
            signature_find_call_context_in_array(node->data.objectLiteral.properties, position, best);
            break;

        case ZR_AST_KEY_VALUE_PAIR:
            signature_find_call_context_in_node(node->data.keyValuePair.key, position, best);
            signature_find_call_context_in_node(node->data.keyValuePair.value, position, best);
            break;

        case ZR_AST_CONSTRUCT_EXPRESSION:
            signature_find_call_context_in_node(node->data.constructExpression.target, position, best);
            signature_find_call_context_in_array(node->data.constructExpression.args, position, best);
            break;

        case ZR_AST_IF_EXPRESSION:
            signature_find_call_context_in_node(node->data.ifExpression.condition, position, best);
            signature_find_call_context_in_node(node->data.ifExpression.thenExpr, position, best);
            signature_find_call_context_in_node(node->data.ifExpression.elseExpr, position, best);
            break;

        case ZR_AST_SWITCH_EXPRESSION:
            signature_find_call_context_in_node(node->data.switchExpression.expr, position, best);
            signature_find_call_context_in_array(node->data.switchExpression.cases, position, best);
            signature_find_call_context_in_node(node->data.switchExpression.defaultCase, position, best);
            break;

        case ZR_AST_SWITCH_CASE:
            signature_find_call_context_in_node(node->data.switchCase.value, position, best);
            signature_find_call_context_in_node(node->data.switchCase.block, position, best);
            break;

        case ZR_AST_SWITCH_DEFAULT:
            signature_find_call_context_in_node(node->data.switchDefault.block, position, best);
            break;

        case ZR_AST_WHILE_LOOP:
            signature_find_call_context_in_node(node->data.whileLoop.cond, position, best);
            signature_find_call_context_in_node(node->data.whileLoop.block, position, best);
            break;

        case ZR_AST_LAMBDA_EXPRESSION:
            signature_find_call_context_in_array(node->data.lambdaExpression.params, position, best);
            signature_find_call_context_in_node(node->data.lambdaExpression.block, position, best);
            break;

        default:
            break;
    }
}

static TZrBool signature_build_receiver_primary_prefix(SZrAstNode *primaryNode,
                                                       TZrSize prefixMemberCount,
                                                       SZrAstNode *tempNode,
                                                       SZrPrimaryExpression *tempPrimary,
                                                       SZrAstNodeArray *tempMembers) {
    SZrPrimaryExpression *originalPrimary;

    if (primaryNode == ZR_NULL ||
        primaryNode->type != ZR_AST_PRIMARY_EXPRESSION ||
        tempNode == ZR_NULL ||
        tempPrimary == ZR_NULL ||
        tempMembers == ZR_NULL) {
        return ZR_FALSE;
    }

    originalPrimary = &primaryNode->data.primaryExpression;
    memset(tempNode, 0, sizeof(*tempNode));
    memset(tempPrimary, 0, sizeof(*tempPrimary));
    memset(tempMembers, 0, sizeof(*tempMembers));

    tempNode->type = ZR_AST_PRIMARY_EXPRESSION;
    tempNode->location = primaryNode->location;
    tempNode->data.primaryExpression = *tempPrimary;
    tempNode->data.primaryExpression.property = originalPrimary->property;

    if (prefixMemberCount > 0) {
        tempMembers->nodes = originalPrimary->members->nodes;
        tempMembers->count = prefixMemberCount;
        tempMembers->capacity = prefixMemberCount;
        tempNode->data.primaryExpression.members = tempMembers;
    } else {
        tempNode->data.primaryExpression.members = ZR_NULL;
    }

    return ZR_TRUE;
}

static SZrString *signature_extract_identifier_name(SZrAstNode *node) {
    if (node != ZR_NULL && node->type == ZR_AST_IDENTIFIER_LITERAL) {
        return node->data.identifier.name;
    }

    return ZR_NULL;
}

static void signature_find_named_value_type_recursive(SZrCompilerState *compilerState,
                                                      SZrAstNode *node,
                                                      const TZrChar *nameText,
                                                      TZrSize nameLength,
                                                      TZrSize cursorOffset,
                                                      SZrInferredType *bestType,
                                                      TZrSize *bestOffset,
                                                      TZrBool *found) {
    if (compilerState == ZR_NULL || node == ZR_NULL || nameText == ZR_NULL || bestType == ZR_NULL ||
        bestOffset == ZR_NULL || found == ZR_NULL) {
        return;
    }

    switch (node->type) {
        case ZR_AST_SCRIPT:
            if (node->data.script.statements != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.script.statements->count; index++) {
                    signature_find_named_value_type_recursive(compilerState,
                                                             node->data.script.statements->nodes[index],
                                                             nameText,
                                                             nameLength,
                                                             cursorOffset,
                                                             bestType,
                                                             bestOffset,
                                                             found);
                }
            }
            break;

        case ZR_AST_BLOCK:
            if (node->data.block.body != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.block.body->count; index++) {
                    signature_find_named_value_type_recursive(compilerState,
                                                             node->data.block.body->nodes[index],
                                                             nameText,
                                                             nameLength,
                                                             cursorOffset,
                                                             bestType,
                                                             bestOffset,
                                                             found);
                }
            }
            break;

        case ZR_AST_TEST_DECLARATION:
            signature_find_named_value_type_recursive(compilerState,
                                                     node->data.testDeclaration.body,
                                                     nameText,
                                                     nameLength,
                                                     cursorOffset,
                                                     bestType,
                                                     bestOffset,
                                                     found);
            break;

        case ZR_AST_FUNCTION_DECLARATION:
            signature_find_named_value_type_recursive(compilerState,
                                                     node->data.functionDeclaration.body,
                                                     nameText,
                                                     nameLength,
                                                     cursorOffset,
                                                     bestType,
                                                     bestOffset,
                                                     found);
            break;

        case ZR_AST_VARIABLE_DECLARATION: {
            SZrVariableDeclaration *varDecl = &node->data.variableDeclaration;
            SZrString *patternName = signature_extract_identifier_name(varDecl->pattern);
            const TZrChar *patternText = signature_string_native(patternName);
            SZrInferredType candidateType;
            TZrBool matched;

            if (node->location.start.offset > cursorOffset || patternName == ZR_NULL || patternText == ZR_NULL) {
                break;
            }

            matched = strlen(patternText) == nameLength && memcmp(patternText, nameText, nameLength) == 0;
            if (!matched || node->location.start.offset < *bestOffset) {
                break;
            }

            ZrParser_InferredType_Init(compilerState->state, &candidateType, ZR_VALUE_TYPE_OBJECT);
            if (varDecl->typeInfo != ZR_NULL &&
                ZrParser_AstTypeToInferredType_Convert(compilerState, varDecl->typeInfo, &candidateType)) {
                *bestOffset = node->location.start.offset;
                ZrParser_InferredType_Free(compilerState->state, bestType);
                ZrParser_InferredType_Copy(compilerState->state, bestType, &candidateType);
                *found = ZR_TRUE;
                ZrParser_InferredType_Free(compilerState->state, &candidateType);
                break;
            }

            if (varDecl->value != ZR_NULL &&
                varDecl->value->type == ZR_AST_CONSTRUCT_EXPRESSION &&
                varDecl->value->data.constructExpression.target != ZR_NULL &&
                varDecl->value->data.constructExpression.target->type == ZR_AST_TYPE &&
                ZrParser_AstTypeToInferredType_Convert(compilerState,
                                                       &varDecl->value->data.constructExpression.target->data.type,
                                                       &candidateType)) {
                *bestOffset = node->location.start.offset;
                ZrParser_InferredType_Free(compilerState->state, bestType);
                ZrParser_InferredType_Copy(compilerState->state, bestType, &candidateType);
                *found = ZR_TRUE;
                ZrParser_InferredType_Free(compilerState->state, &candidateType);
                break;
            }

            if (varDecl->value != ZR_NULL &&
                ZrParser_ExpressionType_Infer(compilerState, varDecl->value, &candidateType)) {
                *bestOffset = node->location.start.offset;
                ZrParser_InferredType_Free(compilerState->state, bestType);
                ZrParser_InferredType_Copy(compilerState->state, bestType, &candidateType);
                *found = ZR_TRUE;
            }
            ZrParser_InferredType_Free(compilerState->state, &candidateType);
            break;
        }

        default:
            break;
    }
}

static TZrBool signature_append_generic_parameter_infos(SZrState *state,
                                                        SZrArray *dest,
                                                        const SZrArray *src) {
    if (state == ZR_NULL || dest == ZR_NULL) {
        return ZR_FALSE;
    }

    if (src == ZR_NULL || src->length == 0) {
        return ZR_TRUE;
    }

    if (!dest->isValid || dest->head == ZR_NULL || dest->capacity == 0 || dest->elementSize == 0) {
        ZrCore_Array_Init(state, dest, sizeof(SZrTypeGenericParameterInfo), src->length);
    }

    for (TZrSize index = 0; index < src->length; index++) {
        SZrTypeGenericParameterInfo *info = (SZrTypeGenericParameterInfo *)ZrCore_Array_Get((SZrArray *)src, index);
        if (info != ZR_NULL) {
            ZrCore_Array_Push(state, dest, info);
        }
    }

    return ZR_TRUE;
}

static TZrBool signature_copy_parameter_passing_modes(SZrState *state,
                                                      SZrArray *dest,
                                                      const SZrArray *src) {
    if (state == ZR_NULL || dest == ZR_NULL) {
        return ZR_FALSE;
    }

    if (src == ZR_NULL || src->length == 0) {
        return ZR_TRUE;
    }

    if (!dest->isValid || dest->head == ZR_NULL || dest->capacity == 0 || dest->elementSize == 0) {
        ZrCore_Array_Init(state, dest, sizeof(EZrParameterPassingMode), src->length);
    }

    for (TZrSize index = 0; index < src->length; index++) {
        EZrParameterPassingMode *mode = (EZrParameterPassingMode *)ZrCore_Array_Get((SZrArray *)src, index);
        if (mode != ZR_NULL) {
            ZrCore_Array_Push(state, dest, mode);
        }
    }

    return ZR_TRUE;
}

static void signature_free_temporary_member_info(SZrState *state, SZrTypeMemberInfo *memberInfo) {
    if (state == ZR_NULL || memberInfo == ZR_NULL) {
        return;
    }

    free_inferred_type_array(state, &memberInfo->parameterTypes);
    if (memberInfo->genericParameters.isValid &&
        memberInfo->genericParameters.head != ZR_NULL &&
        memberInfo->genericParameters.capacity > 0 &&
        memberInfo->genericParameters.elementSize > 0) {
        ZrCore_Array_Free(state, &memberInfo->genericParameters);
    }
    if (memberInfo->parameterPassingModes.isValid &&
        memberInfo->parameterPassingModes.head != ZR_NULL &&
        memberInfo->parameterPassingModes.capacity > 0 &&
        memberInfo->parameterPassingModes.elementSize > 0) {
        ZrCore_Array_Free(state, &memberInfo->parameterPassingModes);
    }
}

static TZrInt32 signature_find_generic_binding_index(const SZrArray *genericParameters, SZrString *typeName) {
    if (genericParameters == ZR_NULL || typeName == ZR_NULL) {
        return ZR_LSP_SIGNATURE_BINDING_INDEX_NONE;
    }

    for (TZrSize index = 0; index < genericParameters->length; index++) {
        SZrTypeGenericParameterInfo *parameterInfo =
            (SZrTypeGenericParameterInfo *)ZrCore_Array_Get((SZrArray *)genericParameters, index);
        if (parameterInfo != ZR_NULL &&
            parameterInfo->name != ZR_NULL &&
            ZrCore_String_Equal(parameterInfo->name, typeName)) {
            return (TZrInt32)index;
        }
    }

    return ZR_LSP_SIGNATURE_BINDING_INDEX_NONE;
}

static TZrBool signature_substitute_receiver_generic_type(SZrState *state,
                                                          const SZrArray *genericParameters,
                                                          const SZrArray *bindingTypes,
                                                          const SZrInferredType *sourceType,
                                                          SZrInferredType *result) {
    TZrInt32 bindingIndex;

    if (state == ZR_NULL || genericParameters == ZR_NULL || bindingTypes == ZR_NULL ||
        sourceType == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    bindingIndex = signature_find_generic_binding_index(genericParameters, sourceType->typeName);
    if (bindingIndex != ZR_LSP_SIGNATURE_BINDING_INDEX_NONE && sourceType->elementTypes.length == 0) {
        SZrInferredType *bindingType =
            (SZrInferredType *)ZrCore_Array_Get((SZrArray *)bindingTypes, (TZrSize)bindingIndex);
        if (bindingType == ZR_NULL) {
            return ZR_FALSE;
        }
        ZrParser_InferredType_Copy(state, result, bindingType);
        return ZR_TRUE;
    }

    ZrParser_InferredType_Init(state, result, sourceType->baseType);
    result->isNullable = sourceType->isNullable;
    result->ownershipQualifier = sourceType->ownershipQualifier;
    result->typeName = sourceType->typeName;
    result->minValue = sourceType->minValue;
    result->maxValue = sourceType->maxValue;
    result->hasRangeConstraint = sourceType->hasRangeConstraint;
    result->arrayFixedSize = sourceType->arrayFixedSize;
    result->arrayMinSize = sourceType->arrayMinSize;
    result->arrayMaxSize = sourceType->arrayMaxSize;
    result->hasArraySizeConstraint = sourceType->hasArraySizeConstraint;

    if (sourceType->elementTypes.length > 0) {
        SZrString *baseName = ZR_NULL;
        SZrArray originalArgumentNames;

        ZrCore_Array_Init(state, &result->elementTypes, sizeof(SZrInferredType), sourceType->elementTypes.length);
        for (TZrSize index = 0; index < sourceType->elementTypes.length; index++) {
            SZrInferredType *sourceElement =
                (SZrInferredType *)ZrCore_Array_Get((SZrArray *)&sourceType->elementTypes, index);
            SZrInferredType resolvedElement;

            if (sourceElement == ZR_NULL) {
                continue;
            }

            ZrParser_InferredType_Init(state, &resolvedElement, ZR_VALUE_TYPE_OBJECT);
            if (!signature_substitute_receiver_generic_type(state,
                                                            genericParameters,
                                                            bindingTypes,
                                                            sourceElement,
                                                            &resolvedElement)) {
                ZrParser_InferredType_Free(state, &resolvedElement);
                return ZR_FALSE;
            }
            ZrCore_Array_Push(state, &result->elementTypes, &resolvedElement);
        }

        ZrCore_Array_Construct(&originalArgumentNames);
        if (sourceType->typeName != ZR_NULL &&
            try_parse_generic_instance_type_name(state, sourceType->typeName, &baseName, &originalArgumentNames) &&
            baseName != ZR_NULL &&
            result->elementTypes.length == originalArgumentNames.length) {
            SZrString *canonicalName = build_generic_instance_name(state, baseName, &result->elementTypes);
            if (canonicalName != ZR_NULL) {
                result->typeName = canonicalName;
            }
        }
        if (originalArgumentNames.isValid && originalArgumentNames.head != ZR_NULL) {
            ZrCore_Array_Free(state, &originalArgumentNames);
        }
    }

    return ZR_TRUE;
}

static TZrBool signature_build_receiver_binding_types(SZrCompilerState *compilerState,
                                                      const SZrArray *argumentTypeNames,
                                                      SZrArray *bindingTypes) {
    if (compilerState == ZR_NULL || bindingTypes == ZR_NULL) {
        return ZR_FALSE;
    }

    if (argumentTypeNames == ZR_NULL || argumentTypeNames->length == 0) {
        return ZR_TRUE;
    }

    ZrCore_Array_Init(compilerState->state,
                      bindingTypes,
                      sizeof(SZrInferredType),
                      argumentTypeNames->length);
    for (TZrSize index = 0; index < argumentTypeNames->length; index++) {
        SZrString **argumentTypeNamePtr = (SZrString **)ZrCore_Array_Get((SZrArray *)argumentTypeNames, index);
        SZrInferredType argumentType;

        if (argumentTypeNamePtr == ZR_NULL || *argumentTypeNamePtr == ZR_NULL) {
            free_inferred_type_array(compilerState->state, bindingTypes);
            return ZR_FALSE;
        }

        ZrParser_InferredType_Init(compilerState->state, &argumentType, ZR_VALUE_TYPE_OBJECT);
        if (!inferred_type_from_type_name(compilerState, *argumentTypeNamePtr, &argumentType)) {
            ZrParser_InferredType_Free(compilerState->state, &argumentType);
            free_inferred_type_array(compilerState->state, bindingTypes);
            return ZR_FALSE;
        }
        ZrCore_Array_Push(compilerState->state, bindingTypes, &argumentType);
    }

    return ZR_TRUE;
}

static TZrBool signature_build_specialized_type_name(SZrCompilerState *compilerState,
                                                     SZrString *sourceTypeName,
                                                     const SZrArray *genericParameters,
                                                     const SZrArray *bindingTypes,
                                                     SZrString **outTypeName) {
    SZrInferredType unresolvedType;
    SZrInferredType resolvedType;
    TZrChar buffer[ZR_LSP_TEXT_BUFFER_LENGTH];
    const TZrChar *displayText;

    if (outTypeName != ZR_NULL) {
        *outTypeName = sourceTypeName;
    }

    if (compilerState == ZR_NULL || sourceTypeName == ZR_NULL || genericParameters == ZR_NULL ||
        bindingTypes == ZR_NULL || outTypeName == ZR_NULL) {
        return sourceTypeName == ZR_NULL;
    }

    ZrParser_InferredType_Init(compilerState->state, &unresolvedType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(compilerState->state, &resolvedType, ZR_VALUE_TYPE_OBJECT);
    if (!inferred_type_from_type_name(compilerState, sourceTypeName, &unresolvedType) ||
        !signature_substitute_receiver_generic_type(compilerState->state,
                                                    genericParameters,
                                                    bindingTypes,
                                                    &unresolvedType,
                                                    &resolvedType)) {
        ZrParser_InferredType_Free(compilerState->state, &unresolvedType);
        ZrParser_InferredType_Free(compilerState->state, &resolvedType);
        return ZR_FALSE;
    }

    displayText = ZrParser_TypeNameString_Get(compilerState->state, &resolvedType, buffer, sizeof(buffer));
    if (displayText != ZR_NULL && displayText[0] != '\0') {
        *outTypeName =
            ZrCore_String_Create(compilerState->state, (TZrNativeString)displayText, strlen(displayText));
    }

    ZrParser_InferredType_Free(compilerState->state, &unresolvedType);
    ZrParser_InferredType_Free(compilerState->state, &resolvedType);
    return *outTypeName != ZR_NULL;
}

static TZrBool signature_build_specialized_type_name_from_ast_type(SZrCompilerState *compilerState,
                                                                   SZrType *sourceType,
                                                                   const SZrArray *genericParameters,
                                                                   const SZrArray *bindingTypes,
                                                                   SZrString **outTypeName) {
    SZrInferredType unresolvedType;
    SZrInferredType resolvedType;
    TZrChar buffer[ZR_LSP_TEXT_BUFFER_LENGTH];
    const TZrChar *displayText;

    if (outTypeName != ZR_NULL) {
        *outTypeName = ZR_NULL;
    }

    if (compilerState == ZR_NULL || sourceType == ZR_NULL || genericParameters == ZR_NULL ||
        bindingTypes == ZR_NULL || outTypeName == ZR_NULL) {
        return sourceType == ZR_NULL;
    }

    ZrParser_InferredType_Init(compilerState->state, &unresolvedType, ZR_VALUE_TYPE_OBJECT);
    ZrParser_InferredType_Init(compilerState->state, &resolvedType, ZR_VALUE_TYPE_OBJECT);
    if (!ZrParser_AstTypeToInferredType_Convert(compilerState, sourceType, &unresolvedType) ||
        !signature_substitute_receiver_generic_type(compilerState->state,
                                                    genericParameters,
                                                    bindingTypes,
                                                    &unresolvedType,
                                                    &resolvedType)) {
        ZrParser_InferredType_Free(compilerState->state, &unresolvedType);
        ZrParser_InferredType_Free(compilerState->state, &resolvedType);
        return ZR_FALSE;
    }

    displayText = ZrParser_TypeNameString_Get(compilerState->state, &resolvedType, buffer, sizeof(buffer));
    if (displayText != ZR_NULL && displayText[0] != '\0') {
        *outTypeName =
            ZrCore_String_Create(compilerState->state, (TZrNativeString)displayText, strlen(displayText));
    }

    ZrParser_InferredType_Free(compilerState->state, &unresolvedType);
    ZrParser_InferredType_Free(compilerState->state, &resolvedType);
    return *outTypeName != ZR_NULL;
}

static SZrType *signature_method_return_type_node(SZrAstNode *declarationNode) {
    if (declarationNode == ZR_NULL) {
        return ZR_NULL;
    }

    switch (declarationNode->type) {
        case ZR_AST_CLASS_METHOD:
            return declarationNode->data.classMethod.returnType;

        case ZR_AST_STRUCT_METHOD:
            return declarationNode->data.structMethod.returnType;

        case ZR_AST_CLASS_META_FUNCTION:
            return declarationNode->data.classMetaFunction.returnType;

        case ZR_AST_STRUCT_META_FUNCTION:
            return declarationNode->data.structMetaFunction.returnType;

        default:
            return ZR_NULL;
    }
}

static SZrTypeMemberInfo *signature_find_member_recursive(SZrCompilerState *compilerState,
                                                          SZrTypePrototypeInfo *prototype,
                                                          SZrString *memberName,
                                                          TZrUInt32 depth) {
    if (compilerState == ZR_NULL || prototype == ZR_NULL || memberName == ZR_NULL ||
        depth > ZR_LSP_AST_RECURSION_MAX_DEPTH) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < prototype->members.length; index++) {
        SZrTypeMemberInfo *memberInfo = (SZrTypeMemberInfo *)ZrCore_Array_Get(&prototype->members, index);
        if (memberInfo != ZR_NULL &&
            memberInfo->name != ZR_NULL &&
            ZrCore_String_Equal(memberInfo->name, memberName)) {
            return memberInfo;
        }
    }

    for (TZrSize index = 0; index < prototype->inherits.length; index++) {
        SZrString **inheritTypeNamePtr = (SZrString **)ZrCore_Array_Get(&prototype->inherits, index);
        SZrTypePrototypeInfo *inheritPrototype;
        SZrTypeMemberInfo *inheritedMember;

        if (inheritTypeNamePtr == ZR_NULL || *inheritTypeNamePtr == ZR_NULL) {
            continue;
        }

        inheritPrototype = find_compiler_type_prototype_inference(compilerState, *inheritTypeNamePtr);
        if (inheritPrototype == ZR_NULL || inheritPrototype == prototype) {
            continue;
        }

        inheritedMember = signature_find_member_recursive(compilerState, inheritPrototype, memberName, depth + 1);
        if (inheritedMember != ZR_NULL) {
            return inheritedMember;
        }
    }

    return ZR_NULL;
}

static TZrBool signature_prepare_specialized_receiver_member(SZrState *state,
                                                             SZrCompilerState *compilerState,
                                                             const SZrInferredType *receiverType,
                                                             SZrString *memberName,
                                                             SZrTypeMemberInfo **resolvedMemberInfo,
                                                             SZrTypeMemberInfo *temporaryMemberInfo) {
    SZrTypePrototypeInfo *closedPrototype = ZR_NULL;
    SZrTypeMemberInfo *memberInfo = ZR_NULL;
    SZrString *baseName = ZR_NULL;
    SZrArray argumentTypeNames;
    SZrArray bindingTypes;
    SZrTypePrototypeInfo *openPrototype = ZR_NULL;
    TZrBool hasGenericReceiver = ZR_FALSE;
    TZrBool success = ZR_FALSE;

    if (resolvedMemberInfo != ZR_NULL) {
        *resolvedMemberInfo = ZR_NULL;
    }
    if (state == ZR_NULL || compilerState == ZR_NULL || receiverType == ZR_NULL ||
        receiverType->typeName == ZR_NULL || memberName == ZR_NULL ||
        resolvedMemberInfo == ZR_NULL || temporaryMemberInfo == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Construct(&argumentTypeNames);
    ZrCore_Array_Construct(&bindingTypes);
    hasGenericReceiver =
        try_parse_generic_instance_type_name(state, receiverType->typeName, &baseName, &argumentTypeNames) &&
        baseName != ZR_NULL;
    if (!hasGenericReceiver) {
        ensure_generic_instance_type_prototype(compilerState, receiverType->typeName);
        closedPrototype = find_compiler_type_prototype_inference(compilerState, receiverType->typeName);
        memberInfo = signature_find_member_recursive(compilerState, closedPrototype, memberName, 0);
        if (memberInfo != ZR_NULL) {
            if (argumentTypeNames.isValid && argumentTypeNames.head != ZR_NULL) {
                ZrCore_Array_Free(state, &argumentTypeNames);
            }
            *resolvedMemberInfo = memberInfo;
            return ZR_TRUE;
        }
        if (argumentTypeNames.isValid && argumentTypeNames.head != ZR_NULL) {
            ZrCore_Array_Free(state, &argumentTypeNames);
        }
        return ZR_FALSE;
    }

    openPrototype = find_compiler_type_prototype_inference(compilerState, baseName);
    memberInfo = signature_find_member_recursive(compilerState, openPrototype, memberName, 0);
    if (memberInfo == ZR_NULL) {
        ZrCore_Array_Free(state, &argumentTypeNames);
        return ZR_FALSE;
    }

    memset(temporaryMemberInfo, 0, sizeof(*temporaryMemberInfo));
    *temporaryMemberInfo = *memberInfo;
    ZrCore_Array_Construct(&temporaryMemberInfo->parameterTypes);
    ZrCore_Array_Construct(&temporaryMemberInfo->genericParameters);
    ZrCore_Array_Construct(&temporaryMemberInfo->parameterPassingModes);

    if (!signature_build_receiver_binding_types(compilerState, &argumentTypeNames, &bindingTypes) ||
        !signature_append_generic_parameter_infos(state,
                                                  &temporaryMemberInfo->genericParameters,
                                                  &memberInfo->genericParameters) ||
        !signature_copy_parameter_passing_modes(state,
                                                &temporaryMemberInfo->parameterPassingModes,
                                                &memberInfo->parameterPassingModes)) {
        goto cleanup;
    }

    if (memberInfo->parameterTypes.length > 0) {
        ZrCore_Array_Init(state,
                          &temporaryMemberInfo->parameterTypes,
                          sizeof(SZrInferredType),
                          memberInfo->parameterTypes.length);
        for (TZrSize index = 0; index < memberInfo->parameterTypes.length; index++) {
            SZrInferredType *sourceType =
                (SZrInferredType *)ZrCore_Array_Get(&memberInfo->parameterTypes, index);
            SZrInferredType resolvedType;

            if (sourceType == ZR_NULL) {
                continue;
            }

            ZrParser_InferredType_Init(state, &resolvedType, ZR_VALUE_TYPE_OBJECT);
            if (!signature_substitute_receiver_generic_type(state,
                                                            &openPrototype->genericParameters,
                                                            &bindingTypes,
                                                            sourceType,
                                                            &resolvedType)) {
                ZrParser_InferredType_Free(state, &resolvedType);
                goto cleanup;
            }
            ZrCore_Array_Push(state, &temporaryMemberInfo->parameterTypes, &resolvedType);
        }
    }

    if (memberInfo->returnTypeName != ZR_NULL) {
        SZrType *returnTypeNode = signature_method_return_type_node(memberInfo->declarationNode);

        if (returnTypeNode != ZR_NULL) {
            if (!signature_build_specialized_type_name_from_ast_type(compilerState,
                                                                     returnTypeNode,
                                                                     &openPrototype->genericParameters,
                                                                     &bindingTypes,
                                                                     &temporaryMemberInfo->returnTypeName)) {
                goto cleanup;
            }
        } else if (!signature_build_specialized_type_name(compilerState,
                                                          memberInfo->returnTypeName,
                                                          &openPrototype->genericParameters,
                                                          &bindingTypes,
                                                          &temporaryMemberInfo->returnTypeName)) {
            goto cleanup;
        }
    }

    *resolvedMemberInfo = temporaryMemberInfo;
    success = ZR_TRUE;

cleanup:
    if (argumentTypeNames.isValid && argumentTypeNames.head != ZR_NULL) {
        ZrCore_Array_Free(state, &argumentTypeNames);
    }
    free_inferred_type_array(state, &bindingTypes);
    if (!success) {
        signature_free_temporary_member_info(state, temporaryMemberInfo);
    }
    return success;
}

static TZrBool signature_string_matches_text(SZrString *value,
                                             const TZrChar *text,
                                             TZrSize textLength) {
    const TZrChar *valueText;

    if (value == ZR_NULL || text == ZR_NULL) {
        return ZR_FALSE;
    }

    valueText = signature_string_native(value);
    return valueText != ZR_NULL && strlen(valueText) == textLength && memcmp(valueText, text, textLength) == 0;
}

static TZrBool signature_collect_generic_parameter_infos_from_ast(SZrState *state,
                                                                  SZrArray *dest,
                                                                  SZrGenericDeclaration *genericDeclaration) {
    if (state == ZR_NULL || dest == ZR_NULL) {
        return ZR_FALSE;
    }

    if (genericDeclaration == ZR_NULL || genericDeclaration->params == ZR_NULL || genericDeclaration->params->count == 0) {
        return ZR_TRUE;
    }

    ZrCore_Array_Init(state,
                      dest,
                      sizeof(SZrTypeGenericParameterInfo),
                      genericDeclaration->params->count);
    for (TZrSize index = 0; index < genericDeclaration->params->count; index++) {
        SZrAstNode *paramNode = genericDeclaration->params->nodes[index];
        SZrTypeGenericParameterInfo info;

        if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
            continue;
        }

        memset(&info, 0, sizeof(info));
        info.name = paramNode->data.parameter.name != ZR_NULL ? paramNode->data.parameter.name->name : ZR_NULL;
        info.genericKind = paramNode->data.parameter.genericKind;
        info.variance = paramNode->data.parameter.variance;
        info.requiresClass = paramNode->data.parameter.genericRequiresClass;
        info.requiresStruct = paramNode->data.parameter.genericRequiresStruct;
        info.requiresNew = paramNode->data.parameter.genericRequiresNew;
        ZrCore_Array_Construct(&info.constraintTypeNames);
        ZrCore_Array_Push(state, dest, &info);
    }

    return ZR_TRUE;
}

static void signature_free_generic_parameter_infos(SZrState *state, SZrArray *genericParameters) {
    if (state == ZR_NULL || genericParameters == ZR_NULL ||
        !genericParameters->isValid || genericParameters->head == ZR_NULL ||
        genericParameters->capacity == 0 || genericParameters->elementSize == 0) {
        return;
    }

    for (TZrSize index = 0; index < genericParameters->length; index++) {
        SZrTypeGenericParameterInfo *info =
            (SZrTypeGenericParameterInfo *)ZrCore_Array_Get(genericParameters, index);
        if (info != ZR_NULL &&
            info->constraintTypeNames.isValid &&
            info->constraintTypeNames.head != ZR_NULL &&
            info->constraintTypeNames.capacity > 0 &&
            info->constraintTypeNames.elementSize > 0) {
            ZrCore_Array_Free(state, &info->constraintTypeNames);
        }
    }

    ZrCore_Array_Free(state, genericParameters);
}

static TZrBool signature_collect_parameter_types_from_ast(SZrCompilerState *compilerState,
                                                          SZrAstNodeArray *params,
                                                          SZrArray *dest) {
    if (compilerState == ZR_NULL || dest == ZR_NULL) {
        return ZR_FALSE;
    }

    if (params == ZR_NULL || params->count == 0) {
        return ZR_TRUE;
    }

    ZrCore_Array_Init(compilerState->state, dest, sizeof(SZrInferredType), params->count);
    for (TZrSize index = 0; index < params->count; index++) {
        SZrAstNode *paramNode = params->nodes[index];
        SZrInferredType paramType;

        if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
            continue;
        }

        ZrParser_InferredType_Init(compilerState->state, &paramType, ZR_VALUE_TYPE_OBJECT);
        if (paramNode->data.parameter.typeInfo != ZR_NULL &&
            !ZrParser_AstTypeToInferredType_Convert(compilerState,
                                                    paramNode->data.parameter.typeInfo,
                                                    &paramType)) {
            ZrParser_InferredType_Free(compilerState->state, &paramType);
            free_inferred_type_array(compilerState->state, dest);
            return ZR_FALSE;
        }

        ZrCore_Array_Push(compilerState->state, dest, &paramType);
    }

    return ZR_TRUE;
}

static TZrBool signature_collect_parameter_passing_modes_from_ast(SZrState *state,
                                                                  SZrAstNodeArray *params,
                                                                  SZrArray *dest) {
    if (state == ZR_NULL || dest == ZR_NULL) {
        return ZR_FALSE;
    }

    if (params == ZR_NULL || params->count == 0) {
        return ZR_TRUE;
    }

    ZrCore_Array_Init(state, dest, sizeof(EZrParameterPassingMode), params->count);
    for (TZrSize index = 0; index < params->count; index++) {
        SZrAstNode *paramNode = params->nodes[index];
        EZrParameterPassingMode passingMode = ZR_PARAMETER_PASSING_MODE_VALUE;

        if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
            continue;
        }

        passingMode = paramNode->data.parameter.passingMode;
        ZrCore_Array_Push(state, dest, &passingMode);
    }

    return ZR_TRUE;
}

static SZrString *signature_create_type_name_from_ast_type(SZrCompilerState *compilerState, SZrType *typeInfo) {
    SZrInferredType inferredType;
    TZrChar buffer[ZR_LSP_TEXT_BUFFER_LENGTH];
    const TZrChar *displayText;
    SZrString *result = ZR_NULL;

    if (compilerState == ZR_NULL || typeInfo == ZR_NULL) {
        return ZR_NULL;
    }

    ZrParser_InferredType_Init(compilerState->state, &inferredType, ZR_VALUE_TYPE_OBJECT);
    if (!ZrParser_AstTypeToInferredType_Convert(compilerState, typeInfo, &inferredType)) {
        ZrParser_InferredType_Free(compilerState->state, &inferredType);
        return ZR_NULL;
    }

    displayText = ZrParser_TypeNameString_Get(compilerState->state, &inferredType, buffer, sizeof(buffer));
    if (displayText != ZR_NULL && displayText[0] != '\0') {
        result = ZrCore_String_Create(compilerState->state, (TZrNativeString)displayText, strlen(displayText));
    }

    ZrParser_InferredType_Free(compilerState->state, &inferredType);
    return result;
}

static SZrAstNode *signature_find_type_declaration_recursive(SZrAstNode *node,
                                                             const TZrChar *typeNameText,
                                                             TZrSize typeNameLength) {
    if (node == ZR_NULL || typeNameText == ZR_NULL) {
        return ZR_NULL;
    }

    switch (node->type) {
        case ZR_AST_SCRIPT:
            if (node->data.script.statements != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.script.statements->count; index++) {
                    SZrAstNode *result =
                        signature_find_type_declaration_recursive(node->data.script.statements->nodes[index],
                                                                  typeNameText,
                                                                  typeNameLength);
                    if (result != ZR_NULL) {
                        return result;
                    }
                }
            }
            break;

        case ZR_AST_BLOCK:
            if (node->data.block.body != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.block.body->count; index++) {
                    SZrAstNode *result =
                        signature_find_type_declaration_recursive(node->data.block.body->nodes[index],
                                                                  typeNameText,
                                                                  typeNameLength);
                    if (result != ZR_NULL) {
                        return result;
                    }
                }
            }
            break;

        case ZR_AST_CLASS_DECLARATION:
            if (node->data.classDeclaration.name != ZR_NULL &&
                signature_string_matches_text(node->data.classDeclaration.name->name, typeNameText, typeNameLength)) {
                return node;
            }
            break;

        case ZR_AST_STRUCT_DECLARATION:
            if (node->data.structDeclaration.name != ZR_NULL &&
                signature_string_matches_text(node->data.structDeclaration.name->name, typeNameText, typeNameLength)) {
                return node;
            }
            break;

        default:
            break;
    }

    return ZR_NULL;
}

static SZrAstNode *signature_find_method_declaration_in_type(SZrAstNode *typeDeclarationNode,
                                                             const TZrChar *memberNameText,
                                                             TZrSize memberNameLength) {
    SZrAstNodeArray *members = ZR_NULL;

    if (typeDeclarationNode == ZR_NULL || memberNameText == ZR_NULL) {
        return ZR_NULL;
    }

    if (typeDeclarationNode->type == ZR_AST_CLASS_DECLARATION) {
        members = typeDeclarationNode->data.classDeclaration.members;
    } else if (typeDeclarationNode->type == ZR_AST_STRUCT_DECLARATION) {
        members = typeDeclarationNode->data.structDeclaration.members;
    }

    if (members == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < members->count; index++) {
        SZrAstNode *memberNode = members->nodes[index];

        if (memberNode == ZR_NULL) {
            continue;
        }

        if (memberNode->type == ZR_AST_CLASS_METHOD &&
            memberNode->data.classMethod.name != ZR_NULL &&
            signature_string_matches_text(memberNode->data.classMethod.name->name, memberNameText, memberNameLength)) {
            return memberNode;
        }

        if (memberNode->type == ZR_AST_STRUCT_METHOD &&
            memberNode->data.structMethod.name != ZR_NULL &&
            signature_string_matches_text(memberNode->data.structMethod.name->name, memberNameText, memberNameLength)) {
            return memberNode;
        }
    }

    return ZR_NULL;
}

static SZrAstNode *signature_find_constructor_declaration_in_type(SZrAstNode *typeDeclarationNode) {
    SZrAstNodeArray *members = ZR_NULL;

    if (typeDeclarationNode == ZR_NULL) {
        return ZR_NULL;
    }

    if (typeDeclarationNode->type == ZR_AST_CLASS_DECLARATION) {
        members = typeDeclarationNode->data.classDeclaration.members;
    } else if (typeDeclarationNode->type == ZR_AST_STRUCT_DECLARATION) {
        members = typeDeclarationNode->data.structDeclaration.members;
    }

    if (members == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < members->count; index++) {
        SZrAstNode *memberNode = members->nodes[index];

        if (memberNode == ZR_NULL) {
            continue;
        }

        if (memberNode->type == ZR_AST_CLASS_META_FUNCTION &&
            memberNode->data.classMetaFunction.meta != ZR_NULL &&
            signature_string_matches_text(memberNode->data.classMetaFunction.meta->name, "constructor", 11)) {
            return memberNode;
        }

        if (memberNode->type == ZR_AST_STRUCT_META_FUNCTION &&
            memberNode->data.structMetaFunction.meta != ZR_NULL &&
            signature_string_matches_text(memberNode->data.structMetaFunction.meta->name, "constructor", 11)) {
            return memberNode;
        }
    }

    return ZR_NULL;
}

static TZrBool signature_prepare_ast_specialized_receiver_constructor(SZrState *state,
                                                                      SZrCompilerState *compilerState,
                                                                      SZrAstNode *rootNode,
                                                                      const SZrInferredType *receiverType,
                                                                      SZrTypeMemberInfo **resolvedMemberInfo,
                                                                      SZrTypeMemberInfo *temporaryMemberInfo) {
    SZrString *baseName = ZR_NULL;
    SZrArray argumentTypeNames;
    SZrArray bindingTypes;
    SZrArray typeGenericParameters;
    SZrArray rawParameterTypes;
    SZrAstNode *typeDeclarationNode;
    SZrAstNode *memberDeclarationNode;
    SZrGenericDeclaration *typeGeneric = ZR_NULL;
    SZrAstNodeArray *params = ZR_NULL;
    SZrType *returnType = ZR_NULL;
    SZrString *memberName = ZR_NULL;
    const TZrChar *baseNameText;
    TZrBool success = ZR_FALSE;

    if (resolvedMemberInfo != ZR_NULL) {
        *resolvedMemberInfo = ZR_NULL;
    }
    if (state == ZR_NULL || compilerState == ZR_NULL || rootNode == ZR_NULL || receiverType == ZR_NULL ||
        receiverType->typeName == ZR_NULL || resolvedMemberInfo == ZR_NULL || temporaryMemberInfo == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Construct(&argumentTypeNames);
    ZrCore_Array_Construct(&bindingTypes);
    ZrCore_Array_Construct(&typeGenericParameters);
    ZrCore_Array_Construct(&rawParameterTypes);

    if (!try_parse_generic_instance_type_name(state, receiverType->typeName, &baseName, &argumentTypeNames)) {
        baseName = receiverType->typeName;
    }

    baseNameText = signature_string_native(baseName);
    typeDeclarationNode =
        signature_find_type_declaration_recursive(rootNode, baseNameText, strlen(baseNameText));
    memberDeclarationNode = signature_find_constructor_declaration_in_type(typeDeclarationNode);
    if (typeDeclarationNode == ZR_NULL || memberDeclarationNode == ZR_NULL) {
        goto cleanup;
    }

    if (typeDeclarationNode->type == ZR_AST_CLASS_DECLARATION) {
        typeGeneric = typeDeclarationNode->data.classDeclaration.generic;
    } else if (typeDeclarationNode->type == ZR_AST_STRUCT_DECLARATION) {
        typeGeneric = typeDeclarationNode->data.structDeclaration.generic;
    }

    if (memberDeclarationNode->type == ZR_AST_CLASS_META_FUNCTION) {
        memberName = memberDeclarationNode->data.classMetaFunction.meta != ZR_NULL
                         ? memberDeclarationNode->data.classMetaFunction.meta->name
                         : ZR_NULL;
        params = memberDeclarationNode->data.classMetaFunction.params;
        returnType = memberDeclarationNode->data.classMetaFunction.returnType;
    } else if (memberDeclarationNode->type == ZR_AST_STRUCT_META_FUNCTION) {
        memberName = memberDeclarationNode->data.structMetaFunction.meta != ZR_NULL
                         ? memberDeclarationNode->data.structMetaFunction.meta->name
                         : ZR_NULL;
        params = memberDeclarationNode->data.structMetaFunction.params;
        returnType = memberDeclarationNode->data.structMetaFunction.returnType;
    } else {
        goto cleanup;
    }

    memset(temporaryMemberInfo, 0, sizeof(*temporaryMemberInfo));
    temporaryMemberInfo->memberType = memberDeclarationNode->type;
    temporaryMemberInfo->name = memberName;
    temporaryMemberInfo->declarationNode = memberDeclarationNode;
    temporaryMemberInfo->isMetaMethod = ZR_TRUE;
    temporaryMemberInfo->metaType = ZR_META_CONSTRUCTOR;
    ZrCore_Array_Construct(&temporaryMemberInfo->parameterTypes);
    ZrCore_Array_Construct(&temporaryMemberInfo->genericParameters);
    ZrCore_Array_Construct(&temporaryMemberInfo->parameterPassingModes);

    if (!signature_collect_generic_parameter_infos_from_ast(state, &typeGenericParameters, typeGeneric) ||
        !signature_collect_parameter_passing_modes_from_ast(state,
                                                            params,
                                                            &temporaryMemberInfo->parameterPassingModes) ||
        !signature_collect_parameter_types_from_ast(compilerState, params, &rawParameterTypes) ||
        !signature_build_receiver_binding_types(compilerState, &argumentTypeNames, &bindingTypes)) {
        goto cleanup;
    }

    if (rawParameterTypes.length > 0) {
        ZrCore_Array_Init(state,
                          &temporaryMemberInfo->parameterTypes,
                          sizeof(SZrInferredType),
                          rawParameterTypes.length);
        for (TZrSize index = 0; index < rawParameterTypes.length; index++) {
            SZrInferredType *sourceType = (SZrInferredType *)ZrCore_Array_Get(&rawParameterTypes, index);
            SZrInferredType resolvedType;

            if (sourceType == ZR_NULL) {
                continue;
            }

            ZrParser_InferredType_Init(state, &resolvedType, ZR_VALUE_TYPE_OBJECT);
            if (!signature_substitute_receiver_generic_type(state,
                                                            &typeGenericParameters,
                                                            &bindingTypes,
                                                            sourceType,
                                                            &resolvedType)) {
                ZrParser_InferredType_Free(state, &resolvedType);
                goto cleanup;
            }
            ZrCore_Array_Push(state, &temporaryMemberInfo->parameterTypes, &resolvedType);
        }
    }

    if (returnType != ZR_NULL) {
        temporaryMemberInfo->returnTypeName = signature_create_type_name_from_ast_type(compilerState, returnType);
        if (!signature_build_specialized_type_name(compilerState,
                                                   temporaryMemberInfo->returnTypeName,
                                                   &typeGenericParameters,
                                                   &bindingTypes,
                                                   &temporaryMemberInfo->returnTypeName)) {
            goto cleanup;
        }
    }

    *resolvedMemberInfo = temporaryMemberInfo;
    success = ZR_TRUE;

cleanup:
    if (argumentTypeNames.isValid && argumentTypeNames.head != ZR_NULL) {
        ZrCore_Array_Free(state, &argumentTypeNames);
    }
    free_inferred_type_array(state, &bindingTypes);
    signature_free_generic_parameter_infos(state, &typeGenericParameters);
    free_inferred_type_array(state, &rawParameterTypes);
    if (!success) {
        signature_free_temporary_member_info(state, temporaryMemberInfo);
    }
    return success;
}

static TZrBool signature_prepare_ast_specialized_receiver_member(SZrState *state,
                                                                 SZrCompilerState *compilerState,
                                                                 SZrAstNode *rootNode,
                                                                 const SZrInferredType *receiverType,
                                                                 SZrString *memberName,
                                                                 SZrTypeMemberInfo **resolvedMemberInfo,
                                                                 SZrTypeMemberInfo *temporaryMemberInfo) {
    SZrString *baseName = ZR_NULL;
    SZrArray argumentTypeNames;
    SZrArray bindingTypes;
    SZrArray typeGenericParameters;
    SZrArray rawParameterTypes;
    SZrAstNode *typeDeclarationNode;
    SZrAstNode *memberDeclarationNode;
    SZrGenericDeclaration *typeGeneric = ZR_NULL;
    SZrGenericDeclaration *methodGeneric = ZR_NULL;
    SZrAstNodeArray *params = ZR_NULL;
    SZrType *returnType = ZR_NULL;
    const TZrChar *baseNameText;
    TZrBool success = ZR_FALSE;

    if (resolvedMemberInfo != ZR_NULL) {
        *resolvedMemberInfo = ZR_NULL;
    }
    if (state == ZR_NULL || compilerState == ZR_NULL || rootNode == ZR_NULL || receiverType == ZR_NULL ||
        receiverType->typeName == ZR_NULL || memberName == ZR_NULL || resolvedMemberInfo == ZR_NULL ||
        temporaryMemberInfo == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Construct(&argumentTypeNames);
    ZrCore_Array_Construct(&bindingTypes);
    ZrCore_Array_Construct(&typeGenericParameters);
    ZrCore_Array_Construct(&rawParameterTypes);

    if (!try_parse_generic_instance_type_name(state, receiverType->typeName, &baseName, &argumentTypeNames)) {
        baseName = receiverType->typeName;
    }

    baseNameText = signature_string_native(baseName);
    typeDeclarationNode =
        signature_find_type_declaration_recursive(rootNode, baseNameText, strlen(baseNameText));
    memberDeclarationNode =
        signature_find_method_declaration_in_type(typeDeclarationNode,
                                                  signature_string_native(memberName),
                                                  strlen(signature_string_native(memberName)));
    if (typeDeclarationNode == ZR_NULL || memberDeclarationNode == ZR_NULL) {
        goto cleanup;
    }

    if (typeDeclarationNode->type == ZR_AST_CLASS_DECLARATION) {
        typeGeneric = typeDeclarationNode->data.classDeclaration.generic;
    } else if (typeDeclarationNode->type == ZR_AST_STRUCT_DECLARATION) {
        typeGeneric = typeDeclarationNode->data.structDeclaration.generic;
    }

    if (memberDeclarationNode->type == ZR_AST_CLASS_METHOD) {
        methodGeneric = memberDeclarationNode->data.classMethod.generic;
        params = memberDeclarationNode->data.classMethod.params;
        returnType = memberDeclarationNode->data.classMethod.returnType;
    } else if (memberDeclarationNode->type == ZR_AST_STRUCT_METHOD) {
        methodGeneric = memberDeclarationNode->data.structMethod.generic;
        params = memberDeclarationNode->data.structMethod.params;
        returnType = memberDeclarationNode->data.structMethod.returnType;
    } else {
        goto cleanup;
    }

    memset(temporaryMemberInfo, 0, sizeof(*temporaryMemberInfo));
    temporaryMemberInfo->memberType = memberDeclarationNode->type;
    temporaryMemberInfo->name = memberName;
    temporaryMemberInfo->declarationNode = memberDeclarationNode;
    ZrCore_Array_Construct(&temporaryMemberInfo->parameterTypes);
    ZrCore_Array_Construct(&temporaryMemberInfo->genericParameters);
    ZrCore_Array_Construct(&temporaryMemberInfo->parameterPassingModes);

    if (!signature_collect_generic_parameter_infos_from_ast(state, &typeGenericParameters, typeGeneric) ||
        !signature_collect_generic_parameter_infos_from_ast(state,
                                                            &temporaryMemberInfo->genericParameters,
                                                            methodGeneric) ||
        !signature_collect_parameter_passing_modes_from_ast(state,
                                                            params,
                                                            &temporaryMemberInfo->parameterPassingModes) ||
        !signature_collect_parameter_types_from_ast(compilerState, params, &rawParameterTypes) ||
        !signature_build_receiver_binding_types(compilerState, &argumentTypeNames, &bindingTypes)) {
        goto cleanup;
    }

    if (rawParameterTypes.length > 0) {
        ZrCore_Array_Init(state,
                          &temporaryMemberInfo->parameterTypes,
                          sizeof(SZrInferredType),
                          rawParameterTypes.length);
        for (TZrSize index = 0; index < rawParameterTypes.length; index++) {
            SZrInferredType *sourceType = (SZrInferredType *)ZrCore_Array_Get(&rawParameterTypes, index);
            SZrInferredType resolvedType;

            if (sourceType == ZR_NULL) {
                continue;
            }

            ZrParser_InferredType_Init(state, &resolvedType, ZR_VALUE_TYPE_OBJECT);
            if (!signature_substitute_receiver_generic_type(state,
                                                            &typeGenericParameters,
                                                            &bindingTypes,
                                                            sourceType,
                                                            &resolvedType)) {
                ZrParser_InferredType_Free(state, &resolvedType);
                goto cleanup;
            }
            ZrCore_Array_Push(state, &temporaryMemberInfo->parameterTypes, &resolvedType);
        }
    }

    temporaryMemberInfo->returnTypeName = signature_create_type_name_from_ast_type(compilerState, returnType);
    if (!signature_build_specialized_type_name(compilerState,
                                               temporaryMemberInfo->returnTypeName,
                                               &typeGenericParameters,
                                               &bindingTypes,
                                               &temporaryMemberInfo->returnTypeName)) {
        goto cleanup;
    }

    *resolvedMemberInfo = temporaryMemberInfo;
    success = ZR_TRUE;

cleanup:
    if (argumentTypeNames.isValid && argumentTypeNames.head != ZR_NULL) {
        ZrCore_Array_Free(state, &argumentTypeNames);
    }
    free_inferred_type_array(state, &bindingTypes);
    signature_free_generic_parameter_infos(state, &typeGenericParameters);
    free_inferred_type_array(state, &rawParameterTypes);
    if (!success) {
        signature_free_temporary_member_info(state, temporaryMemberInfo);
    }
    return success;
}

static TZrInt32 signature_find_binding_index(const SZrArray *bindings, SZrString *typeName) {
    if (bindings == ZR_NULL || typeName == ZR_NULL) {
        return ZR_LSP_SIGNATURE_BINDING_INDEX_NONE;
    }

    for (TZrSize index = 0; index < bindings->length; index++) {
        SZrLspGenericBinding *binding = (SZrLspGenericBinding *)ZrCore_Array_Get((SZrArray *)bindings, index);
        if (binding != ZR_NULL &&
            binding->parameterInfo != ZR_NULL &&
            binding->parameterInfo->name != ZR_NULL &&
            ZrCore_String_Equal(binding->parameterInfo->name, typeName)) {
            return (TZrInt32)index;
        }
    }

    return ZR_LSP_SIGNATURE_BINDING_INDEX_NONE;
}

static TZrBool signature_initialize_bindings(SZrState *state,
                                             const SZrArray *genericParameters,
                                             SZrArray *bindings) {
    if (state == ZR_NULL || genericParameters == ZR_NULL || bindings == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Construct(bindings);
    if (genericParameters->length == 0) {
        return ZR_TRUE;
    }

    ZrCore_Array_Init(state, bindings, sizeof(SZrLspGenericBinding), genericParameters->length);
    for (TZrSize index = 0; index < genericParameters->length; index++) {
        SZrTypeGenericParameterInfo *parameterInfo =
            (SZrTypeGenericParameterInfo *)ZrCore_Array_Get((SZrArray *)genericParameters, index);
        SZrLspGenericBinding binding;

        if (parameterInfo == ZR_NULL) {
            continue;
        }

        memset(&binding, 0, sizeof(binding));
        binding.parameterInfo = parameterInfo;
        binding.isBound = ZR_FALSE;
        ZrParser_InferredType_Init(state, &binding.inferredType, ZR_VALUE_TYPE_OBJECT);
        ZrCore_Array_Push(state, bindings, &binding);
    }

    return ZR_TRUE;
}

static void signature_free_bindings(SZrState *state, SZrArray *bindings) {
    if (state == ZR_NULL || bindings == ZR_NULL ||
        !bindings->isValid || bindings->head == ZR_NULL ||
        bindings->capacity == 0 || bindings->elementSize == 0) {
        return;
    }

    for (TZrSize index = 0; index < bindings->length; index++) {
        SZrLspGenericBinding *binding = (SZrLspGenericBinding *)ZrCore_Array_Get(bindings, index);
        if (binding != ZR_NULL) {
            ZrParser_InferredType_Free(state, &binding->inferredType);
        }
    }

    ZrCore_Array_Free(state, bindings);
}

static TZrBool signature_normalize_inferred_type(SZrCompilerState *compilerState, SZrInferredType *typeInfo) {
    SZrInferredType normalizedType;

    if (compilerState == ZR_NULL || typeInfo == ZR_NULL || typeInfo->typeName == ZR_NULL ||
        typeInfo->elementTypes.length > 0) {
        return ZR_TRUE;
    }

    ZrParser_InferredType_Init(compilerState->state, &normalizedType, ZR_VALUE_TYPE_OBJECT);
    if (inferred_type_from_type_name(compilerState, typeInfo->typeName, &normalizedType) &&
        normalizedType.elementTypes.length > 0) {
        normalizedType.isNullable = typeInfo->isNullable;
        normalizedType.ownershipQualifier = typeInfo->ownershipQualifier;
        ZrParser_InferredType_Free(compilerState->state, typeInfo);
        ZrParser_InferredType_Copy(compilerState->state, typeInfo, &normalizedType);
    }
    ZrParser_InferredType_Free(compilerState->state, &normalizedType);
    return ZR_TRUE;
}

static TZrBool signature_infer_argument_type_with_fallback(SZrSemanticAnalyzer *analyzer,
                                                           SZrCompilerState *compilerState,
                                                           SZrAstNode *argNode,
                                                           TZrSize cursorOffset,
                                                           SZrInferredType *result) {
    if (analyzer == ZR_NULL || compilerState == ZR_NULL || argNode == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrParser_InferredType_Init(compilerState->state, result, ZR_VALUE_TYPE_OBJECT);
    if (!ZrParser_ExpressionType_Infer(compilerState, argNode, result)) {
        ZrParser_InferredType_Free(compilerState->state, result);
        return ZR_FALSE;
    }

    signature_normalize_inferred_type(compilerState, result);
    if (result->typeName == ZR_NULL &&
        argNode->type == ZR_AST_IDENTIFIER_LITERAL &&
        analyzer->ast != ZR_NULL) {
        const TZrChar *nameText = signature_string_native(argNode->data.identifier.name);
        TZrSize bestOffset = 0;
        TZrBool found = ZR_FALSE;

        signature_find_named_value_type_recursive(compilerState,
                                                  analyzer->ast,
                                                  nameText,
                                                  strlen(nameText),
                                                  cursorOffset,
                                                  result,
                                                  &bestOffset,
                                                  &found);
        if (found) {
            signature_normalize_inferred_type(compilerState, result);
        }
    }

    return ZR_TRUE;
}

static TZrBool signature_unify_binding_from_types(SZrState *state,
                                                  SZrArray *bindings,
                                                  const SZrInferredType *expectedType,
                                                  const SZrInferredType *actualType) {
    TZrInt32 bindingIndex;

    if (state == ZR_NULL || bindings == ZR_NULL || expectedType == ZR_NULL || actualType == ZR_NULL) {
        return ZR_FALSE;
    }

    bindingIndex = signature_find_binding_index(bindings, expectedType->typeName);
    if (bindingIndex != ZR_LSP_SIGNATURE_BINDING_INDEX_NONE && expectedType->elementTypes.length == 0) {
        SZrLspGenericBinding *binding =
            (SZrLspGenericBinding *)ZrCore_Array_Get(bindings, (TZrSize)bindingIndex);
        if (binding == ZR_NULL) {
            return ZR_FALSE;
        }

        if (!binding->isBound) {
            binding->isBound = ZR_TRUE;
            ZrParser_InferredType_Copy(state, &binding->inferredType, actualType);
            return ZR_TRUE;
        }

        return ZrParser_InferredType_Equal(&binding->inferredType, actualType);
    }

    if (expectedType->baseType != actualType->baseType ||
        expectedType->elementTypes.length != actualType->elementTypes.length) {
        return ZR_FALSE;
    }

    if (expectedType->typeName != ZR_NULL && actualType->typeName != ZR_NULL) {
        SZrString *expectedBaseName = ZR_NULL;
        SZrString *actualBaseName = ZR_NULL;
        SZrArray expectedArgumentNames;
        SZrArray actualArgumentNames;
        TZrBool matched;

        ZrCore_Array_Construct(&expectedArgumentNames);
        ZrCore_Array_Construct(&actualArgumentNames);
        matched = try_parse_generic_instance_type_name(state, expectedType->typeName, &expectedBaseName, &expectedArgumentNames) &&
                  try_parse_generic_instance_type_name(state, actualType->typeName, &actualBaseName, &actualArgumentNames) &&
                  expectedBaseName != ZR_NULL &&
                  actualBaseName != ZR_NULL &&
                  ZrCore_String_Equal(expectedBaseName, actualBaseName) &&
                  expectedArgumentNames.length == actualArgumentNames.length;
        if (expectedArgumentNames.isValid && expectedArgumentNames.head != ZR_NULL) {
            ZrCore_Array_Free(state, &expectedArgumentNames);
        }
        if (actualArgumentNames.isValid && actualArgumentNames.head != ZR_NULL) {
            ZrCore_Array_Free(state, &actualArgumentNames);
        }
        if (!matched && expectedType->elementTypes.length > 0) {
            return ZR_FALSE;
        }
    }

    for (TZrSize index = 0; index < expectedType->elementTypes.length; index++) {
        SZrInferredType *expectedElement =
            (SZrInferredType *)ZrCore_Array_Get((SZrArray *)&expectedType->elementTypes, index);
        SZrInferredType *actualElement =
            (SZrInferredType *)ZrCore_Array_Get((SZrArray *)&actualType->elementTypes, index);
        if (expectedElement == ZR_NULL || actualElement == ZR_NULL ||
            !signature_unify_binding_from_types(state, bindings, expectedElement, actualElement)) {
            return ZR_FALSE;
        }
    }

    return expectedType->elementTypes.length > 0 || ZrParser_InferredType_Equal(expectedType, actualType);
}

static TZrBool signature_bind_explicit_generic_argument(SZrCompilerState *compilerState,
                                                        const SZrTypeGenericParameterInfo *parameterInfo,
                                                        SZrAstNode *argumentNode,
                                                        SZrInferredType *result) {
    SZrTypeValue evaluatedValue;
    TZrChar integerBuffer[ZR_LSP_INTEGER_BUFFER_LENGTH];

    if (compilerState == ZR_NULL || parameterInfo == ZR_NULL || argumentNode == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrParser_InferredType_Init(compilerState->state, result, ZR_VALUE_TYPE_OBJECT);
    if (parameterInfo->genericKind == ZR_GENERIC_PARAMETER_CONST_INT) {
        if (!ZrParser_Compiler_EvaluateCompileTimeExpression(compilerState, argumentNode, &evaluatedValue)) {
            return ZR_FALSE;
        }
        if (ZR_VALUE_IS_TYPE_INT(evaluatedValue.type)) {
            snprintf(integerBuffer,
                     sizeof(integerBuffer),
                     "%lld",
                     (long long)evaluatedValue.value.nativeObject.nativeInt64);
        } else if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(evaluatedValue.type)) {
            snprintf(integerBuffer,
                     sizeof(integerBuffer),
                     "%llu",
                     (unsigned long long)evaluatedValue.value.nativeObject.nativeUInt64);
        } else {
            return ZR_FALSE;
        }
        ZrParser_InferredType_Free(compilerState->state, result);
        ZrParser_InferredType_InitFull(compilerState->state,
                                       result,
                                       ZR_VALUE_TYPE_OBJECT,
                                       ZR_FALSE,
                                       ZrCore_String_CreateFromNative(compilerState->state, integerBuffer));
        return ZR_TRUE;
    }

    if (argumentNode->type != ZR_AST_TYPE) {
        return ZR_FALSE;
    }

    return ZrParser_AstTypeToInferredType_Convert(compilerState, &argumentNode->data.type, result);
}

static TZrBool signature_resolve_member_call_signature_locally(SZrState *state,
                                                               SZrSemanticAnalyzer *analyzer,
                                                               SZrCompilerState *compilerState,
                                                               const SZrTypeMemberInfo *memberInfo,
                                                               SZrFunctionCall *call,
                                                               TZrSize cursorOffset,
                                                               SZrResolvedCallSignature *resolvedSignature) {
    SZrArray bindings;
    SZrArray argumentTypes;
    SZrArray bindingTypes;
    SZrString *resolvedReturnTypeName = ZR_NULL;

    if (state == ZR_NULL || analyzer == ZR_NULL || compilerState == ZR_NULL || memberInfo == ZR_NULL ||
        call == ZR_NULL || resolvedSignature == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!signature_initialize_bindings(state, &memberInfo->genericParameters, &bindings)) {
        return ZR_FALSE;
    }

    ZrCore_Array_Construct(&argumentTypes);
    ZrCore_Array_Construct(&bindingTypes);
    if (call->genericArguments != ZR_NULL && call->genericArguments->count > 0) {
        if (call->genericArguments->count != memberInfo->genericParameters.length) {
            signature_free_bindings(state, &bindings);
            return ZR_FALSE;
        }

        for (TZrSize index = 0; index < call->genericArguments->count; index++) {
            SZrLspGenericBinding *binding = (SZrLspGenericBinding *)ZrCore_Array_Get(&bindings, index);
            SZrAstNode *argumentNode = call->genericArguments->nodes[index];
            SZrInferredType explicitType;

            if (binding == ZR_NULL || argumentNode == ZR_NULL) {
                signature_free_bindings(state, &bindings);
                return ZR_FALSE;
            }

            if (!signature_bind_explicit_generic_argument(compilerState,
                                                          binding->parameterInfo,
                                                          argumentNode,
                                                          &explicitType)) {
                signature_free_bindings(state, &bindings);
                return ZR_FALSE;
            }

            binding->isBound = ZR_TRUE;
            ZrParser_InferredType_Free(state, &binding->inferredType);
            ZrParser_InferredType_Copy(state, &binding->inferredType, &explicitType);
            ZrParser_InferredType_Free(state, &explicitType);
        }
    }

    ZrCore_Array_Init(state,
                      &argumentTypes,
                      sizeof(SZrInferredType),
                      call->args != ZR_NULL ? call->args->count : 1);
    if (call->args != ZR_NULL) {
        for (TZrSize index = 0; index < call->args->count; index++) {
            SZrInferredType argumentType;
            SZrInferredType *expectedType;

            if (!signature_infer_argument_type_with_fallback(analyzer,
                                                             compilerState,
                                                             call->args->nodes[index],
                                                             cursorOffset,
                                                             &argumentType)) {
                free_inferred_type_array(state, &argumentTypes);
                signature_free_bindings(state, &bindings);
                return ZR_FALSE;
            }
            ZrCore_Array_Push(state, &argumentTypes, &argumentType);

            expectedType = (SZrInferredType *)ZrCore_Array_Get((SZrArray *)&memberInfo->parameterTypes, index);
            if (expectedType == ZR_NULL ||
                !signature_unify_binding_from_types(state, &bindings, expectedType, &argumentType)) {
                free_inferred_type_array(state, &argumentTypes);
                signature_free_bindings(state, &bindings);
                return ZR_FALSE;
            }
        }
    }

    for (TZrSize index = 0; index < bindings.length; index++) {
        SZrLspGenericBinding *binding = (SZrLspGenericBinding *)ZrCore_Array_Get(&bindings, index);
        if (binding == ZR_NULL || !binding->isBound) {
            free_inferred_type_array(state, &argumentTypes);
            signature_free_bindings(state, &bindings);
            return ZR_FALSE;
        }
    }

    if (bindings.length > 0) {
        ZrCore_Array_Init(state, &bindingTypes, sizeof(SZrInferredType), bindings.length);
        for (TZrSize index = 0; index < bindings.length; index++) {
            SZrLspGenericBinding *binding = (SZrLspGenericBinding *)ZrCore_Array_Get(&bindings, index);
            if (binding != ZR_NULL) {
                ZrCore_Array_Push(state, &bindingTypes, &binding->inferredType);
            }
        }
    }

    ZrParser_InferredType_Init(state, &resolvedSignature->returnType, ZR_VALUE_TYPE_OBJECT);
    ZrCore_Array_Construct(&resolvedSignature->parameterTypes);
    ZrCore_Array_Construct(&resolvedSignature->parameterPassingModes);
    signature_copy_parameter_passing_modes(state,
                                           &resolvedSignature->parameterPassingModes,
                                           &memberInfo->parameterPassingModes);

    if (memberInfo->parameterTypes.length > 0) {
        ZrCore_Array_Init(state,
                          &resolvedSignature->parameterTypes,
                          sizeof(SZrInferredType),
                          memberInfo->parameterTypes.length);
        for (TZrSize index = 0; index < memberInfo->parameterTypes.length; index++) {
            SZrInferredType *sourceType = (SZrInferredType *)ZrCore_Array_Get((SZrArray *)&memberInfo->parameterTypes, index);
            SZrInferredType resolvedType;

            if (sourceType == ZR_NULL) {
                continue;
            }

            ZrParser_InferredType_Init(state, &resolvedType, ZR_VALUE_TYPE_OBJECT);
            if (!signature_substitute_receiver_generic_type(state,
                                                            &memberInfo->genericParameters,
                                                            &bindingTypes,
                                                            sourceType,
                                                            &resolvedType)) {
                ZrParser_InferredType_Free(state, &resolvedType);
                free_inferred_type_array(state, &argumentTypes);
                free_inferred_type_array(state, &bindingTypes);
                signature_free_bindings(state, &bindings);
                return ZR_FALSE;
            }
            ZrCore_Array_Push(state, &resolvedSignature->parameterTypes, &resolvedType);
        }
    }

    if (memberInfo->returnTypeName == ZR_NULL) {
        ZrParser_InferredType_Free(state, &resolvedSignature->returnType);
        ZrParser_InferredType_Init(state, &resolvedSignature->returnType, ZR_VALUE_TYPE_NULL);
    } else if (!signature_build_specialized_type_name(compilerState,
                                                      memberInfo->returnTypeName,
                                                      &memberInfo->genericParameters,
                                                      &bindingTypes,
                                                      &resolvedReturnTypeName) ||
               resolvedReturnTypeName == ZR_NULL ||
               !inferred_type_from_type_name(compilerState,
                                             resolvedReturnTypeName,
                                             &resolvedSignature->returnType)) {
        free_inferred_type_array(state, &argumentTypes);
        free_inferred_type_array(state, &bindingTypes);
        signature_free_bindings(state, &bindings);
        return ZR_FALSE;
    }

    free_inferred_type_array(state, &argumentTypes);
    free_inferred_type_array(state, &bindingTypes);
    signature_free_bindings(state, &bindings);
    return ZR_TRUE;
}

static TZrBool signature_populate_help_from_label(SZrState *state,
                                                  const TZrChar *labelText,
                                                  SZrAstNodeArray *params,
                                                  const SZrResolvedCallSignature *resolvedSignature,
                                                  TZrInt32 activeParameter,
                                                  SZrLspSignatureHelp **result) {
    SZrLspSignatureHelp *help;
    SZrLspSignatureInformation *signatureInfo;

    if (state == ZR_NULL || labelText == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    help = (SZrLspSignatureHelp *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspSignatureHelp));
    if (help == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(help, 0, sizeof(*help));
    ZrCore_Array_Init(state, &help->signatures, sizeof(SZrLspSignatureInformation *), 1);
    help->activeSignature = 0;
    help->activeParameter = activeParameter;

    signatureInfo =
        (SZrLspSignatureInformation *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspSignatureInformation));
    if (signatureInfo == ZR_NULL) {
        ZrCore_Array_Free(state, &help->signatures);
        ZrCore_Memory_RawFree(state->global, help, sizeof(*help));
        return ZR_FALSE;
    }

    memset(signatureInfo, 0, sizeof(*signatureInfo));
    signatureInfo->label = ZrCore_String_Create(state, (TZrNativeString)labelText, strlen(labelText));
    ZrCore_Array_Init(state, &signatureInfo->parameters, sizeof(SZrLspParameterInformation *), params != ZR_NULL ? params->count : 1);

    if (params != ZR_NULL) {
        for (TZrSize index = 0; index < params->count; index++) {
            SZrLspParameterInformation *parameterInfo;
            SZrInferredType *resolvedType = ZR_NULL;
            EZrParameterPassingMode passingMode = ZR_PARAMETER_PASSING_MODE_VALUE;
            TZrChar buffer[ZR_LSP_TEXT_BUFFER_LENGTH];
            TZrSize offset = 0;

            if (resolvedSignature != ZR_NULL && index < resolvedSignature->parameterTypes.length) {
                resolvedType = (SZrInferredType *)ZrCore_Array_Get((SZrArray *)&resolvedSignature->parameterTypes, index);
            }
            if (resolvedSignature != ZR_NULL && index < resolvedSignature->parameterPassingModes.length) {
                EZrParameterPassingMode *mode =
                    (EZrParameterPassingMode *)ZrCore_Array_Get((SZrArray *)&resolvedSignature->parameterPassingModes, index);
                if (mode != ZR_NULL) {
                    passingMode = *mode;
                }
            }

            buffer[0] = '\0';
            signature_append_parameter_label(state,
                                             params->nodes[index],
                                             resolvedType,
                                             passingMode,
                                             buffer,
                                             sizeof(buffer),
                                             &offset);
            parameterInfo =
                (SZrLspParameterInformation *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrLspParameterInformation));
            if (parameterInfo == ZR_NULL) {
                continue;
            }

            parameterInfo->label = ZrCore_String_Create(state, buffer, strlen(buffer));
            parameterInfo->documentation = ZR_NULL;
            ZrCore_Array_Push(state, &signatureInfo->parameters, &parameterInfo);
        }
    }

    ZrCore_Array_Push(state, &help->signatures, &signatureInfo);
    *result = help;
    return ZR_TRUE;
}

static TZrBool signature_resolve_direct_base_type(SZrState *state,
                                                  SZrCompilerState *compilerState,
                                                  SZrAstNode *ownerTypeNode,
                                                  SZrInferredType *result) {
    SZrAstNode *inheritNode;

    if (result != ZR_NULL) {
        ZrParser_InferredType_Init(state, result, ZR_VALUE_TYPE_OBJECT);
    }
    if (state == ZR_NULL || compilerState == ZR_NULL || ownerTypeNode == ZR_NULL || result == ZR_NULL ||
        ownerTypeNode->type != ZR_AST_CLASS_DECLARATION ||
        ownerTypeNode->data.classDeclaration.inherits == ZR_NULL ||
        ownerTypeNode->data.classDeclaration.inherits->count == 0) {
        return ZR_FALSE;
    }

    inheritNode = ownerTypeNode->data.classDeclaration.inherits->nodes[0];
    if (inheritNode == ZR_NULL || inheritNode->type != ZR_AST_TYPE) {
        return ZR_FALSE;
    }

    return ZrParser_AstTypeToInferredType_Convert(compilerState, &inheritNode->data.type, result) &&
           result->typeName != ZR_NULL;
}

static const SZrTypeMemberInfo *signature_find_type_meta_member_recursive(SZrCompilerState *compilerState,
                                                                          SZrTypePrototypeInfo *prototype,
                                                                          EZrMetaType metaType,
                                                                          TZrUInt32 depth) {
    if (compilerState == ZR_NULL || prototype == ZR_NULL || depth > ZR_PARSER_RECURSIVE_MEMBER_LOOKUP_MAX_DEPTH) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < prototype->members.length; index++) {
        const SZrTypeMemberInfo *memberInfo =
            (const SZrTypeMemberInfo *)ZrCore_Array_Get(&prototype->members, index);
        if (memberInfo != ZR_NULL && memberInfo->isMetaMethod && memberInfo->metaType == metaType) {
            return memberInfo;
        }
    }

    for (TZrSize index = 0; index < prototype->inherits.length; index++) {
        SZrString **superNamePtr = (SZrString **)ZrCore_Array_Get(&prototype->inherits, index);
        SZrTypePrototypeInfo *superPrototype;
        const SZrTypeMemberInfo *resolvedMember;

        if (superNamePtr == ZR_NULL || *superNamePtr == ZR_NULL) {
            continue;
        }

        superPrototype = find_compiler_type_prototype_inference(compilerState, *superNamePtr);
        if (superPrototype == ZR_NULL || superPrototype == prototype) {
            continue;
        }

        resolvedMember =
            signature_find_type_meta_member_recursive(compilerState, superPrototype, metaType, depth + 1);
        if (resolvedMember != ZR_NULL) {
            return resolvedMember;
        }
    }

    return ZR_NULL;
}

static const SZrTypeMemberInfo *signature_find_type_meta_member(SZrCompilerState *compilerState,
                                                                SZrString *typeName,
                                                                EZrMetaType metaType) {
    SZrTypePrototypeInfo *prototype;

    if (compilerState == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    ensure_generic_instance_type_prototype(compilerState, typeName);
    prototype = find_compiler_type_prototype_inference(compilerState, typeName);
    if (prototype == ZR_NULL) {
        return ZR_NULL;
    }

    return signature_find_type_meta_member_recursive(compilerState, prototype, metaType, 0);
}

static TZrBool signature_resolve_super_constructor_help(SZrState *state,
                                                        SZrSemanticAnalyzer *analyzer,
                                                        SZrCompilerState *compilerState,
                                                        SZrLspCallContext *context,
                                                        SZrFilePosition position,
                                                        SZrLspSignatureHelp **result) {
    SZrInferredType baseType;
    const SZrTypeMemberInfo *constructorInfo;
    SZrTypeMemberInfo temporaryConstructorInfo;
    SZrFunctionCall superCall;
    SZrResolvedCallSignature resolvedSignature;
    TZrChar labelBuffer[ZR_LSP_LONG_TEXT_BUFFER_LENGTH];
    TZrBool resolved = ZR_FALSE;

    if (state == ZR_NULL || analyzer == ZR_NULL || compilerState == ZR_NULL || context == ZR_NULL ||
        result == ZR_NULL || context->kind != ZR_LSP_CALL_CONTEXT_SUPER_CONSTRUCTOR_CALL ||
        context->ownerTypeNode == ZR_NULL || context->metaFunctionNode == ZR_NULL ||
        context->metaFunctionNode->type != ZR_AST_CLASS_META_FUNCTION) {
        return ZR_FALSE;
    }

    if (!signature_resolve_direct_base_type(state, compilerState, context->ownerTypeNode, &baseType)) {
        return ZR_FALSE;
    }

    memset(&temporaryConstructorInfo, 0, sizeof(temporaryConstructorInfo));
    constructorInfo = signature_find_type_meta_member(compilerState, baseType.typeName, ZR_META_CONSTRUCTOR);
    if (constructorInfo == ZR_NULL &&
        !signature_prepare_ast_specialized_receiver_constructor(state,
                                                                compilerState,
                                                                analyzer->ast,
                                                                &baseType,
                                                                (SZrTypeMemberInfo **)&constructorInfo,
                                                                &temporaryConstructorInfo)) {
        ZrParser_InferredType_Free(state, &baseType);
        return ZR_FALSE;
    }

    memset(&superCall, 0, sizeof(superCall));
    superCall.args = context->argumentNodes;
    superCall.hasNamedArgs = ZR_FALSE;
    superCall.genericArguments = ZR_NULL;
    superCall.argNames = ZR_NULL;

    memset(&resolvedSignature, 0, sizeof(resolvedSignature));
    ZrParser_InferredType_Init(state, &resolvedSignature.returnType, ZR_VALUE_TYPE_OBJECT);
    ZrCore_Array_Construct(&resolvedSignature.parameterTypes);
    ZrCore_Array_Construct(&resolvedSignature.parameterPassingModes);

    if (resolve_generic_member_call_signature_detailed(compilerState,
                                                       constructorInfo,
                                                       &superCall,
                                                       &resolvedSignature,
                                                       ZR_NULL,
                                                       0) == ZR_GENERIC_CALL_RESOLVE_OK) {
        resolved = ZR_TRUE;
    } else {
        resolved = signature_resolve_member_call_signature_locally(state,
                                                                   analyzer,
                                                                   compilerState,
                                                                   (SZrTypeMemberInfo *)constructorInfo,
                                                                   &superCall,
                                                                   position.offset,
                                                                   &resolvedSignature);
    }

    if (!resolved ||
        !signature_build_label_from_method(state,
                                           (SZrTypeMemberInfo *)constructorInfo,
                                           &resolvedSignature,
                                           labelBuffer,
                                           sizeof(labelBuffer))) {
        free_resolved_call_signature(state, &resolvedSignature);
        signature_free_temporary_member_info(state, &temporaryConstructorInfo);
        ZrParser_InferredType_Free(state, &baseType);
        return ZR_FALSE;
    }

    if (!signature_populate_help_from_label(state,
                                            labelBuffer,
                                            signature_method_parameter_nodes(constructorInfo->declarationNode),
                                            &resolvedSignature,
                                            signature_active_parameter_index_for_arguments(context->argumentNodes,
                                                                                          position),
                                            result)) {
        free_resolved_call_signature(state, &resolvedSignature);
        signature_free_temporary_member_info(state, &temporaryConstructorInfo);
        ZrParser_InferredType_Free(state, &baseType);
        return ZR_FALSE;
    }

    free_resolved_call_signature(state, &resolvedSignature);
    signature_free_temporary_member_info(state, &temporaryConstructorInfo);
    ZrParser_InferredType_Free(state, &baseType);
    return ZR_TRUE;
}

static TZrBool signature_resolve_function_help(SZrState *state,
                                               SZrCompilerState *compilerState,
                                               SZrLspCallContext *context,
                                               SZrFilePosition position,
                                               SZrLspSignatureHelp **result) {
    SZrFunctionTypeInfo *resolvedFunction = ZR_NULL;
    SZrResolvedCallSignature resolvedSignature;
    SZrPrimaryExpression *primary;
    SZrFunctionCall *call;
    SZrAstNodeArray *signatureParams = ZR_NULL;
    TZrChar labelBuffer[ZR_LSP_LONG_TEXT_BUFFER_LENGTH];
    TZrBool resolved = ZR_FALSE;

    if (state == ZR_NULL || compilerState == ZR_NULL || context == ZR_NULL || context->primaryNode == ZR_NULL ||
        context->callNode == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    primary = &context->primaryNode->data.primaryExpression;
    call = &context->callNode->data.functionCall;
    if (primary->property == ZR_NULL || primary->property->type != ZR_AST_IDENTIFIER_LITERAL) {
        return ZR_FALSE;
    }

    memset(&resolvedSignature, 0, sizeof(resolvedSignature));
    ZrParser_InferredType_Init(state, &resolvedSignature.returnType, ZR_VALUE_TYPE_OBJECT);
    ZrCore_Array_Construct(&resolvedSignature.parameterTypes);
    ZrCore_Array_Construct(&resolvedSignature.parameterPassingModes);

    if (compilerState->typeEnv != ZR_NULL &&
        resolve_best_function_overload(compilerState,
                                       compilerState->typeEnv,
                                       primary->property->data.identifier.name,
                                       call,
                                       context->callNode->location,
                                       &resolvedFunction,
                                       &resolvedSignature)) {
        resolved = ZR_TRUE;
    } else if (compilerState->compileTimeTypeEnv != ZR_NULL &&
               resolve_best_function_overload(compilerState,
                                              compilerState->compileTimeTypeEnv,
                                              primary->property->data.identifier.name,
                                              call,
                                              context->callNode->location,
                                              &resolvedFunction,
                                              &resolvedSignature)) {
        resolved = ZR_TRUE;
    }

    if (!resolved || resolvedFunction == ZR_NULL ||
        !signature_build_label_from_function(state, resolvedFunction, &resolvedSignature, labelBuffer, sizeof(labelBuffer))) {
        free_resolved_call_signature(state, &resolvedSignature);
        return ZR_FALSE;
    }

    if (resolvedFunction->declarationNode != ZR_NULL) {
        switch (resolvedFunction->declarationNode->type) {
            case ZR_AST_FUNCTION_DECLARATION:
                signatureParams = resolvedFunction->declarationNode->data.functionDeclaration.params;
                break;
            case ZR_AST_EXTERN_FUNCTION_DECLARATION:
                signatureParams = resolvedFunction->declarationNode->data.externFunctionDeclaration.params;
                break;
            default:
                break;
        }
    }

    if (!signature_populate_help_from_label(state,
                                            labelBuffer,
                                            signatureParams,
                                            &resolvedSignature,
                                            signature_active_parameter_index(call, position),
                                            result)) {
        free_resolved_call_signature(state, &resolvedSignature);
        return ZR_FALSE;
    }

    free_resolved_call_signature(state, &resolvedSignature);
    return ZR_TRUE;
}

static TZrBool signature_resolve_method_help(SZrState *state,
                                             SZrSemanticAnalyzer *analyzer,
                                             SZrCompilerState *compilerState,
                                             SZrLspCallContext *context,
                                             SZrFilePosition position,
                                             SZrLspSignatureHelp **result) {
    SZrAstNode *tempNode;
    SZrPrimaryExpression tempPrimary;
    SZrAstNodeArray tempMembers;
    SZrInferredType receiverType;
    SZrTypeMemberInfo temporaryMemberInfo;
    SZrTypeMemberInfo *memberInfo;
    SZrMemberExpression *memberExpr;
    SZrResolvedCallSignature resolvedSignature;
    TZrChar labelBuffer[ZR_LSP_LONG_TEXT_BUFFER_LENGTH];

    if (state == ZR_NULL || analyzer == ZR_NULL || compilerState == ZR_NULL || context == ZR_NULL ||
        context->primaryNode == ZR_NULL || context->callNode == ZR_NULL || context->callMemberIndex == 0 ||
        result == ZR_NULL) {
        return ZR_FALSE;
    }

    memberExpr = &context->primaryNode->data.primaryExpression.members->nodes[context->callMemberIndex - 1]->data.memberExpression;
    if (memberExpr->computed || memberExpr->property == ZR_NULL ||
        memberExpr->property->type != ZR_AST_IDENTIFIER_LITERAL) {
        return ZR_FALSE;
    }

    tempNode = (SZrAstNode *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrAstNode));
    if (tempNode == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrParser_InferredType_Init(state, &receiverType, ZR_VALUE_TYPE_OBJECT);
    memset(&temporaryMemberInfo, 0, sizeof(temporaryMemberInfo));
    memset(&tempPrimary, 0, sizeof(tempPrimary));
    memset(&tempMembers, 0, sizeof(tempMembers));
    if (!signature_build_receiver_primary_prefix(context->primaryNode,
                                                 context->callMemberIndex - 1,
                                                 tempNode,
                                                 &tempPrimary,
                                                 &tempMembers) ||
        !ZrParser_ExpressionType_Infer(compilerState, tempNode, &receiverType)) {
        ZrParser_InferredType_Free(state, &receiverType);
        ZrCore_Memory_RawFree(state->global, tempNode, sizeof(SZrAstNode));
        return ZR_FALSE;
    }

    if (receiverType.typeName == ZR_NULL &&
        context->callMemberIndex == 1 &&
        context->primaryNode->data.primaryExpression.property != ZR_NULL &&
        context->primaryNode->data.primaryExpression.property->type == ZR_AST_IDENTIFIER_LITERAL) {
        SZrString *receiverName = context->primaryNode->data.primaryExpression.property->data.identifier.name;
        const TZrChar *receiverNameText = signature_string_native(receiverName);
        TZrSize receiverNameLength = strlen(receiverNameText);
        TZrSize bestReceiverOffset = 0;
        TZrBool foundReceiverType = ZR_FALSE;

        signature_find_named_value_type_recursive(compilerState,
                                                  analyzer->ast,
                                                  receiverNameText,
                                                  receiverNameLength,
                                                  context->callNode->location.start.offset,
                                                  &receiverType,
                                                  &bestReceiverOffset,
                                                  &foundReceiverType);
    }

    if (receiverType.typeName == ZR_NULL) {
        ZrParser_InferredType_Free(state, &receiverType);
        ZrCore_Memory_RawFree(state->global, tempNode, sizeof(SZrAstNode));
        return ZR_FALSE;
    }

    if ((!signature_prepare_specialized_receiver_member(state,
                                                        compilerState,
                                                        &receiverType,
                                                        memberExpr->property->data.identifier.name,
                                                        &memberInfo,
                                                        &temporaryMemberInfo) ||
         memberInfo == ZR_NULL) &&
        (!signature_prepare_ast_specialized_receiver_member(state,
                                                            compilerState,
                                                            analyzer->ast,
                                                            &receiverType,
                                                            memberExpr->property->data.identifier.name,
                                                            &memberInfo,
                                                            &temporaryMemberInfo) ||
         memberInfo == ZR_NULL)) {
        signature_free_temporary_member_info(state, &temporaryMemberInfo);
        ZrParser_InferredType_Free(state, &receiverType);
        ZrCore_Memory_RawFree(state->global, tempNode, sizeof(SZrAstNode));
        return ZR_FALSE;
    }

    memset(&resolvedSignature, 0, sizeof(resolvedSignature));
    ZrParser_InferredType_Init(state, &resolvedSignature.returnType, ZR_VALUE_TYPE_OBJECT);
    ZrCore_Array_Construct(&resolvedSignature.parameterTypes);
    ZrCore_Array_Construct(&resolvedSignature.parameterPassingModes);

    {
        EZrGenericCallResolveStatus genericStatus =
            resolve_generic_member_call_signature_detailed(compilerState,
                                                           memberInfo,
                                                           &context->callNode->data.functionCall,
                                                           &resolvedSignature,
                                                           ZR_NULL,
                                                           0);
        TZrBool localResolved = ZR_FALSE;

        if (genericStatus != ZR_GENERIC_CALL_RESOLVE_OK) {
            localResolved = signature_resolve_member_call_signature_locally(state,
                                                                            analyzer,
                                                                            compilerState,
                                                                            memberInfo,
                                                                            &context->callNode->data.functionCall,
                                                                            context->callNode->location.start.offset,
                                                                            &resolvedSignature);
        }

        if ((genericStatus != ZR_GENERIC_CALL_RESOLVE_OK && !localResolved) ||
            !signature_build_label_from_method(state, memberInfo, &resolvedSignature, labelBuffer, sizeof(labelBuffer))) {
            free_resolved_call_signature(state, &resolvedSignature);
            signature_free_temporary_member_info(state, &temporaryMemberInfo);
            ZrParser_InferredType_Free(state, &receiverType);
            ZrCore_Memory_RawFree(state->global, tempNode, sizeof(SZrAstNode));
            return ZR_FALSE;
        }
    }

    if (!signature_populate_help_from_label(state,
                                            labelBuffer,
                                            signature_method_parameter_nodes(memberInfo->declarationNode),
                                            &resolvedSignature,
                                            signature_active_parameter_index(&context->callNode->data.functionCall, position),
                                            result)) {
        free_resolved_call_signature(state, &resolvedSignature);
        signature_free_temporary_member_info(state, &temporaryMemberInfo);
        ZrParser_InferredType_Free(state, &receiverType);
        ZrCore_Memory_RawFree(state->global, tempNode, sizeof(SZrAstNode));
        return ZR_FALSE;
    }

    free_resolved_call_signature(state, &resolvedSignature);
    signature_free_temporary_member_info(state, &temporaryMemberInfo);
    ZrParser_InferredType_Free(state, &receiverType);
    ZrCore_Memory_RawFree(state->global, tempNode, sizeof(SZrAstNode));
    return ZR_TRUE;
}

TZrBool ZrLanguageServer_Lsp_GetSignatureHelp(SZrState *state,
                                              SZrLspContext *context,
                                              SZrString *uri,
                                              SZrLspPosition position,
                                              SZrLspSignatureHelp **result) {
    SZrSemanticAnalyzer *analyzer;
    SZrFileVersion *fileVersion;
    SZrFilePosition filePosition;
    SZrFileRange fileRange;
    SZrLspCallContext callContext;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    *result = ZR_NULL;
    analyzer = ZrLanguageServer_Lsp_GetOrCreateAnalyzer(state, context, uri);
    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    if (analyzer == ZR_NULL || analyzer->ast == ZR_NULL || analyzer->compilerState == ZR_NULL || fileVersion == ZR_NULL) {
        return ZR_FALSE;
    }

    filePosition = ZrLanguageServer_Lsp_GetDocumentFilePosition(context, uri, position);
    fileRange = ZrParser_FileRange_Create(filePosition, filePosition, uri);
    memset(&callContext, 0, sizeof(callContext));
    signature_find_call_context_in_node(analyzer->ast, fileRange, &callContext);
    if (callContext.kind == ZR_LSP_CALL_CONTEXT_NONE) {
        return ZR_FALSE;
    }

    if (callContext.kind == ZR_LSP_CALL_CONTEXT_SUPER_CONSTRUCTOR_CALL) {
        return signature_resolve_super_constructor_help(state,
                                                        analyzer,
                                                        analyzer->compilerState,
                                                        &callContext,
                                                        filePosition,
                                                        result) &&
               *result != ZR_NULL;
    }

    if (callContext.callNode == ZR_NULL || callContext.primaryNode == ZR_NULL) {
        return ZR_FALSE;
    }

    if (callContext.callMemberIndex > 0 &&
        callContext.primaryNode->data.primaryExpression.members != ZR_NULL &&
        callContext.primaryNode->data.primaryExpression.members->nodes[callContext.callMemberIndex - 1] != ZR_NULL &&
        callContext.primaryNode->data.primaryExpression.members->nodes[callContext.callMemberIndex - 1]->type ==
            ZR_AST_MEMBER_EXPRESSION &&
        signature_resolve_method_help(state, analyzer, analyzer->compilerState, &callContext, filePosition, result)) {
        return *result != ZR_NULL;
    }

    return signature_resolve_function_help(state, analyzer->compilerState, &callContext, filePosition, result);
}

void ZrLanguageServer_LspSignatureHelp_Free(SZrState *state, SZrLspSignatureHelp *help) {
    if (state == ZR_NULL || help == ZR_NULL) {
        return;
    }

    for (TZrSize signatureIndex = 0; signatureIndex < help->signatures.length; signatureIndex++) {
        SZrLspSignatureInformation **signaturePtr =
            (SZrLspSignatureInformation **)ZrCore_Array_Get(&help->signatures, signatureIndex);
        if (signaturePtr != ZR_NULL && *signaturePtr != ZR_NULL) {
            SZrLspSignatureInformation *signature = *signaturePtr;
            for (TZrSize parameterIndex = 0; parameterIndex < signature->parameters.length; parameterIndex++) {
                SZrLspParameterInformation **parameterPtr =
                    (SZrLspParameterInformation **)ZrCore_Array_Get(&signature->parameters, parameterIndex);
                if (parameterPtr != ZR_NULL && *parameterPtr != ZR_NULL) {
                    ZrCore_Memory_RawFree(state->global, *parameterPtr, sizeof(SZrLspParameterInformation));
                }
            }
            ZrCore_Array_Free(state, &signature->parameters);
            ZrCore_Memory_RawFree(state->global, signature, sizeof(SZrLspSignatureInformation));
        }
    }

    ZrCore_Array_Free(state, &help->signatures);
    ZrCore_Memory_RawFree(state->global, help, sizeof(SZrLspSignatureHelp));
}
