#include "zr_vm_cli/command.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

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

void ZrCli_Command_WriteHelp(FILE *stream, const TZrChar *programName) {
    const TZrChar *name = programName != ZR_NULL ? programName : "zr_vm_cli";

    if (stream == ZR_NULL) {
        return;
    }

    fprintf(stream,
            "Usage:\n"
            "  %s\n"
            "  %s <project.zrp> [--execution-mode interp|binary|aot_c|aot_llvm] [--require-aot-path] [--emit-executed-via]\n"
            "  %s --compile <project.zrp> [--intermediate] [--incremental] [--emit-aot-c] [--emit-aot-llvm] [--run] [--execution-mode interp|binary|aot_c|aot_llvm] [--require-aot-path] [--emit-executed-via]\n"
            "  %s --help\n"
            "\n"
            "Modes:\n"
            "  no arguments               Start the minimal REPL.\n"
            "  <project.zrp>             Run the project with source-first loading.\n"
            "  --compile <project.zrp>   Compile reachable project-local modules to .zro.\n"
            "\n"
            "Options:\n"
            "  --intermediate            Also emit .zri files next to .zro outputs.\n"
            "  --incremental             Use manifest-based incremental compilation.\n"
            "  --emit-aot-c              Also emit project-local AOT C sources/artifacts.\n"
            "  --emit-aot-llvm           Also emit project-local AOT LLVM IR artifacts.\n"
            "  --run                     Run the compiled project with binary-first loading.\n"
            "  --execution-mode <mode>   Select interp, binary, aot_c, or aot_llvm for run paths.\n"
            "  --require-aot-path        Fail if an aot_* run cannot prove the requested AOT path.\n"
            "  --emit-executed-via       Print executed_via=<mode> after a successful run.\n"
            "  --help                    Show this help text.\n"
            "\n"
            "Examples:\n"
            "  %s\n"
            "  %s hello_world.zrp\n"
            "  %s --compile hello_world.zrp --intermediate\n"
            "  %s --compile hello_world.zrp --incremental --emit-aot-c --run --execution-mode aot_c\n",
            name,
            name,
            name,
            name,
            name,
            name,
            name,
            name);
}

