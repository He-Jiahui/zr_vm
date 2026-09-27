#ifndef ZR_VM_CLI_REPL_INPUT_SCAN_H
#define ZR_VM_CLI_REPL_INPUT_SCAN_H

#include "zr_vm_common.h"

/** @brief 统一 REPL 命令解析与表达式包装使用的 ASCII 空白边界。 */
TZrBool ZrCli_ReplInput_IsSpace(TZrChar ch);
/** @brief 返回输入中首个非空白字符，输入为 NULL 时原样返回。 */
const TZrChar *ZrCli_ReplInput_SkipSpace(const TZrChar *code);
/** @brief 匹配完整命令或语句关键字，避免把同前缀标识符当作命令。 */
TZrBool ZrCli_ReplInput_StartsWithKeyword(const TZrChar *code, const TZrChar *keyword);
/** @brief 识别单一赋值语句的保留扫描接口；目前主提交路径未调用它。
 *  @note TODO: 核对该接口是否仍需要独立存在；当前仅有声明和定义，扫描结果不影响包装决策。
 */
TZrBool ZrCli_ReplInput_IsSimpleAssignmentStatement(const TZrChar *code);
/** @brief 为 REPL 的裸表达式选择 return 包装；语句及不完整输入交由正式 parser 诊断。
 *  @note 这是输入启发式分类，不是语法验证；调用方不得据此判定源代码有效。
 */
TZrBool ZrCli_ReplInput_ShouldWrapExpression(const TZrChar *code);

#endif
