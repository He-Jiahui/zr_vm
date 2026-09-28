#include "interface/lsp_interface_internal.h"
#include "lsp_canonical_signature_help.h"
#include "lsp_external_callable_signature_help.h"
#include "lsp_signature_help_internal.h"
#include "semantic/semantic_analyzer_internal.h"

#include "zr_vm_parser/semantic_query.h"
#include "type_inference_internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* 泛型绑定查询必须区分第零个参数与未命中，供接收者和实参两条专化路径共用。 */
#define ZR_LSP_SIGNATURE_BINDING_INDEX_NONE ((TZrInt32)-1)
/* 本文件的构造路径与规范、外部 callable 路径共用同一结果所有权边界。 */
#define signature_populate_help_from_label ZrLanguageServer_LspSignatureHelp_PopulateFromLabel

/* 光标匹配阶段只分类调用形态；具体签名来源在顶层入口按形态决定。 */
typedef enum EZrLspCallContextKind {
    ZR_LSP_CALL_CONTEXT_NONE = 0,
    ZR_LSP_CALL_CONTEXT_FUNCTION_CALL,
    ZR_LSP_CALL_CONTEXT_CONSTRUCT_CALL,
    ZR_LSP_CALL_CONTEXT_SUPER_CONSTRUCTOR_CALL
} EZrLspCallContextKind;

/* 保存 AST 中最内层的调用候选；所有节点均借用当前 analyzer 的 AST。span 只用于候选排序。 */
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

/* 本地泛型求解的临时槽；parameterInfo 借用成员元数据，inferredType 由槽独占并统一释放。 */
typedef struct SZrLspGenericBinding {
    const SZrTypeGenericParameterInfo *parameterInfo;
    TZrBool isBound;
    SZrInferredType inferredType;
} SZrLspGenericBinding;

/* 只把普通成员名交给外部 callable 元数据查询；计算属性没有稳定的声明名范围。 */
static TZrBool signature_external_callable_callee_range(
        const SZrLspCallContext *context,
        SZrFileRange *range) {
    SZrAstNode *memberNode;

    if (context == ZR_NULL || range == ZR_NULL ||
        context->primaryNode == ZR_NULL ||
        context->primaryNode->type != ZR_AST_PRIMARY_EXPRESSION ||
        context->primaryNode->data.primaryExpression.members == ZR_NULL ||
        context->callMemberIndex == 0U ||
        context->callMemberIndex >
                context->primaryNode->data.primaryExpression.members->count) {
        return ZR_FALSE;
    }
    memberNode = context->primaryNode->data.primaryExpression.members
                         ->nodes[context->callMemberIndex - 1U];
    if (memberNode == ZR_NULL ||
        memberNode->type != ZR_AST_MEMBER_EXPRESSION ||
        memberNode->data.memberExpression.computed ||
        memberNode->data.memberExpression.property == ZR_NULL ||
        memberNode->data.memberExpression.property->type !=
                ZR_AST_IDENTIFIER_LITERAL) {
        return ZR_FALSE;
    }
    *range = memberNode->data.memberExpression.property->location;
    return ZR_TRUE;
}

typedef struct SZrSignatureTypeResolutionContext SZrSignatureTypeResolutionContext;

static TZrBool signature_convert_ast_type_with_context(
        SZrCompilerState *compilerState,
        SZrType *sourceType,
        const SZrSignatureTypeResolutionContext *context,
        SZrInferredType *outType);

static TZrBool signature_type_prototype_matches_name(const SZrTypePrototypeInfo *prototype, SZrString *typeName) {
    return prototype != ZR_NULL && prototype->name != ZR_NULL && typeName != ZR_NULL &&
           ZrCore_String_Equal(prototype->name, typeName);
}

static const TZrChar *signature_exact_type_failure_text(void) {
    return "cannot infer exact type";
}

/* 标签投影拒绝没有类型名和元素信息的宽泛 object，避免把未知值展示成已解析类型。 */
static TZrBool signature_inferred_type_is_precise(const SZrInferredType *typeInfo) {
    return typeInfo != ZR_NULL &&
           !(typeInfo->baseType == ZR_VALUE_TYPE_OBJECT &&
             typeInfo->typeName == ZR_NULL &&
             (!typeInfo->elementTypes.isValid || typeInfo->elementTypes.length == 0));
}

/* AST 与客户端光标只在双方 source 已知且不同时拒绝；四端偏移均有效才用偏移，否则用行列。 */
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

/* 语义引用与调用目标的已知 source 不同时拒绝；任一端有偏移便比较偏移，否则比较行列。 */
static TZrBool signature_ranges_equal(SZrFileRange left, SZrFileRange right) {
    if (!ZrLanguageServer_Lsp_StringsEqual(left.source, right.source) &&
        left.source != ZR_NULL && right.source != ZR_NULL) {
        return ZR_FALSE;
    }
    if (left.start.offset > 0U || left.end.offset > 0U ||
        right.start.offset > 0U || right.end.offset > 0U) {
        return left.start.offset == right.start.offset &&
               left.end.offset == right.end.offset;
    }
    return left.start.line == right.start.line &&
           left.start.column == right.start.column &&
           left.end.line == right.end.line &&
           left.end.column == right.end.column;
}

/* 为嵌套调用选择构造可比较的范围；参数和泛型实参可能比调用节点的位置更可信。 */
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

/* 同位置重叠候选按跨度选最内层；无偏移时行列打包只是排序分数。 */
/* TODO: 打包基数小于可出现的长行列号时，跨行候选排序可能失真；与 reference_tracker 的回退算法一同核查。 */
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

/* 同时接受调用整体或实参内部的光标，使嵌套调用仍可抢占外层候选。 */
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

/* 多个标签投影函数共用固定缓冲；offset 表示当前有效前缀而非要求的总长度。 */
/* TODO: 截断后调用方仍会把标签当作成功结果；核查超长标识符、类型名的协议输出并决定是否传递溢出状态。 */
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

/* AST/元数据借用 VM 字符串；此处只借出临时原生视图供同步格式化，不转移所有权。 */
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
        case ZR_PARAMETER_PASSING_MODE_IN: return "in";
        case ZR_PARAMETER_PASSING_MODE_OUT: return "out";
        case ZR_PARAMETER_PASSING_MODE_REF: return "ref";
        case ZR_PARAMETER_PASSING_MODE_VALUE:
        default:
            return ZR_NULL;
    }
}

