import * as path from 'node:path';
import * as vscode from 'vscode';
import { resolveNativeCliPath } from '../nativeAssets';
import {
    ZR_DEBUG_ATTACH_COMMAND,
    ZR_DEBUG_CURRENT_PROJECT_COMMAND,
    ZR_DEBUG_TYPE,
} from './constants';
import { ZrDebugAdapter } from './dapSession';
import type { ZrAttachRequestArguments, ZrLaunchRequestArguments } from './types';
import { parseEndpoint } from './zrdbgClient';

type ZrDebugConfiguration = vscode.DebugConfiguration & Partial<ZrLaunchRequestArguments & ZrAttachRequestArguments>;

class ZrDebugConfigurationProvider implements vscode.DebugConfigurationProvider {
    constructor(private readonly context: vscode.ExtensionContext) {}

    async provideDebugConfigurations(folder: vscode.WorkspaceFolder | undefined): Promise<vscode.DebugConfiguration[]> {
        const launchProject = await findProjectFile(folder);
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

        const projectUri = await resolveProjectUri(folder, config.project);
        if (!projectUri) {
            void vscode.window.showErrorMessage('Unable to resolve a ZR project (.zrp) to debug.');
            return undefined;
        }

        const cliPath = resolveNativeCliPath(this.context);
        if (!cliPath) {
            void vscode.window.showErrorMessage(
                'Unable to locate zr_vm_cli. Set zr.debug.cli.path or build/sync the native assets.',
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
            stopOnEntry: config.stopOnEntry !== false,
        };
    }
}

class ZrDebugAdapterFactory implements vscode.DebugAdapterDescriptorFactory {
    createDebugAdapterDescriptor(session: vscode.DebugSession): vscode.ProviderResult<vscode.DebugAdapterDescriptor> {
        return new vscode.DebugAdapterInlineImplementation(new ZrDebugAdapter(session));
    }
}

export function registerDesktopDebugSupport(
    context: vscode.ExtensionContext,
): vscode.Disposable[] {
    const provider = new ZrDebugConfigurationProvider(context);
    const factory = new ZrDebugAdapterFactory();

    return [
        vscode.debug.registerDebugConfigurationProvider(ZR_DEBUG_TYPE, provider),
        vscode.debug.registerDebugAdapterDescriptorFactory(ZR_DEBUG_TYPE, factory),
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

async function resolveProjectUri(
    folder: vscode.WorkspaceFolder | undefined,
    projectPath: unknown,
): Promise<vscode.Uri | undefined> {
    if (typeof projectPath === 'string' && projectPath.trim().length > 0) {
        const resolved = resolveRelativePath(folder, projectPath.trim());
        return vscode.Uri.file(resolved);
    }

    return findProjectFile(folder);
}

async function findProjectFile(folder: vscode.WorkspaceFolder | undefined): Promise<vscode.Uri | undefined> {
    const activeUri = vscode.window.activeTextEditor?.document.uri;
    if (activeUri && activeUri.scheme === 'file' && activeUri.fsPath.toLowerCase().endsWith('.zrp')) {
        return activeUri;
    }

    const searchRoots = folder ? [folder] : (vscode.workspace.workspaceFolders ?? []);
    const candidates: vscode.Uri[] = [];

    for (const searchRoot of searchRoots) {
        const found = await vscode.workspace.findFiles(
            new vscode.RelativePattern(searchRoot, '*.zrp'),
            undefined,
            10,
        );
        candidates.push(...found);
    }

    if (candidates.length === 1) {
        return candidates[0];
    }
    if (candidates.length > 1) {
        const picked = await vscode.window.showQuickPick(
            candidates.map((uri) => ({
                label: path.basename(uri.fsPath),
                description: uri.fsPath,
                uri,
            })),
            {
                title: 'Select ZR project to debug',
            },
        );
        return picked?.uri;
    }

    return undefined;
}

function activeWorkspaceFolder(): vscode.WorkspaceFolder | undefined {
    const activeDocument = vscode.window.activeTextEditor?.document.uri;
    return activeDocument ? vscode.workspace.getWorkspaceFolder(activeDocument) : vscode.workspace.workspaceFolders?.[0];
}

function resolveRelativePath(folder: vscode.WorkspaceFolder | undefined, value: string): string {
    if (path.isAbsolute(value)) {
        return path.normalize(value);
    }

    const base = folder?.uri.fsPath ?? process.cwd();
    return path.normalize(path.join(base, value));
}
