#include "semantic/semantic_analyzer_internal.h"
#include "zr_vm_parser/semantic_type_use.h"

#include <stdarg.h>
#include <stdlib.h>

/* parser 的查找可能实例化闭合泛型并扩容 typePrototypes；调用方不得跨此调用保留数组元素地址。 */
SZrTypePrototypeInfo *find_compiler_type_prototype_inference(SZrCompilerState *cs, SZrString *typeName);

/* 仅在推断声明类型的短暂窗口借存 parser 的当前类型/函数上下文，不拥有其中任何指针。 */
typedef struct SZrSemanticPrototypeContextSnapshot {
    SZrTypePrototypeInfo *typePrototype;
    SZrAstNode *typeNode;
    SZrString *typeName;
    SZrAstNode *functionNode;
} SZrSemanticPrototypeContextSnapshot;

/* 外壳登记和详情填充依赖本地精确名字，避免查找时额外实例化泛型类型。返回值只在 typePrototypes 未扩容前有效。 */
static SZrTypePrototypeInfo *semantic_type_prototypes_find_exact(SZrCompilerState *compilerState,
                                                                 SZrString *typeName) {
    if (compilerState == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < compilerState->typePrototypes.length; index++) {
        SZrTypePrototypeInfo *prototype =
            (SZrTypePrototypeInfo *)ZrCore_Array_Get(&compilerState->typePrototypes, index);
        if (prototype != ZR_NULL &&
            prototype->name != ZR_NULL &&
            ZrCore_String_Equal(prototype->name, typeName)) {
            return prototype;
        }
    }

    return ZR_NULL;
}

/* 把声明 AST 的名字映射到原型登记键；返回 AST 借用的字符串，供两遍扫描复用。 */
static SZrString *semantic_type_prototypes_owner_name(SZrAstNode *ownerTypeNode) {
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
        case ZR_AST_ENUM_DECLARATION:
            return ownerTypeNode->data.enumDeclaration.name != ZR_NULL
                       ? ownerTypeNode->data.enumDeclaration.name->name
                       : ZR_NULL;
        case ZR_AST_UNION_DECLARATION:
            return ownerTypeNode->data.unionDeclaration.name != ZR_NULL
                       ? ownerTypeNode->data.unionDeclaration.name->name
                       : ZR_NULL;
        default:
            return ZR_NULL;
    }
}

/* parser 的类型转换仍读取 currentType/currentFunction；调用者必须在所有成功与失败出口配对恢复。 */
static void semantic_type_prototypes_push_context(SZrSemanticAnalyzer *analyzer,
                                                  SZrAstNode *ownerTypeNode,
                                                  SZrAstNode *functionNode,
                                                  SZrSemanticPrototypeContextSnapshot *snapshot) {
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

    typeName = semantic_type_prototypes_owner_name(ownerTypeNode);
    if (typeName != ZR_NULL) {
        compilerState->currentTypeNode = ownerTypeNode;
        compilerState->currentTypeName = typeName;
        compilerState->currentTypePrototypeInfo = semantic_type_prototypes_find_exact(compilerState, typeName);
    }

    if (functionNode != ZR_NULL) {
        compilerState->currentFunctionNode = functionNode;
    }
}

/* 恢复进入声明推断前的 parser 上下文，防止相邻成员的泛型或属性绑定互相污染。 */
static void semantic_type_prototypes_pop_context(SZrSemanticAnalyzer *analyzer,
                                                 const SZrSemanticPrototypeContextSnapshot *snapshot) {
    SZrCompilerState *compilerState;

    if (analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL || snapshot == ZR_NULL) {
        return;
    }

    compilerState = analyzer->compilerState;
    /* BUG: 嵌套声明推断可实例化闭合泛型并扩容 typePrototypes；snapshot 借存的元素地址
     * 随扩容失效。此处恢复旧地址后，parser 的 const 泛型查找会解引用悬空原型。 */
    compilerState->currentTypePrototypeInfo = snapshot->typePrototype;
    compilerState->currentTypeNode = snapshot->typeNode;
    compilerState->currentTypeName = snapshot->typeName;
    compilerState->currentFunctionNode = snapshot->functionNode;
}

/* 统一属性声明交由 parser 的绑定器处理，保证访问器身份和普通成员走同一契约。 */
static TZrBool semantic_type_prototypes_bind_property(
        SZrSemanticAnalyzer *analyzer,
        SZrAstNode *ownerTypeNode,
        SZrTypePrototypeInfo *prototype,
        SZrAstNode *propertyNode,
        TZrUInt32 declarationOrder) {
    SZrSemanticPrototypeContextSnapshot snapshot;
    TZrBool bound;

    if (analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL ||
        ownerTypeNode == ZR_NULL || prototype == ZR_NULL ||
        propertyNode == ZR_NULL || propertyNode->type != ZR_AST_PROPERTY_DECLARATION) {
        return ZR_FALSE;
    }

    semantic_type_prototypes_push_context(analyzer, ownerTypeNode, propertyNode, &snapshot);
    bound = ZrParser_Compiler_BindPropertyDeclaration(
            analyzer->compilerState,
            prototype,
            propertyNode,
            prototype->name,
            prototype->extendsTypeName,
            declarationOrder);
    semantic_type_prototypes_pop_context(analyzer, &snapshot);
    return bound;
}

/* 为 LSP 的临时声明外壳建立与 parser 相同的数组/构造许可默认值，嵌套数组由 CompilerState_Free 释放。 */
static void semantic_type_prototypes_init_prototype(SZrState *state,
                                                    SZrTypePrototypeInfo *info,
                                                    SZrString *name,
                                                    EZrObjectPrototypeType prototypeType,
                                                    EZrAccessModifier accessModifier) {
    if (state == ZR_NULL || info == ZR_NULL) {
        return;
    }

    memset(info, 0, sizeof(*info));
    info->name = name;
    info->type = prototypeType;
    info->accessModifier = accessModifier;
    info->isImportedNative = ZR_FALSE;
    info->allowValueConstruction =
        prototypeType != ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE && prototypeType != ZR_OBJECT_PROTOTYPE_TYPE_MODULE;
    info->allowBoxedConstruction =
        prototypeType != ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE && prototypeType != ZR_OBJECT_PROTOTYPE_TYPE_MODULE;
    info->nextVirtualSlotIndex = 0;
    info->nextPropertyIdentity = 0;
    ZrCore_Value_ResetAsNull(&info->decoratorMetadataValue);
    ZrCore_Array_Init(state, &info->inherits, sizeof(SZrString *), ZR_PARSER_INITIAL_CAPACITY_TINY);
    ZrCore_Array_Init(state, &info->implements, sizeof(SZrString *), ZR_PARSER_INITIAL_CAPACITY_PAIR);
    ZrCore_Array_Init(state,
                      &info->genericParameters,
                      sizeof(SZrTypeGenericParameterInfo),
                      ZR_PARSER_INITIAL_CAPACITY_PAIR);
    ZrCore_Array_Init(state, &info->decorators, sizeof(SZrTypeDecoratorInfo), ZR_PARSER_INITIAL_CAPACITY_TINY);
    ZrCore_Array_Init(state, &info->members, sizeof(SZrTypeMemberInfo), ZR_PARSER_INITIAL_CAPACITY_SMALL);
}

/* 成员事实进入原型数组前统一设置未知槽位哨兵；最终数组复制的是结构值及其嵌套数组所有权。 */
static void semantic_type_prototypes_init_member_defaults(SZrTypeMemberInfo *memberInfo) {
    if (memberInfo == ZR_NULL) {
        return;
    }

    memset(memberInfo, 0, sizeof(*memberInfo));
    memberInfo->minArgumentCount = ZR_MEMBER_PARAMETER_COUNT_UNKNOWN;
    memberInfo->virtualSlotIndex = (TZrUInt32)-1;
    memberInfo->interfaceContractSlot = (TZrUInt32)-1;
    memberInfo->propertyIdentity = (TZrUInt32)-1;
    memberInfo->metaType = ZR_META_ENUM_MAX;
    ZrCore_Array_Construct(&memberInfo->parameterTypes);
    ZrCore_Array_Construct(&memberInfo->parameterNames);
    ZrCore_Array_Construct(&memberInfo->parameterHasDefaultValues);
    ZrCore_Array_Construct(&memberInfo->parameterDefaultValues);
    ZrCore_Array_Construct(&memberInfo->genericParameters);
    ZrCore_Array_Construct(&memberInfo->parameterPassingModes);
    ZrCore_Array_Construct(&memberInfo->decorators);
    ZrCore_Value_ResetAsNull(&memberInfo->decoratorMetadataValue);
}

/* 属性的公开名保持原样，覆写/槽位链使用 parser 约定的隐藏访问器名定位具体 getter/setter。 */
static SZrString *semantic_type_prototypes_create_hidden_property_accessor_name(SZrState *state,
                                                                                SZrString *propertyName,
                                                                                TZrBool isSetter) {
    const TZrChar *propertyNameText;
    const TZrChar *prefix = isSetter ? "__set_" : "__get_";
    TZrChar buffer[ZR_PARSER_DECLARATION_BUFFER_LENGTH];
    TZrInt32 written;

    if (state == ZR_NULL || propertyName == ZR_NULL) {
        return ZR_NULL;
    }

    propertyNameText = semantic_string_native(propertyName);
    if (propertyNameText == ZR_NULL || propertyNameText[0] == '\0') {
        return ZR_NULL;
    }

    written = snprintf(buffer, sizeof(buffer), "%s%s", prefix, propertyNameText);
    if (written <= 0 || (TZrSize)written >= sizeof(buffer)) {
        return ZR_NULL;
    }

    return ZrCore_String_Create(state, buffer, (TZrSize)written);
}

/* 类型名渲染必须完整落入固定缓冲区；失败让调用方放弃该事实，不能发布截断的类型标识。 */
static TZrBool semantic_type_prototypes_append_text(TZrChar *buffer,
                                                    TZrSize bufferSize,
                                                    TZrSize *offset,
                                                    const TZrChar *text) {
    TZrSize textLength;

    if (buffer == ZR_NULL || offset == ZR_NULL || text == ZR_NULL) {
        return ZR_FALSE;
    }

    textLength = strlen(text);
    if (*offset + textLength >= bufferSize) {
        return ZR_FALSE;
    }

    memcpy(buffer + *offset, text, textLength);
    *offset += textLength;
    buffer[*offset] = '\0';
    return ZR_TRUE;
}

/* 对数组约束追加格式化片段时沿用同一原子失败约定，避免半个名称进入原型索引。 */
static TZrBool semantic_type_prototypes_append_format(TZrChar *buffer,
                                                      TZrSize bufferSize,
                                                      TZrSize *offset,
                                                      const TZrChar *format,
                                                      ...) {
    va_list args;
    TZrInt32 written;

    if (buffer == ZR_NULL || offset == ZR_NULL || format == ZR_NULL || *offset >= bufferSize) {
        return ZR_FALSE;
    }

    va_start(args, format);
    written = vsnprintf(buffer + *offset, bufferSize - *offset, format, args);
    va_end(args);
    if (written < 0 || (TZrSize)written >= bufferSize - *offset) {
        return ZR_FALSE;
    }

    *offset += (TZrSize)written;
    return ZR_TRUE;
}

static SZrString *semantic_type_prototypes_render_generic_argument(SZrSemanticAnalyzer *analyzer,
                                                                   SZrAstNode *node);

