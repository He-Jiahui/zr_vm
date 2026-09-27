#ifndef ZR_VM_CLI_TEST_COMMAND_H
#define ZR_VM_CLI_TEST_COMMAND_H

#include <stdio.h>

#include "command/command.h"

/** @brief 执行 test 模式：发现测试清单，按用例创建隔离进程并汇总结果。
 * @pre command 来自成功的命令解析；executablePath 必须指向可再启动的 CLI。
 * @return 退出码由测试运行器语义决定；非零还可能表示发现或隔离启动失败。
 */
int ZrCli_TestCommand_Run(
        const SZrCliCommand *command,
        const TZrChar *executablePath,
        FILE *output,
        FILE *errorOutput);

#endif
