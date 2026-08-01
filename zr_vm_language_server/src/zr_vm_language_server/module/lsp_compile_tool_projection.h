#ifndef ZR_VM_LANGUAGE_SERVER_LSP_COMPILE_TOOL_PROJECTION_H
#define ZR_VM_LANGUAGE_SERVER_LSP_COMPILE_TOOL_PROJECTION_H

#include "zr_vm_library/native_binding.h"
#include "zr_vm_parser/compile_tool.h"

const ZrLibModuleDescriptor *ZrLanguageServer_LspCompileToolProjection_FindModule(
        const TZrChar *moduleName);

TZrBool ZrLanguageServer_LspCompileToolProjection_MatchesCanonical(
        const ZrLibModuleDescriptor *projection,
        const SZrParserCompileToolModuleDescriptor *canonical);

#endif