/* 为泛型和元组的声明投影生成稳定文本键，供原型查找与签名展示共用；新字符串归 state/GC 管理。 */
static SZrString *semantic_type_prototypes_render_type_name_node(SZrSemanticAnalyzer *analyzer,
                                                                 SZrAstNode *typeNameNode) {
    TZrChar buffer[ZR_PARSER_DECLARATION_BUFFER_LENGTH];
    TZrSize offset = 0;

    if (analyzer == ZR_NULL || typeNameNode == ZR_NULL) {
        return ZR_NULL;
    }

    if (typeNameNode->type == ZR_AST_IDENTIFIER_LITERAL) {
        return typeNameNode->data.identifier.name;
    }

    buffer[0] = '\0';
    if (typeNameNode->type == ZR_AST_GENERIC_TYPE) {
        SZrGenericType *genericType = &typeNameNode->data.genericType;

        if (genericType->name == ZR_NULL || genericType->name->name == ZR_NULL ||
            !semantic_type_prototypes_append_text(buffer,
                                                  sizeof(buffer),
                                                  &offset,
                                                  semantic_string_native(genericType->name->name)) ||
            !semantic_type_prototypes_append_text(buffer, sizeof(buffer), &offset, "<")) {
            return ZR_NULL;
        }

        if (genericType->params != ZR_NULL) {
            for (TZrSize index = 0; index < genericType->params->count; index++) {
                SZrString *argumentName =
                    semantic_type_prototypes_render_generic_argument(analyzer, genericType->params->nodes[index]);

                if (index > 0 &&
                    !semantic_type_prototypes_append_text(buffer, sizeof(buffer), &offset, ", ")) {
                    return ZR_NULL;
                }
                if (argumentName == ZR_NULL ||
                    !semantic_type_prototypes_append_text(buffer,
                                                          sizeof(buffer),
                                                          &offset,
                                                          semantic_string_native(argumentName))) {
                    return ZR_NULL;
                }
            }
        }

        if (!semantic_type_prototypes_append_text(buffer, sizeof(buffer), &offset, ">")) {
            return ZR_NULL;
        }
        return ZrCore_String_Create(analyzer->compilerState->state, buffer, offset);
    }

    if (typeNameNode->type == ZR_AST_TUPLE_TYPE) {
        SZrTupleType *tupleType = &typeNameNode->data.tupleType;

        if (!semantic_type_prototypes_append_text(buffer, sizeof(buffer), &offset, "(")) {
            return ZR_NULL;
        }

        if (tupleType->elements != ZR_NULL) {
            for (TZrSize index = 0; index < tupleType->elements->count; index++) {
                SZrString *elementName =
                    semantic_type_prototypes_render_generic_argument(analyzer, tupleType->elements->nodes[index]);

                if (index > 0 &&
                    !semantic_type_prototypes_append_text(buffer, sizeof(buffer), &offset, ", ")) {
                    return ZR_NULL;
                }
                if (elementName == ZR_NULL ||
                    !semantic_type_prototypes_append_text(buffer,
                                                          sizeof(buffer),
                                                          &offset,
                                                          semantic_string_native(elementName))) {
                    return ZR_NULL;
                }
            }
        }

        if (!semantic_type_prototypes_append_text(buffer, sizeof(buffer), &offset, ")")) {
            return ZR_NULL;
        }
        return ZrCore_String_Create(analyzer->compilerState->state, buffer, offset);
    }

    return ZR_NULL;
}

/* 保留限定名和最外层数组大小约束的声明拼写，以便 LSP 查找闭合泛型和显示源码类型。 */
static SZrString *semantic_type_prototypes_render_type(SZrSemanticAnalyzer *analyzer,
                                                       const SZrType *typeNode) {
    TZrChar buffer[ZR_PARSER_DECLARATION_BUFFER_LENGTH];
    TZrSize offset = 0;
    SZrString *baseName;
    SZrString *constraintText = ZR_NULL;

    if (analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL || typeNode == ZR_NULL || typeNode->name == ZR_NULL) {
        return ZR_NULL;
    }

    baseName = semantic_type_prototypes_render_type_name_node(analyzer, typeNode->name);
    if (baseName == ZR_NULL) {
        return ZR_NULL;
    }

    buffer[0] = '\0';
    if (!semantic_type_prototypes_append_text(buffer, sizeof(buffer), &offset, semantic_string_native(baseName))) {
        return ZR_NULL;
    }

    if (typeNode->subType != ZR_NULL) {
        SZrString *subTypeName = semantic_type_prototypes_render_type(analyzer, typeNode->subType);
        if (subTypeName == ZR_NULL ||
            !semantic_type_prototypes_append_text(buffer, sizeof(buffer), &offset, ".") ||
            !semantic_type_prototypes_append_text(buffer,
                                                  sizeof(buffer),
                                                  &offset,
                                                  semantic_string_native(subTypeName))) {
            return ZR_NULL;
        }
    }

    if (typeNode->hasArraySizeConstraint) {
        TZrChar constraintBuffer[ZR_PARSER_INTEGER_BUFFER_LENGTH];

        if (typeNode->arraySizeExpression != ZR_NULL) {
            constraintText =
                semantic_type_prototypes_render_generic_argument(analyzer, typeNode->arraySizeExpression);
        } else if (typeNode->arrayFixedSize > 0 ||
                   (typeNode->arrayMinSize > 0 && typeNode->arrayMinSize == typeNode->arrayMaxSize)) {
            TZrSize fixedSize = typeNode->arrayFixedSize > 0 ? typeNode->arrayFixedSize : typeNode->arrayMinSize;
            TZrInt32 written = snprintf(constraintBuffer, sizeof(constraintBuffer), "%zu", fixedSize);
            if (written > 0 && (TZrSize)written < sizeof(constraintBuffer)) {
                constraintText = ZrCore_String_Create(analyzer->compilerState->state,
                                                      constraintBuffer,
                                                      (TZrSize)written);
            }
        } else if (typeNode->arrayMinSize > 0 || typeNode->arrayMaxSize > 0) {
            TZrInt32 written;

            if (typeNode->arrayMaxSize > 0) {
                written = snprintf(constraintBuffer,
                                   sizeof(constraintBuffer),
                                   "%zu..%zu",
                                   typeNode->arrayMinSize,
                                   typeNode->arrayMaxSize);
            } else {
                written = snprintf(constraintBuffer,
                                   sizeof(constraintBuffer),
                                   "%zu..",
                                   typeNode->arrayMinSize);
            }

            if (written > 0 && (TZrSize)written < sizeof(constraintBuffer)) {
                constraintText = ZrCore_String_Create(analyzer->compilerState->state,
                                                      constraintBuffer,
                                                      (TZrSize)written);
            }
        }
    }

    for (TZrInt32 index = 0; index < typeNode->dimensions; index++) {
        TZrBool isOutermost = (TZrBool)(index + 1 == typeNode->dimensions);

        if (isOutermost && typeNode->hasArraySizeConstraint && constraintText != ZR_NULL) {
            if (!semantic_type_prototypes_append_format(buffer,
                                                        sizeof(buffer),
                                                        &offset,
                                                        "[%s]",
                                                        semantic_string_native(constraintText))) {
                return ZR_NULL;
            }
            continue;
        }

        if (!semantic_type_prototypes_append_text(buffer, sizeof(buffer), &offset, "[]")) {
            return ZR_NULL;
        }
    }

    return ZrCore_String_Create(analyzer->compilerState->state, buffer, offset);
}

/* 泛型实参优先按声明引用表示，表达式只在可求值时折叠为整数文本；失败不应阻断后续声明扫描。 */
static SZrString *semantic_type_prototypes_render_generic_argument(SZrSemanticAnalyzer *analyzer,
                                                                   SZrAstNode *node) {
    SZrTypeValue evaluatedValue;
    TZrChar buffer[ZR_PARSER_INTEGER_BUFFER_LENGTH];

    if (analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL || node == ZR_NULL) {
        return ZR_NULL;
    }

    if (node->type == ZR_AST_TYPE) {
        return semantic_type_prototypes_render_type(analyzer, &node->data.type);
    }
    if (node->type == ZR_AST_IDENTIFIER_LITERAL) {
        return node->data.identifier.name;
    }
    if (node->type == ZR_AST_INTEGER_LITERAL && node->data.integerLiteral.literal != ZR_NULL) {
        return node->data.integerLiteral.literal;
    }

    if (ZrParser_Compiler_EvaluateCompileTimeExpression(analyzer->compilerState, node, &evaluatedValue)) {
        switch (evaluatedValue.type) {
            case ZR_VALUE_TYPE_INT8:
            case ZR_VALUE_TYPE_INT16:
            case ZR_VALUE_TYPE_INT32:
            case ZR_VALUE_TYPE_INT64:
                snprintf(buffer,
                         sizeof(buffer),
                         "%lld",
                         (long long)evaluatedValue.value.nativeObject.nativeInt64);
                return ZrCore_String_CreateFromNative(analyzer->compilerState->state, buffer);
            case ZR_VALUE_TYPE_UINT8:
            case ZR_VALUE_TYPE_UINT16:
            case ZR_VALUE_TYPE_UINT32:
            case ZR_VALUE_TYPE_UINT64:
                snprintf(buffer,
                         sizeof(buffer),
                         "%llu",
                         (unsigned long long)evaluatedValue.value.nativeObject.nativeUInt64);
                return ZrCore_String_CreateFromNative(analyzer->compilerState->state, buffer);
            default:
                break;
        }
    }

    /* TODO: 求值失败只清除 hasError，未恢复 errorMessage/结构化错误等字段；核对失败诊断是否会被后续节点误消费。 */
    analyzer->compilerState->hasError = ZR_FALSE;
    return ZR_NULL;
}

/* 简单标识符先借用 parser 的权威类型转换；临时屏蔽语义发布以免同一类型引用出现重复事实。 */
static TZrBool semantic_type_prototypes_try_parser_conversion(
        SZrSemanticAnalyzer *analyzer,
        const SZrType *typeNode,
        SZrInferredType *outType) {
    SZrCompilerState *compilerState;
    SZrSemanticContext *savedSemanticContext;
    TZrBool savedHasError;
    TZrBool savedHadRecoverableError;
    const TZrChar *savedErrorMessage;
    SZrFileRange savedErrorLocation;
    TZrBool savedHasStructuredError;
    SZrStructuredDiagnostic savedStructuredError;
    TZrBool savedHasFatalError;
    TZrBool savedHasCompileTimeError;
    TZrBool converted;

    if (analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL || typeNode == ZR_NULL ||
        outType == ZR_NULL) {
        return ZR_FALSE;
    }

    compilerState = analyzer->compilerState;
    savedSemanticContext = compilerState->semanticContext;
    savedHasError = compilerState->hasError;
    savedHadRecoverableError = compilerState->hadRecoverableError;
    savedErrorMessage = compilerState->errorMessage;
    savedErrorLocation = compilerState->errorLocation;
    savedHasStructuredError = compilerState->hasStructuredError;
    savedStructuredError = compilerState->structuredError;
    savedHasFatalError = compilerState->hasFatalError;
    savedHasCompileTimeError = compilerState->hasCompileTimeError;

    /* 只探测 parser 权威转换，语义引用由本文件在确认解析度后发布一次。 */
    compilerState->semanticContext = ZR_NULL;
    converted = ZrParser_AstTypeToInferredType_Convert(compilerState, typeNode, outType);
    compilerState->semanticContext = savedSemanticContext;

    if (!converted) {
        compilerState->hasError = savedHasError;
        compilerState->hadRecoverableError = savedHadRecoverableError;
        compilerState->errorMessage = savedErrorMessage;
        compilerState->errorLocation = savedErrorLocation;
        compilerState->hasStructuredError = savedHasStructuredError;
        compilerState->structuredError = savedStructuredError;
        compilerState->hasFatalError = savedHasFatalError;
        compilerState->hasCompileTimeError = savedHasCompileTimeError;
    }

    return converted;
}