/* 元数据不含已解析类型时，仍由声明 AST 给出可识别的参数与泛型约束文本。 */
static void signature_append_ast_type(SZrType *typeInfo,
                                      TZrChar *buffer,
                                      TZrSize bufferSize,
                                      TZrSize *offset) {
    if (typeInfo == ZR_NULL || typeInfo->name == ZR_NULL) {
        signature_buffer_append(buffer, bufferSize, offset, "%s", signature_exact_type_failure_text());
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
            signature_buffer_append(buffer, bufferSize, offset, "%s", signature_exact_type_failure_text());
            break;
    }

    for (TZrInt32 dimension = 0; dimension < typeInfo->dimensions; dimension++) {
        signature_buffer_append(buffer, bufferSize, offset, "[]");
    }
}

/* 专化结果优先于声明文本；不可精确推断时明确显示未知而不伪造 object。 */
static void signature_format_type(SZrState *state,
                                  const SZrInferredType *typeInfo,
                                  TZrChar *buffer,
                                  TZrSize bufferSize) {
    const TZrChar *text;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }

    buffer[0] = '\0';
    if (state == ZR_NULL || !signature_inferred_type_is_precise(typeInfo)) {
        snprintf(buffer, bufferSize, "%s", signature_exact_type_failure_text());
        return;
    }

    text = ZrParser_TypeNameString_Get(state, typeInfo, buffer, bufferSize);
    if (text == ZR_NULL || text[0] == '\0') {
        snprintf(buffer, bufferSize, "%s", signature_exact_type_failure_text());
    }
}

/* 泛型声明标签保留 const 整数参数和方差，供构造器签名与语言声明对齐。 */
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

/* 仅在声明 AST 仍可用时投影泛型参数，元数据签名不凭空补造声明。 */
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

/* 构造器签名末尾携带声明中的泛型约束，以免标签只展示参数而掩盖调用限制。 */
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
                                        typeBuffer[0] != '\0' ? typeBuffer : signature_exact_type_failure_text());
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

/* AST 参数允许用已求得的类型覆盖原始类型；模式与名称仍来自声明。 */
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
                            typeBuffer[0] != '\0' ? typeBuffer : signature_exact_type_failure_text());
}

/* 无声明 AST 的类型成员仍可用成员表中的名称、模式和类型生成参数标签。 */
static void signature_append_parameter_label_from_metadata(SZrState *state,
                                                           const SZrTypeMemberInfo *memberInfo,
                                                           TZrSize index,
                                                           const SZrInferredType *resolvedType,
                                                           EZrParameterPassingMode passingMode,
                                                           TZrChar *buffer,
                                                           TZrSize bufferSize,
                                                           TZrSize *offset) {
    const SZrInferredType *typeToRender = resolvedType;
    SZrString *parameterName = ZR_NULL;
    const TZrChar *passingModeText;
    TZrChar typeBuffer[ZR_LSP_TYPE_BUFFER_LENGTH];

    if (memberInfo == ZR_NULL || buffer == ZR_NULL || offset == ZR_NULL) {
        return;
    }

    if (parameterName == ZR_NULL && index < memberInfo->parameterNames.length) {
        SZrString **namePtr = (SZrString **)ZrCore_Array_Get((SZrArray *)&memberInfo->parameterNames, index);
        if (namePtr != ZR_NULL) {
            parameterName = *namePtr;
        }
    }
    if (typeToRender == ZR_NULL && index < memberInfo->parameterTypes.length) {
        typeToRender = (const SZrInferredType *)ZrCore_Array_Get((SZrArray *)&memberInfo->parameterTypes, index);
    }

    passingModeText = signature_parameter_passing_mode_text(passingMode);
    if (passingModeText != ZR_NULL) {
        signature_buffer_append(buffer, bufferSize, offset, "%s ", passingModeText);
    }

    if (typeToRender != ZR_NULL) {
        signature_format_type(state, typeToRender, typeBuffer, sizeof(typeBuffer));
    } else {
        typeBuffer[0] = '\0';
    }

    signature_buffer_append(buffer,
                            bufferSize,
                            offset,
                            "%s: %s",
                            signature_string_native(parameterName),
                            typeBuffer[0] != '\0' ? typeBuffer : signature_exact_type_failure_text());
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

/* 普通方法和元函数共用构造器标签路径，参数节点均借用声明 AST。 */
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

/* 本地构造器求解完成后才投影标签；声明 AST 与成员元数据两种来源共享输出格式。 */
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
    } else {
        TZrSize parameterCount = memberInfo->parameterCount;
        if (memberInfo->parameterTypes.length > parameterCount) {
            parameterCount = memberInfo->parameterTypes.length;
        }
        if (memberInfo->parameterNames.length > parameterCount) {
            parameterCount = memberInfo->parameterNames.length;
        }

        for (TZrSize index = 0; index < parameterCount; index++) {
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
            } else if (index < memberInfo->parameterPassingModes.length) {
                EZrParameterPassingMode *mode =
                    (EZrParameterPassingMode *)ZrCore_Array_Get((SZrArray *)&memberInfo->parameterPassingModes, index);
                if (mode != ZR_NULL) {
                    passingMode = *mode;
                }
            }

            signature_append_parameter_label_from_metadata(state,
                                                           memberInfo,
                                                           index,
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

/* 普通函数调用把光标映射到当前实参，结果传给规范与外部 callable 投影。 */
/* TODO: 光标越过最后一个完整实参时这里保留该索引，而构造器路径会前移一位；核查逗号后的签名高亮语义。 */
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

/* 构造器和 super 调用在已结束实参之后预选下一参数；空列表默认首参数。 */
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

/* AST 遍历发现多个重叠调用时只保留最窄候选，避免外层实参吞掉内层签名。 */
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

/* 根据 primary 链中的调用序号定位真实被调用表达式，供语义事实核对声明身份。 */
static const SZrAstNode *signature_context_call_callee(
        const SZrLspCallContext *context) {
    const SZrAstNode *memberNode;

    if (context == ZR_NULL ||
        context->kind != ZR_LSP_CALL_CONTEXT_FUNCTION_CALL ||
        context->primaryNode == ZR_NULL ||
        context->primaryNode->type != ZR_AST_PRIMARY_EXPRESSION) {
        return ZR_NULL;
    }
    if (context->callMemberIndex == 0U) {
        return context->primaryNode->data.primaryExpression.property;
    }
    if (context->primaryNode->data.primaryExpression.members == ZR_NULL ||
        context->callMemberIndex >
                context->primaryNode->data.primaryExpression.members->count) {
        return ZR_NULL;
    }
    memberNode = context->primaryNode->data.primaryExpression.members->nodes[
            context->callMemberIndex - 1U];
    return memberNode != ZR_NULL && memberNode->type == ZR_AST_MEMBER_EXPRESSION
                   ? memberNode->data.memberExpression.property
                   : ZR_NULL;
}

/* 源码声明由 parser 的规范调用事实解析；本地元数据求解不能替代它的身份和类型规则。 */
static TZrBool signature_declaration_requires_canonical_source_call(
        const SZrAstNode *node) {
    return node != ZR_NULL &&
           (node->type == ZR_AST_FUNCTION_DECLARATION ||
            node->type == ZR_AST_LAMBDA_EXPRESSION ||
            node->type == ZR_AST_CLASS_METHOD ||
            node->type == ZR_AST_STRUCT_METHOD ||
            node->type == ZR_AST_INTERFACE_METHOD_SIGNATURE ||
            node->type == ZR_AST_CLASS_META_FUNCTION ||
            node->type == ZR_AST_STRUCT_META_FUNCTION);
}

/* 在构造器回退前检查调用引用和声明节点，避免失败的规范查询退化成近似元数据签名。 */
static TZrBool signature_context_requires_canonical_source_call(
        SZrSemanticAnalyzer *analyzer,
        const SZrLspCallContext *context) {
    const SZrAstNode *callee;
    const SZrAstNode *expressionNode;
    const SZrSemanticReferenceFact *declaration;
    const SZrSemanticExpressionFact *expression;
    const SZrSemanticReferenceFact *callReference;

    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL ||
        context == ZR_NULL) {
        return ZR_FALSE;
    }
    expressionNode = context->kind == ZR_LSP_CALL_CONTEXT_CONSTRUCT_CALL
                             ? context->callNode
                             : context->kind == ZR_LSP_CALL_CONTEXT_SUPER_CONSTRUCTOR_CALL
                                     ? context->metaFunctionNode
                                     : context->primaryNode;
    expression = ZrParser_SemanticFacts_FindExpressionByNode(
            analyzer->semanticContext, expressionNode);
    if (expression != ZR_NULL) {
        callReference = ZrParser_SemanticFacts_FindReferenceAtPositionByKind(
                analyzer->semanticContext,
                expression->callTargetRange,
                ZR_SEMANTIC_REFERENCE_CALL);
        if (callReference != ZR_NULL &&
            signature_ranges_equal(
                    callReference->range, expression->callTargetRange)) {
            declaration = ZrParser_SemanticQuery_DeclarationOf(
                    analyzer->semanticContext,
                    callReference->symbolId,
                    ZR_NULL);
            if (declaration != ZR_NULL &&
                signature_declaration_requires_canonical_source_call(
                        declaration->node)) {
                return ZR_TRUE;
            }
        }
    }
    callee = signature_context_call_callee(context);
    if (callee == ZR_NULL || callee->type != ZR_AST_IDENTIFIER_LITERAL) {
        return ZR_FALSE;
    }
    declaration = ZrParser_SemanticQuery_DefinitionOf(
            analyzer->semanticContext, callee->location, ZR_NULL);
    if (declaration == ZR_NULL || declaration->node == ZR_NULL) {
        return ZR_FALSE;
    }
    return signature_declaration_requires_canonical_source_call(
            declaration->node);
}

/* 已有本文件可解析的非成员调用事实时优先返回规范结果，不再尝试外部元数据解释。 */
static TZrBool signature_context_has_local_canonical_call(
        SZrSemanticAnalyzer *analyzer,
        const SZrLspCallContext *context) {
    const SZrAstNode *callee;
    SZrFileRange calleeRange;
    SZrParserSemanticCallQuery query;

    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL ||
        context == ZR_NULL || context->kind != ZR_LSP_CALL_CONTEXT_FUNCTION_CALL ||
        context->callNode == ZR_NULL ||
        !analyzer->semanticContext->expressionFacts.isValid) {
        return ZR_FALSE;
    }
    memset(&calleeRange, 0, sizeof(calleeRange));
    if (!signature_external_callable_callee_range(context, &calleeRange)) {
        callee = signature_context_call_callee(context);
        if (callee == ZR_NULL) {
            return ZR_FALSE;
        }
        calleeRange = callee->location;
    }
    memset(&query, 0, sizeof(query));
    return ZrParser_SemanticQuery_CallAt(
                   analyzer->semanticContext, calleeRange, ZR_NULL, &query) &&
           query.expression != ZR_NULL &&
           !query.expression->isMemberCall && query.reference != ZR_NULL &&
           signature_ranges_equal(query.reference->range, calleeRange);
}

