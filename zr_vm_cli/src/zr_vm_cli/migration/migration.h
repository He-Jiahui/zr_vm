#ifndef ZR_VM_CLI_MIGRATION_H
#define ZR_VM_CLI_MIGRATION_H

#include <stdio.h>

#include "command/command.h"

int ZrCli_Migration_Run(const SZrCliCommand *command, FILE *output, FILE *errorOutput);

#endif
