import * as vscode from 'vscode';
import { isIgnorableLanguageServerRequestError } from './languageClientRequests';
import { activeWorkspaceFolder, resolveSelectedProjectUri } from './workspaceProjects';

/** 与 stdio 服务端的 selected-project 通知名保持一致。 */
const ZR_SELECTED_PROJECT_NOTIFICATION_METHOD = 'zr/selectedProject';

/** 桌面客户端建立后及项目选择变化时推送 URI；停止中的客户端错误按共享分类忽略。 */
export async function sendZrSelectedProjectToLanguageServer(
    context: vscode.ExtensionContext,
    client: { sendNotification(type: unknown, params?: unknown): Thenable<void> } | undefined,
): Promise<void> {
    // BUG: 首次通知已解析 A、完成前用户改选 B，事件因全局 client 未发布而丢弃；
    // 若 A 通知成功且活动编辑器未覆盖 B，启动流程不重放 B，服务端保留 A 至后续事件。
    if (client === undefined) {
        return;
    }
    // BUG: client 已存在时，项目扫描失败或未分类通知错误会使本函数拒绝；
    // 桌面选择事件以 void 调用，拒绝无人接收且服务端可能保留旧项目 URI。
    const uri = await resolveSelectedProjectUri(context, activeWorkspaceFolder(), false);
    try {
        /* 通知可携带 null 以清除服务端选择；客户端停止中的已知错误不妨碍本地选择。 */
        await client.sendNotification(ZR_SELECTED_PROJECT_NOTIFICATION_METHOD, { uri: uri?.toString() ?? null });
    } catch (error) {
        if (isIgnorableLanguageServerRequestError(error)) {
            return;
        }

        throw error;
    }
}