static TZrBool signature_construct_node_is_supported(const SZrAstNode *node) {
    return node != ZR_NULL &&
           (node->type == ZR_AST_CONSTRUCT_EXPRESSION ||
            node->type == ZR_AST_STRUCT_INIT_EXPRESSION);
}

static SZrAstNodeArray *signature_construct_node_arguments(SZrAstNode *node) {
    if (!signature_construct_node_is_supported(node)) {
        return ZR_NULL;
    }
    return node->type == ZR_AST_STRUCT_INIT_EXPRESSION
                   ? node->data.structInitExpression.args
                   : node->data.constructExpression.args;
}

static TZrBool signature_construct_node_has_named_arguments(const SZrAstNode *node) {
    return node != ZR_NULL && node->type == ZR_AST_STRUCT_INIT_EXPRESSION &&
           node->data.structInitExpression.hasNamedArgs;
}

static SZrArray *signature_construct_node_argument_names(SZrAstNode *node) {
    return node != ZR_NULL && node->type == ZR_AST_STRUCT_INIT_EXPRESSION
                   ? node->data.structInitExpression.argNames
                   : ZR_NULL;
}

static SZrArray *signature_construct_node_argument_markers(SZrAstNode *node) {
    return node != ZR_NULL && node->type == ZR_AST_STRUCT_INIT_EXPRESSION
                   ? node->data.structInitExpression.argumentMarkers
                   : ZR_NULL;
}

/* struct 初始化只信任精确语义事实；其类型身份不能靠旧式构造表达式推断。 */
static TZrBool signature_copy_exact_construct_type(SZrState *state,
                                                   SZrSemanticAnalyzer *analyzer,
                                                   SZrAstNode *constructNode,
                                                   SZrInferredType *outType) {
    const SZrSemanticExpressionFact *fact;

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL ||
        !signature_construct_node_is_supported(constructNode) || outType == ZR_NULL) {
        return ZR_FALSE;
    }
    fact = ZrParser_SemanticFacts_FindExpressionByNode(analyzer->semanticContext, constructNode);
    if (fact == ZR_NULL || fact->exactness != ZR_SEMANTIC_FACT_EXACT ||
        fact->typeId == ZR_SEMANTIC_ID_INVALID ||
        !ZrLanguageServer_SemanticAnalyzer_IsPreciseInferredType(&fact->inferredType)) {
        return ZR_FALSE;
    }
    ZrParser_InferredType_Copy(state, outType, &fact->inferredType);
    return outType->typeName != ZR_NULL;
}

