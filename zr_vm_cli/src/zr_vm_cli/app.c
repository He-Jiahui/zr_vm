#include "zr_vm_cli/app.h"

#include "zr_vm_cli/command.h"
#include "zr_vm_cli/compiler.h"
#include "zr_vm_cli/repl.h"
#include "zr_vm_cli/runtime.h"
#include "zr_vm_cli/conf.h"
#include "zr_vm_core/log.h"

#include <stdio.h>

static int zr_cli_app_maybe_run_interactive_tail(const SZrCliCommand *command, int result) {
    if (result != 0 || command == ZR_NULL || !command->interactiveAfterRun) {
        return result;
    }

    return ZrCli_Repl_Run();
}

int ZrCli_App_Run(int argc, char **argv) {
    SZrCliCommand command;
    TZrChar error[ZR_CLI_ERROR_BUFFER_LENGTH];

    if (!ZrCli_Command_Parse(argc, argv, &command, error, sizeof(error))) {
        ZrCore_Log_Error(ZR_NULL, "%s\n", error);
        ZrCli_Command_LogHelp(ZR_NULL, ZR_TRUE, argc > 0 ? argv[0] : "zr_vm_cli");
        return 1;
    }

    switch (command.mode) {
        case ZR_CLI_MODE_HELP:
            ZrCli_Command_LogHelp(ZR_NULL, ZR_FALSE, argc > 0 ? argv[0] : "zr_vm_cli");
            return 0;

        case ZR_CLI_MODE_VERSION:
            ZrCli_Command_LogVersion(ZR_NULL, ZR_FALSE);
            return 0;

        case ZR_CLI_MODE_REPL:
            return ZrCli_Repl_Run();

        case ZR_CLI_MODE_RUN_PROJECT:
        case ZR_CLI_MODE_RUN_PROJECT_MODULE:
            return zr_cli_app_maybe_run_interactive_tail(&command, ZrCli_Runtime_RunProject(&command));

        case ZR_CLI_MODE_RUN_INLINE:
            return zr_cli_app_maybe_run_interactive_tail(&command, ZrCli_Runtime_RunInline(&command));

        case ZR_CLI_MODE_COMPILE_PROJECT: {
            int compileResult = ZrCli_Compiler_CompileProject(&command);
            if (compileResult != 0) {
                return compileResult;
            }
            if (command.runAfterCompile) {
                return zr_cli_app_maybe_run_interactive_tail(&command, ZrCli_Runtime_RunProject(&command));
            }
            return 0;
        }

        default:
            ZrCore_Log_Error(ZR_NULL, "unknown CLI mode: %d\n", (int)command.mode);
            return 1;
    }
}
