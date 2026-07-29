//
// Compiler-owned descriptors for compile-only native modules.
//

#ifndef ZR_VM_PARSER_COMPILE_TOOL_H
#define ZR_VM_PARSER_COMPILE_TOOL_H

#include "zr_vm_parser/conf.h"
#include "zr_vm_library/zrm.h"

#define ZR_PARSER_COMPILE_TOOL_MODULE_BUILD "zr.compile"
#define ZR_PARSER_COMPILE_TOOL_MODULE_DECLARATION "zr.compile.declaration"
#define ZR_PARSER_COMPILE_TOOL_BUILD_PUBLIC_CONTRACT_HASH "fnv1a64:00907d878657beb9"

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
} SZrParserCompileToolModuleDescriptor;

ZR_PARSER_API const SZrParserCompileToolModuleDescriptor *ZrParser_CompileTool_FindModule(
        const TZrChar *moduleName);
ZR_PARSER_API TZrBool ZrParser_CompileTool_IsModuleName(const TZrChar *moduleName);
ZR_PARSER_API const SZrParserCompileToolCallableDescriptor *ZrParser_CompileTool_FindCallable(
        const SZrParserCompileToolModuleDescriptor *module,
        EZrParserCompileToolRole role);
ZR_PARSER_API TZrUInt64 ZrParser_CompileTool_ComputePublicContractHash(
        const SZrParserCompileToolModuleDescriptor *module);

#endif // ZR_VM_PARSER_COMPILE_TOOL_H