TZrBool ZrCli_Command_Parse(int argc,
                            char **argv,
                            SZrCliCommand *outCommand,
                            TZrChar *errorBuffer,
                            TZrSize errorBufferSize) {
    TZrBool compileSeen = ZR_FALSE;
    TZrBool helpSeen = ZR_FALSE;
    const TZrChar *compilePath = ZR_NULL;
    const TZrChar *positionalPath = ZR_NULL;
    int index;

    if (outCommand == ZR_NULL) {
        zr_cli_write_error(errorBuffer, errorBufferSize, "Output command is required.");
        return ZR_FALSE;
    }

    outCommand->mode = ZR_CLI_MODE_REPL;
    outCommand->executionMode = ZR_CLI_EXECUTION_MODE_INTERP;
    outCommand->projectPath = ZR_NULL;
    outCommand->runAfterCompile = ZR_FALSE;
    outCommand->emitIntermediate = ZR_FALSE;
    outCommand->incremental = ZR_FALSE;
    outCommand->emitAotC = ZR_FALSE;
    outCommand->emitAotLlvm = ZR_FALSE;
    outCommand->requireAotPath = ZR_FALSE;
    outCommand->emitExecutedVia = ZR_FALSE;

    if (errorBuffer != ZR_NULL && errorBufferSize > 0) {
        errorBuffer[0] = '\0';
    }

    for (index = 1; index < argc; index++) {
        const TZrChar *argument = argv[index];

        if (strcmp(argument, "--help") == 0) {
            if (helpSeen) {
                zr_cli_write_error(errorBuffer, errorBufferSize, "Duplicate mode option: --help");
                return ZR_FALSE;
            }
            helpSeen = ZR_TRUE;
            continue;
        }

        if (strcmp(argument, "--compile") == 0) {
            if (compileSeen) {
                zr_cli_write_error(errorBuffer, errorBufferSize, "Duplicate mode option: --compile");
                return ZR_FALSE;
            }
            if (index + 1 >= argc) {
                zr_cli_write_error(errorBuffer, errorBufferSize, "Missing <project.zrp> after --compile");
                return ZR_FALSE;
            }
            if (argv[index + 1][0] == '-') {
                zr_cli_write_error(errorBuffer, errorBufferSize, "Missing <project.zrp> after --compile");
                return ZR_FALSE;
            }

            compileSeen = ZR_TRUE;
            compilePath = argv[++index];
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

        if (strcmp(argument, "--execution-mode") == 0) {
            if (index + 1 >= argc) {
                zr_cli_write_error(errorBuffer, errorBufferSize, "Missing execution mode after --execution-mode");
                return ZR_FALSE;
            }
            if (!zr_cli_command_parse_execution_mode(argv[index + 1], &outCommand->executionMode)) {
                zr_cli_write_error(errorBuffer,
                                   errorBufferSize,
                                   "Unknown execution mode: %s",
                                   argv[index + 1]);
                return ZR_FALSE;
            }
            index += 1;
            continue;
        }

        if (argument[0] == '-') {
            zr_cli_write_error(errorBuffer, errorBufferSize, "Unknown option: %s", argument);
            return ZR_FALSE;
        }

        if (positionalPath != ZR_NULL) {
            zr_cli_write_error(errorBuffer, errorBufferSize, "Unexpected positional argument: %s", argument);
            return ZR_FALSE;
        }

        positionalPath = argument;
    }

    if (helpSeen) {
        if (compileSeen || positionalPath != ZR_NULL || outCommand->emitIntermediate || outCommand->incremental ||
            outCommand->runAfterCompile || outCommand->emitAotC || outCommand->emitAotLlvm ||
            outCommand->requireAotPath || outCommand->emitExecutedVia ||
            outCommand->executionMode != ZR_CLI_EXECUTION_MODE_INTERP) {
            zr_cli_write_error(errorBuffer, errorBufferSize, "--help cannot be combined with other options");
            return ZR_FALSE;
        }

        outCommand->mode = ZR_CLI_MODE_HELP;
        return ZR_TRUE;
    }

    if (!compileSeen && (outCommand->emitIntermediate || outCommand->incremental || outCommand->runAfterCompile ||
                         outCommand->emitAotC || outCommand->emitAotLlvm)) {
        zr_cli_write_error(errorBuffer,
                           errorBufferSize,
                           "--run, --intermediate, --incremental, --emit-aot-c, and --emit-aot-llvm require --compile <project.zrp>");
        return ZR_FALSE;
    }

    if (compileSeen && positionalPath != ZR_NULL) {
        zr_cli_write_error(errorBuffer, errorBufferSize, "Unexpected positional argument: %s", positionalPath);
        return ZR_FALSE;
    }

    if (!compileSeen && positionalPath == ZR_NULL &&
        (outCommand->requireAotPath || outCommand->emitExecutedVia ||
         outCommand->executionMode != ZR_CLI_EXECUTION_MODE_INTERP)) {
        zr_cli_write_error(errorBuffer,
                           errorBufferSize,
                           "--execution-mode, --require-aot-path, and --emit-executed-via require a project run path");
        return ZR_FALSE;
    }

    if (compileSeen && !outCommand->runAfterCompile &&
        (outCommand->requireAotPath || outCommand->emitExecutedVia ||
         outCommand->executionMode != ZR_CLI_EXECUTION_MODE_INTERP)) {
        zr_cli_write_error(errorBuffer,
                           errorBufferSize,
                           "--execution-mode, --require-aot-path, and --emit-executed-via require an active run path");
        return ZR_FALSE;
    }

    if (outCommand->requireAotPath && !zr_cli_command_execution_mode_is_aot(outCommand->executionMode)) {
        zr_cli_write_error(errorBuffer,
                           errorBufferSize,
                           "--require-aot-path requires --execution-mode aot_c or aot_llvm");
        return ZR_FALSE;
    }

    if (compileSeen) {
        if (outCommand->runAfterCompile &&
            outCommand->executionMode == ZR_CLI_EXECUTION_MODE_INTERP) {
            outCommand->executionMode = ZR_CLI_EXECUTION_MODE_BINARY;
        }
        outCommand->mode = ZR_CLI_MODE_COMPILE_PROJECT;
        outCommand->projectPath = compilePath;
        return ZR_TRUE;
    }

    if (positionalPath != ZR_NULL) {
        outCommand->mode = ZR_CLI_MODE_RUN_PROJECT;
        outCommand->projectPath = positionalPath;
        return ZR_TRUE;
    }

    outCommand->mode = ZR_CLI_MODE_REPL;
    return ZR_TRUE;
}
