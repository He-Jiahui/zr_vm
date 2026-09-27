#ifndef ZR_VM_LANGUAGE_SERVER_STDIO_REQUEST_PROGRESS_H
#define ZR_VM_LANGUAGE_SERVER_STDIO_REQUEST_PROGRESS_H

#include "cJSON/cJSON.h"

#include "zr_vm_language_server/conf.h"

/** @brief 请求级进度状态属于 stdio server；本接口仅借用请求消息内的 token。 */
typedef struct SZrStdioServer SZrStdioServer;

/** @brief 清除上一个请求借用的 token 和已发布 begin 标记。 */
void stdio_request_progress_clear(SZrStdioServer *server);
/** @brief 从支持进度的方法参数中借用 token，并校验其 JSON-RPC 表示。
 * @pre params 所属消息树在 end/clear 前有效；同一 server 串行处理请求。
 * @return token 无效时返回 false，调用方应返回 Invalid params。
 */
TZrBool stdio_request_progress_prepare(SZrStdioServer *server,
                                        const char *method,
                                        const cJSON *params);
/** @brief 在处理器运行前发送 work-done begin；仅发送成功才记为已开始。 */
TZrBool stdio_request_progress_begin(SZrStdioServer *server, const char *method);
/** @brief 为已开始的进度发送 end，并结束 token 借用期。 */
void stdio_request_progress_end(SZrStdioServer *server);
/** @brief 在最终响应前分批发布支持的结果，并用协议要求的空完成值替换原结果。
 * @pre inOutResult 指向调用方拥有的 JSON 树；失败时仍由调用方拥有原树。
 * @return false 表示发布、构造或取消中断；调用方负责发送错误响应并释放原树。
 */
TZrBool stdio_request_progress_publish_partial_result(SZrStdioServer *server,
                                                      const char *method,
                                                      cJSON **inOutResult);

#endif
