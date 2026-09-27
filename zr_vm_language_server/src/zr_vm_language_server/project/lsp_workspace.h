#ifndef ZR_VM_LANGUAGE_SERVER_LSP_WORKSPACE_H
#define ZR_VM_LANGUAGE_SERVER_LSP_WORKSPACE_H

#include "zr_vm_language_server/lsp_interface.h"

/** @brief 随 LSP context 存活的工作区根集合；根 URI 由初始化和文件夹变更通知管理。 */
typedef struct SZrLspWorkspace SZrLspWorkspace;

/** @brief 为新建的 LSP context 建立空的工作区范围，随后由 initialize 注入根目录。
 *  @return context 持有返回值；构造失败返回空指针。 */
ZR_LANGUAGE_SERVER_API SZrLspWorkspace *ZrLanguageServer_LspWorkspace_New(SZrState *state);
/** @brief 在 context 销毁时释放工作区容器；不接管 URI 对象或项目索引。 */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_LspWorkspace_Free(SZrState *state, SZrLspWorkspace *workspace);
/** @brief 每次处理 initialize 的根目录参数前清空旧根；项目索引的移除由文件夹事件另行负责。 */
ZR_LANGUAGE_SERVER_API void ZrLanguageServer_LspWorkspace_Reset(SZrState *state, SZrLspContext *context);
/** @brief 将客户端的文件 URI 登记为可处理的工作区根，等价 URI 只保留一份。
 *  @pre uri 可转换为本地文件路径；非文件 URI 不会进入根集合。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspWorkspace_AddFolder(SZrState *state,
                                                                        SZrLspContext *context,
                                                                        SZrString *uri);
/** @brief 处理 didChangeWorkspaceFolders 的删除：撤销选中项目并逐出不再受任何根覆盖的项目索引。
 *  @note 打开的文档由项目移除路径保留，避免编辑器覆盖层被工作区变更清除。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspWorkspace_RemoveFolder(SZrState *state,
                                                                           SZrLspContext *context,
                                                                           SZrString *uri);
/** @brief 限制文件监听及文件操作通知的作用域为工作区根内或已打开的文档。
 *  @note 此过滤不决定项目归属；项目层随后重新发现或更新索引。 */
ZR_LANGUAGE_SERVER_API TZrBool ZrLanguageServer_LspWorkspace_CanProcessFileEvent(SZrLspContext *context,
                                                                                  SZrString *uri);

#endif
