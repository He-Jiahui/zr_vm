//
// zr.system.console descriptor registry.
//

#include "zr_vm_lib_system/console_registry.h"

#ifndef ZR_ARRAY_COUNT
#define ZR_ARRAY_COUNT(value) (sizeof(value) / sizeof((value)[0]))
#endif

/** 返回静态只读 module descriptor；不分配、不转移嵌套数组或 provider image 的所有权。 */
const ZrLibModuleDescriptor *ZrSystem_ConsoleRegistry_GetModule(void) {
    /* 回调表约束脚本可见名称、元数和 callback 地址；提示表给编译器/工具投影相同 API。静态嵌套数据借用本 provider image。 */
    static const ZrLibFunctionDescriptor kFunctions[] = {
            {"print", 1, 1, ZrSystem_Console_Print, "null", "Print text to stdout without a trailing newline.", ZR_NULL, 0},
            {"printLine", 1, 1, ZrSystem_Console_PrintLine, "null", "Print text to stdout with a trailing newline.", ZR_NULL, 0},
            {"printError", 1, 1, ZrSystem_Console_PrintError, "null", "Print text to stderr without a trailing newline.", ZR_NULL, 0},
            {"printErrorLine", 1, 1, ZrSystem_Console_PrintErrorLine, "null", "Print text to stderr with a trailing newline.", ZR_NULL, 0},
            {"read", 0, 0, ZrSystem_Console_Read, "string?", "Read one UTF-8 code point from stdin and return null on EOF.", ZR_NULL, 0},
            {"readLine", 0, 0, ZrSystem_Console_ReadLine, "string?", "Read one UTF-8 line from stdin and return null on EOF before any bytes.", ZR_NULL, 0},
    };
    /* TODO: descriptor 后续 dispatchFlags 字段未显式初始化，C 静态初始化将其置零并映射为 GC_AWARE。
     * Read/ReadLine 的 fgetc 可阻塞，print 也可同步进入宿主 observer；这些阶段可能延迟并发全局 GC pause。
     * 需沿 native dispatch 的 safepoint 生命周期验证受影响范围，并设计拆分 VM 访问与阻塞段的安全方案；不能直接把整段
     * callback 标为 BLOCKING_DETACHED，因为 callback 在 I/O 前后仍访问 VM state、参数和结果槽。 */
    /* 静态工具提示与 callback 导出一一对应；由 metadata 投影读取，不参与 callback 元数检查。 */
    static const ZrLibTypeHintDescriptor kHints[] = {
            {"print", "function", "print(value: any): null", "Print text to stdout without a trailing newline."},
            {"printLine", "function", "printLine(value: any): null", "Print text to stdout with a trailing newline."},
            {"printError", "function", "printError(value: any): null", "Print text to stderr without a trailing newline."},
            {"printErrorLine", "function", "printErrorLine(value: any): null", "Print text to stderr with a trailing newline."},
            {"read", "function", "read(): string?", "Read one UTF-8 code point from stdin and return null on EOF."},
            {"readLine", "function", "readLine(): string?", "Read one UTF-8 line from stdin and return null on EOF before any bytes."},
    };
    /* 注册器可导出的 JSON 提示只声明 schema 与模块名，不是上述逐函数 hint 表的序列化副本。 */
    static const TZrChar kHintsJson[] =
            "{\n"
            "  \"schema\": \"zr.native.hints/v1\",\n"
            "  \"module\": \"zr.system.console\"\n"
            "}\n";
    /* module descriptor 浅引用下面的函数表、提示表、字符串和静态 callback 指针；注册器保存引用时 image 必须继续驻留。 */
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
