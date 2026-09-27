#ifndef ZR_VM_PARSER_WRITER_BINARY_INTERNAL_H
#define ZR_VM_PARSER_WRITER_BINARY_INTERNAL_H

#include <stdio.h>

#include "zr_vm_parser/writer.h"

/** @brief 为 .zro 头计算整棵函数树是否带调试信息；调用方据此设置头标志。 */
TZrBool ZrParser_Writer_FunctionTreeHasDebugInfo(const SZrFunction *function);
/** @brief 在调用点缓存表之后写入与 core reader 对应的绑定契约小节。
 *  @pre file 可写且 function 的缓存长度与缓存数组一致。
 *  @return 所有小节行成功写入时为真；调用方仍须处理最终文件关闭错误。
 */
TZrBool ZrParser_Writer_WriteCallBindings(FILE *file, const SZrFunction *function);

/** @brief 递归写入入口及子函数的 .zro 函数体，供外层文件 writer 封装头部。
 *  @pre file 已打开且头部已写入；defaultName 仅在函数无名称时使用。
 */
TZrBool ZrParser_Writer_WriteIoFunction(SZrState *state,
                                        FILE *file,
                                        SZrFunction *function,
                                        const TZrChar *defaultName,
                                        const SZrBinaryWriterOptions *options);

#endif
