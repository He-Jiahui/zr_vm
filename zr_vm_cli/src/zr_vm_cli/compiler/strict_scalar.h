#ifndef ZR_CLI_STRICT_SCALAR_H
#define ZR_CLI_STRICT_SCALAR_H

#include "zr_vm_parser/ast.h"
#include "zr_vm_strict/program.h"

/* The current canonical producer covers a scalar script entry. Refuse any
 * unsupported source shape before the ordinary compiler can evaluate macros. */
TZrBool ZrCli_StrictScalar_SupportsSource(SZrAstNode *root);
ZrStrictProgramStatus ZrCli_StrictScalar_Compile(SZrState *state, const TZrChar *source, TZrSize length,
                                                const TZrChar *path, ZrStrictProgram *out);

#endif