/* 设计意图是给手工投影的整数内建类型补值域，以供后续诊断使用。 */
static void semantic_type_prototypes_apply_primitive_numeric_range(SZrInferredType *type) {
    if (type == ZR_NULL || !ZR_VALUE_IS_TYPE_INT(type->baseType)) {
        return;
    }

    switch (type->baseType) {
        case ZR_VALUE_TYPE_INT8:
            type->minValue = ZR_TYPE_RANGE_INT8_MIN;
            type->maxValue = ZR_TYPE_RANGE_INT8_MAX;
            break;
        case ZR_VALUE_TYPE_INT16:
            type->minValue = ZR_TYPE_RANGE_INT16_MIN;
            type->maxValue = ZR_TYPE_RANGE_INT16_MAX;
            break;
        case ZR_VALUE_TYPE_INT32:
            type->minValue = ZR_TYPE_RANGE_INT32_MIN;
            type->maxValue = ZR_TYPE_RANGE_INT32_MAX;
            break;
        case ZR_VALUE_TYPE_INT64:
            type->minValue = ZR_TYPE_RANGE_INT64_MIN;
            type->maxValue = ZR_TYPE_RANGE_INT64_MAX;
            break;
        case ZR_VALUE_TYPE_UINT8:
            type->minValue = 0;
            type->maxValue = ZR_TYPE_RANGE_UINT8_MAX;
            break;
        case ZR_VALUE_TYPE_UINT16:
            type->minValue = 0;
            type->maxValue = ZR_TYPE_RANGE_UINT16_MAX;
            break;
        case ZR_VALUE_TYPE_UINT32:
            type->minValue = 0;
            type->maxValue = ZR_TYPE_RANGE_UINT32_MAX;
            break;
        case ZR_VALUE_TYPE_UINT64:
            type->minValue = 0;
            type->maxValue = ZR_TYPE_RANGE_UINT64_MAX;
            break;
        default:
            return;
    }

    type->hasRangeConstraint = ZR_TRUE;
}

/* 将可折叠的数组长度写入结构化推断事实；调用方只应把非负且 TZrSize 可表示的整数视为固定大小。 */
static TZrBool semantic_type_prototypes_parse_size_text(SZrString *sizeText, TZrSize *outSize) {
    const TZrChar *text;
    TZrChar *endPtr = ZR_NULL;
    unsigned long long parsed;

    if (sizeText == ZR_NULL || outSize == ZR_NULL) {
        return ZR_FALSE;
    }

    text = semantic_string_native(sizeText);
    if (text == ZR_NULL || text[0] == '\0') {
        return ZR_FALSE;
    }

    /* BUG: `[-1]` 会由 parser 的表达式回退进入此处；strtoull 接受负号并转成巨大无符号值，
     * LSP 的 arrayFixedSize 随后错误地成为 SIZE_MAX。见 parser_types.c 的表达式回退与下方固定大小赋值。 */
    parsed = strtoull(text, &endPtr, 10);
    if (endPtr == text || endPtr == ZR_NULL || *endPtr != '\0') {
        return ZR_FALSE;
    }

    *outSize = (TZrSize)parsed;
    return ZR_TRUE;
}

static TZrBool semantic_type_prototypes_build_generic_argument_inferred_type(
        SZrSemanticAnalyzer *analyzer,
        SZrAstNode *ownerTypeNode,
        SZrAstNode *functionNode,
        SZrAstNode *argumentNode,
        SZrInferredType *outType);

/* 取函数/方法自己的泛型作用域，供实参分类先于宿主类型作用域查询。 */
static SZrGenericDeclaration *semantic_type_prototypes_generic_declaration_for_callable(
        SZrAstNode *node) {
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    switch (node->type) {
        case ZR_AST_FUNCTION_DECLARATION:
            return node->data.functionDeclaration.generic;
        case ZR_AST_CLASS_METHOD:
            return node->data.classMethod.generic;
        case ZR_AST_STRUCT_METHOD:
            return node->data.structMethod.generic;
        default:
            return ZR_NULL;
    }
}

/* 取宿主类型的泛型作用域，使成员签名能保留类/结构/接口级的占位参数。 */
static SZrGenericDeclaration *semantic_type_prototypes_generic_declaration_for_owner(
        SZrAstNode *node) {
    if (node == ZR_NULL) {
        return ZR_NULL;
    }

    switch (node->type) {
        case ZR_AST_CLASS_DECLARATION:
            return node->data.classDeclaration.generic;
        case ZR_AST_STRUCT_DECLARATION:
            return node->data.structDeclaration.generic;
        case ZR_AST_INTERFACE_DECLARATION:
            return node->data.interfaceDeclaration.generic;
        case ZR_AST_UNION_DECLARATION:
            return node->data.unionDeclaration.generic;
        default:
            return ZR_NULL;
    }
}

