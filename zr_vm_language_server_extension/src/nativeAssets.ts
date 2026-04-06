import * as fs from 'node:fs';
import * as path from 'node:path';
import * as vscode from 'vscode';
import { resolvePreferredCliSetting } from './executablePath';

export const LANGUAGE_SERVER_CONFIG_SECTION = 'zr.languageServer';
export const DEBUG_CONFIG_SECTION = 'zr.debug';
export const ROOT_CONFIG_SECTION = 'zr';

type NativeBinaryKind = 'languageServer' | 'cli';

function executableName(kind: NativeBinaryKind): string {
    switch (kind) {
        case 'cli':
            return process.platform === 'win32' ? 'zr_vm_cli.exe' : 'zr_vm_cli';
        case 'languageServer':
        default:
            return process.platform === 'win32'
                ? 'zr_vm_language_server_stdio.exe'
                : 'zr_vm_language_server_stdio';
    }
}

function defaultBuildDirs(extensionRoot: string): string[] {
    return [
        path.join(extensionRoot, 'server', 'native', `${process.platform}-${process.arch}`),
        path.join(extensionRoot, 'server'),
        path.join(extensionRoot, '..', 'build', 'codex-lsp', 'bin', 'Debug'),
        path.join(extensionRoot, '..', 'build', 'codex-lsp', 'bin'),
    ];
}

function resolveNativeExecutable(
    context: vscode.ExtensionContext,
    kind: NativeBinaryKind,
    configuredPath?: string,
): string | undefined {
    const extensionRoot = context.extensionPath;
    const fileName = executableName(kind);
    const explicitPath = (configuredPath ?? '').trim();
    const candidates = explicitPath.length > 0
        ? [explicitPath]
        : defaultBuildDirs(extensionRoot).map((directory) => path.join(directory, fileName));

    for (const candidate of candidates) {
        if (candidate.length > 0 && fs.existsSync(candidate)) {
            return candidate;
        }
    }

    return undefined;
}

export function bundledNativeExecutablePath(context: vscode.ExtensionContext, kind: NativeBinaryKind): string {
    return path.join(
        context.extensionPath,
        'server',
        'native',
        `${process.platform}-${process.arch}`,
        executableName(kind),
    );
}

export function resolveNativeLanguageServerPath(
    context: vscode.ExtensionContext,
    config: vscode.WorkspaceConfiguration,
): string | undefined {
    return resolveNativeExecutable(context, 'languageServer', config.get<string>('native.path', ''));
}

export function resolveNativeCliPath(
    context: vscode.ExtensionContext,
    config?: vscode.WorkspaceConfiguration,
): string | undefined {
    const rootConfig = vscode.workspace.getConfiguration(ROOT_CONFIG_SECTION);
    const debugConfig = config ?? vscode.workspace.getConfiguration(DEBUG_CONFIG_SECTION);
    const configuredPath = resolvePreferredCliSetting({
        executablePath: rootConfig.get<string>('executablePath', ''),
        legacyDebugCliPath: debugConfig.get<string>('cli.path', ''),
    });
    return resolveNativeExecutable(context, 'cli', configuredPath);
}
