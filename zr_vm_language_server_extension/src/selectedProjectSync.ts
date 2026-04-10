import { NotificationType } from 'vscode-languageserver-protocol';
import * as vscode from 'vscode';
import { activeWorkspaceFolder, resolveSelectedProjectUri } from './workspaceProjects';

export const ZrSelectedProjectUriNotification = new NotificationType<{ uri: string | null }>('zr/selectedProject');

export async function sendZrSelectedProjectToLanguageServer(
    context: vscode.ExtensionContext,
    client: { sendNotification(type: unknown, params?: unknown): Thenable<void> } | undefined,
): Promise<void> {
    if (client === undefined) {
        return;
    }
    const uri = await resolveSelectedProjectUri(context, activeWorkspaceFolder(), false);
    await client.sendNotification(ZrSelectedProjectUriNotification, { uri: uri?.toString() ?? null });
}
