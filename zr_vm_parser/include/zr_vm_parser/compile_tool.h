//
// Compiler-owned descriptors for compile-only native modules.
//

#ifndef ZR_VM_PARSER_COMPILE_TOOL_H
#define ZR_VM_PARSER_COMPILE_TOOL_H

#include "zr_vm_parser/conf.h"
#include "zr_vm_parser/attribute_contract.h"
#include "zr_vm_library/zrm.h"

#define ZR_PARSER_COMPILE_TOOL_MODULE_BUILD "zr.compile"
#define ZR_PARSER_COMPILE_TOOL_MODULE_DECLARATION "zr.compile.declaration"
#define ZR_PARSER_COMPILE_TOOL_BUILD_PUBLIC_CONTRACT_HASH "fnv1a64:ca60a1b2107c893b"
#define ZR_PARSER_COMPILE_TOOL_DECLARATION_PUBLIC_CONTRACT_HASH "fnv1a64:2e5f7a42c6adbe21"

typedef enum EZrParserCompileToolRole {
    ZR_PARSER_COMPILE_TOOL_ROLE_NONE = 0,
    ZR_PARSER_COMPILE_TOOL_ROLE_BUILD_FEATURE = 1,
    ZR_PARSER_COMPILE_TOOL_ROLE_ASSERT = 2,
    ZR_PARSER_COMPILE_TOOL_ROLE_ERROR = 3,
    ZR_PARSER_COMPILE_TOOL_ROLE_WARNING = 4
} EZrParserCompileToolRole;

typedef enum EZrParserCompileToolEffect {
    ZR_PARSER_COMPILE_TOOL_EFFECT_PURE_VALUE = 0,
    ZR_PARSER_COMPILE_TOOL_EFFECT_DIAGNOSTIC = 1,
    ZR_PARSER_COMPILE_TOOL_EFFECT_DECLARATION_BUILD = 2
} EZrParserCompileToolEffect;

typedef enum EZrParserCompilePhase {
    ZR_PARSER_COMPILE_PHASE_BUILD_FACTS = 0,
    ZR_PARSER_COMPILE_PHASE_SIGNATURE = 1,
    ZR_PARSER_COMPILE_PHASE_EXPANSION = 2,
    ZR_PARSER_COMPILE_PHASE_LAYOUT = 3,
    ZR_PARSER_COMPILE_PHASE_LATE_CHECK = 4
} EZrParserCompilePhase;

typedef enum EZrParserCompileToolTypeRole {
    ZR_PARSER_COMPILE_TOOL_TYPE_NONE = 0,
    ZR_PARSER_COMPILE_TOOL_TYPE_SYMBOL_ID = 1,
    ZR_PARSER_COMPILE_TOOL_TYPE_DECLARATION_VIEW = 2,
    ZR_PARSER_COMPILE_TOOL_TYPE_TYPE_VIEW = 3,
    ZR_PARSER_COMPILE_TOOL_TYPE_CLASS_VIEW = 4,
    ZR_PARSER_COMPILE_TOOL_TYPE_STRUCT_VIEW = 5,
    ZR_PARSER_COMPILE_TOOL_TYPE_FUNCTION_VIEW = 6,
    ZR_PARSER_COMPILE_TOOL_TYPE_FIELD_VIEW = 7,
    ZR_PARSER_COMPILE_TOOL_TYPE_METHOD_VIEW = 8,
    ZR_PARSER_COMPILE_TOOL_TYPE_PROPERTY_VIEW = 9,
    ZR_PARSER_COMPILE_TOOL_TYPE_PARAMETER_VIEW = 10,
    ZR_PARSER_COMPILE_TOOL_TYPE_PATCH = 11,
    ZR_PARSER_COMPILE_TOOL_TYPE_GENERATED_DECLARATION = 12,
    ZR_PARSER_COMPILE_TOOL_TYPE_GENERATED_TYPE = 13,
    ZR_PARSER_COMPILE_TOOL_TYPE_GENERATED_FIELD = 14,
    ZR_PARSER_COMPILE_TOOL_TYPE_GENERATED_METHOD = 15,
    ZR_PARSER_COMPILE_TOOL_TYPE_GENERATED_PROPERTY = 16,
    ZR_PARSER_COMPILE_TOOL_TYPE_DIAGNOSTIC = 17
} EZrParserCompileToolTypeRole;

typedef struct SZrParserCompileToolTypeDescriptor {
    EZrParserCompileToolTypeRole role;
    const TZrChar *name;
    const TZrChar *qualifiedName;
    EZrParserCompilePhase minimumPhase;
    TZrBool immutableView;
} SZrParserCompileToolTypeDescriptor;

typedef struct SZrParserCompileToolCallableDescriptor {
    EZrParserCompileToolRole role;
    const TZrChar *qualifiedName;
    const TZrChar *returnTypeName;
    const TZrChar *const *parameterTypeNames;
    TZrSize parameterCount;
    EZrParserCompileToolEffect effect;
    EZrParserCompilePhase minimumPhase;
} SZrParserCompileToolCallableDescriptor;

typedef struct SZrParserCompileToolModuleDescriptor {
    const TZrChar *moduleName;
    EZrLibrary_ProviderPhase providerPhase;
    const TZrChar *publicContractHash;
    const TZrChar *canonicalContract;
    TZrUInt64 computedPublicContractHash;
    const SZrParserCompileToolCallableDescriptor *callables;
    TZrSize callableCount;
    const SZrParserCompileToolTypeDescriptor *types;
    TZrSize typeCount;
    const EZrParserAttributeRole *metadataRoles;
    TZrSize metadataRoleCount;
} SZrParserCompileToolModuleDescriptor;

ZR_PARSER_API const SZrParserCompileToolModuleDescriptor *ZrParser_CompileTool_FindModule(
        const TZrChar *moduleName);
ZR_PARSER_API TZrBool ZrParser_CompileTool_IsModuleName(const TZrChar *moduleName);
ZR_PARSER_API const SZrParserCompileToolCallableDescriptor *ZrParser_CompileTool_FindCallable(
        const SZrParserCompileToolModuleDescriptor *module,
        EZrParserCompileToolRole role);
ZR_PARSER_API const SZrParserCompileToolTypeDescriptor *ZrParser_CompileTool_FindType(
        const SZrParserCompileToolModuleDescriptor *module,
        const TZrChar *name);
ZR_PARSER_API const SZrParserAttributeSchema *ZrParser_CompileTool_FindMetadataRole(
        const SZrParserCompileToolModuleDescriptor *module,
        EZrParserAttributeRole role);
ZR_PARSER_API TZrUInt64 ZrParser_CompileTool_ComputePublicContractHash(
        const SZrParserCompileToolModuleDescriptor *module);

#endif // ZR_VM_PARSER_COMPILE_TOOL_H
