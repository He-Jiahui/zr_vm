import * as fs from 'node:fs';
import * as path from 'node:path';

export interface CliSettingInputs {
    executablePath?: string;
    legacyDebugCliPath?: string;
}

export interface ConfiguredPathResolutionInputs {
    configuredPath?: string;
    workspaceFolderPaths?: string[];
    extensionPath?: string;
    existsSync?: (candidate: string) => boolean;
}

function trimConfiguredPath(value: string | undefined): string {
    return typeof value === 'string' ? value.trim() : '';
}

function dedupeCandidates(values: string[]): string[] {
    const seen = new Set<string>();
    const result: string[] = [];

    for (const value of values) {
        const normalized = path.resolve(value);
        if (seen.has(normalized)) {
            continue;
        }

        seen.add(normalized);
        result.push(normalized);
    }

    return result;
}

function isAbsoluteConfiguredPath(value: string): boolean {
    return path.isAbsolute(value) || /^[A-Za-z]:[\\/]/.test(value);
}

export function configuredPathCandidates(inputs: ConfiguredPathResolutionInputs): string[] {
    const configuredPath = trimConfiguredPath(inputs.configuredPath);
    if (configuredPath.length === 0) {
        return [];
    }

    if (isAbsoluteConfiguredPath(configuredPath)) {
        return [path.resolve(configuredPath)];
    }

    const candidates: string[] = [];

    for (const workspaceFolderPath of inputs.workspaceFolderPaths ?? []) {
        const trimmedWorkspacePath = trimConfiguredPath(workspaceFolderPath);
        if (trimmedWorkspacePath.length === 0) {
            continue;
        }

        candidates.push(path.resolve(trimmedWorkspacePath, configuredPath));
    }

    const extensionPath = trimConfiguredPath(inputs.extensionPath);
    if (extensionPath.length > 0) {
        candidates.push(path.resolve(extensionPath, configuredPath));
    }

    return dedupeCandidates(candidates);
}

export function resolveConfiguredPath(inputs: ConfiguredPathResolutionInputs): string | undefined {
    const existsSync = inputs.existsSync ?? fs.existsSync;

    for (const candidate of configuredPathCandidates(inputs)) {
        if (existsSync(candidate)) {
            return candidate;
        }
    }

    return undefined;
}

export function resolvePreferredCliSetting(inputs: CliSettingInputs): string {
    const explicitExecutablePath = trimConfiguredPath(inputs.executablePath);
    if (explicitExecutablePath.length > 0) {
        return explicitExecutablePath;
    }

    return trimConfiguredPath(inputs.legacyDebugCliPath);
}
