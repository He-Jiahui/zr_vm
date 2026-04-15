#ifndef ZR_VM_PARSER_PROJECT_IMPORTS_H
#define ZR_VM_PARSER_PROJECT_IMPORTS_H

#include "zr_vm_parser/conf.h"
#include "zr_vm_parser/ast.h"

#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"

ZR_PARSER_API TZrBool ZrParser_ProjectImports_CanonicalizeAst(SZrState *state,
                                                              SZrAstNode *ast,
                                                              SZrString *sourceName,
                                                              SZrString **outCurrentModuleKey,
                                                              TZrChar *errorBuffer,
                                                              TZrSize errorBufferSize,
                                                              SZrFileRange *outErrorLocation);

#endif
