#ifndef ZR_VM_CLI_MIGRATION_H
#define ZR_VM_CLI_MIGRATION_H

#include <stdio.h>

#include "command/command.h"

/** @brief 按已解析的 migrate syntax 请求报告或写入可机器应用的迁移建议。
 * @pre 输出流由调用方持有，command 须选择 check 或 write 且路径有效。
 * @return 0 表示所有选中文件完成，1 表示验证、读写或迁移失败。
 */
int ZrCli_Migration_Run(const SZrCliCommand *command, FILE *output, FILE *errorOutput);

#endif
