#ifndef ZR_VM_PARSER_COMPILER_USING_PLUGIN_GUARD_ESCAPE_H
#define ZR_VM_PARSER_COMPILER_USING_PLUGIN_GUARD_ESCAPE_H

#include "zr_vm_parser/ast.h"
#include "zr_vm_parser/compiler.h"

TZrBool ZrParser_Compiler_ValidateUsingPluginGuardEscape(SZrCompilerState *cs, SZrUsingStatement *stmt);
TZrBool ZrParser_Compiler_ValidateUsingPluginGuardEscapeBindings(SZrCompilerState *cs,
                                                                  SZrUsingStatement *stmt,
                                                                  SZrAstNodeArray *bindings,
                                                                  SZrString *moduleName);

#endif