/* 两种构造 AST 共享光标范围规则，末实参位置可修正节点末尾的不完整范围。 */
static SZrFileRange signature_construct_call_context_range(SZrAstNode *constructNode) {
    SZrAstNodeArray *arguments;
    SZrFileRange range;

    memset(&range, 0, sizeof(range));
    if (!signature_construct_node_is_supported(constructNode)) {
        return range;
    }

    arguments = signature_construct_node_arguments(constructNode);
    range = constructNode->location;
    if (arguments != ZR_NULL && arguments->count > 0) {
        SZrAstNode *lastArg = arguments->nodes[arguments->count - 1];
        if (lastArg != ZR_NULL) {
            range.end = lastArg->location.end;
        }
    }

    return range;
}

/* 允许光标落在构造整体或实参节点中，供统一最内层调用筛选。 */
static TZrBool signature_construct_call_matches_position(SZrAstNode *constructNode, SZrFileRange position) {
    SZrAstNodeArray *arguments;
    SZrFileRange constructRange;

    if (!signature_construct_node_is_supported(constructNode)) {
        return ZR_FALSE;
    }

    arguments = signature_construct_node_arguments(constructNode);
    constructRange = signature_construct_call_context_range(constructNode);
    if (signature_range_contains_position(constructRange, position)) {
        return ZR_TRUE;
    }

    if (arguments != ZR_NULL) {
        for (TZrSize index = 0; index < arguments->count; index++) {
            SZrAstNode *argNode = arguments->nodes[index];
            if (argNode != ZR_NULL && signature_range_contains_position(argNode->location, position)) {
                return ZR_TRUE;
            }
        }
    }

    return ZR_FALSE;
}

/* struct 初始化与旧式 construct 都归为构造器候选，但后续签名来源仍由入口判断。 */
static void signature_update_best_construct_context(SZrLspCallContext *best, SZrAstNode *constructNode) {
    TZrSize span;
    SZrAstNode *primaryNode = ZR_NULL;

    if (best == ZR_NULL || !signature_construct_node_is_supported(constructNode)) {
        return;
    }

    if (constructNode->type == ZR_AST_CONSTRUCT_EXPRESSION &&
        constructNode->data.constructExpression.target != ZR_NULL &&
        constructNode->data.constructExpression.target->type == ZR_AST_PRIMARY_EXPRESSION) {
        primaryNode = constructNode->data.constructExpression.target;
    }

    span = signature_range_span(signature_construct_call_context_range(constructNode));
    if (best->kind == ZR_LSP_CALL_CONTEXT_NONE || span <= best->span) {
        best->kind = ZR_LSP_CALL_CONTEXT_CONSTRUCT_CALL;
        best->ownerTypeNode = ZR_NULL;
        best->primaryNode = primaryNode;
        best->callNode = constructNode;
        best->metaFunctionNode = ZR_NULL;
        best->argumentNodes = signature_construct_node_arguments(constructNode);
        best->callMemberIndex = 0;
        best->span = span;
    }
}

static SZrFileRange signature_super_call_context_range(SZrAstNode *metaFunctionNode) {
    SZrFileRange range;

    memset(&range, 0, sizeof(range));
    if (metaFunctionNode == ZR_NULL ||
        metaFunctionNode->type != ZR_AST_CLASS_META_FUNCTION) {
        return range;
    }
    return metaFunctionNode->data.classMetaFunction.superCallRange;
}

/* 只有声明中确有 super 调用且光标命中调用或其参数，才生成父类构造器候选。 */
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

/* super 候选保留所属 class 与元函数，规范查询需要两者对应的语义事实。 */
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

/* 统一递归入口扫描表达式数组；元素和结果节点都只在当前 analyzer 的 AST 内有效。 */
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

/* primary 链中的每个调用成员独立参与候选比较，成员序号随后用于定位 callee。 */
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

/* 从文档 AST 追到调用、构造与 super 子树，并按跨度保留光标处最内层候选。 */
/* BUG: parser 会生成 FOR/FOREACH 节点，但本 switch 未下钻其条件、迭代式或循环体；其中的调用会失去签名帮助。 */
/* TODO: 同样核查 using、try/catch 等其余未覆盖的 AST 节点，以及恢复遍历后的嵌套候选优先级。 */
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
            if (signature_construct_call_matches_position(node, position)) {
                signature_update_best_construct_context(best, node);
            }
            signature_find_call_context_in_node(node->data.constructExpression.target, position, best);
            signature_find_call_context_in_array(node->data.constructExpression.args, position, best);
            break;

        case ZR_AST_STRUCT_INIT_EXPRESSION:
            if (signature_construct_call_matches_position(node, position)) {
                signature_update_best_construct_context(best, node);
            }
            signature_find_call_context_in_array(node->data.structInitExpression.args, position, best);
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

static SZrString *signature_extract_identifier_name(SZrAstNode *node) {
    if (node != ZR_NULL && node->type == ZR_AST_IDENTIFIER_LITERAL) {
        return node->data.identifier.name;
    }

    return ZR_NULL;
}

