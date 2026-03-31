#ifndef ZR_VM_CLI_RUNTIME_H
#define ZR_VM_CLI_RUNTIME_H

#include "zr_vm_common.h"

int ZrCli_Runtime_RunProjectSourceFirst(const TZrChar *projectPath);
int ZrCli_Runtime_RunProjectBinaryFirst(const TZrChar *projectPath);

#endif
