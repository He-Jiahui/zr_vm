#include "zr_vm_parser/compile_tool.h"

#include <string.h>

#define ZR_COMPILE_TOOL_HASH_OFFSET ((TZrUInt64)14695981039346656037ULL)
#define ZR_COMPILE_TOOL_HASH_PRIME ((TZrUInt64)1099511628211ULL)

static const TZrChar *const g_feature_parameters[] = {"string"};
static const TZrChar *const g_assert_parameters[] = {"bool", "string", "SymbolId?"};
static const TZrChar *const g_diagnostic_parameters[] = {"string", "SymbolId?"};

static const SZrParserCompileToolCallableDescriptor g_build_callables[] = {
        {
                .role = ZR_PARSER_COMPILE_TOOL_ROLE_BUILD_FEATURE,
                .qualifiedName = "compile.build.feature",
                .returnTypeName = "bool",
                .parameterTypeNames = g_feature_parameters,
                .parameterCount = ZR_ARRAY_COUNT(g_feature_parameters),
                .effect = ZR_PARSER_COMPILE_TOOL_EFFECT_PURE_VALUE,
                .minimumPhase = ZR_PARSER_COMPILE_PHASE_BUILD_FACTS,
        },
        {
                .role = ZR_PARSER_COMPILE_TOOL_ROLE_ASSERT,
                .qualifiedName = "compile.assert",
                .returnTypeName = "void",
                .parameterTypeNames = g_assert_parameters,
                .parameterCount = ZR_ARRAY_COUNT(g_assert_parameters),
                .effect = ZR_PARSER_COMPILE_TOOL_EFFECT_DIAGNOSTIC,
                .minimumPhase = ZR_PARSER_COMPILE_PHASE_LATE_CHECK,
        },
        {
                .role = ZR_PARSER_COMPILE_TOOL_ROLE_ERROR,
                .qualifiedName = "compile.error",
                .returnTypeName = "void",
                .parameterTypeNames = g_diagnostic_parameters,
                .parameterCount = ZR_ARRAY_COUNT(g_diagnostic_parameters),
                .effect = ZR_PARSER_COMPILE_TOOL_EFFECT_DIAGNOSTIC,
                .minimumPhase = ZR_PARSER_COMPILE_PHASE_LATE_CHECK,
        },
        {
                .role = ZR_PARSER_COMPILE_TOOL_ROLE_WARNING,
                .qualifiedName = "compile.warning",
                .returnTypeName = "void",
                .parameterTypeNames = g_diagnostic_parameters,
                .parameterCount = ZR_ARRAY_COUNT(g_diagnostic_parameters),
                .effect = ZR_PARSER_COMPILE_TOOL_EFFECT_DIAGNOSTIC,
                .minimumPhase = ZR_PARSER_COMPILE_PHASE_LATE_CHECK,
        },
};

static const EZrParserAttributeRole g_build_metadata_roles[] = {
        ZR_PARSER_ATTRIBUTE_ROLE_CONDITIONAL,
};

static const EZrParserAttributeRole g_declaration_metadata_roles[] = {
        ZR_PARSER_ATTRIBUTE_ROLE_DECLARATION_TRANSFORM,
};

