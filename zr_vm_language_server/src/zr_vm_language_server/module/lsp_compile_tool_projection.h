#ifndef ZR_VM_LANGUAGE_SERVER_LSP_COMPILE_TOOL_PROJECTION_H
#define ZR_VM_LANGUAGE_SERVER_LSP_COMPILE_TOOL_PROJECTION_H

#include "zr_vm_language_server/conf.h"
#include "zr_vm_library/native_binding.h"
#include "zr_vm_parser/compile_tool.h"

/** @brief 将 parser 拥有的 CompileTool 规范映射成 LSP 可展示的静态原生描述符。
 *  @pre moduleName 为 NUL 结尾的模块名；仅 build/declaration 两种规范模块有投影。
 *  @return 契约不匹配或未知模块时为空；返回指针为静态只读存储，不得释放。 */
ZR_LANGUAGE_SERVER_API const ZrLibModuleDescriptor *ZrLanguageServer_LspCompileToolProjection_FindModule(
        const TZrChar *moduleName);

/** @brief 在暴露虚拟声明前检查 LSP 投影与 parser 规范的来源、哈希及角色。
 *  @note 这里只核对部分结构；具体字段由投影静态表与测试共同维护。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspCompileToolProjection_MatchesCanonical(
        const ZrLibModuleDescriptor *projection,
        const SZrParserCompileToolModuleDescriptor *canonical);

#endif
