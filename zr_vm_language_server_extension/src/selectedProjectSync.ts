import * as vscode from 'vscode';
import { isIgnorableLanguageServerRequestError } from './languageClientRequests';
import { activeWorkspaceFolder, resolveSelectedProjectUri } from './workspaceProjects';

/** 与 stdio 服务端的 selected-project 通知名保持一致。 */
const ZR_SELECTED_PROJECT_NOTIFICATION_METHOD = 'zr/selectedProject';

/** 桌面客户端建立后及项目选择变化时推送 URI；退休会话不发送迟到通知。 */
export async function sendZrSelectedProjectToLanguageServer(
    context: vscode.ExtensionContext,
    client: { sendNotification(type: unknown, params?: unknown): Thenable<void> } | undefined,
    signal?: AbortSignal,
): Promise<void> {
    if (client === undefined || signal?.aborted) {
        return;
    }
    // BUG: client 已存在时，项目扫描失败或未分类通知错误会使本函数拒绝；
    // 桌面选择事件以 void 调用，拒绝无人接收且服务端可能保留旧项目 URI。
    const uri = await resolveSelectedProjectUri(context, activeWorkspaceFolder(), false);
    if (signal?.aborted) { return; }
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
