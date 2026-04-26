import * as vscode from 'vscode';
import { activeWorkspaceFolder, resolveSelectedProjectUri } from './workspaceProjects';

const ZR_SELECTED_PROJECT_NOTIFICATION_METHOD = 'zr/selectedProject';

export async function sendZrSelectedProjectToLanguageServer(
    context: vscode.ExtensionContext,
    client: { sendNotification(type: unknown, params?: unknown): Thenable<void> } | undefined,
): Promise<void> {
    if (client === undefined) {
        return;
    }
    const uri = await resolveSelectedProjectUri(context, activeWorkspaceFolder(), false);
    await client.sendNotification(ZR_SELECTED_PROJECT_NOTIFICATION_METHOD, { uri: uri?.toString() ?? null });
}
