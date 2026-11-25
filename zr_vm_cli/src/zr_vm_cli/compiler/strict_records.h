#ifndef ZR_CLI_STRICT_RECORDS_H
#define ZR_CLI_STRICT_RECORDS_H

#include "zr_vm_parser/ast.h"
#include "zr_vm_strict/program.h"

TZrBool ZrCli_StrictRecords_Compile(SZrState *state, const TZrChar *source, TZrSize length,
                                    const TZrChar *path, ZrStrictProgram *out,
                                    TZrChar *error, TZrSize errorSize);
#endif