/* 仅 const-int 占位符允许作为数组长度或值实参；避免把同名类型参数误当常量。 */
static TZrBool semantic_type_prototypes_generic_declaration_has_const_parameter(
        SZrGenericDeclaration *genericDeclaration,
        SZrString *name) {
    if (genericDeclaration == ZR_NULL || genericDeclaration->params == ZR_NULL || name == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < genericDeclaration->params->count; index++) {
        SZrAstNode *parameterNode = genericDeclaration->params->nodes[index];
        SZrParameter *parameter;

        if (parameterNode == ZR_NULL || parameterNode->type != ZR_AST_PARAMETER) {
            continue;
        }

        parameter = &parameterNode->data.parameter;
        if (parameter->genericKind == ZR_GENERIC_PARAMETER_CONST_INT &&
            parameter->name != ZR_NULL &&
            parameter->name->name != ZR_NULL &&
            ZrCore_String_Equal(parameter->name->name, name)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

/* 声明解析度检查借此识别合法的类型占位符，避免将开放泛型标为缺失类型。 */
static TZrBool semantic_type_prototypes_generic_declaration_has_type_parameter(
        SZrGenericDeclaration *genericDeclaration,
        SZrString *name) {
    if (genericDeclaration == ZR_NULL || genericDeclaration->params == ZR_NULL || name == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < genericDeclaration->params->count; index++) {
        SZrAstNode *parameterNode = genericDeclaration->params->nodes[index];
        SZrParameter *parameter;

        if (parameterNode == ZR_NULL || parameterNode->type != ZR_AST_PARAMETER) {
            continue;
        }

        parameter = &parameterNode->data.parameter;
        if (parameter->genericKind == ZR_GENERIC_PARAMETER_TYPE &&
            parameter->name != ZR_NULL && parameter->name->name != ZR_NULL &&
            ZrCore_String_Equal(parameter->name->name, name)) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

/* 联合方法与宿主作用域识别 const 参数，供实参投影保留占位语义。 */
static TZrBool semantic_type_prototypes_is_const_generic_parameter_reference(
        SZrAstNode *ownerTypeNode,
        SZrAstNode *functionNode,
        SZrString *name) {
    /* TODO: 若方法类型参数与宿主 const 参数同名，当前布尔并集会把方法实参错认作 const；
     * 核对解析器是否禁止跨作用域同名，再决定是否按最近作用域短路。 */
    return semantic_type_prototypes_generic_declaration_has_const_parameter(
                   semantic_type_prototypes_generic_declaration_for_callable(functionNode),
                   name) ||
           semantic_type_prototypes_generic_declaration_has_const_parameter(
                   semantic_type_prototypes_generic_declaration_for_owner(ownerTypeNode),
                   name);
}

/* 开放泛型声明的类型参数可解析为有效引用，不要求已有闭合原型。 */
static TZrBool semantic_type_prototypes_is_type_generic_parameter_reference(
        SZrAstNode *ownerTypeNode,
        SZrAstNode *functionNode,
        SZrString *name) {
    return semantic_type_prototypes_generic_declaration_has_type_parameter(
                   semantic_type_prototypes_generic_declaration_for_callable(functionNode),
                   name) ||
           semantic_type_prototypes_generic_declaration_has_type_parameter(
                   semantic_type_prototypes_generic_declaration_for_owner(ownerTypeNode),
                   name);
}

/* 解析度检查只查根类型定义，泛型实参另行递归核对。 */
static SZrString *semantic_type_prototypes_root_type_name(const SZrType *typeNode) {
    if (typeNode == ZR_NULL || typeNode->name == ZR_NULL) {
        return ZR_NULL;
    }

    if (typeNode->name->type == ZR_AST_IDENTIFIER_LITERAL) {
        return typeNode->name->data.identifier.name;
    }

    if (typeNode->name->type == ZR_AST_GENERIC_TYPE &&
        typeNode->name->data.genericType.name != ZR_NULL) {
        return typeNode->name->data.genericType.name->name;
    }

    return ZR_NULL;
}

/* 声明类型投影优先复用 parser 结果，其余泛型/元组/数组保留结构供签名与类型引用消费。
 * outType 须由调用方初始化，内部嵌套数组由其最终所有者释放。 */
static TZrBool semantic_type_prototypes_build_inferred_type(SZrSemanticAnalyzer *analyzer,
                                                            SZrAstNode *ownerTypeNode,
                                                            SZrAstNode *functionNode,
                                                            const SZrType *typeNode,
                                                            SZrInferredType *outType) {
    SZrState *state;
    SZrSemanticPrototypeContextSnapshot snapshot;
    SZrString *renderedTypeName;
    EZrOwnershipQualifier ownershipQualifier;
    const SZrType *innerTypeNode;

    if (analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL || typeNode == ZR_NULL || outType == ZR_NULL) {
        return ZR_FALSE;
    }

    state = analyzer->compilerState->state;
    semantic_type_prototypes_push_context(analyzer, ownerTypeNode, functionNode, &snapshot);
    if (ZrParser_AstType_TryUnwrapOwnershipGeneric(typeNode, &ownershipQualifier, &innerTypeNode)) {
        if (!semantic_type_prototypes_build_inferred_type(analyzer,
                                                          ownerTypeNode,
                                                          functionNode,
                                                          innerTypeNode,
                                                          outType)) {
            semantic_type_prototypes_pop_context(analyzer, &snapshot);
            return ZR_FALSE;
        }
        outType->ownershipQualifier = ownershipQualifier;
        semantic_type_prototypes_pop_context(analyzer, &snapshot);
        return ZR_TRUE;
    }

    if (typeNode->dimensions == 0 && typeNode->name->type == ZR_AST_IDENTIFIER_LITERAL) {
        SZrInferredType parserType;

        ZrParser_InferredType_Init(state, &parserType, ZR_VALUE_TYPE_OBJECT);
        if (semantic_type_prototypes_try_parser_conversion(analyzer, typeNode, &parserType)) {
            ZrParser_InferredType_Free(state, outType);
            ZrParser_InferredType_Init(state, outType, ZR_VALUE_TYPE_OBJECT);
            ZrParser_InferredType_Copy(state, outType, &parserType);
            ZrParser_InferredType_Free(state, &parserType);
            semantic_type_prototypes_pop_context(analyzer, &snapshot);
            return ZR_TRUE;
        }
        ZrParser_InferredType_Free(state, &parserType);
    }

    renderedTypeName = semantic_type_prototypes_render_type(analyzer, typeNode);
    if (renderedTypeName == ZR_NULL) {
        semantic_type_prototypes_pop_context(analyzer, &snapshot);
        return ZR_FALSE;
    }

    if (typeNode->dimensions > 0) {
        SZrType elementTypeNode;
        SZrInferredType elementType;
        TZrSize fixedSize = 0;
        TZrBool hasFixedSize = ZR_FALSE;

        elementTypeNode = *typeNode;
        elementTypeNode.dimensions--;
        elementTypeNode.arrayFixedSize = 0;
        elementTypeNode.arrayMinSize = 0;
        elementTypeNode.arrayMaxSize = 0;
        elementTypeNode.hasArraySizeConstraint = ZR_FALSE;
        elementTypeNode.arraySizeExpression = ZR_NULL;

        ZrParser_InferredType_Init(state, &elementType, ZR_VALUE_TYPE_OBJECT);
        if (!semantic_type_prototypes_build_inferred_type(analyzer,
                                                          ownerTypeNode,
                                                          functionNode,
                                                          &elementTypeNode,
                                                          &elementType)) {
            ZrParser_InferredType_Free(state, &elementType);
            semantic_type_prototypes_pop_context(analyzer, &snapshot);
            return ZR_FALSE;
        }

        ZrParser_InferredType_Free(state, outType);
        ZrParser_InferredType_InitFull(state, outType, ZR_VALUE_TYPE_ARRAY, ZR_FALSE, renderedTypeName);
        outType->protocolMask = ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ITERABLE);
        ZrCore_Array_Init(state, &outType->elementTypes, sizeof(SZrInferredType), 1);
        ZrCore_Array_Push(state, &outType->elementTypes, &elementType);
        outType->ownershipQualifier = typeNode->ownershipQualifier;
        outType->hasArraySizeConstraint = typeNode->hasArraySizeConstraint;
        outType->arrayFixedSize = typeNode->arrayFixedSize;
        outType->arrayMinSize = typeNode->arrayMinSize;
        outType->arrayMaxSize = typeNode->arrayMaxSize;

        if (typeNode->hasArraySizeConstraint && typeNode->arraySizeExpression != ZR_NULL) {
            SZrString *constraintText =
                semantic_type_prototypes_render_generic_argument(analyzer, typeNode->arraySizeExpression);
            if (constraintText != ZR_NULL && semantic_type_prototypes_parse_size_text(constraintText, &fixedSize)) {
                hasFixedSize = ZR_TRUE;
            }

            if (hasFixedSize) {
                outType->arrayFixedSize = fixedSize;
                outType->arrayMinSize = fixedSize;
                outType->arrayMaxSize = fixedSize;
            }
        }

        semantic_type_prototypes_pop_context(analyzer, &snapshot);
        return ZR_TRUE;
    }

    ZrParser_InferredType_Free(analyzer->compilerState->state, outType);
    ZrParser_InferredType_InitFull(analyzer->compilerState->state,
                                   outType,
                                   ZR_VALUE_TYPE_OBJECT,
                                   ZR_FALSE,
                                   renderedTypeName);
    outType->ownershipQualifier = typeNode->ownershipQualifier;
    /* TODO: 此分支刚把 baseType 固定成 OBJECT，numeric range 辅助函数的整数分支不可达；
     * 核对是否应改用 parser 的内建类型分类，或删除这段无效的补值域意图。 */
    semantic_type_prototypes_apply_primitive_numeric_range(outType);

    if (typeNode->name->type == ZR_AST_GENERIC_TYPE) {
        SZrGenericType *genericType = &typeNode->name->data.genericType;

        ZrCore_Array_Init(state,
                          &outType->elementTypes,
                          sizeof(SZrInferredType),
                          genericType->params != ZR_NULL && genericType->params->count > 0
                              ? genericType->params->count
                              : 1);
        if (genericType->params != ZR_NULL) {
            for (TZrSize index = 0; index < genericType->params->count; index++) {
                SZrInferredType argumentType;

                ZrParser_InferredType_Init(state, &argumentType, ZR_VALUE_TYPE_OBJECT);
                if (!semantic_type_prototypes_build_generic_argument_inferred_type(analyzer,
                                                                                   ownerTypeNode,
                                                                                   functionNode,
                                                                                   genericType->params->nodes[index],
                                                                                   &argumentType)) {
                    ZrParser_InferredType_Free(state, &argumentType);
                    ZrParser_InferredType_Free(state, outType);
                    semantic_type_prototypes_pop_context(analyzer, &snapshot);
                    return ZR_FALSE;
                }

                ZrCore_Array_Push(state, &outType->elementTypes, &argumentType);
            }
        }

        {
            SZrTypePrototypeInfo *prototype = find_compiler_type_prototype_inference(
                    analyzer->compilerState,
                    outType->typeName);
            if (prototype != ZR_NULL) {
                outType->protocolMask |= prototype->protocolMask;
            }
        }
    }

    semantic_type_prototypes_pop_context(analyzer, &snapshot);
    return ZR_TRUE;
}

/* 泛型类型实参、const 整数和 const 占位符必须保持不同种类，供闭合原型实例化验证。 */
static TZrBool semantic_type_prototypes_build_generic_argument_inferred_type(
        SZrSemanticAnalyzer *analyzer,
        SZrAstNode *ownerTypeNode,
        SZrAstNode *functionNode,
        SZrAstNode *argumentNode,
        SZrInferredType *outType) {
    SZrString *argumentText;
    SZrState *state;
    SZrTypeValue evaluatedValue;
    TZrInt64 constIntValue;

    if (analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL || argumentNode == ZR_NULL || outType == ZR_NULL) {
        return ZR_FALSE;
    }

    state = analyzer->compilerState->state;
    if (argumentNode->type == ZR_AST_TYPE) {
        SZrType *argumentType = &argumentNode->data.type;
        SZrString *argumentName = argumentType->dimensions == 0 && argumentType->name != ZR_NULL &&
                                          argumentType->name->type == ZR_AST_IDENTIFIER_LITERAL
                                      ? argumentType->name->data.identifier.name
                                      : ZR_NULL;

        if (argumentName != ZR_NULL &&
            semantic_type_prototypes_is_const_generic_parameter_reference(
                    ownerTypeNode,
                    functionNode,
                    argumentName)) {
            ZrParser_InferredType_Free(state, outType);
            ZrParser_InferredType_InitConstParameterGenericArgument(state, outType, argumentName);
            return ZR_TRUE;
        }

        return semantic_type_prototypes_build_inferred_type(analyzer,
                                                            ownerTypeNode,
                                                            functionNode,
                                                            argumentType,
                                                            outType);
    }

    argumentText = semantic_type_prototypes_render_generic_argument(analyzer, argumentNode);
    if (argumentText == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZrParser_Compiler_EvaluateCompileTimeExpression(analyzer->compilerState,
                                                         argumentNode,
                                                         &evaluatedValue) &&
        (ZR_VALUE_IS_TYPE_INT(evaluatedValue.type) ||
         (ZR_VALUE_IS_TYPE_UNSIGNED_INT(evaluatedValue.type) &&
          evaluatedValue.value.nativeObject.nativeUInt64 <= (TZrUInt64)LLONG_MAX))) {
        constIntValue = ZR_VALUE_IS_TYPE_INT(evaluatedValue.type)
                            ? evaluatedValue.value.nativeObject.nativeInt64
                            : (TZrInt64)evaluatedValue.value.nativeObject.nativeUInt64;
        ZrParser_InferredType_Free(state, outType);
        ZrParser_InferredType_InitConstIntGenericArgument(state,
                                                          outType,
                                                          constIntValue,
                                                          argumentText);
        return ZR_TRUE;
    }

    ZrParser_InferredType_Free(state, outType);
    ZrParser_InferredType_InitFull(state, outType, ZR_VALUE_TYPE_OBJECT, ZR_FALSE, argumentText);
    return ZR_TRUE;
}

/* 泛型约束仅从类型 AST 提取声明名；非类型约束留给上层诊断，不制造假的类型边。 */
static SZrString *semantic_type_prototypes_extract_constraint_name(SZrSemanticAnalyzer *analyzer,
                                                                   SZrAstNode *typeNode) {
    return analyzer != ZR_NULL && typeNode != ZR_NULL && typeNode->type == ZR_AST_TYPE
               ? semantic_type_prototypes_render_type(analyzer, &typeNode->data.type)
               : ZR_NULL;
}

/* 将声明的 variance/const/where 约束复制到原型，用于补全和闭合泛型校验；嵌套数组随原型生命周期释放。 */
static void semantic_type_prototypes_collect_generic_parameters(SZrSemanticAnalyzer *analyzer,
                                                                SZrAstNode *ownerTypeNode,
                                                                SZrAstNode *functionNode,
                                                                SZrArray *genericParameters,
                                                                SZrGenericDeclaration *genericDeclaration) {
    SZrCompilerState *compilerState;

    if (analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL || genericParameters == ZR_NULL ||
        genericDeclaration == ZR_NULL || genericDeclaration->params == ZR_NULL) {
        return;
    }

    compilerState = analyzer->compilerState;
    if (!genericParameters->isValid || genericParameters->head == ZR_NULL ||
        genericParameters->capacity == 0 || genericParameters->elementSize == 0) {
        ZrCore_Array_Init(compilerState->state,
                          genericParameters,
                          sizeof(SZrTypeGenericParameterInfo),
                          genericDeclaration->params->count > 0 ? genericDeclaration->params->count : 1);
    }

    for (TZrSize index = 0; index < genericDeclaration->params->count; index++) {
        SZrAstNode *paramNode = genericDeclaration->params->nodes[index];
        SZrTypeGenericParameterInfo genericInfo;
        SZrParameter *parameter;

        if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
            continue;
        }

        parameter = &paramNode->data.parameter;
        memset(&genericInfo, 0, sizeof(genericInfo));
        genericInfo.name = parameter->name != ZR_NULL ? parameter->name->name : ZR_NULL;
        genericInfo.genericKind = parameter->genericKind;
        genericInfo.variance = parameter->variance;
        genericInfo.requiresClass = parameter->genericRequiresClass;
        genericInfo.requiresStruct = parameter->genericRequiresStruct;
        genericInfo.requiresNew = parameter->genericRequiresNew;
        if (parameter->genericKind == ZR_GENERIC_PARAMETER_CONST_INT &&
            parameter->typeInfo != ZR_NULL) {
            SZrInferredType declaredType;

            ZrParser_InferredType_Init(
                    compilerState->state, &declaredType, ZR_VALUE_TYPE_OBJECT);
            (void)ZrLanguageServer_SemanticAnalyzer_BuildDeclaredTypeInferredType(
                    analyzer,
                    ownerTypeNode,
                    functionNode,
                    parameter->typeInfo,
                    &declaredType);
            ZrParser_InferredType_Free(compilerState->state, &declaredType);
        }
        ZrCore_Array_Init(compilerState->state,
                          &genericInfo.constraintTypeNames,
                          sizeof(SZrString *),
                          parameter->genericTypeConstraints != ZR_NULL
                              ? parameter->genericTypeConstraints->count
                              : 1);

        if (parameter->genericTypeConstraints != ZR_NULL) {
            for (TZrSize constraintIndex = 0;
                 constraintIndex < parameter->genericTypeConstraints->count;
                 constraintIndex++) {
                SZrAstNode *constraintNode = parameter->genericTypeConstraints->nodes[constraintIndex];
                SZrString *constraintName =
                    semantic_type_prototypes_extract_constraint_name(analyzer, constraintNode);
                if (constraintName != ZR_NULL) {
                    ZrCore_Array_Push(compilerState->state, &genericInfo.constraintTypeNames, &constraintName);
                }
            }
        }

        ZrCore_Array_Push(compilerState->state, genericParameters, &genericInfo);
    }
}

/* 字段、返回值与继承关系共享该类型名投影；借出的字符串由 state/GC 持有，推断临时数组立即释放。 */
static SZrString *semantic_type_prototypes_type_name_from_type_node(SZrSemanticAnalyzer *analyzer,
                                                                     SZrAstNode *ownerTypeNode,
                                                                     SZrAstNode *functionNode,
                                                                     const SZrType *typeNode) {
    SZrCompilerState *compilerState;
    SZrInferredType inferredType;
    SZrString *typeName = ZR_NULL;
    TZrChar formattedTypeName[ZR_PARSER_TYPE_NAME_BUFFER_LENGTH];
    const TZrChar *formattedTypeNameText;

    if (analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL || typeNode == ZR_NULL) {
        return ZR_NULL;
    }

    compilerState = analyzer->compilerState;
    ZrParser_InferredType_Init(compilerState->state, &inferredType, ZR_VALUE_TYPE_OBJECT);
    if (semantic_type_prototypes_build_inferred_type(analyzer, ownerTypeNode, functionNode, typeNode, &inferredType)) {
        if (inferredType.typeName != ZR_NULL) {
            typeName = inferredType.typeName;
        } else {
            formattedTypeNameText = ZrParser_TypeNameString_Get(
                    compilerState->state,
                    &inferredType,
                    formattedTypeName,
                    sizeof(formattedTypeName));
            if (formattedTypeNameText != ZR_NULL && formattedTypeNameText[0] != '\0') {
                typeName = ZrCore_String_CreateFromNative(
                        compilerState->state,
                        formattedTypeNameText);
            }
        }
    }
    ZrParser_InferredType_Free(compilerState->state, &inferredType);
    return typeName;
}

/* 方法返回值除展示名外还保留嵌套推断结构，供泛型调用与签名求解；原型析构负责释放。 */
static void semantic_type_prototypes_capture_structured_return_type(SZrSemanticAnalyzer *analyzer,
                                                                     SZrAstNode *ownerTypeNode,
                                                                     SZrAstNode *functionNode,
                                                                     const SZrType *returnType,
                                                                     SZrTypeMemberInfo *memberInfo) {
    SZrCompilerState *compilerState;

    if (analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL || returnType == ZR_NULL ||
        memberInfo == ZR_NULL) {
        return;
    }

    compilerState = analyzer->compilerState;
    ZrParser_InferredType_Init(compilerState->state, &memberInfo->structuredReturnType, ZR_VALUE_TYPE_OBJECT);
    if (!semantic_type_prototypes_build_inferred_type(analyzer,
                                                       ownerTypeNode,
                                                       functionNode,
                                                       returnType,
                                                       &memberInfo->structuredReturnType)) {
        ZrParser_InferredType_Free(compilerState->state, &memberInfo->structuredReturnType);
        return;
    }

    memberInfo->hasStructuredReturnType = ZR_TRUE;
}

/* 成员参数的名称、类型和传递模式按声明位置并行保存，供签名帮助按索引还原调用约束。 */
static void semantic_type_prototypes_collect_parameter_signature(SZrSemanticAnalyzer *analyzer,
                                                                 SZrAstNode *ownerTypeNode,
                                                                 SZrAstNode *functionNode,
                                                                 SZrAstNodeArray *params,
                                                                 SZrArray *parameterTypes,
                                                                 SZrArray *parameterNames,
                                                                 SZrArray *parameterPassingModes,
                                                                 TZrUInt32 *outParameterCount,
                                                                 TZrUInt32 *outMinArgumentCount) {
    SZrCompilerState *compilerState;
    SZrSemanticPrototypeContextSnapshot snapshot;
    TZrUInt32 parameterCount = 0;
    TZrUInt32 minArgumentCount = 0;

    if (outParameterCount != ZR_NULL) {
        *outParameterCount = 0;
    }
    if (outMinArgumentCount != ZR_NULL) {
        *outMinArgumentCount = 0;
    }

    if (analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL || params == ZR_NULL) {
        return;
    }

    compilerState = analyzer->compilerState;
    semantic_type_prototypes_push_context(analyzer, ownerTypeNode, functionNode, &snapshot);
    ZrCore_Array_Init(compilerState->state, parameterTypes, sizeof(SZrInferredType), params->count);
    ZrCore_Array_Init(compilerState->state, parameterNames, sizeof(SZrString *), params->count);
    ZrCore_Array_Init(compilerState->state, parameterPassingModes, sizeof(EZrParameterPassingMode), params->count);

    for (TZrSize index = 0; index < params->count; index++) {
        SZrAstNode *paramNode = params->nodes[index];
        SZrParameter *parameter;
        SZrInferredType inferredType;
        EZrParameterPassingMode passingMode = ZR_PARAMETER_PASSING_MODE_VALUE;
        SZrString *parameterName = ZR_NULL;

        if (paramNode == ZR_NULL || paramNode->type != ZR_AST_PARAMETER) {
            continue;
        }

        parameter = &paramNode->data.parameter;
        parameterName = parameter->name != ZR_NULL ? parameter->name->name : ZR_NULL;
        passingMode = parameter->passingMode;

        if (parameterName != ZR_NULL) {
            ZrCore_Array_Push(compilerState->state, parameterNames, &parameterName);
        }
        ZrCore_Array_Push(compilerState->state, parameterPassingModes, &passingMode);

        if (parameter->defaultValue == ZR_NULL) {
            minArgumentCount++;
        }

        ZrParser_InferredType_Init(compilerState->state, &inferredType, ZR_VALUE_TYPE_OBJECT);
        /* BUG: parser 允许无类型参数及错误恢复的空 typeInfo；此处分支省略该位置的
         * parameterTypes 元素，却仍计入 parameterCount。泛型成员调用按声明索引读取该数组，
         * `fn m(a, b: int)` 的首个形参会误取 b 的类型；普通成员评分还会因数组短于形参而拒绝。 */
        if (parameter->typeInfo != ZR_NULL &&
            semantic_type_prototypes_build_inferred_type(analyzer,
                                                         ownerTypeNode,
                                                         functionNode,
                                                         parameter->typeInfo,
                                                         &inferredType)) {
            ZrCore_Array_Push(compilerState->state, parameterTypes, &inferredType);
        } else {
            ZrParser_InferredType_Free(compilerState->state, &inferredType);
            parameterCount++;
            continue;
        }
        parameterCount++;
    }

    if (outParameterCount != ZR_NULL) {
        *outParameterCount = parameterCount;
    }
    if (outMinArgumentCount != ZR_NULL) {
        *outMinArgumentCount = minArgumentCount;
    }
    semantic_type_prototypes_pop_context(analyzer, &snapshot);
}

/* 类成员投影保留声明顺序、属性隐藏名及方法签名，使后续补全/hover 与 parser 成员图一致。 */
static void semantic_type_prototypes_append_class_member(SZrState *state,
                                                         SZrSemanticAnalyzer *analyzer,
                                                         SZrAstNode *ownerTypeNode,
                                                         SZrTypePrototypeInfo *prototype,
                                                         SZrAstNode *memberNode,
                                                         TZrUInt32 declarationOrder) {
    SZrCompilerState *compilerState;
    SZrTypeMemberInfo memberInfo;

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL ||
        prototype == ZR_NULL || memberNode == ZR_NULL) {
        return;
    }

    if (memberNode->type == ZR_AST_PROPERTY_DECLARATION) {
        /* TODO: 属性绑定失败被忽略，BootstrapTypePrototypes 末尾又清 hasError；核对错误恢复 AST 中
         * 该失败是否应成为诊断而不是静默缺失的成员。 */
        (void)semantic_type_prototypes_bind_property(
                analyzer,
                ownerTypeNode,
                prototype,
                memberNode,
                declarationOrder);
        return;
    }

    compilerState = analyzer->compilerState;
    semantic_type_prototypes_init_member_defaults(&memberInfo);
    memberInfo.declarationNode = memberNode;
    memberInfo.declarationOrder = declarationOrder;
    memberInfo.ownerTypeName = prototype->name;
    memberInfo.baseDefinitionOwnerTypeName = prototype->name;

    switch (memberNode->type) {
        case ZR_AST_CLASS_FIELD: {
            SZrClassField *field = &memberNode->data.classField;
            memberInfo.memberType = ZR_AST_CLASS_FIELD;
            memberInfo.accessModifier = field->access;
            memberInfo.isStatic = field->isStatic;
            memberInfo.isConst = field->isConst;
            memberInfo.reservedRemovedUsingManaged = ZR_FALSE;
            memberInfo.name = field->name != ZR_NULL ? field->name->name : ZR_NULL;
            memberInfo.baseDefinitionName = memberInfo.name;
            memberInfo.fieldType = field->typeInfo;
            memberInfo.fieldTypeName =
                semantic_type_prototypes_type_name_from_type_node(analyzer, ownerTypeNode, ZR_NULL, field->typeInfo);
            memberInfo.ownershipQualifier =
                field->typeInfo != ZR_NULL ? field->typeInfo->ownershipQualifier : ZR_OWNERSHIP_QUALIFIER_NONE;
            break;
        }
        case ZR_AST_CLASS_METHOD: {
            SZrClassMethod *method = &memberNode->data.classMethod;
            memberInfo.memberType = ZR_AST_CLASS_METHOD;
            memberInfo.accessModifier = method->access;
            memberInfo.isStatic = method->isStatic;
            memberInfo.modifierFlags = method->modifierFlags;
            memberInfo.receiverQualifier = method->receiverQualifier;
            memberInfo.name = method->name != ZR_NULL ? method->name->name : ZR_NULL;
            memberInfo.baseDefinitionName = memberInfo.name;
            memberInfo.returnTypeName =
                semantic_type_prototypes_type_name_from_type_node(analyzer,
                                                                  ownerTypeNode,
                                                                  memberNode,
                                                                  method->returnType);
            semantic_type_prototypes_capture_structured_return_type(analyzer,
                                                                     ownerTypeNode,
                                                                     memberNode,
                                                                     method->returnType,
                                                                     &memberInfo);
            semantic_type_prototypes_collect_generic_parameters(analyzer,
                                                                ownerTypeNode,
                                                                memberNode,
                                                                &memberInfo.genericParameters,
                                                                method->generic);
            semantic_type_prototypes_collect_parameter_signature(analyzer,
                                                                 ownerTypeNode,
                                                                 memberNode,
                                                                 method->params,
                                                                 &memberInfo.parameterTypes,
                                                                 &memberInfo.parameterNames,
                                                                 &memberInfo.parameterPassingModes,
                                                                 &memberInfo.parameterCount,
                                                                 &memberInfo.minArgumentCount);
            break;
        }
        case ZR_AST_CLASS_PROPERTY: {
            SZrClassProperty *property = &memberNode->data.classProperty;
            memberInfo.memberType = ZR_AST_CLASS_PROPERTY;
            memberInfo.accessModifier = property->access;
            memberInfo.isStatic = property->isStatic;
            memberInfo.modifierFlags = property->modifierFlags;
            memberInfo.propertyIdentity = prototype->nextPropertyIdentity++;

            if (property->modifier == ZR_NULL) {
                return;
            }

            if (property->modifier->type == ZR_AST_PROPERTY_GET) {
                SZrPropertyGet *getter = &property->modifier->data.propertyGet;

                memberInfo.modifierFlags |= getter->modifierFlags;
                memberInfo.accessorRole = 1;
                memberInfo.name = getter->name != ZR_NULL ? getter->name->name : ZR_NULL;
                memberInfo.baseDefinitionName = memberInfo.name;
                memberInfo.fieldType = getter->targetType;
                memberInfo.fieldTypeName = semantic_type_prototypes_type_name_from_type_node(analyzer,
                                                                                             ownerTypeNode,
                                                                                             memberNode,
                                                                                             getter->targetType);
                memberInfo.returnTypeName = memberInfo.fieldTypeName;
                memberInfo.ownershipQualifier = getter->targetType != ZR_NULL
                                                    ? getter->targetType->ownershipQualifier
                                                    : ZR_OWNERSHIP_QUALIFIER_NONE;
            } else if (property->modifier->type == ZR_AST_PROPERTY_SET) {
                SZrPropertySet *setter = &property->modifier->data.propertySet;
                EZrParameterPassingMode passingMode = ZR_PARAMETER_PASSING_MODE_VALUE;
                SZrInferredType parameterType;

                memberInfo.modifierFlags |= setter->modifierFlags;
                memberInfo.accessorRole = 2;
                memberInfo.name = setter->name != ZR_NULL ? setter->name->name : ZR_NULL;
                memberInfo.baseDefinitionName = memberInfo.name;
                memberInfo.fieldType = setter->targetType;
                memberInfo.fieldTypeName = semantic_type_prototypes_type_name_from_type_node(analyzer,
                                                                                             ownerTypeNode,
                                                                                             memberNode,
                                                                                             setter->targetType);
                memberInfo.ownershipQualifier = setter->targetType != ZR_NULL
                                                    ? setter->targetType->ownershipQualifier
                                                    : ZR_OWNERSHIP_QUALIFIER_NONE;

                if (setter->targetType != ZR_NULL) {
                    ZrCore_Array_Init(compilerState->state, &memberInfo.parameterTypes, sizeof(SZrInferredType), 1);
                    ZrParser_InferredType_Init(compilerState->state, &parameterType, ZR_VALUE_TYPE_OBJECT);
                    if (semantic_type_prototypes_build_inferred_type(analyzer,
                                                                     ownerTypeNode,
                                                                     memberNode,
                                                                     setter->targetType,
                                                                     &parameterType)) {
                        ZrCore_Array_Push(compilerState->state, &memberInfo.parameterTypes, &parameterType);
                    } else {
                        ZrParser_InferredType_Free(compilerState->state, &parameterType);
                    }
                }

                if (setter->param != ZR_NULL && setter->param->name != ZR_NULL) {
                    ZrCore_Array_Init(compilerState->state, &memberInfo.parameterNames, sizeof(SZrString *), 1);
                    ZrCore_Array_Push(compilerState->state, &memberInfo.parameterNames, &setter->param->name);
                }
                ZrCore_Array_Init(compilerState->state,
                                  &memberInfo.parameterPassingModes,
                                  sizeof(EZrParameterPassingMode),
                                  1);
                ZrCore_Array_Push(compilerState->state, &memberInfo.parameterPassingModes, &passingMode);
                memberInfo.parameterCount = (TZrUInt32)memberInfo.parameterTypes.length;
                memberInfo.minArgumentCount = memberInfo.parameterCount;
            } else {
                return;
            }

            if (memberInfo.name != ZR_NULL) {
                memberInfo.baseDefinitionName =
                    semantic_type_prototypes_create_hidden_property_accessor_name(state,
                                                                                  memberInfo.name,
                                                                                  memberInfo.accessorRole == 2);
            }
            break;
        }
        default:
            return;
    }

    if (memberInfo.name != ZR_NULL) {
        ZrCore_Array_Push(compilerState->state, &prototype->members, &memberInfo);
    }
}

/* 结构体沿用类的字段/方法签名事实，但仅登记结构体允许的成员种类。 */
static void semantic_type_prototypes_append_struct_member(SZrState *state,
                                                          SZrSemanticAnalyzer *analyzer,
                                                          SZrAstNode *ownerTypeNode,
                                                          SZrTypePrototypeInfo *prototype,
                                                          SZrAstNode *memberNode,
                                                          TZrUInt32 declarationOrder) {
    SZrCompilerState *compilerState;
    SZrTypeMemberInfo memberInfo;

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL ||
        prototype == ZR_NULL || memberNode == ZR_NULL) {
        return;
    }

    if (memberNode->type == ZR_AST_PROPERTY_DECLARATION) {
        (void)semantic_type_prototypes_bind_property(
                analyzer,
                ownerTypeNode,
                prototype,
                memberNode,
                declarationOrder);
        return;
    }

    compilerState = analyzer->compilerState;
    semantic_type_prototypes_init_member_defaults(&memberInfo);
    memberInfo.declarationNode = memberNode;
    memberInfo.declarationOrder = declarationOrder;
    memberInfo.ownerTypeName = prototype->name;
    memberInfo.baseDefinitionOwnerTypeName = prototype->name;

    switch (memberNode->type) {
        case ZR_AST_STRUCT_FIELD: {
            SZrStructField *field = &memberNode->data.structField;
            memberInfo.memberType = ZR_AST_STRUCT_FIELD;
            memberInfo.accessModifier = field->access;
            memberInfo.isStatic = field->isStatic;
            memberInfo.isConst = field->isConst;
            memberInfo.reservedRemovedUsingManaged = ZR_FALSE;
            memberInfo.name = field->name != ZR_NULL ? field->name->name : ZR_NULL;
            memberInfo.baseDefinitionName = memberInfo.name;
            memberInfo.fieldType = field->typeInfo;
            memberInfo.fieldTypeName =
                semantic_type_prototypes_type_name_from_type_node(analyzer, ownerTypeNode, ZR_NULL, field->typeInfo);
            memberInfo.ownershipQualifier =
                field->typeInfo != ZR_NULL ? field->typeInfo->ownershipQualifier : ZR_OWNERSHIP_QUALIFIER_NONE;
            break;
        }
        case ZR_AST_STRUCT_METHOD: {
            SZrStructMethod *method = &memberNode->data.structMethod;
            memberInfo.memberType = ZR_AST_STRUCT_METHOD;
            memberInfo.accessModifier = method->access;
            memberInfo.isStatic = method->isStatic;
            memberInfo.receiverQualifier = method->receiverQualifier;
            memberInfo.name = method->name != ZR_NULL ? method->name->name : ZR_NULL;
            memberInfo.baseDefinitionName = memberInfo.name;
            memberInfo.returnTypeName =
                semantic_type_prototypes_type_name_from_type_node(analyzer,
                                                                  ownerTypeNode,
                                                                  memberNode,
                                                                  method->returnType);
            semantic_type_prototypes_capture_structured_return_type(analyzer,
                                                                     ownerTypeNode,
                                                                     memberNode,
                                                                     method->returnType,
                                                                     &memberInfo);
            semantic_type_prototypes_collect_generic_parameters(analyzer,
                                                                ownerTypeNode,
                                                                memberNode,
                                                                &memberInfo.genericParameters,
                                                                method->generic);
            semantic_type_prototypes_collect_parameter_signature(analyzer,
                                                                 ownerTypeNode,
                                                                 memberNode,
                                                                 method->params,
                                                                 &memberInfo.parameterTypes,
                                                                 &memberInfo.parameterNames,
                                                                 &memberInfo.parameterPassingModes,
                                                                 &memberInfo.parameterCount,
                                                                 &memberInfo.minArgumentCount);
            break;
        }
        default:
            return;
    }

    if (memberInfo.name != ZR_NULL) {
        ZrCore_Array_Push(compilerState->state, &prototype->members, &memberInfo);
    }
}

/* 接口签名映射为可查询的抽象字段/方法事实，供实现检查与 LSP 导航统一消费。 */
static void semantic_type_prototypes_append_interface_member(SZrState *state,
                                                             SZrSemanticAnalyzer *analyzer,
                                                             SZrAstNode *ownerTypeNode,
                                                             SZrTypePrototypeInfo *prototype,
                                                             SZrAstNode *memberNode,
                                                             TZrUInt32 declarationOrder) {
    SZrCompilerState *compilerState;
    SZrTypeMemberInfo memberInfo;

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL ||
        prototype == ZR_NULL || memberNode == ZR_NULL) {
        return;
    }

    if (memberNode->type == ZR_AST_PROPERTY_DECLARATION) {
        (void)semantic_type_prototypes_bind_property(
                analyzer,
                ownerTypeNode,
                prototype,
                memberNode,
                declarationOrder);
        return;
    }

    compilerState = analyzer->compilerState;
    semantic_type_prototypes_init_member_defaults(&memberInfo);
    memberInfo.declarationNode = memberNode;
    memberInfo.declarationOrder = declarationOrder;
    memberInfo.ownerTypeName = prototype->name;
    memberInfo.baseDefinitionOwnerTypeName = prototype->name;

    switch (memberNode->type) {
        case ZR_AST_INTERFACE_FIELD_DECLARATION: {
            SZrInterfaceFieldDeclaration *field = &memberNode->data.interfaceFieldDeclaration;
            memberInfo.memberType = ZR_AST_CLASS_FIELD;
            memberInfo.accessModifier = field->access;
            memberInfo.isConst = field->isConst;
            memberInfo.name = field->name != ZR_NULL ? field->name->name : ZR_NULL;
            memberInfo.baseDefinitionName = memberInfo.name;
            memberInfo.fieldType = field->typeInfo;
            memberInfo.fieldTypeName =
                semantic_type_prototypes_type_name_from_type_node(analyzer, ownerTypeNode, ZR_NULL, field->typeInfo);
            memberInfo.ownershipQualifier =
                field->typeInfo != ZR_NULL ? field->typeInfo->ownershipQualifier : ZR_OWNERSHIP_QUALIFIER_NONE;
            break;
        }
        case ZR_AST_INTERFACE_METHOD_SIGNATURE: {
            SZrInterfaceMethodSignature *method = &memberNode->data.interfaceMethodSignature;
            memberInfo.memberType = ZR_AST_CLASS_METHOD;
            memberInfo.accessModifier = method->access;
            memberInfo.name = method->name != ZR_NULL ? method->name->name : ZR_NULL;
            memberInfo.baseDefinitionName = memberInfo.name;
            memberInfo.returnTypeName =
                semantic_type_prototypes_type_name_from_type_node(analyzer,
                                                                  ownerTypeNode,
                                                                  memberNode,
                                                                  method->returnType);
            semantic_type_prototypes_capture_structured_return_type(analyzer,
                                                                     ownerTypeNode,
                                                                     memberNode,
                                                                     method->returnType,
                                                                     &memberInfo);
            semantic_type_prototypes_collect_generic_parameters(analyzer,
                                                                ownerTypeNode,
                                                                memberNode,
                                                                &memberInfo.genericParameters,
                                                                method->generic);
            semantic_type_prototypes_collect_parameter_signature(analyzer,
                                                                 ownerTypeNode,
                                                                 memberNode,
                                                                 method->params,
                                                                 &memberInfo.parameterTypes,
                                                                 &memberInfo.parameterNames,
                                                                 &memberInfo.parameterPassingModes,
                                                                 &memberInfo.parameterCount,
                                                                 &memberInfo.minArgumentCount);
            break;
        }
        default:
            return;
    }

    if (memberInfo.name != ZR_NULL) {
        ZrCore_Array_Push(compilerState->state, &prototype->members, &memberInfo);
    }
}

