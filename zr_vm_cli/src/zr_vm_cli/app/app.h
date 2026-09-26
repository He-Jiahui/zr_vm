#ifndef ZR_VM_CLI_APP_H
#define ZR_VM_CLI_APP_H

/**
 * @brief 将参数解析为单一 CLI 模式，并把各命令结果作为进程状态返回。
 * @note argv 仅在本次调用中借用；运行模式由 command parser 验证后才分发。
 */
int ZrCli_App_Run(int argc, char **argv);

#endif
