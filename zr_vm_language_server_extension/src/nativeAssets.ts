import * as fs from 'node:fs';
import * as path from 'node:path';
import * as vscode from 'vscode';
import assetLayout from '../asset-layout.json';
import { resolveConfiguredPath, resolvePreferredCliSetting } from './executablePath';
import {
    pickFirstExistingDirectoryWithFiles,
    pickFirstExistingPath,
    pickLatestExistingDirectoryWithFiles,
    pickLatestExistingPath,
} from './nativePathSelection';

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
    const executable = assetLayout.native.executables[kind] as Record<string, string>;
    return executable[process.platform] ?? executable.default;
}

function renderPathTemplate(template: string, replacements: Record<string, string> = {}): string {
    const rendered = Object.entries(replacements).reduce(
        (value, [key, replacement]) => value.replace(new RegExp(`\\{${key}\\}`, 'g'), replacement),
        template,
    );
    const segments = rendered
        .split(/[\\/]+/)
        .filter((segment) => segment.length > 0 && segment !== '.');
    return segments.length > 0 ? path.join(...segments) : '.';
}

function resolveBundledNativeAssetDir(extensionRoot: string): string {
    return path.join(
        extensionRoot,
        renderPathTemplate(assetLayout.native.bundledRelativeDir, {
            platform: process.platform,
            arch: process.arch,
        }),
    );
}

function requiredRuntimeFiles(): string[] {
    const filesByPlatform = assetLayout.native.requiredRuntimeFiles as Record<string, string[]>;
    const files = filesByPlatform[process.platform] ?? filesByPlatform.default;
    return Array.isArray(files) ? [...files] : WINDOWS_REQUIRED_RUNTIME_FILES;
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

function resolveTemplateDirs(baseDir: string, templates: string[]): string[] {
    return templates.map((template) => {
        const relativePath = renderPathTemplate(template);
        return relativePath === '.'
            ? path.resolve(baseDir)
            : path.resolve(baseDir, relativePath);
    });
}

function collectBuildCandidateDirs(buildRoot: string): string[] {
    const scannedDirs: string[] = [];

    try {
        for (const entry of fs.readdirSync(buildRoot, { withFileTypes: true })) {
            if (!entry.isDirectory()) {
                continue;
            }

            scannedDirs.push(
                ...resolveTemplateDirs(
                    path.join(buildRoot, entry.name),
                    assetLayout.native.scannedBuildSubdirs,
                ),
            );
        }
    } catch {
        // Ignore missing build roots and fall back to bundled assets.
    }

    return dedupePaths(scannedDirs);
}

function orderedWorkspaceFolderPaths(): string[] {
    const activeWorkspaceFolder = vscode.window.activeTextEditor?.document
        ? vscode.workspace.getWorkspaceFolder(vscode.window.activeTextEditor.document.uri)
        : undefined;
    const allWorkspaceFolders = vscode.workspace.workspaceFolders ?? [];
    const ordered = [
        activeWorkspaceFolder?.uri.fsPath,
        ...allWorkspaceFolders.map((folder) => folder.uri.fsPath),
    ].filter((value): value is string => typeof value === 'string' && value.length > 0);

    return dedupePaths(ordered);
}

function resolveNativeAssetDirectory(context: vscode.ExtensionContext): string | undefined {
    const bundledDir = resolveBundledNativeAssetDir(context.extensionPath);
    const buildCandidateDirs = collectBuildCandidateDirs(path.join(context.extensionPath, '..', 'build'));

    return pickFirstExistingDirectoryWithFiles([bundledDir], requiredRuntimeFiles()) ??
        pickLatestExistingDirectoryWithFiles(buildCandidateDirs, requiredRuntimeFiles());
}

function resolveNativeExecutable(
    context: vscode.ExtensionContext,
    kind: NativeBinaryKind,
    configuredPath?: string,
): string | undefined {
    const extensionRoot = context.extensionPath;
    const fileName = executableName(kind);
    const explicitPath = resolveConfiguredPath({
        configuredPath,
        workspaceFolderPaths: orderedWorkspaceFolderPaths(),
        extensionPath: extensionRoot,
    });
    if (explicitPath) {
        return explicitPath;
    }

    const assetDirectory = resolveNativeAssetDirectory(context);
    if (assetDirectory) {
        return path.join(assetDirectory, fileName);
    }

    const bundledExecutable = path.join(resolveBundledNativeAssetDir(extensionRoot), fileName);
    const buildExecutables = collectBuildCandidateDirs(path.join(extensionRoot, '..', 'build'))
        .map((directory) => path.join(directory, fileName));

    return pickFirstExistingPath([bundledExecutable]) ??
        pickLatestExistingPath(buildExecutables);
}

export function bundledNativeExecutablePath(context: vscode.ExtensionContext, kind: NativeBinaryKind): string {
    return path.join(
        resolveBundledNativeAssetDir(context.extensionPath),
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
