#ifndef ZR_VM_LANGUAGE_SERVER_WASM_DIAGNOSTIC_JSON_H
#define ZR_VM_LANGUAGE_SERVER_WASM_DIAGNOSTIC_JSON_H

struct cJSON;
struct SZrArray;
struct SZrState;
struct SZrString;

/** @brief 将 native 诊断存储投影成浏览器 worker 使用的 LSP 数组。
 * @pre diagnostics 在序列化期间有效；本函数只借用其元素和 uri。
 * @return 独立的 cJSON 数组交调用方接管；原始诊断只须在本函数执行期间有效。
 */
cJSON *ZrLanguageServer_Wasm_SerializeDiagnostics(
        SZrState *state,
        SZrArray *diagnostics,
        const SZrString *uri);

#endif // ZR_VM_LANGUAGE_SERVER_WASM_DIAGNOSTIC_JSON_H