/* 在所有本地外壳可见后再分类继承边，并补默认 Object；成员推断因此可引用后声明的类型。 */
static void semantic_type_prototypes_populate_class(SZrState *state,
                                                    SZrSemanticAnalyzer *analyzer,
                                                    SZrAstNode *node,
                                                    SZrTypePrototypeInfo *prototype) {
    SZrClassDeclaration *classDecl;
    SZrString *primarySuperTypeName = ZR_NULL;

    if (state == ZR_NULL || analyzer == ZR_NULL || node == ZR_NULL || prototype == ZR_NULL ||
        node->type != ZR_AST_CLASS_DECLARATION) {
        return;
    }

    classDecl = &node->data.classDeclaration;
    prototype->modifierFlags = classDecl->modifierFlags;

    if (classDecl->inherits != ZR_NULL) {
        for (TZrSize index = 0; index < classDecl->inherits->count; index++) {
            SZrAstNode *inheritNode = classDecl->inherits->nodes[index];
            SZrString *inheritTypeName;
            SZrTypePrototypeInfo *inheritPrototype;

            if (inheritNode == ZR_NULL || inheritNode->type != ZR_AST_TYPE) {
                continue;
            }

            inheritTypeName =
                semantic_type_prototypes_type_name_from_type_node(analyzer, node, ZR_NULL, &inheritNode->data.type);
            if (inheritTypeName == ZR_NULL) {
                continue;
            }

            inheritPrototype = semantic_type_prototypes_find_exact(analyzer->compilerState, inheritTypeName);
            if (inheritPrototype != ZR_NULL && inheritPrototype->type == ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE) {
                ZrCore_Array_Push(state, &prototype->implements, &inheritTypeName);
            } else if (primarySuperTypeName == ZR_NULL) {
                primarySuperTypeName = inheritTypeName;
            }

            ZrCore_Array_Push(state, &prototype->inherits, &inheritTypeName);
        }
    }

    if (primarySuperTypeName == ZR_NULL) {
        primarySuperTypeName = ZrCore_String_Create(state, "zr.builtin.Object", strlen("zr.builtin.Object"));
        if (primarySuperTypeName != ZR_NULL) {
            ZrCore_Array_Push(state, &prototype->inherits, &primarySuperTypeName);
        }
    }
    prototype->extendsTypeName = primarySuperTypeName;

    if (classDecl->members != ZR_NULL) {
        for (TZrSize index = 0; index < classDecl->members->count; index++) {
            semantic_type_prototypes_append_class_member(state,
                                                         analyzer,
                                                         node,
                                                         prototype,
                                                         classDecl->members->nodes[index],
                                                         (TZrUInt32)index);
        }
    }
}