/* 仅在普通表达式推断缺少类型名时，沿声明 AST 寻找光标前同名值的候选类型。 */
/* TODO: 目前只按声明偏移选最近同名值，没有核对词法作用域；用跨函数同名局部变量测试回退是否误绑定。 */
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
                signature_convert_ast_type_with_context(compilerState, varDecl->typeInfo, ZR_NULL, &candidateType)) {
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
                signature_convert_ast_type_with_context(compilerState,
                                                        &varDecl->value->data.constructExpression.target->data.type,
                                                        ZR_NULL,
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

/* 本地求解结果保留成员声明的传参模式，标签投影才能与调用契约一致。 */
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

/* 仅释放从导入 AST 合成的成员信息所拥有的数组；name、declarationNode 等仍借用 AST。 */
static void signature_free_temporary_member_info(SZrState *state, SZrTypeMemberInfo *memberInfo) {
    if (state == ZR_NULL || memberInfo == ZR_NULL) {
        return;
    }

    free_inferred_type_array(state, &memberInfo->parameterTypes);
    if (memberInfo->hasStructuredReturnType) {
        ZrParser_InferredType_Free(state, &memberInfo->structuredReturnType);
        memberInfo->hasStructuredReturnType = ZR_FALSE;
    }
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

/* 把构造接收者的泛型实参代入参数和返回类型，保留数组约束、所有权等类型附加信息。 */
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
    result->genericArgumentKind = sourceType->genericArgumentKind;
    result->genericConstIntValue = sourceType->genericConstIntValue;
    result->minValue = sourceType->minValue;
    result->maxValue = sourceType->maxValue;
    result->hasRangeConstraint = sourceType->hasRangeConstraint;
    ZrParser_InferredType_CopyRangeSegments(state, result, sourceType);
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

/* 将构造目标名中的泛型实参转成可递归代入的推断类型，失败时不留下半成品。 */
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

/* 本地泛型求解后把成员返回类型改写成具体类型名，供调用标签和返回类型投影。 */
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

/* AST 类型转换暂借编译器的类型/函数上下文，保证导入声明里的相对类型名按原声明解析。 */
struct SZrSignatureTypeResolutionContext {
    SZrTypePrototypeInfo *typePrototype;
    SZrAstNode *typeNode;
    SZrAstNode *functionNode;
};

/* push/pop 之间必须成对恢复编译器当前上下文，避免查询污染后续语义分析。 */
typedef struct SZrSignatureCompilerContextSnapshot {
    SZrTypePrototypeInfo *typePrototype;
    SZrAstNode *typeNode;
    SZrAstNode *functionNode;
    SZrString *typeName;
} SZrSignatureCompilerContextSnapshot;

/* 只在同步类型转换期间切换编译器上下文；snapshot 属于本次调用栈。 */
static void signature_push_type_resolution_context(
        SZrCompilerState *compilerState,
        const SZrSignatureTypeResolutionContext *context,
        SZrSignatureCompilerContextSnapshot *snapshot) {
    if (compilerState == ZR_NULL || snapshot == ZR_NULL) {
        return;
    }

    snapshot->typePrototype = compilerState->currentTypePrototypeInfo;
    snapshot->typeNode = compilerState->currentTypeNode;
    snapshot->functionNode = compilerState->currentFunctionNode;
    snapshot->typeName = compilerState->currentTypeName;

    if (context == ZR_NULL) {
        return;
    }

    if (context->typePrototype != ZR_NULL) {
        compilerState->currentTypePrototypeInfo = context->typePrototype;
        compilerState->currentTypeName = context->typePrototype->name;
    }
    if (context->typeNode != ZR_NULL) {
        compilerState->currentTypeNode = context->typeNode;
    }
    if (context->functionNode != ZR_NULL) {
        compilerState->currentFunctionNode = context->functionNode;
    }
}

/* 无论 AST 转换成功与否，都恢复调用方原有的编译器上下文。 */
static void signature_pop_type_resolution_context(
        SZrCompilerState *compilerState,
        const SZrSignatureCompilerContextSnapshot *snapshot) {
    if (compilerState == ZR_NULL || snapshot == ZR_NULL) {
        return;
    }

    compilerState->currentTypePrototypeInfo = snapshot->typePrototype;
    compilerState->currentTypeNode = snapshot->typeNode;
    compilerState->currentFunctionNode = snapshot->functionNode;
    compilerState->currentTypeName = snapshot->typeName;
}

/* 复用 analyzer 的声明类型构造规则，并在返回前归还编译器上下文。 */
static TZrBool signature_convert_ast_type_with_context(
        SZrCompilerState *compilerState,
        SZrType *sourceType,
        const SZrSignatureTypeResolutionContext *context,
        SZrInferredType *outType) {
    SZrSemanticAnalyzer analyzer;
    SZrSignatureCompilerContextSnapshot snapshot;
    TZrBool success;

    if (compilerState == ZR_NULL || sourceType == ZR_NULL || outType == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(&snapshot, 0, sizeof(snapshot));
    memset(&analyzer, 0, sizeof(analyzer));
    analyzer.state = compilerState->state;
    analyzer.compilerState = compilerState;
    analyzer.semanticContext = compilerState->semanticContext;
    signature_push_type_resolution_context(compilerState, context, &snapshot);
    success = ZrLanguageServer_SemanticAnalyzer_BuildDeclaredTypeInferredType(&analyzer,
                                                                              ZR_NULL,
                                                                              ZR_NULL,
                                                                              sourceType,
                                                                              outType);
    signature_pop_type_resolution_context(compilerState, &snapshot);
    return success;
}

/* 导入模块只有 AST 声明时，先按声明上下文解析返回类型，再代入接收者泛型。 */
static TZrBool signature_build_specialized_type_name_from_ast_type(SZrCompilerState *compilerState,
                                                                   SZrType *sourceType,
                                                                   const SZrArray *genericParameters,
                                                                   const SZrArray *bindingTypes,
                                                                   const SZrSignatureTypeResolutionContext *context,
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
    if (!signature_convert_ast_type_with_context(compilerState, sourceType, context, &unresolvedType) ||
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

/* 把类型声明的泛型槽映射成成员求解器可消费的临时信息；名称借用 AST。 */
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

/* 与 AST 泛型信息收集配对释放每个约束数组，不释放借用的 VM 字符串。 */
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

/* 导入构造器缺成员元数据时从声明 AST 构造参数类型，失败清理已收集的类型。 */
static TZrBool signature_collect_parameter_types_from_ast(SZrCompilerState *compilerState,
                                                          SZrAstNodeArray *params,
                                                          const SZrSignatureTypeResolutionContext *context,
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
            !signature_convert_ast_type_with_context(compilerState,
                                                     paramNode->data.parameter.typeInfo,
                                                     context,
                                                     &paramType)) {
            ZrParser_InferredType_Free(compilerState->state, &paramType);
            free_inferred_type_array(compilerState->state, dest);
            return ZR_FALSE;
        }

        ZrCore_Array_Push(compilerState->state, dest, &paramType);
    }

    return ZR_TRUE;
}

/* 与 AST 参数类型收集保持相同顺序，供专化签名显示 ref/out 等传参约束。 */
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

/* 在当前或导入模块 AST 内找构造目标的类/结构声明，返回值只在该 AST 生命周期内有效。 */
/* TODO: 当前按短名取第一个声明；用嵌套作用域及导入同名类型核查是否会选错构造器。 */
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

/* 从目标类型自身的 meta 成员取得 constructor 声明，供缺元数据时合成临时成员。 */
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

/* 项目索引或当前文档只有源码 AST 时，临时重建构造器成员并代入接收者泛型。 */
/* 成功时 resolvedMemberInfo 指向调用方提供的 temporaryMemberInfo；调用方须在结果投影后释放其中数组。 */
static TZrBool signature_prepare_ast_specialized_receiver_constructor(SZrState *state,
                                                                      SZrCompilerState *compilerState,
                                                                      SZrAstNode *rootNode,
                                                                      const SZrInferredType *receiverType,
                                                                      SZrTypeMemberInfo **resolvedMemberInfo,
                                                                      SZrTypeMemberInfo *temporaryMemberInfo) {
    SZrTypePrototypeInfo *openPrototype = ZR_NULL;
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
    SZrSignatureTypeResolutionContext resolutionContext;
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
    openPrototype = baseName != ZR_NULL ? find_compiler_type_prototype_inference(compilerState, baseName) : ZR_NULL;
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

    memset(&resolutionContext, 0, sizeof(resolutionContext));
    resolutionContext.typePrototype = openPrototype;
    resolutionContext.typeNode = typeDeclarationNode;
    resolutionContext.functionNode = memberDeclarationNode;

    if (!signature_collect_generic_parameter_infos_from_ast(state, &typeGenericParameters, typeGeneric) ||
        !signature_collect_parameter_passing_modes_from_ast(state,
                                                            params,
                                                            &temporaryMemberInfo->parameterPassingModes) ||
        !signature_collect_parameter_types_from_ast(compilerState, params, &resolutionContext, &rawParameterTypes) ||
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
        if (!signature_build_specialized_type_name_from_ast_type(compilerState,
                                                                 returnType,
                                                                 &typeGenericParameters,
                                                                 &bindingTypes,
                                                                 &resolutionContext,
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

/* 从成员泛型参数建独占求解槽，后续显式泛型和实参推断都写入这些槽。 */
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

/* 与槽初始化配对释放推断类型；parameterInfo 属于借用的成员元数据。 */
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

/* 当推断只给出编码类型名时展开泛型元素，使后续结构化绑定可逐层统一。 */
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

/* 实参主推断缺类型名时，才尝试从同名声明补全以支持本地泛型绑定。 */
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

/* 显式或隐式泛型绑定必须与每个实参一致；嵌套泛型递归比较，冲突直接拒绝近似签名。 */
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

/* 显式类型参数和 const 整数参数走不同解析路径，输出同一绑定槽可用的推断表示。 */
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

    return signature_convert_ast_type_with_context(compilerState, &argumentNode->data.type, ZR_NULL, result);
}

/* parser 详细求解不能处理某些 AST 合成成员时，按显式泛型和实参类型做有限本地求解。 */
/* 只在全部泛型槽已绑定且参数一致时交付结果；临时数组在成功和失败路径都由本函数释放。 */
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

/* 规范、外部 callable 与本地构造器共用的结果工厂；成功结果由 SignatureHelp_Free 释放。 */
/* 参数 label 从 AST 和专化类型生成，语义文档由实参事实补充；无 AST 参数时仅创建外壳供调用方追加参数。 */
TZrBool signature_populate_help_from_label(SZrState *state,
                                                  SZrSemanticAnalyzer *analyzer,
                                                  const TZrChar *labelText,
                                                  SZrAstNodeArray *params,
                                                  SZrAstNodeArray *argumentNodes,
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
                /* BUG: 单个参数分配失败仍返回成功；序列化会缺参数，activeParameter 可能指向错误项。 */
                continue;
            }

            parameterInfo->label = ZrCore_String_Create(state, buffer, strlen(buffer));
            parameterInfo->documentation =
                argumentNodes != ZR_NULL && index < argumentNodes->count
                    ? ZrLanguageServer_Lsp_BuildSignatureArgumentSemanticFactDocumentation(
                          state,
                          analyzer,
                          argumentNodes->nodes[index])
                    : ZR_NULL;
            ZrCore_Array_Push(state, &signatureInfo->parameters, &parameterInfo);
        }
    }

    ZrCore_Array_Push(state, &help->signatures, &signatureInfo);
    *result = help;
    return ZR_TRUE;
}

/* 在类型和祖先原型中查找 meta 成员；深度预算防止继承环导致查询无限递归。 */
static const SZrTypeMemberInfo *signature_find_type_meta_member_recursive(SZrCompilerState *compilerState,
                                                                          SZrTypePrototypeInfo *prototype,
                                                                          EZrMetaType metaType,
                                                                          TZrUInt32 depth) {
    SZrArray membersSnapshot;
    SZrArray inheritsSnapshot;
    SZrString *prototypeName;

    if (compilerState == ZR_NULL || prototype == ZR_NULL || depth > ZR_PARSER_RECURSIVE_MEMBER_LOOKUP_MAX_DEPTH) {
        return ZR_NULL;
    }

    membersSnapshot = prototype->members;
    inheritsSnapshot = prototype->inherits;
    prototypeName = prototype->name;

    for (TZrSize index = 0; index < membersSnapshot.length; index++) {
        const SZrTypeMemberInfo *memberInfo =
            (const SZrTypeMemberInfo *)ZrCore_Array_Get(&membersSnapshot, index);
        if (memberInfo != ZR_NULL && memberInfo->isMetaMethod && memberInfo->metaType == metaType) {
            return memberInfo;
        }
    }

    for (TZrSize index = 0; index < inheritsSnapshot.length; index++) {
        SZrString **superNamePtr = (SZrString **)ZrCore_Array_Get(&inheritsSnapshot, index);
        SZrTypePrototypeInfo *superPrototype;
        const SZrTypeMemberInfo *resolvedMember;

        if (superNamePtr == ZR_NULL || *superNamePtr == ZR_NULL) {
            continue;
        }

        superPrototype = find_compiler_type_prototype_inference(compilerState, *superNamePtr);
        if (superPrototype == ZR_NULL ||
            signature_type_prototype_matches_name(superPrototype, prototypeName)) {
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

/* 泛型实例可能尚未物化原型；先确保可查询，再借出 constructor 等 meta 成员。 */
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

/* 只承接无法由规范语义调用事实解析的构造调用；项目索引可补充导入模块中的构造器 AST。 */
/* resolvedSignature、临时成员与 constructedType 均在交付可独立释放的 LSP 结果后清理。 */
static TZrBool signature_resolve_construct_help(SZrState *state,
                                                SZrLspContext *lspContext,
                                                SZrString *uri,
                                                SZrSemanticAnalyzer *analyzer,
                                                SZrCompilerState *compilerState,
                                                SZrLspCallContext *context,
                                                SZrFilePosition position,
                                                SZrLspSignatureHelp **result) {
    SZrAstNodeArray *constructArguments;
    SZrConstructExpression *legacyConstruct = ZR_NULL;
    TZrBool isCanonicalStructInit;
    SZrInferredType constructedType;
    const SZrTypeMemberInfo *constructorInfo;
    SZrTypeMemberInfo temporaryConstructorInfo;
    SZrFunctionCall constructorCall;
    SZrResolvedCallSignature resolvedSignature;
    TZrChar labelBuffer[ZR_LSP_LONG_TEXT_BUFFER_LENGTH];
    TZrBool resolved = ZR_FALSE;

    if (state == ZR_NULL || analyzer == ZR_NULL || compilerState == ZR_NULL || context == ZR_NULL ||
        result == ZR_NULL || context->kind != ZR_LSP_CALL_CONTEXT_CONSTRUCT_CALL ||
        !signature_construct_node_is_supported(context->callNode)) {
        return ZR_FALSE;
    }

    constructArguments = signature_construct_node_arguments(context->callNode);
    isCanonicalStructInit = context->callNode->type == ZR_AST_STRUCT_INIT_EXPRESSION;
    if (!isCanonicalStructInit) {
        legacyConstruct = &context->callNode->data.constructExpression;
    }
    ZrParser_InferredType_Init(state, &constructedType, ZR_VALUE_TYPE_OBJECT);
    if ((isCanonicalStructInit &&
         !signature_copy_exact_construct_type(state, analyzer, context->callNode, &constructedType)) ||
        (!isCanonicalStructInit &&
         (!infer_construct_expression_type(compilerState, context->callNode, &constructedType) ||
          constructedType.typeName == ZR_NULL))) {
        ZrParser_InferredType_Free(state, &constructedType);
        return ZR_FALSE;
    }

    constructorInfo = signature_find_type_meta_member(compilerState, constructedType.typeName, ZR_META_CONSTRUCTOR);
    memset(&temporaryConstructorInfo, 0, sizeof(temporaryConstructorInfo));
    if (!isCanonicalStructInit &&
        (constructorInfo == ZR_NULL ||
         (constructorInfo->declarationNode == ZR_NULL &&
          constructorInfo->parameterNames.length == 0 &&
          constructorInfo->parameterTypes.length == 0)) &&
        lspContext != ZR_NULL &&
        uri != ZR_NULL &&
        legacyConstruct->target != ZR_NULL &&
        legacyConstruct->target->type == ZR_AST_PRIMARY_EXPRESSION &&
        legacyConstruct->target->data.primaryExpression.property != ZR_NULL) {
        SZrInferredType moduleType;
        SZrLspProjectIndex *projectIndex;
        SZrLspProjectFileRecord *record = ZR_NULL;
        SZrFileVersion *moduleFileVersion = ZR_NULL;

        ZrParser_InferredType_Init(state, &moduleType, ZR_VALUE_TYPE_OBJECT);
        if (ZrParser_ExpressionType_Infer(compilerState,
                                          legacyConstruct->target->data.primaryExpression.property,
                                          &moduleType) &&
            moduleType.typeName != ZR_NULL &&
            type_name_is_module_prototype_inference(compilerState, moduleType.typeName)) {
            projectIndex = ZrLanguageServer_LspProject_FindProjectForUri(lspContext, uri);
            if (projectIndex != ZR_NULL) {
                record = ZrLanguageServer_LspProject_FindRecordByModuleName(projectIndex, moduleType.typeName);
                if (record != ZR_NULL && record->uri != ZR_NULL) {
                    moduleFileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(lspContext, record->uri);
                }
            }
            if (moduleFileVersion != ZR_NULL &&
                moduleFileVersion->ast != ZR_NULL &&
                signature_prepare_ast_specialized_receiver_constructor(state,
                                                                       compilerState,
                                                                       moduleFileVersion->ast,
                                                                       &constructedType,
                                                                       (SZrTypeMemberInfo **)&constructorInfo,
                                                                       &temporaryConstructorInfo)) {
                ZrParser_InferredType_Free(state, &moduleType);
            } else {
                ZrParser_InferredType_Free(state, &moduleType);
            }
        } else {
            ZrParser_InferredType_Free(state, &moduleType);
        }
    }
    if (isCanonicalStructInit &&
        (constructorInfo == ZR_NULL ||
         (constructorInfo->declarationNode == ZR_NULL &&
          constructorInfo->parameterNames.length == 0 &&
          constructorInfo->parameterTypes.length == 0))) {
        ZrParser_InferredType_Free(state, &constructedType);
        return ZR_FALSE;
    }
    if (!isCanonicalStructInit && constructorInfo == ZR_NULL &&
        !signature_prepare_ast_specialized_receiver_constructor(state,
                                                                compilerState,
                                                                analyzer->ast,
                                                                &constructedType,
                                                                (SZrTypeMemberInfo **)&constructorInfo,
                                                                &temporaryConstructorInfo)) {
        ZrParser_InferredType_Free(state, &constructedType);
        return ZR_FALSE;
    }

    memset(&constructorCall, 0, sizeof(constructorCall));
    constructorCall.args = constructArguments;
    constructorCall.argNames = signature_construct_node_argument_names(context->callNode);
    constructorCall.hasNamedArgs = signature_construct_node_has_named_arguments(context->callNode);
    constructorCall.genericArguments = ZR_NULL;
    constructorCall.argumentMarkers = signature_construct_node_argument_markers(context->callNode);

    memset(&resolvedSignature, 0, sizeof(resolvedSignature));
    ZrParser_InferredType_Init(state, &resolvedSignature.returnType, ZR_VALUE_TYPE_OBJECT);
    ZrCore_Array_Construct(&resolvedSignature.parameterTypes);
    ZrCore_Array_Construct(&resolvedSignature.parameterPassingModes);

    if (resolve_generic_member_call_signature_detailed(compilerState,
                                                       constructorInfo,
                                                       &constructorCall,
                                                       &resolvedSignature,
                                                       ZR_NULL,
                                                       0) == ZR_GENERIC_CALL_RESOLVE_OK) {
        resolved = ZR_TRUE;
    } else {
        resolved = signature_resolve_member_call_signature_locally(state,
                                                                   analyzer,
                                                                   compilerState,
                                                                   (SZrTypeMemberInfo *)constructorInfo,
                                                                   &constructorCall,
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
        ZrParser_InferredType_Free(state, &constructedType);
        return ZR_FALSE;
    }

    if (!signature_populate_help_from_label(state,
                                            analyzer,
                                            labelBuffer,
                                            signature_method_parameter_nodes(constructorInfo->declarationNode),
                                            constructArguments,
                                            &resolvedSignature,
                                            signature_active_parameter_index_for_arguments(constructArguments,
                                                                                          position),
                                            result)) {
        free_resolved_call_signature(state, &resolvedSignature);
        signature_free_temporary_member_info(state, &temporaryConstructorInfo);
        ZrParser_InferredType_Free(state, &constructedType);
        return ZR_FALSE;
    }

    free_resolved_call_signature(state, &resolvedSignature);
    signature_free_temporary_member_info(state, &temporaryConstructorInfo);
    ZrParser_InferredType_Free(state, &constructedType);
    return ZR_TRUE;
}

/* LSP/悬停入口：按当前文档快照定位最内层调用，优先规范语义事实，再查外部 callable，最后处理构造器。 */
/* 成功且 result 非空时由调用方用 SignatureHelp_Free 释放；非代码光标可成功返回空结果。 */
TZrBool ZrLanguageServer_Lsp_GetSignatureHelp(SZrState *state,
                                              SZrLspContext *context,
                                              SZrString *uri,
                                              SZrLspPosition position,
                                              SZrLspSignatureHelp **result) {
    SZrSemanticAnalyzer *analyzer;
    SZrFileVersion *fileVersion;
    SZrFileVersionContentSnapshot snapshot = {0};
    SZrFilePosition filePosition;
    SZrFileRange fileRange;
    SZrLspCallContext callContext;

    if (state == ZR_NULL || context == ZR_NULL || uri == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    *result = ZR_NULL;
    analyzer = ZrLanguageServer_Lsp_GetOrCreateAnalyzer(state, context, uri);
    fileVersion = ZrLanguageServer_Lsp_GetDocumentFileVersion(context, uri);
    if (analyzer == ZR_NULL || analyzer->ast == ZR_NULL || fileVersion == ZR_NULL) {
        return ZR_FALSE;
    }

    filePosition = ZrLanguageServer_Lsp_GetDocumentFilePosition(context, uri, position);
    if (ZrLanguageServer_FileVersionContentSnapshot_Acquire(state, fileVersion, &snapshot) &&
        !ZrLanguageServer_Lsp_IsCursorOffsetInCodeSpan(snapshot.content,
                                                       snapshot.contentLength,
                                                       filePosition.offset)) {
        ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);
        return ZR_TRUE;
    }
    ZrLanguageServer_FileVersionContentSnapshot_Free(state, &snapshot);

    fileRange = ZrParser_FileRange_Create(filePosition, filePosition, uri);
    memset(&callContext, 0, sizeof(callContext));
    signature_find_call_context_in_node(analyzer->ast, fileRange, &callContext);
    if (callContext.kind == ZR_LSP_CALL_CONTEXT_NONE) {
        return ZR_FALSE;
    }

    /* 本地已确认的规范调用不允许被外部元数据遮盖；外部目标不可用时也不回退成同名源码签名。 */
    if (callContext.kind == ZR_LSP_CALL_CONTEXT_FUNCTION_CALL &&
        callContext.callNode != ZR_NULL &&
        callContext.callNode->type == ZR_AST_FUNCTION_CALL) {
        if (signature_context_has_local_canonical_call(analyzer, &callContext)) {
            return ZrLanguageServer_LspCanonicalSignatureHelp_Resolve(
                           state,
                           analyzer,
                           fileRange,
                           callContext.argumentNodes,
                           signature_active_parameter_index(
                                   &callContext.callNode->data.functionCall,
                                   filePosition),
                           result) &&
                   *result != ZR_NULL;
        }
        SZrFileRange calleeRange;
        EZrLspExternalCallableSignatureStatus externalStatus =
                callContext.callMemberIndex > 0U &&
                        signature_external_callable_callee_range(
                        &callContext, &calleeRange)
                        ? ZrLanguageServer_LspExternalCallableSignatureHelp_Resolve(
                                  state,
                                  context,
                                  analyzer,
                                  uri,
                                  calleeRange,
                                  callContext.argumentNodes,
                                  signature_active_parameter_index(
                                          &callContext.callNode->data.functionCall,
                                          filePosition),
                                  result)
                        : ZR_LSP_EXTERNAL_CALLABLE_SIGNATURE_NOT_EXTERNAL;
        if (externalStatus == ZR_LSP_EXTERNAL_CALLABLE_SIGNATURE_RESOLVED) {
            return *result != ZR_NULL;
        }
        if (externalStatus == ZR_LSP_EXTERNAL_CALLABLE_SIGNATURE_UNAVAILABLE) {
            return ZR_FALSE;
        }
    }

    if (callContext.kind == ZR_LSP_CALL_CONTEXT_FUNCTION_CALL &&
        callContext.callNode != ZR_NULL &&
        callContext.callNode->type == ZR_AST_FUNCTION_CALL &&
        ZrLanguageServer_LspCanonicalSignatureHelp_Resolve(
                state,
                analyzer,
                fileRange,
                callContext.argumentNodes,
                signature_active_parameter_index(
                        &callContext.callNode->data.functionCall, filePosition),
                result)) {
        return *result != ZR_NULL;
    }

    if (callContext.kind == ZR_LSP_CALL_CONTEXT_CONSTRUCT_CALL &&
        signature_construct_node_is_supported(callContext.callNode) &&
        signature_context_requires_canonical_source_call(
                analyzer, &callContext) &&
        ZrLanguageServer_LspCanonicalSignatureHelp_Resolve(
                state,
                analyzer,
                fileRange,
                callContext.argumentNodes,
                signature_active_parameter_index_for_arguments(
                        callContext.argumentNodes, filePosition),
                result)) {
        return *result != ZR_NULL;
    }

    if (callContext.kind == ZR_LSP_CALL_CONTEXT_SUPER_CONSTRUCTOR_CALL &&
        signature_context_requires_canonical_source_call(
                analyzer, &callContext) &&
        ZrLanguageServer_LspCanonicalSignatureHelp_Resolve(
                state,
                analyzer,
                fileRange,
                callContext.argumentNodes,
                signature_active_parameter_index_for_arguments(
                        callContext.argumentNodes, filePosition),
                result)) {
        return *result != ZR_NULL;
    }

    /* 已解析到源码声明却缺规范签名时宁可不返回，不能把旧元数据解释冒充精确答案。 */
    if (signature_context_requires_canonical_source_call(analyzer, &callContext)) {
        return ZR_FALSE;
    }

    if (analyzer->compilerState == ZR_NULL) {
        return ZR_FALSE;
    }

    if (callContext.kind == ZR_LSP_CALL_CONTEXT_CONSTRUCT_CALL) {
        return signature_resolve_construct_help(state,
                                                context,
                                                uri,
                                                analyzer,
                                                analyzer->compilerState,
                                                &callContext,
                                                filePosition,
                                                result) &&
               *result != ZR_NULL;
    }

    return ZR_FALSE;
}

/* 对称释放结果外壳、签名与参数对象；label/documentation 是 VM 字符串，不在此单独 RawFree。 */
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
