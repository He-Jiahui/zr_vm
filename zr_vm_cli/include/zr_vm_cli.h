//
// Created by HeJiahui on 2025/6/5.
//

#ifndef ZR_VM_CLI_H
#define ZR_VM_CLI_H

#include "zr_vm_cli/conf.h"

/**
 * @brief 预留的程序化 CLI 入口，与可执行文件使用同一命令分发路径。
 * @note argv 在调用期间保持有效；调用方需自行管理标准输入输出。
 * TODO: 当前仅构建和安装 CLI 可执行文件，仓库内无此入口调用方，且接口丢弃退出码；
 *       核查是否需要可链接库、头文件安装及返回状态，再确定其对外契约。
 */
ZR_CLI_API void ZrCli_Main(int argc, char **argv);

#endif //ZR_VM_CLI_H