/* 结构体继承边与成员事实投影给类型查询；第一条继承边仍作为单一 extends 入口。 */
static void semantic_type_prototypes_populate_struct(SZrState *state,
                                                     SZrSemanticAnalyzer *analyzer,
                                                     SZrAstNode *node,
                                                     SZrTypePrototypeInfo *prototype) {
    SZrStructDeclaration *structDecl;

    if (state == ZR_NULL || analyzer == ZR_NULL || node == ZR_NULL || prototype == ZR_NULL ||
        node->type != ZR_AST_STRUCT_DECLARATION) {
        return;
    }

    structDecl = &node->data.structDeclaration;
    if (structDecl->inherits != ZR_NULL) {
        for (TZrSize index = 0; index < structDecl->inherits->count; index++) {
            SZrAstNode *inheritNode = structDecl->inherits->nodes[index];
            SZrString *inheritTypeName;

            if (inheritNode == ZR_NULL || inheritNode->type != ZR_AST_TYPE) {
                continue;
            }

            inheritTypeName =
                semantic_type_prototypes_type_name_from_type_node(analyzer, node, ZR_NULL, &inheritNode->data.type);
            if (inheritTypeName == ZR_NULL) {
                continue;
            }

            if (prototype->extendsTypeName == ZR_NULL) {
                prototype->extendsTypeName = inheritTypeName;
            }
            ZrCore_Array_Push(state, &prototype->inherits, &inheritTypeName);
        }
    }

    if (structDecl->members != ZR_NULL) {
        for (TZrSize index = 0; index < structDecl->members->count; index++) {
            semantic_type_prototypes_append_struct_member(state,
                                                          analyzer,
                                                          node,
                                                          prototype,
                                                          structDecl->members->nodes[index],
                                                          (TZrUInt32)index);
        }
    }
}

