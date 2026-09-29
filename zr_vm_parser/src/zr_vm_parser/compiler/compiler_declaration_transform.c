#include "compiler_declaration_transform.h"

#include "compile_tool_binding.h"

/** @brief 提取类型路径末段，供声明工具 provider 查找类型描述符；路径归属由上层校验。 */
static SZrString *transform_type_segment_name(const SZrType *type) {
    const SZrType *segment = type;

    while (segment != ZR_NULL && segment->subType != ZR_NULL) {
        segment = segment->subType;
    }
    if (segment == ZR_NULL || segment->name == ZR_NULL) {
        return ZR_NULL;
    }
    if (segment->name->type == ZR_AST_IDENTIFIER_LITERAL) {
        return segment->name->data.identifier.name;
    }
    /* TODO: generic leaf 只按基础名称查内建描述符；需确认 Patch<T>/View<T> 应拒绝还是另有泛型契约。 */
    if (segment->name->type == ZR_AST_GENERIC_TYPE &&
        segment->name->data.genericType.name != ZR_NULL) {
        return segment->name->data.genericType.name->name;
    }
    return ZR_NULL;
}

/**
 * @brief 从当前编译作用域解析 `alias.Type` 的声明工具类型，而非相信源码中的别名拼写。
 * @pre cs 持有已登记的 compile-tool binding；type 为待验证的参数或返回类型 AST。
 * @return 仅单层限定路径且根别名实际绑定到声明 provider 时返回借用的类型描述符，否则返回 NULL。
 */
static const SZrParserCompileToolTypeDescriptor *transform_type_descriptor(
        SZrCompilerState *cs,
        const SZrType *type) {
    const SZrCompileToolBinding *binding;
    SZrString *leafName;

    if (cs == ZR_NULL || type == ZR_NULL || type->name == ZR_NULL ||
        type->name->type != ZR_AST_IDENTIFIER_LITERAL ||
        type->name->data.identifier.name == ZR_NULL ||
        type->subType == ZR_NULL || type->subType->subType != ZR_NULL) {
        return ZR_NULL;
    }
    binding = ZrParser_CompileToolBinding_Resolve(
            cs, type->name->data.identifier.name);
    if (binding == ZR_NULL || binding->kind != ZR_COMPILE_TOOL_BINDING_PROVIDER ||
        binding->provider == ZR_NULL ||
        strcmp(binding->provider->moduleName,
               ZR_PARSER_COMPILE_TOOL_MODULE_DECLARATION) != 0) {
        return ZR_NULL;
    }
    leafName = transform_type_segment_name(type);
    return leafName != ZR_NULL
                   ? ZrParser_CompileTool_FindType(
                             binding->provider,
                             ZrCore_String_GetNativeString(leafName))
                   : ZR_NULL;
}

/**
 * @brief 校验 declarationTransform 函数只能接收不可变声明视图并返回 Patch，作为静态声明改写入口。
 * @pre 调用方已确认 functionNode 具有 declarationTransform 属性，编译状态保留当前 comptime 上下文及导入绑定。
 * @return 合法签名返回 true；非法签名发布 compiler error 并返回 false，空状态或非函数 AST 直接返回 false。
 */
TZrBool ZrParser_DeclarationTransform_ValidateSignature(
        SZrCompilerState *cs,
        SZrAstNode *functionNode) {
    SZrFunctionDeclaration *declaration;
    SZrParameter *targetParameter;
    const SZrParserCompileToolTypeDescriptor *targetType;
    const SZrParserCompileToolTypeDescriptor *returnType;

    if (cs == ZR_NULL || functionNode == ZR_NULL ||
        functionNode->type != ZR_AST_FUNCTION_DECLARATION) {
        return ZR_FALSE;
    }
    declaration = &functionNode->data.functionDeclaration;
    /* 变换器在声明扩展阶段同步运行；async、泛型、接收者或多参数都无法维持确定的目标视图契约。 */
    if (!cs->isInCompileTimeContext || declaration->isAsync ||
        declaration->generic != ZR_NULL || declaration->args != ZR_NULL ||
        declaration->params == ZR_NULL || declaration->params->count != 1U ||
        declaration->params->nodes[0] == ZR_NULL ||
        declaration->params->nodes[0]->type != ZR_AST_PARAMETER ||
        declaration->returnType == ZR_NULL || declaration->body == ZR_NULL) {
        ZrParser_Compiler_Error(
                cs,
                "declaration_transform.signature: expected an ordinary non-generic comptime fn(target: declaration view): declaration.Patch",
                functionNode->location);
        return ZR_FALSE;
    }

    targetParameter = &declaration->params->nodes[0]->data.parameter;
    targetType = transform_type_descriptor(cs, targetParameter->typeInfo);
    returnType = transform_type_descriptor(cs, declaration->returnType);
    /* 形状校验还需落到真实 provider 描述符：参数只借用 immutable view，结果必须是可提交的 Patch。 */
    if (targetParameter->defaultValue != ZR_NULL || targetParameter->isConst ||
        targetParameter->passingMode != ZR_PARAMETER_PASSING_MODE_VALUE ||
        targetType == ZR_NULL || !targetType->immutableView ||
        targetType->role < ZR_PARSER_COMPILE_TOOL_TYPE_DECLARATION_VIEW ||
        targetType->role > ZR_PARSER_COMPILE_TOOL_TYPE_PARAMETER_VIEW ||
        returnType == ZR_NULL ||
        returnType->role != ZR_PARSER_COMPILE_TOOL_TYPE_PATCH) {
        ZrParser_Compiler_Error(
                cs,
                "declaration_transform.signature: target must be an immutable zr.compile.declaration view and return type must be Patch",
                functionNode->location);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}
