export interface ParsedProjectManifest {
    name?: string;
    source: string;
    binary: string;
    entry: string;
    pathAliases?: Record<string, string>;
    dependency?: string;
    local?: string;
    projectPath: string;
}

export interface ProjectPathMatchCandidate {
    id: string;
    sourceRootPath: string;
}

type ManifestRecord = Record<string, unknown>;

export function normalizeFilePath(value: string): string {
    return value.replace(/[\\/]+/g, '/').replace(/\/+$/g, '');
}

export function dirnameFromPath(value: string): string {
    const normalized = normalizeFilePath(value);
    const lastSlash = normalized.lastIndexOf('/');
    return lastSlash > 0 ? normalized.slice(0, lastSlash) : normalized;
}

export function joinNormalizedPath(basePath: string, relativePath: string): string {
    const normalizedBasePath = normalizeFilePath(basePath);
    const normalizedRelativePath = normalizeFilePath(relativePath);
    if (normalizedRelativePath.startsWith('/')) {
        return normalizedRelativePath;
    }

    const combined = `${normalizedBasePath}/${normalizedRelativePath}`;
    const segments = combined.split('/');
    const resolved: string[] = [];

    for (const segment of segments) {
        if (!segment || segment === '.') {
            if (resolved.length === 0 && combined.startsWith('/')) {
                resolved.push('');
            }
            continue;
        }

        if (segment === '..') {
            if (resolved.length > 1 || (resolved.length === 1 && resolved[0] !== '')) {
                resolved.pop();
            }
            continue;
        }

        resolved.push(segment);
    }

    return resolved.length === 1 && resolved[0] === ''
        ? '/'
        : resolved.join('/');
}

export function parseProjectManifestText(
    text: string,
    projectPath: string,
): ParsedProjectManifest | undefined {
    let parsed: ManifestRecord;

    try {
        parsed = JSON.parse(text) as ManifestRecord;
    } catch {
        return undefined;
    }

    const source = readRequiredString(parsed, 'source');
    const binary = readRequiredString(parsed, 'binary');
    const entry = readRequiredString(parsed, 'entry');
    if (!source || !binary || !entry) {
        return undefined;
    }

    return {
        name: readOptionalString(parsed, 'name'),
        source,
        binary,
        entry,
        pathAliases: readPathAliases(parsed),
        dependency: readOptionalString(parsed, 'dependency'),
        local: readOptionalString(parsed, 'local'),
        projectPath: normalizeFilePath(projectPath),
    };
}

export function isPathInsideDirectory(filePath: string, directoryPath: string): boolean {
    const normalizedFilePath = normalizeFilePath(filePath);
    const normalizedDirectoryPath = normalizeFilePath(directoryPath);

    if (normalizedDirectoryPath.length === 0) {
        return false;
    }
    if (normalizedFilePath === normalizedDirectoryPath) {
        return true;
    }

    return normalizedFilePath.startsWith(`${normalizedDirectoryPath}/`);
}

export function pickBestProjectForFile<T extends ProjectPathMatchCandidate>(
    filePath: string,
    candidates: T[],
): T | undefined {
    const matches = candidates
        .filter((candidate) => isPathInsideDirectory(filePath, candidate.sourceRootPath))
        .sort((left, right) => right.sourceRootPath.length - left.sourceRootPath.length);

    return matches[0];
}

function readRequiredString(record: ManifestRecord, key: string): string | undefined {
    const value = readOptionalString(record, key);
    return value && value.length > 0 ? value : undefined;
}

function readOptionalString(record: ManifestRecord, key: string): string | undefined {
    const value = record[key];
    return typeof value === 'string' ? value.trim() : undefined;
}

function readPathAliases(record: ManifestRecord): Record<string, string> | undefined {
    const value = record.pathAliases;
    const result: Record<string, string> = {};

    if (!value || typeof value !== 'object' || Array.isArray(value)) {
        return undefined;
    }

    for (const [alias, rawPrefix] of Object.entries(value)) {
        const trimmedAlias = alias.trim();
        const trimmedPrefix = typeof rawPrefix === 'string' ? normalizeFilePath(rawPrefix.trim()) : '';
        if (!trimmedAlias || !trimmedPrefix) {
            continue;
        }

        result[trimmedAlias] = trimmedPrefix;
    }

    return Object.keys(result).length > 0 ? result : undefined;
}