static const SZrParserCompileToolTypeDescriptor g_declaration_types[] = {
        {ZR_PARSER_COMPILE_TOOL_TYPE_SYMBOL_ID, "SymbolId", "zr.compile.declaration.SymbolId", ZR_PARSER_COMPILE_PHASE_SIGNATURE, ZR_TRUE},
        {ZR_PARSER_COMPILE_TOOL_TYPE_DECLARATION_VIEW, "DeclarationView", "zr.compile.declaration.DeclarationView", ZR_PARSER_COMPILE_PHASE_SIGNATURE, ZR_TRUE},
        {ZR_PARSER_COMPILE_TOOL_TYPE_TYPE_VIEW, "TypeView", "zr.compile.declaration.TypeView", ZR_PARSER_COMPILE_PHASE_SIGNATURE, ZR_TRUE},
        {ZR_PARSER_COMPILE_TOOL_TYPE_CLASS_VIEW, "Class", "zr.compile.declaration.Class", ZR_PARSER_COMPILE_PHASE_SIGNATURE, ZR_TRUE},
        {ZR_PARSER_COMPILE_TOOL_TYPE_STRUCT_VIEW, "Struct", "zr.compile.declaration.Struct", ZR_PARSER_COMPILE_PHASE_SIGNATURE, ZR_TRUE},
        {ZR_PARSER_COMPILE_TOOL_TYPE_FUNCTION_VIEW, "Function", "zr.compile.declaration.Function", ZR_PARSER_COMPILE_PHASE_SIGNATURE, ZR_TRUE},
        {ZR_PARSER_COMPILE_TOOL_TYPE_FIELD_VIEW, "Field", "zr.compile.declaration.Field", ZR_PARSER_COMPILE_PHASE_SIGNATURE, ZR_TRUE},
        {ZR_PARSER_COMPILE_TOOL_TYPE_METHOD_VIEW, "Method", "zr.compile.declaration.Method", ZR_PARSER_COMPILE_PHASE_SIGNATURE, ZR_TRUE},
        {ZR_PARSER_COMPILE_TOOL_TYPE_PROPERTY_VIEW, "Property", "zr.compile.declaration.Property", ZR_PARSER_COMPILE_PHASE_SIGNATURE, ZR_TRUE},
        {ZR_PARSER_COMPILE_TOOL_TYPE_PARAMETER_VIEW, "Parameter", "zr.compile.declaration.Parameter", ZR_PARSER_COMPILE_PHASE_SIGNATURE, ZR_TRUE},
        {ZR_PARSER_COMPILE_TOOL_TYPE_PATCH, "Patch", "zr.compile.declaration.Patch", ZR_PARSER_COMPILE_PHASE_EXPANSION, ZR_FALSE},
        {ZR_PARSER_COMPILE_TOOL_TYPE_GENERATED_DECLARATION, "GeneratedDeclaration", "zr.compile.declaration.GeneratedDeclaration", ZR_PARSER_COMPILE_PHASE_EXPANSION, ZR_FALSE},
        {ZR_PARSER_COMPILE_TOOL_TYPE_GENERATED_FIELD, "GeneratedField", "zr.compile.declaration.GeneratedField", ZR_PARSER_COMPILE_PHASE_EXPANSION, ZR_FALSE},
        {ZR_PARSER_COMPILE_TOOL_TYPE_DIAGNOSTIC, "CompileDiagnostic", "zr.compile.declaration.CompileDiagnostic", ZR_PARSER_COMPILE_PHASE_EXPANSION, ZR_FALSE},
        {ZR_PARSER_COMPILE_TOOL_TYPE_ATTRIBUTE_DATA, "AttributeData", "zr.compile.declaration.AttributeData", ZR_PARSER_COMPILE_PHASE_EXPANSION, ZR_FALSE},
};

static const TZrChar g_build_contract[] =
        "zr.compile/v2\n"
        "compile.build.feature(string)->bool|PureValue|BuildFacts\n"
        "compile.assert(bool,string,SymbolId?)->void|Diagnostic|LateCheck\n"
        "compile.error(string,SymbolId?)->void|Diagnostic|LateCheck\n"
        "compile.warning(string,SymbolId?)->void|Diagnostic|LateCheck\n"
        "metadata:conditional(string)->direct-void-call-elision|Artifact\n";

static const TZrChar g_declaration_contract[] =
        "zr.compile.declaration/v2\n"
        "types:SymbolId,DeclarationView,TypeView,Class,Struct,Function,Field,Method,Property,Parameter|Signature|Immutable\n"
        "types:Patch,GeneratedDeclaration,GeneratedField,CompileDiagnostic,AttributeData|Expansion|TypedData\n"
        "typed-constructor:GeneratedField(name:string,type:TypeId,visibility:Visibility,mutability:Mutability,initializer:ConstantValue?)|Expansion\n"
        "typed-constructor:CompileDiagnostic(isError:bool,message:string,target:SymbolId)|Expansion\n"
        "typed-constructor:AttributeData(typeId:TypeId,fieldValues:ConstantValue[])|Expansion\n"
        "metadata:declarationTransform(comptime-fn)->Patch|Expansion\n";

static const SZrParserCompileToolModuleDescriptor g_build_descriptor = {
        .moduleName = ZR_PARSER_COMPILE_TOOL_MODULE_BUILD,
        .providerPhase = ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL,
        .publicContractHash = ZR_PARSER_COMPILE_TOOL_BUILD_PUBLIC_CONTRACT_HASH,
        .canonicalContract = g_build_contract,
        .computedPublicContractHash = (TZrUInt64)0xca60a1b2107c893bULL,
        .callables = g_build_callables,
        .callableCount = ZR_ARRAY_COUNT(g_build_callables),
        .metadataRoles = g_build_metadata_roles,
        .metadataRoleCount = ZR_ARRAY_COUNT(g_build_metadata_roles),
};

