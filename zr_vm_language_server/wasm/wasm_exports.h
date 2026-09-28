//
// Created by Auto on 2025/01/XX.
// WASM 导出函数头文件
//

#ifndef ZR_VM_LANGUAGE_SERVER_WASM_EXPORTS_H
#define ZR_VM_LANGUAGE_SERVER_WASM_EXPORTS_H

#include "zr_vm_common.h"

/** @brief 浏览器 worker 的 C ABI 声明；CMake 导出表和 cpp 的 KEEPALIVE 决定 Emscripten 可见符号。
 * @note 除显式释放函数和上下文管理外，查询入口统一返回 caller-owned JSON 字符串；
 * 当前 bridge 解码后经 Module._free 释放；ABI 亦提供同堆 wasm_free。URI、内容、查询词的长度按 UTF-8 字节计算，
 * LSP 行列按零基 UTF-16 位置计算；context 必须来自当前模块的 ContextNew。
 */
/* TODO: WASM_EXPORT 在本头文件声明及 cpp 定义中均未使用；非 Emscripten 的
 * export_name 分支是否真的提供预期 ABI，需核对独立工具链的导出清单。 */
#ifdef __EMSCRIPTEN__
    #ifdef __cplusplus
        // C++ 定义在 cpp 中单独使用 EMSCRIPTEN_KEEPALIVE，声明保持空属性。
        #define WASM_EXPORT
    #else
        // 若 C 实现使用本宏则保留符号；当前公开实现为 C++。
        #include <emscripten.h>
        #define WASM_EXPORT EMSCRIPTEN_KEEPALIVE
    #endif
#else
    // 非 Emscripten ABI 仍待验证，当前构建仅配置 Emscripten。
    #define WASM_EXPORT __attribute__((export_name))
#endif

// 如果没有定义 ZR_WASM_BUILD，则不导出
#ifndef ZR_WASM_BUILD
    #undef WASM_EXPORT
    #define WASM_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 为 JS 侧需要显式写入线性内存的调用分配缓冲区。
 * @note 当前 bridge 通过 ccall 的 string 参数自动编码，响应字符串仍需显式释放。
 */
void* wasm_malloc(size_t size);

/** @brief 释放 wasm_malloc 或 JSON 返回值；调用方不得在释放后继续读取指针。 */
void wasm_free(void* ptr);

/** @brief worker 初始化时建立单个 LSP 上下文；零表示全局状态初始化失败。 */
void* wasm_ZrLspContextNew(void);

/** @brief worker 退出时释放上下文；所有依赖该上下文的调用必须先结束。 */
void wasm_ZrLspContextFree(void* context);

/** @brief 提交完整文档快照并由 native LSP 更新增量解析状态。
 * @pre uriLen/contentLen 是对应缓冲区的 UTF-8 字节长度，version 应严格递增。
 * @return JSON 封装字符串，由调用方通过 wasm_free 释放；null 表示封装也分配失败。
 */
const char* wasm_ZrLspUpdateDocument(void* context, const char* uri, int uriLen, 
                                 const char* content, int contentLen, int version);

/** @brief 在 didClose 队列末尾移除解析器与分析器对该 URI 的状态。 */
const char* wasm_ZrLspCloseDocument(void* context, const char* uri, int uriLen);

/** @brief 返回原始诊断数组；worker 常规路径优先使用带 resultId 的报告接口。 */
const char* wasm_ZrLspGetDiagnostics(void* context, const char* uri, int uriLen);

/** @brief 拉取单文档诊断报告及内容身份，供 worker 去重和版本围栏使用。 */
const char* wasm_ZrLspGetDiagnosticReport(void* context, const char* uri, int uriLen);

/** @brief 枚举当前工作区文档并构造与单文档报告一致的诊断投影。 */
const char* wasm_ZrLspGetWorkspaceDiagnosticReports(void* context);

/** @brief 将当前快照的语义补全转成 LSP CompletionItem 数组。
 * @pre line/character 是 LSP 零基 UTF-16 位置；输入长度均以 UTF-8 字节计算。
 */
const char* wasm_ZrLspGetCompletion(void* context, const char* uri, int uriLen,
                               int line, int character);

/** @brief 获取普通 Markdown 悬停；无命中以成功封装的 null 表示。 */
const char* wasm_ZrLspGetHover(void* context, const char* uri, int uriLen,
                          int line, int character);

