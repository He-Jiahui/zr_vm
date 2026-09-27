#ifndef ZR_VM_CLI_REPL_H
#define ZR_VM_CLI_REPL_H

/** @brief 从 CLI 的交互入口运行一段持续到 :quit 或输入结束的会话。
 *  @note 调用方只接收进程退出码；会话的 VM、提交记录和输入缓冲均由本函数持有并释放。
 */
int ZrCli_Repl_Run(void);

#endif
