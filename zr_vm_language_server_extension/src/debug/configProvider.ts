import * as path from 'node:path';
import * as vscode from 'vscode';
import { resolveNativeCliPath } from '../nativeAssets';
import {
    activeWorkspaceFolder,
    resolveProjectUri as resolveWorkspaceProjectUri,
    resolveRelativePath,
    resolveSelectedProjectUri,
} from '../workspaceProjects';
import {
    ZR_DEBUG_ATTACH_COMMAND,
    ZR_DEBUG_CURRENT_PROJECT_COMMAND,
    ZR_DEBUG_SELECTED_PROJECT_COMMAND,
    ZR_DEBUG_TYPE,
} from './constants';
import { ZrDebugAdapter } from './dapSession';
import type { ZrAttachRequestArguments, ZrLaunchRequestArguments } from './types';
import { parseEndpoint } from './zrdbgClient';

/** VS Code 传入的未解析配置；provider 补齐 launch/attach 各自所需字段后交给适配器。 */
type ZrDebugConfiguration = vscode.DebugConfiguration & Partial<ZrLaunchRequestArguments & ZrAttachRequestArguments>;

/** 将 launch.json 和项目命令统一为本地调试配置；已识别的配置错误或取消返回 undefined。 */
class ZrDebugConfigurationProvider implements vscode.DebugConfigurationProvider {
    /** 使用扩展的工作区状态选择项目，不在 provider 内保存另一份选择。 */
    constructor(private readonly context: vscode.ExtensionContext) {}

    /** 为 VS Code 新建调试配置提供模板，项目选择可触发工作区选择器。 */
    async provideDebugConfigurations(folder: vscode.WorkspaceFolder | undefined): Promise<vscode.DebugConfiguration[]> {
        const launchProject = await resolveSelectedProjectUri(this.context, folder, true);
        const launchCwd = folder?.uri.fsPath ?? (launchProject ? path.dirname(launchProject.fsPath) : undefined);

        return [
            {
                type: ZR_DEBUG_TYPE,
                name: 'ZR: Launch Project',
                request: 'launch',
                project: launchProject?.fsPath ?? '',
                cwd: launchCwd,
                executionMode: 'interp',
                stopOnEntry: true,
            },
            {
                type: ZR_DEBUG_TYPE,
                name: 'ZR: Attach to zrdbg/1 Endpoint',
                request: 'attach',
                endpoint: '127.0.0.1:9000',
            },
        ];
    }

    /** 分开处理现有端点 attach 与本地 CLI launch；后者按工作区解释相对路径。 */
    async resolveDebugConfiguration(
        folder: vscode.WorkspaceFolder | undefined,
        config: ZrDebugConfiguration,
    ): Promise<vscode.DebugConfiguration | undefined> {
        if (!config.type) {
            config.type = ZR_DEBUG_TYPE;
        }
        if (!config.request) {
            config.request = 'launch';
        }
        if (!config.name) {
            config.name = config.request === 'attach' ? 'ZR: Attach to zrdbg/1 Endpoint' : 'ZR: Launch Project';
        }

        // attach 只校验连接目标，不要求项目或 CLI；启动进程的所有权仍在外部调用者。
        if (config.request === 'attach') {
            const endpoint = typeof config.endpoint === 'string' ? config.endpoint.trim() : '';
            if (!endpoint) {
                void vscode.window.showErrorMessage('ZR attach configuration requires endpoint.');
                return undefined;
            }

            try {
                parseEndpoint(endpoint);
            } catch (error) {
                void vscode.window.showErrorMessage(error instanceof Error ? error.message : String(error));
                return undefined;
            }

            return {
                ...config,
                endpoint,
            };
        }

        const projectUri = typeof config.project === 'string' && config.project.trim().length > 0
            ? await resolveWorkspaceProjectUri(folder, config.project)
            : await resolveSelectedProjectUri(this.context, folder, true);
        if (!projectUri) {
            void vscode.window.showErrorMessage('Unable to resolve a ZR project (.zrp) to debug.');
            return undefined;
        }

        // BUG: 此处先要求默认 CLI 可发现，早于下面采用 config.cliPath 的分支；
        // 因而仅在 launch.json 配置有效 cliPath、未安装默认资产时也会拒绝启动。
        const cliPath = resolveNativeCliPath(this.context);
        if (!cliPath) {
            void vscode.window.showErrorMessage(
                'Unable to locate zr_vm_cli. Set zr.executablePath (relative or absolute) or build/sync the native assets.',
            );
            return undefined;
        }

        return {
            ...config,
            type: ZR_DEBUG_TYPE,
            request: 'launch',
            project: projectUri.fsPath,
            cwd: typeof config.cwd === 'string' && config.cwd.length > 0
                ? resolveRelativePath(folder, config.cwd)
                : folder?.uri.fsPath ?? path.dirname(projectUri.fsPath),
            executionMode: config.executionMode === 'binary' ? 'binary' : 'interp',
            cliPath: typeof config.cliPath === 'string' && config.cliPath.length > 0
                ? resolveRelativePath(folder, config.cliPath)
                : cliPath,
            debugAddress: typeof config.debugAddress === 'string' && config.debugAddress.length > 0
                ? config.debugAddress
                : '127.0.0.1:0',
            authToken: typeof config.authToken === 'string' && config.authToken.length > 0
                ? config.authToken
                : undefined,
            stopOnEntry: config.stopOnEntry !== false,
        };
    }
}

