#ifndef ZR_VM_CLI_COMMAND_H
#define ZR_VM_CLI_COMMAND_H

#include "zr_vm_cli/conf.h"
#include "commands/explain_optimize_command.h"

struct SZrState;

/** @brief CLI 解析完成后交给 app 分派的互斥主路径；数值不是持久化格式。 */
typedef enum EZrCliMode {
    ZR_CLI_MODE_HELP = 0,
    ZR_CLI_MODE_VERSION = 1,
    ZR_CLI_MODE_REPL = 2,
    ZR_CLI_MODE_RUN_PROJECT = 3,
    ZR_CLI_MODE_COMPILE_PROJECT = 4,
    ZR_CLI_MODE_RUN_INLINE = 5,
    ZR_CLI_MODE_RUN_PROJECT_MODULE = 6,
    ZR_CLI_MODE_DUMP_ZRP_METADATA = 7,
    ZR_CLI_MODE_DIFF_ZRP_METADATA = 8,
    ZR_CLI_MODE_CHECK_ZRP_METADATA_VERSION = 9,
    ZR_CLI_MODE_MIGRATE_SYNTAX = 10,
    ZR_CLI_MODE_TEST = 11,
    ZR_CLI_MODE_EXPLAIN_OPTIMIZE = 12
} EZrCliMode;

/** @brief 迁移报告的输出协议，由 migrate syntax 专用参数选择。 */
typedef enum EZrCliMigrationFormat {
    ZR_CLI_MIGRATION_FORMAT_JSON = 0,
    ZR_CLI_MIGRATION_FORMAT_TEXT = 1
} EZrCliMigrationFormat;

/** @brief 运行路径选择解释器或已编译二进制；编译后 --run 默认转为二进制。 */
typedef enum EZrCliExecutionMode {
    ZR_CLI_EXECUTION_MODE_INTERP = 0,
    ZR_CLI_EXECUTION_MODE_BINARY = 1
} EZrCliExecutionMode;

/** @brief 从 argv 到 app 分派的临时值对象。
 * @note 字符串与 programArgs 借用 argv；调用方必须让 argv 存活至命令执行结束。
 *       字段只对对应 mode 有意义，解析失败时不得分派部分填写的对象。
 */
typedef struct SZrCliCommand {
    EZrCliMode mode;
    EZrCliExecutionMode executionMode;
    const TZrChar *projectPath;
    const TZrChar *inlineCode;
    const TZrChar *inlineModeAlias;
    const TZrChar *moduleName;
    const TZrChar *zrpMetadataPath;
    const TZrChar *zrpMetadataBeforePath;
    const TZrChar *zrpMetadataAfterPath;
    const TZrChar *zrpMetadataVersionCheckPath;
    const TZrChar *migrationPath;
    const TZrChar *testPath;
    const TZrChar *testFilter;
    const TZrChar *const *programArgs;
    TZrSize programArgCount;
    TZrUInt32 testJobs;
    TZrUInt64 testTimeoutMilliseconds;
    const TZrChar *debugAddress;
    const TZrChar *profileOutputPath;
    const TZrChar *coverageOutputPath;
    const TZrChar *dumpBytecodeOutputPath;
    const TZrChar *heapSummaryOutputPath;
    TZrBool runAfterCompile;
    TZrBool interactiveAfterRun;
    TZrBool emitIntermediate;
    TZrBool emitZrm;
    TZrBool emitAotC;
    TZrBool incremental;
    TZrBool emitExecutedVia;
    TZrBool debugEnabled;
    TZrBool debugWait;
    TZrBool debugPrintEndpoint;
    TZrBool profileEnabled;
    TZrBool coverageEnabled;
    TZrBool dumpBytecodeEnabled;
    TZrBool heapSummaryEnabled;
    TZrBool migrationCheck;
    TZrBool migrationWrite;
    TZrBool migrationIncludeGenerated;
    TZrBool testList;
    EZrCliMigrationFormat migrationFormat;
    /* 查询条件按值传到分派层；优化记录存储由编译器或运行时持有。 */
    SZrCliExplainOptimizeOptions explainOptimize;
} SZrCliCommand;

/** @brief 将命令行语法和跨模式约束归一化为单一可分派命令。
 * @pre argc/argv 符合进程入口约定，argv[0..argc-1] 是有效字符串。
 * @return 失败时 errorBuffer 尽量给出诊断；outCommand 的部分字段不可继续使用。
 */
TZrBool ZrCli_Command_Parse(int argc,
                            char **argv,
                            SZrCliCommand *outCommand,
                            TZrChar *errorBuffer,
                            TZrSize errorBufferSize);

/** @brief 通过核心日志通道输出帮助，以便 CLI 与嵌入方沿用同一输出路由。 */
void ZrCli_Command_LogHelp(struct SZrState *state, TZrBool writeToStdErr, const TZrChar *programName);
/** @brief 通过核心日志通道输出构建版本。 */
void ZrCli_Command_LogVersion(struct SZrState *state, TZrBool writeToStdErr);

#endif
