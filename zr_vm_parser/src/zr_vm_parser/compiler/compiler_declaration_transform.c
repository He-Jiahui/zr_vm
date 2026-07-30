#include "compiler_declaration_transform.h"

#include "compile_tool_binding.h"

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
    if (segment->name->type == ZR_AST_GENERIC_TYPE &&
        segment->name->data.genericType.name != ZR_NULL) {
        return segment->name->data.genericType.name->name;
    }
    return ZR_NULL;
}

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