/* 接口只作为抽象契约类型；继承列表与成员签名用于实现关系和成员查询。 */
static void semantic_type_prototypes_populate_interface(SZrState *state,
                                                        SZrSemanticAnalyzer *analyzer,
                                                        SZrAstNode *node,
                                                        SZrTypePrototypeInfo *prototype) {
    SZrInterfaceDeclaration *interfaceDecl;

    if (state == ZR_NULL || analyzer == ZR_NULL || node == ZR_NULL || prototype == ZR_NULL ||
        node->type != ZR_AST_INTERFACE_DECLARATION) {
        return;
    }

    interfaceDecl = &node->data.interfaceDeclaration;
    prototype->modifierFlags = ZR_DECLARATION_MODIFIER_ABSTRACT;
    prototype->allowValueConstruction = ZR_FALSE;
    prototype->allowBoxedConstruction = ZR_FALSE;

    if (interfaceDecl->inherits != ZR_NULL) {
        for (TZrSize index = 0; index < interfaceDecl->inherits->count; index++) {
            SZrAstNode *inheritNode = interfaceDecl->inherits->nodes[index];
            SZrString *inheritTypeName;

            if (inheritNode == ZR_NULL || inheritNode->type != ZR_AST_TYPE) {
                continue;
            }

            inheritTypeName =
                semantic_type_prototypes_type_name_from_type_node(analyzer, node, ZR_NULL, &inheritNode->data.type);
            if (inheritTypeName == ZR_NULL) {
                continue;
            }

            if (prototype->extendsTypeName == ZR_NULL) {
                prototype->extendsTypeName = inheritTypeName;
            }
            ZrCore_Array_Push(state, &prototype->inherits, &inheritTypeName);
        }
    }

    if (interfaceDecl->members != ZR_NULL) {
        for (TZrSize index = 0; index < interfaceDecl->members->count; index++) {
            semantic_type_prototypes_append_interface_member(state,
                                                             analyzer,
                                                             node,
                                                             prototype,
                                                             interfaceDecl->members->nodes[index],
                                                             (TZrUInt32)index);
        }
    }
}

/* 枚举底层值类型进入原型，供 hover 与类型约束使用同一声明事实。 */
static void semantic_type_prototypes_populate_enum(SZrSemanticAnalyzer *analyzer,
                                                   SZrAstNode *node,
                                                   SZrTypePrototypeInfo *prototype) {
    if (analyzer == ZR_NULL || node == ZR_NULL || prototype == ZR_NULL || node->type != ZR_AST_ENUM_DECLARATION) {
        return;
    }

    prototype->enumValueTypeName =
        semantic_type_prototypes_type_name_from_type_node(analyzer, node, ZR_NULL, node->data.enumDeclaration.baseType);
}

/* 第一遍只注册本地声明外壳及泛型形参，让后续细节扫描能解析前向引用。 */
static TZrBool semantic_type_prototypes_register_shell(SZrState *state,
                                                       SZrSemanticAnalyzer *analyzer,
                                                       SZrAstNode *node) {
    SZrCompilerState *compilerState;
    SZrString *typeName;
    SZrTypePrototypeInfo prototypeInfo;
    EZrObjectPrototypeType prototypeType;
    EZrAccessModifier accessModifier;

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL || node == ZR_NULL) {
        return ZR_TRUE;
    }

    compilerState = analyzer->compilerState;
    typeName = semantic_type_prototypes_owner_name(node);
    if (typeName == ZR_NULL || semantic_type_prototypes_find_exact(compilerState, typeName) != ZR_NULL) {
        return ZR_TRUE;
    }

    prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_INVALID;
    accessModifier = ZR_ACCESS_PRIVATE;
    switch (node->type) {
        case ZR_AST_CLASS_DECLARATION:
            prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_CLASS;
            accessModifier = node->data.classDeclaration.accessModifier;
            break;
        case ZR_AST_STRUCT_DECLARATION:
            prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_STRUCT;
            accessModifier = node->data.structDeclaration.accessModifier;
            break;
        case ZR_AST_INTERFACE_DECLARATION:
            prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE;
            accessModifier = node->data.interfaceDeclaration.accessModifier;
            break;
        case ZR_AST_ENUM_DECLARATION:
            prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_ENUM;
            accessModifier = node->data.enumDeclaration.accessModifier;
            break;
        case ZR_AST_UNION_DECLARATION:
            prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_UNION;
            accessModifier = node->data.unionDeclaration.accessModifier;
            break;
        default:
            return ZR_TRUE;
    }

    semantic_type_prototypes_init_prototype(state, &prototypeInfo, typeName, prototypeType, accessModifier);
    switch (node->type) {
        case ZR_AST_CLASS_DECLARATION:
            semantic_type_prototypes_collect_generic_parameters(analyzer,
                                                                node,
                                                                ZR_NULL,
                                                                &prototypeInfo.genericParameters,
                                                                node->data.classDeclaration.generic);
            prototypeInfo.modifierFlags = node->data.classDeclaration.modifierFlags;
            break;
        case ZR_AST_STRUCT_DECLARATION:
            semantic_type_prototypes_collect_generic_parameters(analyzer,
                                                                node,
                                                                ZR_NULL,
                                                                &prototypeInfo.genericParameters,
                                                                node->data.structDeclaration.generic);
            break;
        case ZR_AST_INTERFACE_DECLARATION:
            semantic_type_prototypes_collect_generic_parameters(analyzer,
                                                                node,
                                                                ZR_NULL,
                                                                &prototypeInfo.genericParameters,
                                                                node->data.interfaceDeclaration.generic);
            prototypeInfo.modifierFlags = ZR_DECLARATION_MODIFIER_ABSTRACT;
            prototypeInfo.allowValueConstruction = ZR_FALSE;
            prototypeInfo.allowBoxedConstruction = ZR_FALSE;
            break;
        case ZR_AST_UNION_DECLARATION:
            semantic_type_prototypes_collect_generic_parameters(analyzer,
                                                                node,
                                                                ZR_NULL,
                                                                &prototypeInfo.genericParameters,
                                                                node->data.unionDeclaration.generic);
            break;
        default:
            break;
    }

    ZrCore_Array_Push(state, &compilerState->typePrototypes, &prototypeInfo);
    return ZR_TRUE;
}