static const SZrParserCompileToolModuleDescriptor g_declaration_descriptor = {
        .moduleName = ZR_PARSER_COMPILE_TOOL_MODULE_DECLARATION,
        .providerPhase = ZR_LIBRARY_PROVIDER_PHASE_COMPILE_TOOL,
        .publicContractHash = ZR_PARSER_COMPILE_TOOL_DECLARATION_PUBLIC_CONTRACT_HASH,
        .canonicalContract = g_declaration_contract,
        .computedPublicContractHash = (TZrUInt64)0xb4e4667f4100e100ULL,
        .callables = ZR_NULL,
        .callableCount = 0,
        .types = g_declaration_types,
        .typeCount = ZR_ARRAY_COUNT(g_declaration_types),
        .metadataRoles = g_declaration_metadata_roles,
        .metadataRoleCount = ZR_ARRAY_COUNT(g_declaration_metadata_roles),
};

const SZrParserCompileToolModuleDescriptor *ZrParser_CompileTool_FindModule(
        const TZrChar *moduleName) {
    if (moduleName == ZR_NULL) {
        return ZR_NULL;
    }
    if (strcmp(moduleName, g_build_descriptor.moduleName) == 0) {
        return &g_build_descriptor;
    }
    if (strcmp(moduleName, g_declaration_descriptor.moduleName) == 0) {
        return &g_declaration_descriptor;
    }
    return ZR_NULL;
}

const SZrParserCompileToolTypeDescriptor *ZrParser_CompileTool_FindType(
        const SZrParserCompileToolModuleDescriptor *module,
        const TZrChar *name) {
    if (module == ZR_NULL || name == ZR_NULL || module->types == ZR_NULL) {
        return ZR_NULL;
    }
    for (TZrSize index = 0; index < module->typeCount; index++) {
        if (module->types[index].name != ZR_NULL &&
            strcmp(module->types[index].name, name) == 0) {
            return &module->types[index];
        }
    }
    return ZR_NULL;
}

TZrBool ZrParser_CompileTool_IsModuleName(const TZrChar *moduleName) {
    return moduleName != ZR_NULL &&
           (strcmp(moduleName, ZR_PARSER_COMPILE_TOOL_MODULE_BUILD) == 0 ||
            strcmp(moduleName, ZR_PARSER_COMPILE_TOOL_MODULE_DECLARATION) == 0);
}

const SZrParserCompileToolCallableDescriptor *ZrParser_CompileTool_FindCallable(
        const SZrParserCompileToolModuleDescriptor *module,
        EZrParserCompileToolRole role) {
    if (module == ZR_NULL || module->callables == ZR_NULL || role == ZR_PARSER_COMPILE_TOOL_ROLE_NONE) {
        return ZR_NULL;
    }
    for (TZrSize index = 0; index < module->callableCount; index++) {
        if (module->callables[index].role == role) {
            return &module->callables[index];
        }
    }
    return ZR_NULL;
}

const SZrParserAttributeSchema *ZrParser_CompileTool_FindMetadataRole(
        const SZrParserCompileToolModuleDescriptor *module,
        EZrParserAttributeRole role) {
    const SZrParserAttributeSchema *schema;

    if (module == ZR_NULL || module->metadataRoles == ZR_NULL ||
        role == ZR_PARSER_ATTRIBUTE_ROLE_NONE) {
        return ZR_NULL;
    }
    for (TZrSize index = 0; index < module->metadataRoleCount; index++) {
        if (module->metadataRoles[index] != role) {
            continue;
        }
        schema = ZrParser_AttributeContract_FindBuiltinByRole(role);
        if (schema != ZR_NULL && strcmp(schema->ownerModule, module->moduleName) == 0 &&
            schema->providerPhase == module->providerPhase) {
            return schema;
        }
    }
    return ZR_NULL;
}

TZrUInt64 ZrParser_CompileTool_ComputePublicContractHash(
        const SZrParserCompileToolModuleDescriptor *module) {
    TZrUInt64 hash = ZR_COMPILE_TOOL_HASH_OFFSET;
    const TZrByte *cursor;

    if (module == ZR_NULL || module->canonicalContract == ZR_NULL) {
        return 0U;
    }

    cursor = (const TZrByte *)module->canonicalContract;
    while (*cursor != 0U) {
        hash ^= (TZrUInt64)*cursor++;
        hash *= ZR_COMPILE_TOOL_HASH_PRIME;
    }
    return hash;
}