/** 为每个 VS Code 调试会话创建独立内联适配器，避免共享断点与运行时连接状态。 */
class ZrDebugAdapterFactory implements vscode.DebugAdapterDescriptorFactory {
    /** 由 VS Code 调试服务调用；适配器生命周期随 session，宿主管理其 dispose。 */
    createDebugAdapterDescriptor(session: vscode.DebugSession): vscode.ProviderResult<vscode.DebugAdapterDescriptor> {
        return new vscode.DebugAdapterInlineImplementation(new ZrDebugAdapter(session));
    }
}

/** 桌面 activate 注册入口；调用者把返回的 provider、factory 和命令加入 subscriptions。 */
export function registerDesktopDebugSupport(
    context: vscode.ExtensionContext,
): vscode.Disposable[] {
    const provider = new ZrDebugConfigurationProvider(context);
    const factory = new ZrDebugAdapterFactory();

    return [
        vscode.debug.registerDebugConfigurationProvider(ZR_DEBUG_TYPE, provider),
        vscode.debug.registerDebugAdapterDescriptorFactory(ZR_DEBUG_TYPE, factory),
        // 状态栏、项目树和命令面板共用选择规则，解析完成后再让 VS Code 创建会话。
        vscode.commands.registerCommand(ZR_DEBUG_SELECTED_PROJECT_COMMAND, async () => {
            const workspaceFolder = activeWorkspaceFolder();
            const configuration = await provider.resolveDebugConfiguration(workspaceFolder, {
                type: ZR_DEBUG_TYPE,
                name: 'ZR: Debug Selected Project',
                request: 'launch',
            });

            if (configuration) {
                await vscode.debug.startDebugging(workspaceFolder, configuration);
            }
        }),
        // 当前项目入口也复用选择器；活动清单优先级由 workspaceProjects 定义。
        vscode.commands.registerCommand(ZR_DEBUG_CURRENT_PROJECT_COMMAND, async () => {
            const workspaceFolder = activeWorkspaceFolder();
            const configuration = await provider.resolveDebugConfiguration(workspaceFolder, {
                type: ZR_DEBUG_TYPE,
                name: 'ZR: Debug Current Project',
                request: 'launch',
            });

            if (configuration) {
                await vscode.debug.startDebugging(workspaceFolder, configuration);
            }
        }),
        // 交互式 attach 接受已经运行的本地 zrdbg/1 端点，取消输入不创建会话。
        vscode.commands.registerCommand(ZR_DEBUG_ATTACH_COMMAND, async () => {
            const endpoint = await vscode.window.showInputBox({
                prompt: 'ZR debug endpoint',
                placeHolder: '127.0.0.1:9000',
                ignoreFocusOut: true,
            });
            if (!endpoint) {
                return;
            }

            const workspaceFolder = activeWorkspaceFolder();
            const configuration = await provider.resolveDebugConfiguration(workspaceFolder, {
                type: ZR_DEBUG_TYPE,
                name: 'ZR: Attach to zrdbg/1 Endpoint',
                request: 'attach',
                endpoint,
            });
            if (configuration) {
                await vscode.debug.startDebugging(workspaceFolder, configuration);
            }
        }),
    ];
}