/* 仅穿透脚本、extern 和 compile-time 包装；其余局部表达式声明不成为模块级类型。 */
static TZrBool semantic_type_prototypes_walk_shells(SZrState *state,
                                                    SZrSemanticAnalyzer *analyzer,
                                                    SZrAstNode *node) {
    if (state == ZR_NULL || analyzer == ZR_NULL || node == ZR_NULL) {
        return ZR_TRUE;
    }

    switch (node->type) {
        case ZR_AST_SCRIPT:
            if (node->data.script.statements != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.script.statements->count; index++) {
                    if (!semantic_type_prototypes_walk_shells(state,
                                                              analyzer,
                                                              node->data.script.statements->nodes[index])) {
                        return ZR_FALSE;
                    }
                }
            }
            return ZR_TRUE;
        case ZR_AST_EXTERN_BLOCK:
            if (node->data.externBlock.declarations != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.externBlock.declarations->count; index++) {
                    if (!semantic_type_prototypes_walk_shells(state,
                                                              analyzer,
                                                              node->data.externBlock.declarations->nodes[index])) {
                        return ZR_FALSE;
                    }
                }
            }
            return ZR_TRUE;
        case ZR_AST_COMPILE_TIME_DECLARATION:
            return semantic_type_prototypes_walk_shells(state,
                                                        analyzer,
                                                        node->data.compileTimeDeclaration.declaration);
        case ZR_AST_CLASS_DECLARATION:
        case ZR_AST_STRUCT_DECLARATION:
        case ZR_AST_INTERFACE_DECLARATION:
        case ZR_AST_ENUM_DECLARATION:
        case ZR_AST_UNION_DECLARATION:
            return semantic_type_prototypes_register_shell(state, analyzer, node);
        default:
            return ZR_TRUE;
    }
}

/* 第二遍填充既有外壳，避免依赖源码声明先后；已有导入或已填充原型不被重复覆盖。 */
static TZrBool semantic_type_prototypes_walk_details(SZrState *state,
                                                     SZrSemanticAnalyzer *analyzer,
                                                     SZrAstNode *node) {
    SZrTypePrototypeInfo *prototype;

    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL || node == ZR_NULL) {
        return ZR_TRUE;
    }

    switch (node->type) {
        case ZR_AST_SCRIPT:
            if (node->data.script.statements != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.script.statements->count; index++) {
                    if (!semantic_type_prototypes_walk_details(state,
                                                               analyzer,
                                                               node->data.script.statements->nodes[index])) {
                        return ZR_FALSE;
                    }
                }
            }
            return ZR_TRUE;
        case ZR_AST_EXTERN_BLOCK:
            if (node->data.externBlock.declarations != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.externBlock.declarations->count; index++) {
                    if (!semantic_type_prototypes_walk_details(state,
                                                               analyzer,
                                                               node->data.externBlock.declarations->nodes[index])) {
                        return ZR_FALSE;
                    }
                }
            }
            return ZR_TRUE;
        case ZR_AST_COMPILE_TIME_DECLARATION:
            return semantic_type_prototypes_walk_details(state,
                                                         analyzer,
                                                         node->data.compileTimeDeclaration.declaration);
        case ZR_AST_CLASS_DECLARATION:
        case ZR_AST_STRUCT_DECLARATION:
        case ZR_AST_INTERFACE_DECLARATION:
        case ZR_AST_ENUM_DECLARATION:
        case ZR_AST_UNION_DECLARATION:
            /* BUG: populate_* 期间成员的闭合泛型查询会向 typePrototypes Push；当外壳数已达容量，
             * 扩容使这里借到的 prototype 元素地址失效，随后写 members/inherits 会落到旧缓冲。
             * 查 type_inference_core.c 的 ensure_generic_instance_type_prototype_internal 与 array.h 的 Push。 */
            prototype = semantic_type_prototypes_find_exact(analyzer->compilerState,
                                                            semantic_type_prototypes_owner_name(node));
            if (prototype == ZR_NULL || prototype->members.length > 0 || prototype->inherits.length > 0 ||
                prototype->implements.length > 0 || prototype->extendsTypeName != ZR_NULL ||
                prototype->enumValueTypeName != ZR_NULL) {
                return ZR_TRUE;
            }
            switch (node->type) {
                case ZR_AST_CLASS_DECLARATION:
                    semantic_type_prototypes_populate_class(state, analyzer, node, prototype);
                    break;
                case ZR_AST_STRUCT_DECLARATION:
                    semantic_type_prototypes_populate_struct(state, analyzer, node, prototype);
                    break;
                case ZR_AST_INTERFACE_DECLARATION:
                    semantic_type_prototypes_populate_interface(state, analyzer, node, prototype);
                    break;
                case ZR_AST_ENUM_DECLARATION:
                    semantic_type_prototypes_populate_enum(analyzer, node, prototype);
                    break;
                case ZR_AST_UNION_DECLARATION:
                    break;
                default:
                    break;
            }
            return ZR_TRUE;
        default:
            return ZR_TRUE;
    }
}

/* 分析器重建 compiler state 后执行两遍扫描，为 LSP 先建立可查询声明图，再由普通语义分析补事实。 */
TZrBool ZrLanguageServer_SemanticAnalyzer_BootstrapTypePrototypes(SZrState *state,
                                                                  SZrSemanticAnalyzer *analyzer,
                                                                  SZrAstNode *ast) {
    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL || ast == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!semantic_type_prototypes_walk_shells(state, analyzer, ast)) {
        return ZR_FALSE;
    }

    if (!semantic_type_prototypes_walk_details(state, analyzer, ast)) {
        return ZR_FALSE;
    }

    analyzer->compilerState->hasError = ZR_FALSE;
    return ZR_TRUE;
}

/* 对声明引用逐层核验真实存在的根类型与泛型实参，区别可展示的推断类型和可导航的解析事实。 */
static TZrBool semantic_type_prototypes_declared_type_is_resolved(
        SZrSemanticAnalyzer *analyzer,
        SZrAstNode *ownerTypeNode,
        SZrAstNode *functionNode,
        const SZrType *typeNode) {
    EZrOwnershipQualifier ownershipQualifier;
    const SZrType *innerTypeNode;
    SZrString *rootTypeName;

    if (analyzer == ZR_NULL || analyzer->compilerState == ZR_NULL ||
        typeNode == ZR_NULL || typeNode->name == ZR_NULL) {
        return ZR_FALSE;
    }

    if (ZrParser_AstType_TryUnwrapOwnershipGeneric(
                typeNode, &ownershipQualifier, &innerTypeNode)) {
        return semantic_type_prototypes_declared_type_is_resolved(
                analyzer, ownerTypeNode, functionNode, innerTypeNode);
    }

    if (typeNode->dimensions > 0) {
        SZrType elementTypeNode = *typeNode;
        elementTypeNode.dimensions--;
        elementTypeNode.arrayFixedSize = 0;
        elementTypeNode.arrayMinSize = 0;
        elementTypeNode.arrayMaxSize = 0;
        elementTypeNode.hasArraySizeConstraint = ZR_FALSE;
        elementTypeNode.arraySizeExpression = ZR_NULL;
        return semantic_type_prototypes_declared_type_is_resolved(
                analyzer, ownerTypeNode, functionNode, &elementTypeNode);
    }

    if (typeNode->name->type == ZR_AST_TUPLE_TYPE) {
        SZrTupleType *tupleType = (SZrTupleType *)&typeNode->name->data.tupleType;
        if (tupleType->elements == ZR_NULL) {
            return ZR_TRUE;
        }
        for (TZrSize index = 0; index < tupleType->elements->count; index++) {
            SZrAstNode *elementNode = tupleType->elements->nodes[index];
            if (elementNode == ZR_NULL || elementNode->type != ZR_AST_TYPE ||
                !semantic_type_prototypes_declared_type_is_resolved(
                        analyzer, ownerTypeNode, functionNode, &elementNode->data.type)) {
                return ZR_FALSE;
            }
        }
        return ZR_TRUE;
    }

    rootTypeName = semantic_type_prototypes_root_type_name(typeNode);
    if (rootTypeName == ZR_NULL) {
        return ZR_FALSE;
    }

    if (typeNode->dimensions == 0 && typeNode->name->type == ZR_AST_IDENTIFIER_LITERAL) {
        SZrInferredType parserType;
        SZrSemanticPrototypeContextSnapshot snapshot;
        TZrBool resolved;

        ZrParser_InferredType_Init(analyzer->compilerState->state,
                                   &parserType,
                                   ZR_VALUE_TYPE_OBJECT);
        semantic_type_prototypes_push_context(
                analyzer, ownerTypeNode, functionNode, &snapshot);
        resolved = semantic_type_prototypes_try_parser_conversion(
                analyzer, typeNode, &parserType);
        semantic_type_prototypes_pop_context(analyzer, &snapshot);
        ZrParser_InferredType_Free(analyzer->compilerState->state, &parserType);
        return resolved;
    }

    if (semantic_type_prototypes_is_type_generic_parameter_reference(
                ownerTypeNode, functionNode, rootTypeName)) {
        return ZR_TRUE;
    }

    if (find_compiler_type_prototype_inference(analyzer->compilerState, rootTypeName) == ZR_NULL) {
        return ZR_FALSE;
    }

    if (typeNode->name->type == ZR_AST_GENERIC_TYPE) {
        SZrGenericType *genericType = &typeNode->name->data.genericType;
        if (genericType->params != ZR_NULL) {
            for (TZrSize index = 0; index < genericType->params->count; index++) {
                SZrAstNode *argumentNode = genericType->params->nodes[index];
                if (argumentNode != ZR_NULL && argumentNode->type == ZR_AST_TYPE &&
                    !semantic_type_prototypes_declared_type_is_resolved(
                            analyzer, ownerTypeNode, functionNode, &argumentNode->data.type)) {
                    return ZR_FALSE;
                }
            }
        }
    }

    return ZR_TRUE;
}

/* 只在 semanticContext 可用时发布类型使用关系，供定义跳转等查询按同一 canonical type 身份索引。 */
static void semantic_type_prototypes_publish_declared_type_reference(
        SZrSemanticAnalyzer *analyzer,
        SZrAstNode *ownerTypeNode,
        SZrAstNode *functionNode,
        const SZrType *typeNode,
        const SZrInferredType *inferredType) {
    TZrTypeId typeId;

    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL || typeNode == ZR_NULL ||
        typeNode->name == ZR_NULL || inferredType == ZR_NULL) {
        return;
    }

    typeId = ZrParser_Semantic_RegisterInferredType(analyzer->semanticContext,
                                                     inferredType,
                                                     ZR_SEMANTIC_TYPE_KIND_REFERENCE,
                                                     inferredType->typeName,
                                                     typeNode->name);
    if (typeId == ZR_SEMANTIC_ID_INVALID) {
        return;
    }

    (void)ZrParser_SemanticTypeUse_Publish(
            analyzer->semanticContext, typeNode, typeId,
            semantic_type_prototypes_declared_type_is_resolved(
                    analyzer, ownerTypeNode, functionNode, typeNode));
}

/* 对符号分析、签名帮助与补全提供统一的声明类型推断，并在成功后发布解析状态。
 * 返回 false 表示类型未能确认解析；outType 可能已有可展示的部分投影，仍由调用方释放。 */
TZrBool ZrLanguageServer_SemanticAnalyzer_BuildDeclaredTypeInferredType(
        SZrSemanticAnalyzer *analyzer,
        SZrAstNode *ownerTypeNode,
        SZrAstNode *functionNode,
        const SZrType *typeNode,
        SZrInferredType *outType) {
    TZrBool isResolved;

    if (analyzer == ZR_NULL || typeNode == ZR_NULL || outType == ZR_NULL) {
        return ZR_FALSE;
    }

    isResolved = semantic_type_prototypes_build_inferred_type(analyzer,
                                                               ownerTypeNode,
                                                               functionNode,
                                                               typeNode,
                                                               outType);
    if (isResolved) {
        semantic_type_prototypes_publish_declared_type_reference(
                analyzer, ownerTypeNode, functionNode, typeNode, outType);
        isResolved = semantic_type_prototypes_declared_type_is_resolved(
                analyzer, ownerTypeNode, functionNode, typeNode);
    }
    return isResolved;
}
