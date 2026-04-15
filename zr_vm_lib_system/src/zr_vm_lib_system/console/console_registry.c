//
// zr.system.console descriptor registry.
//

#include "zr_vm_lib_system/console_registry.h"

#ifndef ZR_ARRAY_COUNT
#define ZR_ARRAY_COUNT(value) (sizeof(value) / sizeof((value)[0]))
#endif

const ZrLibModuleDescriptor *ZrSystem_ConsoleRegistry_GetModule(void) {
    static const ZrLibFunctionDescriptor kFunctions[] = {
            {"print", 1, 1, ZrSystem_Console_Print, "null", "Print text to stdout without a trailing newline.", ZR_NULL, 0},
            {"printLine", 1, 1, ZrSystem_Console_PrintLine, "null", "Print text to stdout with a trailing newline.", ZR_NULL, 0},
            {"printError", 1, 1, ZrSystem_Console_PrintError, "null", "Print text to stderr without a trailing newline.", ZR_NULL, 0},
            {"printErrorLine", 1, 1, ZrSystem_Console_PrintErrorLine, "null", "Print text to stderr with a trailing newline.", ZR_NULL, 0},
            {"read", 0, 0, ZrSystem_Console_Read, "string?", "Read one UTF-8 code point from stdin and return null on EOF.", ZR_NULL, 0},
            {"readLine", 0, 0, ZrSystem_Console_ReadLine, "string?", "Read one UTF-8 line from stdin and return null on EOF before any bytes.", ZR_NULL, 0},
    };
    static const ZrLibTypeHintDescriptor kHints[] = {
            {"print", "function", "print(value: any): null", "Print text to stdout without a trailing newline."},
            {"printLine", "function", "printLine(value: any): null", "Print text to stdout with a trailing newline."},
            {"printError", "function", "printError(value: any): null", "Print text to stderr without a trailing newline."},
            {"printErrorLine", "function", "printErrorLine(value: any): null", "Print text to stderr with a trailing newline."},
            {"read", "function", "read(): string?", "Read one UTF-8 code point from stdin and return null on EOF."},
            {"readLine", "function", "readLine(): string?", "Read one UTF-8 line from stdin and return null on EOF before any bytes."},
    };
    static const TZrChar kHintsJson[] =
            "{\n"
            "  \"schema\": \"zr.native.hints/v1\",\n"
            "  \"module\": \"zr.system.console\"\n"
            "}\n";
    static const ZrLibModuleDescriptor kModule = {
            ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
            "zr.system.console",
            ZR_NULL,
            0,
            kFunctions,
            ZR_ARRAY_COUNT(kFunctions),
            ZR_NULL,
            0,
            kHints,
            ZR_ARRAY_COUNT(kHints),
            kHintsJson,
            "Console I/O helpers.",
            ZR_NULL,
            0,
            "1.0.0",
            ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
            0,
    };

    return &kModule;
}
