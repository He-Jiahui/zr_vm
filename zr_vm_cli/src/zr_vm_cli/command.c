#include "zr_vm_cli/command.h"

#include <stdarg.h>
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

void ZrCli_Command_WriteHelp(FILE *stream, const TZrChar *programName) {
    const TZrChar *name = programName != ZR_NULL ? programName : "zr_vm_cli";

    if (stream == ZR_NULL) {
        return;
    }

    fprintf(stream,
            "Usage:\n"
            "  %s\n"
            "  %s <project.zrp>\n"
            "  %s --compile <project.zrp> [--intermediate] [--incremental] [--run]\n"
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
            "  --run                     Run the compiled project with binary-first loading.\n"
            "  --help                    Show this help text.\n"
            "\n"
            "Examples:\n"
            "  %s\n"
            "  %s hello_world.zrp\n"
            "  %s --compile hello_world.zrp --intermediate\n"
            "  %s --compile hello_world.zrp --incremental --run\n",
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
    outCommand->projectPath = ZR_NULL;
    outCommand->runAfterCompile = ZR_FALSE;
    outCommand->emitIntermediate = ZR_FALSE;
    outCommand->incremental = ZR_FALSE;

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

        if (strcmp(argument, "--run") == 0) {
            outCommand->runAfterCompile = ZR_TRUE;
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
            outCommand->runAfterCompile) {
            zr_cli_write_error(errorBuffer, errorBufferSize, "--help cannot be combined with other options");
            return ZR_FALSE;
        }

        outCommand->mode = ZR_CLI_MODE_HELP;
        return ZR_TRUE;
    }

    if (!compileSeen && (outCommand->emitIntermediate || outCommand->incremental || outCommand->runAfterCompile)) {
        zr_cli_write_error(errorBuffer,
                           errorBufferSize,
                           "--run, --intermediate, and --incremental require --compile <project.zrp>");
        return ZR_FALSE;
    }

    if (compileSeen && positionalPath != ZR_NULL) {
        zr_cli_write_error(errorBuffer, errorBufferSize, "Unexpected positional argument: %s", positionalPath);
        return ZR_FALSE;
    }

    if (compileSeen) {
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
