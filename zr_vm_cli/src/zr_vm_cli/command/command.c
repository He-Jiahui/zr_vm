#include "command/command.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "zr_vm_common/zr_version_info.h"
#include "zr_vm_core/log.h"

typedef enum EZrCliPrimaryMode {
    ZR_CLI_PRIMARY_MODE_NONE = 0,
    ZR_CLI_PRIMARY_MODE_HELP = 1,
    ZR_CLI_PRIMARY_MODE_VERSION = 2,
    ZR_CLI_PRIMARY_MODE_POSITIONAL_PROJECT = 3,
    ZR_CLI_PRIMARY_MODE_COMPILE = 4,
    ZR_CLI_PRIMARY_MODE_INLINE = 5,
    ZR_CLI_PRIMARY_MODE_PROJECT = 6
} EZrCliPrimaryMode;

static void zr_cli_write_error(TZrChar *buffer, TZrSize bufferSize, const TZrChar *format, ...) {
    va_list args;

    if (buffer == ZR_NULL || bufferSize == 0 || format == ZR_NULL) {
        return;
    }

    va_start(args, format);
    vsnprintf(buffer, bufferSize, format, args);
    va_end(args);
    buffer[bufferSize - 1] = '\0';
}

static void zr_cli_command_init(SZrCliCommand *command) {
    if (command == ZR_NULL) {
        return;
    }

    command->mode = ZR_CLI_MODE_REPL;
    command->executionMode = ZR_CLI_EXECUTION_MODE_INTERP;
    command->projectPath = ZR_NULL;
    command->inlineCode = ZR_NULL;
    command->inlineModeAlias = ZR_NULL;
    command->moduleName = ZR_NULL;
    command->programArgs = ZR_NULL;
    command->programArgCount = 0;
    command->debugAddress = ZR_NULL;
    command->runAfterCompile = ZR_FALSE;
    command->interactiveAfterRun = ZR_FALSE;
    command->emitIntermediate = ZR_FALSE;
    command->incremental = ZR_FALSE;
    command->emitAotC = ZR_FALSE;
    command->emitAotLlvm = ZR_FALSE;
    command->requireAotPath = ZR_FALSE;
    command->emitExecutedVia = ZR_FALSE;
    command->debugEnabled = ZR_FALSE;
    command->debugWait = ZR_FALSE;
    command->debugPrintEndpoint = ZR_FALSE;
}