/** @brief 获取含 section 角色的结构化悬停，供扩展专用视图消费。 */
const char* wasm_ZrLspGetRichHover(void* context, const char* uri, int uriLen,
                              int line, int character);

/** @brief 定位定义；无符号时使用空位置数组维持 LSP 查询语义。 */
const char* wasm_ZrLspGetDefinition(void* context, const char* uri, int uriLen,
                                int line, int character);

/** @brief 根据光标查找引用；includeDeclaration 控制是否计入定义处。 */
const char* wasm_ZrLspFindReferences(void* context, const char* uri, int uriLen,
                               int line, int character, int includeDeclaration);

/** @brief 请求 native 重命名结果；newNameLen 同样按 UTF-8 字节计。 */
const char* wasm_ZrLspRename(void* context, const char* uri, int uriLen,
                        int line, int character, const char* newName, int newNameLen);

/** @brief 将文档符号转换成包含位置的 SymbolInformation 数组。 */
const char* wasm_ZrLspGetDocumentSymbols(void* context, const char* uri, int uriLen);

/** @brief 按 worker 传入的可见范围查询内联提示。 */
const char* wasm_ZrLspGetInlayHints(void* context,
                                    const char* uri,
                                    int uriLen,
                                    int startLine,
                                    int startCharacter,
                                    int endLine,
                                    int endCharacter);

/** @brief 使用 UTF-8 查询词聚合工作区符号，供搜索面板消费。 */
const char* wasm_ZrLspGetWorkspaceSymbols(void* context, const char* query, int queryLen);

/** @brief 读取 native 声明虚拟文档，供扩展的只读文档提供器使用。 */
const char* wasm_ZrLspGetNativeDeclarationDocument(void* context, const char* uri, int uriLen);

/** @brief 返回指定项目的模块摘要，供扩展工作区视图导航。 */
const char* wasm_ZrLspGetProjectModules(void* context, const char* projectUri, int projectUriLen);

/** @brief 返回当前位置同符号的文档高亮；未命中为空数组。 */
const char* wasm_ZrLspGetDocumentHighlights(void* context, const char* uri, int uriLen,
                                      int line, int character);

/** @brief 返回 LSP delta 编码语义 token 全量数据。 */
const char* wasm_ZrLspGetSemanticTokens(void* context, const char* uri, int uriLen);

/** @brief 查询重命名的范围和占位文字；不可重命名时返回 null 载荷。 */
const char* wasm_ZrLspPrepareRename(void* context, const char* uri, int uriLen,
                               int line, int character);

/** @brief 为完整文档生成文本编辑；结果在序列化后由 native 释放。 */
const char* wasm_ZrLspGetFormatting(void* context, const char* uri, int uriLen);

/** @brief 为 LSP 范围生成文本编辑；行列边界遵循 UTF-16 位置约定。 */
const char* wasm_ZrLspGetRangeFormatting(void* context,
                                    const char* uri,
                                    int uriLen,
                                    int startLine,
                                    int startCharacter,
                                    int endLine,
                                    int endCharacter);

/** @brief 将指定范围的 native 操作及其编辑集投影成 LSP CodeAction。 */
const char* wasm_ZrLspGetCodeActions(void* context,
                                const char* uri,
                                int uriLen,
                                int startLine,
                                int startCharacter,
                                int endLine,
                                int endCharacter);

/** @brief 返回文档折叠范围，供浏览器编辑器折叠视图使用。 */
const char* wasm_ZrLspGetFoldingRanges(void* context, const char* uri, int uriLen);

/** @brief 返回单光标的嵌套选择范围链。 */
const char* wasm_ZrLspGetSelectionRange(void* context,
                                   const char* uri,
                                   int uriLen,
                                   int line,
                                   int character);

/** @brief 返回可导航的文档链接及可选提示。 */
const char* wasm_ZrLspGetDocumentLinks(void* context, const char* uri, int uriLen);

/** @brief 返回附带命令参数的 CodeLens，供 worker 注册能力使用。 */
const char* wasm_ZrLspGetCodeLens(void* context, const char* uri, int uriLen);

#ifdef __cplusplus
}
#endif

#endif //ZR_VM_LANGUAGE_SERVER_WASM_EXPORTS_H
