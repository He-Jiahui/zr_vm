#ifndef ZR_CLI_STRICT_RECORDS_H
#define ZR_CLI_STRICT_RECORDS_H

#include "zr_vm_parser/ast.h"
#include "zr_vm_strict/program.h"

typedef struct SZrCliStrictModuleInput {
    const TZrChar *key;
    const TZrChar *path;
    SZrAstNode *root; /* borrowed, with imports already canonicalized */
} SZrCliStrictModuleInput;

TZrBool ZrCli_StrictRecords_Compile(SZrState *state, const SZrCliStrictModuleInput *modules, TZrSize moduleCount,
                                    ZrStrictProgram *out, TZrChar *error, TZrSize errorSize);
#endif
