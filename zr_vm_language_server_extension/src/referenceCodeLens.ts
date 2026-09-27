import * as vscode from 'vscode';

/** 与服务端 CodeLens 命令及扩展清单共用的点击入口。 */
export const ZR_SHOW_REFERENCES_COMMAND = 'zr.showReferences';

/** 服务端经 JSON-RPC 传来的位置形状；命令可由外部调用，因此字段仍需检查。 */
type SerializedPosition = {
    line?: number;
    character?: number;
};

/**
 * 在桌面和 Web 激活时注册 CodeLens 点击命令，返回句柄交给 context.subscriptions。
 * 服务端提供 URI 和声明位置；点击时重新询问引用提供器，再交给编辑器展示，
 * 避免把生成 CodeLens 时的引用计数当成当前引用列表。
 * TODO: 现有宿主测试只断言服务端暴露命令，需在真实桌面/Web 宿主触发点击，
 * 核查空引用及提供器异常时的编辑器呈现和错误传播。
 */
export function registerReferenceCodeLensCommand(): vscode.Disposable {
    return vscode.commands.registerCommand(
        ZR_SHOW_REFERENCES_COMMAND,
        async (uriText?: string, position?: SerializedPosition) => {
            // 外部命令或旧版 CodeLens 可能缺少参数；不能在无定位信息时请求引用。
            // TODO: 这里只检查 number 类型；需核查公开命令传入 NaN、负数或小数时
            // VS Code Position/引用提供器的行为，并决定是否要求非负整数坐标。
            if (typeof uriText !== 'string' ||
                typeof position?.line !== 'number' ||
                typeof position?.character !== 'number') {
                return;
            }

            const uri = vscode.Uri.parse(uriText);
            const vscodePosition = new vscode.Position(position.line, position.character);
            // 在用户点击时重新走当前语言客户端的引用提供器；计数仅用于 CodeLens 标题。
            const references = await vscode.commands.executeCommand<vscode.Location[]>(
                'vscode.executeReferenceProvider',
                uri,
                vscodePosition,
            );
            // 没有可用引用时仍以空列表交给 VS Code，保持命令结果由编辑器统一呈现。
            await vscode.commands.executeCommand(
                'editor.action.showReferences',
                uri,
                vscodePosition,
                references ?? [],
            );
        },
    );
}