static TZrBool zr_cli_command_parse_execution_mode(const TZrChar *text, EZrCliExecutionMode *outMode) {
    if (text == ZR_NULL || outMode == ZR_NULL) {
        return ZR_FALSE;
    }

    if (strcmp(text, "interp") == 0) {
        *outMode = ZR_CLI_EXECUTION_MODE_INTERP;
        return ZR_TRUE;
    }
    if (strcmp(text, "binary") == 0) {
        *outMode = ZR_CLI_EXECUTION_MODE_BINARY;
        return ZR_TRUE;
    }
    if (strcmp(text, "aot_c") == 0) {
        *outMode = ZR_CLI_EXECUTION_MODE_AOT_C;
        return ZR_TRUE;
    }
    if (strcmp(text, "aot_llvm") == 0) {
        *outMode = ZR_CLI_EXECUTION_MODE_AOT_LLVM;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

static TZrBool zr_cli_command_execution_mode_is_aot(EZrCliExecutionMode mode) {
    return (TZrBool)(mode == ZR_CLI_EXECUTION_MODE_AOT_C || mode == ZR_CLI_EXECUTION_MODE_AOT_LLVM);
}

static TZrBool zr_cli_command_set_primary_mode(EZrCliPrimaryMode *currentMode,
                                               EZrCliPrimaryMode nextMode,
                                               const TZrChar *optionLabel,
                                               TZrChar *errorBuffer,
                                               TZrSize errorBufferSize) {
    if (currentMode == ZR_NULL || optionLabel == ZR_NULL) {
        zr_cli_write_error(errorBuffer, errorBufferSize, "Internal CLI parser error.");
        return ZR_FALSE;
    }

    if (*currentMode == ZR_CLI_PRIMARY_MODE_NONE) {
        *currentMode = nextMode;
        return ZR_TRUE;
    }

    if (*currentMode == nextMode) {
        zr_cli_write_error(errorBuffer, errorBufferSize, "Duplicate mode option: %s", optionLabel);
        return ZR_FALSE;
    }

    zr_cli_write_error(errorBuffer, errorBufferSize, "%s cannot be combined with other main modes", optionLabel);
    return ZR_FALSE;
}

static TZrBool zr_cli_command_is_help_alias(const TZrChar *argument) {
    return (TZrBool)(argument != ZR_NULL &&
                     (strcmp(argument, "--help") == 0 || strcmp(argument, "-h") == 0 || strcmp(argument, "-?") == 0));
}

static TZrBool zr_cli_command_is_version_alias(const TZrChar *argument) {
    return (TZrBool)(argument != ZR_NULL &&
                     (strcmp(argument, "--version") == 0 || strcmp(argument, "-V") == 0));
}

static TZrChar *zr_cli_command_format_help_text(const TZrChar *programName) {
    const TZrChar *name = programName != ZR_NULL ? programName : "zr_vm_cli";
    int requiredLength;
    TZrChar *buffer;

    requiredLength = snprintf(
            ZR_NULL,
            0,
            "Usage:\n"
            "  %s\n"
            "  %s <project.zrp> [run-options] [-- <args...>]\n"
            "  %s --compile <project.zrp> [compile-options] [--run [run-options]] [-- <args...>]\n"
            "  %s -e <code> [-- <args...>]\n"
            "  %s -c <code> [-- <args...>]\n"
            "  %s --project <project.zrp> -m <module> [run-options] [-- <args...>]\n"
            "  %s -h | --help | -?\n"
            "  %s -V | --version\n"
            "\n"
            "Main Modes:\n"
            "  no arguments                      Start the REPL.\n"
            "  <project.zrp>                    Run the project entry from the .zrp file.\n"
            "  --compile <project.zrp>          Compile reachable project-local modules.\n"
            "  -e <code>, -c <code>             Execute inline source with a bare global runtime.\n"
            "  --project <project.zrp> -m <m>   Run a specific module entry inside the project.\n"
            "\n"
            "Modifiers:\n"
            "  -i, --interactive                Run the selected mode, then enter a fresh REPL.\n"
            "  --execution-mode <mode>          Select interp, binary, aot_c, or aot_llvm for active run paths.\n"
            "  --require-aot-path               Fail if an aot_* run cannot prove the requested AOT path.\n"
            "  --emit-executed-via              Print executed_via=<mode> after a successful run.\n"
            "  --debug                          Start the project under the ZR debugger agent.\n"
            "  --debug-address <addr>           Bind the debugger agent to the given host:port.\n"
            "  --debug-wait                     Wait for the debugger client before running user code.\n"
            "  --debug-print-endpoint           Print the resolved debugger endpoint after startup.\n"
            "  --intermediate                   Also emit .zri files next to .zro outputs.\n"
            "  --incremental                    Use manifest-based incremental compilation.\n"
            "  --emit-aot-c                     Also emit project-local AOT C sources/artifacts.\n"
            "  --emit-aot-llvm                  Also emit project-local AOT LLVM IR artifacts.\n"
            "  --run                            Run the compiled project after a successful compile.\n"
            "\n"
            "Passthrough:\n"
            "  Use -- to stop CLI parsing. Everything after -- is injected into zr.system.process.arguments.\n"
            "  The injected array starts with an entry identifier, then your passthrough arguments.\n"
            "\n"
            "Examples:\n"
            "  %s\n"
            "  %s demo.zrp --execution-mode binary -- arg1 -x\n"
            "  %s --compile demo.zrp --run -- arg1 -x\n"
            "  %s -e \"return 1;\" -- foo bar\n"
            "  %s --project demo.zrp -m tools.seed --execution-mode binary -- foo bar\n",
            name,
            name,
            name,
            name,
            name,
            name,
            name,
            name,
            name,
            name,
            name,
            name,
            name);
    if (requiredLength < 0) {
        return ZR_NULL;
    }

    buffer = (TZrChar *)malloc((size_t)requiredLength + 1u);
    if (buffer == ZR_NULL) {
        return ZR_NULL;
    }

    snprintf(buffer,
             (size_t)requiredLength + 1u,
             "Usage:\n"
             "  %s\n"
             "  %s <project.zrp> [run-options] [-- <args...>]\n"
             "  %s --compile <project.zrp> [compile-options] [--run [run-options]] [-- <args...>]\n"
             "  %s -e <code> [-- <args...>]\n"
             "  %s -c <code> [-- <args...>]\n"
             "  %s --project <project.zrp> -m <module> [run-options] [-- <args...>]\n"
             "  %s -h | --help | -?\n"
             "  %s -V | --version\n"
             "\n"
             "Main Modes:\n"
             "  no arguments                      Start the REPL.\n"
             "  <project.zrp>                    Run the project entry from the .zrp file.\n"
             "  --compile <project.zrp>          Compile reachable project-local modules.\n"
             "  -e <code>, -c <code>             Execute inline source with a bare global runtime.\n"
             "  --project <project.zrp> -m <m>   Run a specific module entry inside the project.\n"
             "\n"
             "Modifiers:\n"
             "  -i, --interactive                Run the selected mode, then enter a fresh REPL.\n"
             "  --execution-mode <mode>          Select interp, binary, aot_c, or aot_llvm for active run paths.\n"
             "  --require-aot-path               Fail if an aot_* run cannot prove the requested AOT path.\n"
             "  --emit-executed-via              Print executed_via=<mode> after a successful run.\n"
             "  --debug                          Start the project under the ZR debugger agent.\n"
             "  --debug-address <addr>           Bind the debugger agent to the given host:port.\n"
             "  --debug-wait                     Wait for the debugger client before running user code.\n"
             "  --debug-print-endpoint           Print the resolved debugger endpoint after startup.\n"
             "  --intermediate                   Also emit .zri files next to .zro outputs.\n"
             "  --incremental                    Use manifest-based incremental compilation.\n"
             "  --emit-aot-c                     Also emit project-local AOT C sources/artifacts.\n"
             "  --emit-aot-llvm                  Also emit project-local AOT LLVM IR artifacts.\n"
             "  --run                            Run the compiled project after a successful compile.\n"
             "\n"
             "Passthrough:\n"
             "  Use -- to stop CLI parsing. Everything after -- is injected into zr.system.process.arguments.\n"
             "  The injected array starts with an entry identifier, then your passthrough arguments.\n"
             "\n"
             "Examples:\n"
             "  %s\n"
             "  %s demo.zrp --execution-mode binary -- arg1 -x\n"
             "  %s --compile demo.zrp --run -- arg1 -x\n"
             "  %s -e \"return 1;\" -- foo bar\n"
             "  %s --project demo.zrp -m tools.seed --execution-mode binary -- foo bar\n",
             name,
             name,
             name,
             name,
             name,
             name,
             name,
             name,
             name,
             name,
             name,
             name,
             name);
    return buffer;
}

void ZrCli_Command_LogHelp(struct SZrState *state, TZrBool writeToStdErr, const TZrChar *programName) {
    TZrChar *text = zr_cli_command_format_help_text(programName);

    if (text == ZR_NULL) {
        return;
    }

    ZrCore_Log_Write(state,
                     ZR_LOG_LEVEL_INFO,
                     writeToStdErr ? ZR_OUTPUT_CHANNEL_STDERR : ZR_OUTPUT_CHANNEL_STDOUT,
                     ZR_OUTPUT_KIND_HELP,
                     text);
    free(text);
}

void ZrCli_Command_LogVersion(struct SZrState *state, TZrBool writeToStdErr) {
    ZrCore_Log_Printf(state,
                      ZR_LOG_LEVEL_INFO,
                      writeToStdErr ? ZR_OUTPUT_CHANNEL_STDERR : ZR_OUTPUT_CHANNEL_STDOUT,
                      ZR_OUTPUT_KIND_META,
                      "%s\n",
                      ZR_VM_VERSION_FULL);
}

TZrBool ZrCli_Command_Parse(int argc,
                            char **argv,
                            SZrCliCommand *outCommand,
                            TZrChar *errorBuffer,
                            TZrSize errorBufferSize) {
    EZrCliPrimaryMode primaryMode = ZR_CLI_PRIMARY_MODE_NONE;
    TZrBool interactiveRequested = ZR_FALSE;
    TZrBool compileSeen = ZR_FALSE;
    TZrBool explicitProjectSeen = ZR_FALSE;
    TZrBool positionalSeen = ZR_FALSE;
    const TZrChar *compilePath = ZR_NULL;
    const TZrChar *explicitProjectPath = ZR_NULL;
    const TZrChar *positionalPath = ZR_NULL;
    int index;

    if (outCommand == ZR_NULL) {
        zr_cli_write_error(errorBuffer, errorBufferSize, "Output command is required.");
        return ZR_FALSE;
    }

    zr_cli_command_init(outCommand);
    if (errorBuffer != ZR_NULL && errorBufferSize > 0) {
        errorBuffer[0] = '\0';
    }

    for (index = 1; index < argc; index++) {
        const TZrChar *argument = argv[index];

        if (strcmp(argument, "--") == 0) {
            outCommand->programArgs = (const TZrChar *const *)(argv + index + 1);
            outCommand->programArgCount = (TZrSize)(argc - index - 1);
            break;
        }

        if (zr_cli_command_is_help_alias(argument)) {
            if (!zr_cli_command_set_primary_mode(&primaryMode,
                                                 ZR_CLI_PRIMARY_MODE_HELP,
                                                 argument,
                                                 errorBuffer,
                                                 errorBufferSize)) {
                return ZR_FALSE;
            }
            continue;
        }

        if (zr_cli_command_is_version_alias(argument)) {
            if (!zr_cli_command_set_primary_mode(&primaryMode,
                                                 ZR_CLI_PRIMARY_MODE_VERSION,
                                                 argument,
                                                 errorBuffer,
                                                 errorBufferSize)) {
                return ZR_FALSE;
            }
            continue;
        }

        if (strcmp(argument, "--compile") == 0) {
            if (!zr_cli_command_set_primary_mode(&primaryMode,
                                                 ZR_CLI_PRIMARY_MODE_COMPILE,
                                                 "--compile",
                                                 errorBuffer,
                                                 errorBufferSize)) {
                return ZR_FALSE;
            }
            if (index + 1 >= argc || argv[index + 1][0] == '-') {
                zr_cli_write_error(errorBuffer, errorBufferSize, "Missing <project.zrp> after --compile");
                return ZR_FALSE;
            }

            compileSeen = ZR_TRUE;
            compilePath = argv[++index];
            continue;
        }

        if (strcmp(argument, "--project") == 0) {
            if (!zr_cli_command_set_primary_mode(&primaryMode,
                                                 ZR_CLI_PRIMARY_MODE_PROJECT,
                                                 "--project",
                                                 errorBuffer,
                                                 errorBufferSize)) {
                return ZR_FALSE;
            }
            if (index + 1 >= argc || argv[index + 1][0] == '-') {
                zr_cli_write_error(errorBuffer, errorBufferSize, "Missing <project.zrp> after --project");
                return ZR_FALSE;
            }

            explicitProjectSeen = ZR_TRUE;
            explicitProjectPath = argv[++index];
            continue;
        }

        if (strcmp(argument, "-e") == 0 || strcmp(argument, "-c") == 0) {
            if (!zr_cli_command_set_primary_mode(&primaryMode,
                                                 ZR_CLI_PRIMARY_MODE_INLINE,
                                                 argument,
                                                 errorBuffer,
                                                 errorBufferSize)) {
                return ZR_FALSE;
            }
            if (index + 1 >= argc) {
                zr_cli_write_error(errorBuffer, errorBufferSize, "Missing inline code after %s", argument);
                return ZR_FALSE;
            }

            outCommand->inlineModeAlias = argument;
            outCommand->inlineCode = argv[++index];
            continue;
        }

        if (strcmp(argument, "-m") == 0) {
            if (outCommand->moduleName != ZR_NULL) {
                zr_cli_write_error(errorBuffer, errorBufferSize, "Duplicate mode option: -m");
                return ZR_FALSE;
            }
            if (index + 1 >= argc || argv[index + 1][0] == '-') {
                zr_cli_write_error(errorBuffer, errorBufferSize, "Missing <module> after -m");
                return ZR_FALSE;
            }

            outCommand->moduleName = argv[++index];
            continue;
        }

        if (strcmp(argument, "-i") == 0 || strcmp(argument, "--interactive") == 0) {
            interactiveRequested = ZR_TRUE;
            continue;
        }

        if (strcmp(argument, "--intermediate") == 0) {
            outCommand->emitIntermediate = ZR_TRUE;
            continue;
        }

        if (strcmp(argument, "--incremental") == 0) {
            outCommand->incremental = ZR_TRUE;
            continue;
        }

        if (strcmp(argument, "--emit-aot-c") == 0) {
            outCommand->emitAotC = ZR_TRUE;
            continue;
        }

        if (strcmp(argument, "--emit-aot-llvm") == 0) {
            outCommand->emitAotLlvm = ZR_TRUE;
            continue;
        }

        if (strcmp(argument, "--run") == 0) {
            outCommand->runAfterCompile = ZR_TRUE;
            continue;
        }

        if (strcmp(argument, "--require-aot-path") == 0) {
            outCommand->requireAotPath = ZR_TRUE;
            continue;
        }

        if (strcmp(argument, "--emit-executed-via") == 0) {
            outCommand->emitExecutedVia = ZR_TRUE;
            continue;
        }

        if (strcmp(argument, "--debug") == 0) {
            outCommand->debugEnabled = ZR_TRUE;
            continue;
        }

        if (strcmp(argument, "--debug-wait") == 0) {
            outCommand->debugWait = ZR_TRUE;
            continue;
        }

        if (strcmp(argument, "--debug-print-endpoint") == 0) {
            outCommand->debugPrintEndpoint = ZR_TRUE;
            continue;
        }

        if (strcmp(argument, "--debug-address") == 0) {
            if (index + 1 >= argc || argv[index + 1][0] == '-') {
                zr_cli_write_error(errorBuffer, errorBufferSize, "Missing address after --debug-address");
                return ZR_FALSE;
            }

            outCommand->debugAddress = argv[++index];
            continue;
        }

        if (strcmp(argument, "--execution-mode") == 0) {
            if (index + 1 >= argc) {
                zr_cli_write_error(errorBuffer, errorBufferSize, "Missing execution mode after --execution-mode");
                return ZR_FALSE;
            }
            if (!zr_cli_command_parse_execution_mode(argv[index + 1], &outCommand->executionMode)) {
                zr_cli_write_error(errorBuffer, errorBufferSize, "Unknown execution mode: %s", argv[index + 1]);
                return ZR_FALSE;
            }
            index += 1;
            continue;
        }

        if (argument[0] == '-') {
            zr_cli_write_error(errorBuffer, errorBufferSize, "Unknown option: %s", argument);
            return ZR_FALSE;
        }

        if (positionalSeen) {
            zr_cli_write_error(errorBuffer, errorBufferSize, "Unexpected positional argument: %s", argument);
            return ZR_FALSE;
        }

        if (!zr_cli_command_set_primary_mode(&primaryMode,
                                             ZR_CLI_PRIMARY_MODE_POSITIONAL_PROJECT,
                                             argument,
                                             errorBuffer,
                                             errorBufferSize)) {
            return ZR_FALSE;
        }

        positionalSeen = ZR_TRUE;
        positionalPath = argument;
    }

    if (!outCommand->debugEnabled &&
        (outCommand->debugAddress != ZR_NULL || outCommand->debugWait || outCommand->debugPrintEndpoint)) {
        zr_cli_write_error(errorBuffer,
                           errorBufferSize,
                           "--debug-address, --debug-wait, and --debug-print-endpoint require --debug");
        return ZR_FALSE;
    }

    if (!compileSeen && (outCommand->emitIntermediate || outCommand->incremental || outCommand->runAfterCompile ||
                         outCommand->emitAotC || outCommand->emitAotLlvm)) {
        zr_cli_write_error(errorBuffer,
                           errorBufferSize,
                           "--run, --intermediate, --incremental, --emit-aot-c, and --emit-aot-llvm require --compile <project.zrp>");
        return ZR_FALSE;
    }

    if (primaryMode == ZR_CLI_PRIMARY_MODE_HELP) {
        if (interactiveRequested || outCommand->emitIntermediate || outCommand->incremental || outCommand->runAfterCompile ||
            outCommand->emitAotC || outCommand->emitAotLlvm || outCommand->requireAotPath || outCommand->emitExecutedVia ||
            outCommand->debugEnabled || outCommand->debugWait || outCommand->debugPrintEndpoint ||
            outCommand->debugAddress != ZR_NULL || outCommand->executionMode != ZR_CLI_EXECUTION_MODE_INTERP ||
            outCommand->moduleName != ZR_NULL || outCommand->programArgCount > 0) {
            zr_cli_write_error(errorBuffer, errorBufferSize, "--help cannot be combined with other options");
            return ZR_FALSE;
        }

        outCommand->mode = ZR_CLI_MODE_HELP;
        return ZR_TRUE;
    }

    if (primaryMode == ZR_CLI_PRIMARY_MODE_VERSION) {
        if (interactiveRequested || outCommand->emitIntermediate || outCommand->incremental || outCommand->runAfterCompile ||
            outCommand->emitAotC || outCommand->emitAotLlvm || outCommand->requireAotPath || outCommand->emitExecutedVia ||
            outCommand->debugEnabled || outCommand->debugWait || outCommand->debugPrintEndpoint ||
            outCommand->debugAddress != ZR_NULL || outCommand->executionMode != ZR_CLI_EXECUTION_MODE_INTERP ||
            outCommand->moduleName != ZR_NULL || outCommand->programArgCount > 0) {
            zr_cli_write_error(errorBuffer, errorBufferSize, "--version cannot be combined with other options");
            return ZR_FALSE;
        }

        outCommand->mode = ZR_CLI_MODE_VERSION;
        return ZR_TRUE;
    }

    if (compileSeen && outCommand->moduleName != ZR_NULL) {
        zr_cli_write_error(errorBuffer, errorBufferSize, "-m cannot be combined with --compile");
        return ZR_FALSE;
    }

    if (explicitProjectSeen && outCommand->moduleName == ZR_NULL) {
        zr_cli_write_error(errorBuffer, errorBufferSize, "--project requires -m <module>");
        return ZR_FALSE;
    }

    if (!explicitProjectSeen && outCommand->moduleName != ZR_NULL) {
        zr_cli_write_error(errorBuffer, errorBufferSize, "-m requires --project <project.zrp>");
        return ZR_FALSE;
    }

    if (primaryMode == ZR_CLI_PRIMARY_MODE_INLINE &&
        (outCommand->emitIntermediate || outCommand->incremental || outCommand->runAfterCompile ||
         outCommand->emitAotC || outCommand->emitAotLlvm || outCommand->requireAotPath || outCommand->emitExecutedVia ||
         outCommand->debugEnabled || outCommand->debugWait || outCommand->debugPrintEndpoint ||
         outCommand->debugAddress != ZR_NULL || outCommand->executionMode != ZR_CLI_EXECUTION_MODE_INTERP ||
         outCommand->moduleName != ZR_NULL || compileSeen || explicitProjectSeen)) {
        zr_cli_write_error(errorBuffer,
                           errorBufferSize,
                           "Inline -e/-c mode does not support compile, debug, module, or execution-mode options");
        return ZR_FALSE;
    }

    if (compileSeen && !outCommand->runAfterCompile &&
        (outCommand->requireAotPath || outCommand->emitExecutedVia || outCommand->debugEnabled ||
         outCommand->executionMode != ZR_CLI_EXECUTION_MODE_INTERP || outCommand->programArgCount > 0)) {
        zr_cli_write_error(errorBuffer,
                           errorBufferSize,
                           "--execution-mode, --require-aot-path, --emit-executed-via, --debug, and user program arguments require an active run path");
        return ZR_FALSE;
    }

    if (primaryMode == ZR_CLI_PRIMARY_MODE_NONE &&
        (outCommand->requireAotPath || outCommand->emitExecutedVia || outCommand->debugEnabled ||
         outCommand->executionMode != ZR_CLI_EXECUTION_MODE_INTERP || outCommand->programArgCount > 0)) {
        zr_cli_write_error(errorBuffer,
                           errorBufferSize,
                           "--execution-mode, --require-aot-path, --emit-executed-via, --debug, and user program arguments require a project run path");
        return ZR_FALSE;
    }

    if (outCommand->requireAotPath && !zr_cli_command_execution_mode_is_aot(outCommand->executionMode)) {
        zr_cli_write_error(errorBuffer, errorBufferSize, "--require-aot-path requires --execution-mode aot_c or aot_llvm");
        return ZR_FALSE;
    }

    if (explicitProjectSeen && zr_cli_command_execution_mode_is_aot(outCommand->executionMode)) {
        zr_cli_write_error(errorBuffer,
                           errorBufferSize,
                           "--project ... -m only supports --execution-mode interp or binary in v1; aot_* is not supported");
        return ZR_FALSE;
    }

    switch (primaryMode) {
        case ZR_CLI_PRIMARY_MODE_INLINE:
            if (interactiveRequested) {
                outCommand->interactiveAfterRun = ZR_TRUE;
            }
            outCommand->mode = ZR_CLI_MODE_RUN_INLINE;
            return ZR_TRUE;

        case ZR_CLI_PRIMARY_MODE_COMPILE:
            if (interactiveRequested && !outCommand->runAfterCompile) {
                zr_cli_write_error(errorBuffer,
                                   errorBufferSize,
                                   "--interactive cannot be combined with help, version, or compile-only paths");
                return ZR_FALSE;
            }
            if (outCommand->runAfterCompile && outCommand->executionMode == ZR_CLI_EXECUTION_MODE_INTERP) {
                outCommand->executionMode = ZR_CLI_EXECUTION_MODE_BINARY;
            }
            if (interactiveRequested) {
                outCommand->interactiveAfterRun = ZR_TRUE;
            }
            outCommand->mode = ZR_CLI_MODE_COMPILE_PROJECT;
            outCommand->projectPath = compilePath;
            return ZR_TRUE;

        case ZR_CLI_PRIMARY_MODE_PROJECT:
            if (interactiveRequested) {
                outCommand->interactiveAfterRun = ZR_TRUE;
            }
            outCommand->mode = ZR_CLI_MODE_RUN_PROJECT_MODULE;
            outCommand->projectPath = explicitProjectPath;
            return ZR_TRUE;

        case ZR_CLI_PRIMARY_MODE_POSITIONAL_PROJECT:
            if (interactiveRequested) {
                outCommand->interactiveAfterRun = ZR_TRUE;
            }
            outCommand->mode = ZR_CLI_MODE_RUN_PROJECT;
            outCommand->projectPath = positionalPath;
            return ZR_TRUE;

        case ZR_CLI_PRIMARY_MODE_NONE:
            outCommand->mode = ZR_CLI_MODE_REPL;
            return ZR_TRUE;

        case ZR_CLI_PRIMARY_MODE_HELP:
        case ZR_CLI_PRIMARY_MODE_VERSION:
        default:
            zr_cli_write_error(errorBuffer, errorBufferSize, "Unknown CLI parse state.");
            return ZR_FALSE;
    }
}
