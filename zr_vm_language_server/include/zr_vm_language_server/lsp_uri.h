//
// Canonical LSP URI and native-path boundary helpers.
//

#ifndef ZR_VM_LANGUAGE_SERVER_LSP_URI_H
#define ZR_VM_LANGUAGE_SERVER_LSP_URI_H

#include "zr_vm_language_server/conf.h"
#include "zr_vm_core/string.h"

/** @brief 将 file URI 解析为本机绝对路径，供工作区、项目索引和源码读取共用同一文件身份。
 * @pre buffer 可写且 bufferSize 非零；虚拟文档方案不能进入本机文件 API。
 * @return 成功时写入以 NUL 结尾的路径；失败时调用方不得使用 buffer 中的内容。
 * @note Windows 上不具备绝对本机路径身份的 file URI 会被拒绝；调用方只可在返回真时读取输出。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspUri_FileToNativePath(SZrString *uri,
                                                                         TZrChar *buffer,
                                                                         TZrSize bufferSize);

/** @brief 把项目发现或导航得到的绝对本机路径转换为客户端可识别的 file URI。
 * @pre nativePath 是以 NUL 结尾的绝对路径；Windows UNC 路径须包含主机和共享名。
 * @return state 管理的 URI；路径不合法或编码失败时为 NULL。 */
ZR_LANGUAGE_SERVER_API SZrString *ZrLanguageServer_LspUri_FromNativePath(SZrState *state,
                                                                           const TZrChar *nativePath);

/** @brief 让打开文档、工作区根目录与项目记录按同一文件身份比较 URI。
 * @note file URI 经本机路径词法归一化比较，不解析符号链接；虚拟 URI 应按完整长度比较，当前实现有下述缺陷。
 * BUG: 当前实现先用 strcmp 比较 SZrString，含内嵌 NUL 的 URI 可能被误判为同一身份；参见协议实现。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspUri_Equivalent(SZrString *left,
                                                                    SZrString *right);

#endif // ZR_VM_LANGUAGE_SERVER_LSP_URI_H
