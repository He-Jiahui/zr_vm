#ifndef ZR_VM_LANGUAGE_SERVER_WASM_RESPONSE_H
#define ZR_VM_LANGUAGE_SERVER_WASM_RESPONSE_H

#include "cJSON/cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 将 WASM 请求失败转换成 worker 可识别的 JSON 错误封装。
 * @return 由 cJSON 分配的字符串；默认 WASM hooks 下通过 wasm_free 释放，
 * 测试自定义 hooks 时须用对应 cJSON_free；分配失败返回 null。
 */
const char *ZrLanguageServer_Wasm_ErrorResponse(int code, const char *message);
/** @brief 将 LSP 业务结果封装为 JSON 成功响应，供 WASM 导出统一返回。
 * @note 无论封装成功与否都接管并销毁 data；返回字符串归调用方所有。
 */
const char *ZrLanguageServer_Wasm_SuccessResponse(cJSON *data);

#ifdef __cplusplus
}
#endif

#endif
