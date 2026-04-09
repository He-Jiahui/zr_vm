import * as fs from 'node:fs';
import * as path from 'node:path';

export function pickLatestExistingPath(
    candidates: string[],
    options?: {
        existsSync?: (candidate: string) => boolean;
        statSync?: (candidate: string) => { mtimeMs: number };
    },
): string | undefined {
    const existsSync = options?.existsSync ?? fs.existsSync;
    const statSync = options?.statSync ?? fs.statSync;
    let bestCandidate: string | undefined;
    let bestMtime = Number.NEGATIVE_INFINITY;

    for (const candidate of candidates) {
        if (!candidate || !existsSync(candidate)) {
            continue;
        }

        let mtimeMs = 0;
        try {
            mtimeMs = statSync(candidate).mtimeMs;
        } catch {
            mtimeMs = 0;
        }

        if (bestCandidate === undefined || mtimeMs > bestMtime) {
            bestCandidate = candidate;
            bestMtime = mtimeMs;
        }
    }

    return bestCandidate;
}

export function pickLatestExistingDirectoryWithFiles(
    candidates: string[],
    requiredFiles: string[],
    options?: {
        existsSync?: (candidate: string) => boolean;
        statSync?: (candidate: string) => { mtimeMs: number };
    },
): string | undefined {
    const existsSync = options?.existsSync ?? fs.existsSync;
    const statSync = options?.statSync ?? fs.statSync;
    let bestCandidate: string | undefined;
    let bestMtime = Number.NEGATIVE_INFINITY;

    for (const candidate of candidates) {
        if (!candidate || !existsSync(candidate)) {
            continue;
        }

        const normalizedCandidate = path.resolve(candidate);
        if (!requiredFiles.every((fileName) => existsSync(path.join(normalizedCandidate, fileName)))) {
            continue;
        }

        let candidateMtime = 0;
        for (const fileName of requiredFiles) {
            try {
                const fileMtime = statSync(path.join(normalizedCandidate, fileName)).mtimeMs;
                if (fileMtime > candidateMtime) {
                    candidateMtime = fileMtime;
                }
            } catch {
                candidateMtime = 0;
            }
        }

        if (bestCandidate === undefined || candidateMtime > bestMtime) {
            bestCandidate = normalizedCandidate;
            bestMtime = candidateMtime;
        }
    }

    return bestCandidate;
}
