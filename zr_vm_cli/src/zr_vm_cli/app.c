#include "zr_vm_cli/app.h"

#include "zr_vm_cli/command.h"
#include "zr_vm_cli/compiler.h"
#include "zr_vm_cli/repl.h"
#include "zr_vm_cli/runtime.h"

#include <stdio.h>

int ZrCli_App_Run(int argc, char **argv) {
    SZrCliCommand command;
    TZrChar error[512];

    if (!ZrCli_Command_Parse(argc, argv, &command, error, sizeof(error))) {
        fprintf(stderr, "%s\n", error);
        ZrCli_Command_WriteHelp(stderr, argc > 0 ? argv[0] : "zr_vm_cli");
        return 1;
    }

    switch (command.mode) {
        case ZR_CLI_MODE_HELP:
            ZrCli_Command_WriteHelp(stdout, argc > 0 ? argv[0] : "zr_vm_cli");
            return 0;

        case ZR_CLI_MODE_REPL:
            return ZrCli_Repl_Run();

        case ZR_CLI_MODE_RUN_PROJECT:
            return ZrCli_Runtime_RunProjectSourceFirst(command.projectPath);

        case ZR_CLI_MODE_COMPILE_PROJECT: {
            int compileResult = ZrCli_Compiler_CompileProject(&command);
            if (compileResult != 0) {
                return compileResult;
            }
            if (command.runAfterCompile) {
                return ZrCli_Runtime_RunProjectBinaryFirst(command.projectPath);
            }
            return 0;
        }

        default:
            fprintf(stderr, "unknown CLI mode: %d\n", (int) command.mode);
            return 1;
    }
}
