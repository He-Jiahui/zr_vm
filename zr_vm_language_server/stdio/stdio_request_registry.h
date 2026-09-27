#ifndef ZR_VM_LANGUAGE_SERVER_STDIO_REQUEST_REGISTRY_H
#define ZR_VM_LANGUAGE_SERVER_STDIO_REQUEST_REGISTRY_H

#include "cJSON/cJSON.h"

#include "zr_vm_language_server/conf.h"

/** @brief 由读线程预留、取消，主线程查询和完成的请求 ID 注册表。 */
typedef struct SZrStdioRequestRegistry SZrStdioRequestRegistry;

/** @brief 区分已预留、同类同值 ID 冲突与无法建立预留的情况。 */
typedef enum EZrStdioRequestReservation {
    ZR_STDIO_REQUEST_RESERVATION_NONE = 0,
    ZR_STDIO_REQUEST_RESERVATION_ACCEPTED,
    ZR_STDIO_REQUEST_RESERVATION_DUPLICATE,
    ZR_STDIO_REQUEST_RESERVATION_FAILED,
} EZrStdioRequestReservation;

/** @brief 建立供 stdio 读线程和请求处理线程共享的注册表。
 * @return 成功时返回需由 Free 销毁的对象；分配或锁初始化失败时返回 NULL。
 */
SZrStdioRequestRegistry *ZrLanguageServer_StdioRequestRegistry_New(void);
/** @brief 销毁所有未完成预留及其 ID 副本。
 * @pre 读线程已停止且没有线程正在访问注册表。
 */
void ZrLanguageServer_StdioRequestRegistry_Free(SZrStdioRequestRegistry *registry);
/** @brief 在消息入队前预留请求 ID，使后续取消通知能作用于排队请求。
 * @pre id 是 JSON-RPC 信封校验接受的 null、数字或字符串；直到返回前保持有效。
 * @return 重复 ID 与内存/参数失败分别返回 DUPLICATE、FAILED；成功预留须调用 Complete。
 */
EZrStdioRequestReservation ZrLanguageServer_StdioRequestRegistry_Reserve(
        SZrStdioRequestRegistry *registry,
        const cJSON *id);
/** @brief 从读线程标记已预留请求取消，供处理线程协作式轮询。
 * @return 仅在当前存在同类型、同值 ID 的预留时返回 true。
 */
TZrBool ZrLanguageServer_StdioRequestRegistry_Cancel(SZrStdioRequestRegistry *registry,
                                                      const cJSON *id);
/** @brief 查询当前请求是否被取消；未知或无效 ID 均视为未取消。 */
TZrBool ZrLanguageServer_StdioRequestRegistry_IsCancelled(
        SZrStdioRequestRegistry *registry,
        const cJSON *id);
/** @brief 请求处理结束后释放预留，以允许后续请求复用该 ID。
 * @pre 传入的 id 在调用期间有效；完成与取消通知的先后由注册表锁决定。
 */
void ZrLanguageServer_StdioRequestRegistry_Complete(SZrStdioRequestRegistry *registry,
                                                     const cJSON *id);

#endif
