#ifndef ZR_VM_CLI_REPL_INPUT_SCAN_H
#define ZR_VM_CLI_REPL_INPUT_SCAN_H

#include "zr_vm_common.h"

TZrBool ZrCli_ReplInput_IsSpace(TZrChar ch);
const TZrChar *ZrCli_ReplInput_SkipSpace(const TZrChar *code);
TZrBool ZrCli_ReplInput_StartsWithKeyword(const TZrChar *code, const TZrChar *keyword);
TZrBool ZrCli_ReplInput_IsSimpleAssignmentStatement(const TZrChar *code);
TZrBool ZrCli_ReplInput_ShouldWrapExpression(const TZrChar *code);

#endif
