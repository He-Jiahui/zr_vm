#ifndef ZR_VM_CLI_TEST_COMMAND_H
#define ZR_VM_CLI_TEST_COMMAND_H

#include <stdio.h>

#include "command/command.h"

int ZrCli_TestCommand_Run(
        const SZrCliCommand *command,
        const TZrChar *executablePath,
        FILE *output,
        FILE *errorOutput);

#endif
