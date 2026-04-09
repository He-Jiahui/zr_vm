import * as fs from 'node:fs';
import * as path from 'node:path';
import * as vscode from 'vscode';
import { resolvePreferredCliSetting } from './executablePath';
import { pickLatestExistingDirectoryWithFiles, pickLatestExistingPath } from './nativePathSelection';

export const LANGUAGE_SERVER_CONFIG_SECTION = 'zr.languageServer';
export const DEBUG_CONFIG_SECTION = 'zr.debug';
export const ROOT_CONFIG_SECTION = 'zr';

type NativeBinaryKind = 'languageServer' | 'cli';

const WINDOWS_REQUIRED_RUNTIME_FILES = [
    'zr_vm_language_server_stdio.exe',
    'zr_vm_cli.exe',
    'zr_vm_core.dll',
    'zr_vm_debug.dll',
    'zr_vm_language_server.dll',
    'zr_vm_library.dll',
    'zr_vm_parser.dll',
    'zr_vm_lib_container.dll',
    'zr_vm_lib_ffi.dll',
    'zr_vm_lib_math.dll',
    'zr_vm_lib_system.dll',
];

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

function defaultAssetDirs(extensionRoot: string): string[] {
    const buildRoot = path.join(extensionRoot, '..', 'build');

    return dedupePaths([
        path.join(extensionRoot, 'server', 'native', `${process.platform}-${process.arch}`),
        path.join(extensionRoot, 'server'),
        ...collectBuildCandidateDirs(buildRoot),
    ]);
}

function collectBuildCandidateDirs(buildRoot: string): string[] {
    const seedDirs = [
        path.join(buildRoot, 'codex-lsp', 'bin', 'Debug'),
        path.join(buildRoot, 'codex-lsp', 'bin'),
        path.join(buildRoot, 'codex-msvc-debug', 'bin', 'Debug'),
        path.join(buildRoot, 'codex-msvc-debug', 'bin'),
    ];
    const scannedDirs: string[] = [];

    try {
        for (const entry of fs.readdirSync(buildRoot, { withFileTypes: true })) {
            if (!entry.isDirectory()) {
                continue;
            }

            const candidateBinDir = path.join(buildRoot, entry.name, 'bin');
            scannedDirs.push(candidateBinDir);
            scannedDirs.push(path.join(candidateBinDir, 'Debug'));
            scannedDirs.push(path.join(candidateBinDir, 'Release'));
            scannedDirs.push(path.join(candidateBinDir, 'RelWithDebInfo'));
        }
    } catch {
        // Ignore missing build roots and fall back to bundled assets.
    }

    return dedupePaths([...seedDirs, ...scannedDirs]);
}

function requiredRuntimeFiles(): string[] {
    if (process.platform === 'win32') {
        return WINDOWS_REQUIRED_RUNTIME_FILES;
    }

    return [
        executableName('languageServer'),
        executableName('cli'),
    ];
}

function dedupePaths(paths: string[]): string[] {
    const seen = new Set<string>();
    const result: string[] = [];

    for (const value of paths) {
        const normalized = path.resolve(value);
        if (seen.has(normalized)) {
            continue;
        }

        seen.add(normalized);
        result.push(normalized);
    }

    return result;
}

function resolveNativeAssetDirectory(context: vscode.ExtensionContext): string | undefined {
    return pickLatestExistingDirectoryWithFiles(
        defaultAssetDirs(context.extensionPath),
        requiredRuntimeFiles(),
    );
}

function resolveNativeExecutable(
    context: vscode.ExtensionContext,
    kind: NativeBinaryKind,
    configuredPath?: string,
): string | undefined {
    const extensionRoot = context.extensionPath;
    const fileName = executableName(kind);
    const explicitPath = (configuredPath ?? '').trim();
    if (explicitPath.length > 0) {
        return fs.existsSync(explicitPath) ? explicitPath : undefined;
    }

    const assetDirectory = resolveNativeAssetDirectory(context);
    if (assetDirectory) {
        return path.join(assetDirectory, fileName);
    }

    return pickLatestExistingPath(
        defaultAssetDirs(extensionRoot).map((directory) => path.join(directory, fileName)),
    );
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
