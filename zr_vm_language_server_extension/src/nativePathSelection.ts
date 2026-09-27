import * as fs from 'node:fs';
import * as path from 'node:path';

/** 开发构建的单文件回退选最近更新者；时间相同时保留先出现的候选。
 * @note 注入文件系统查询仅供不依赖真实构建目录的调用或测试。
 */
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

/** 捆绑资产的优先级由候选顺序指定，时间戳不覆盖该顺序。 */
export function pickFirstExistingPath(
    candidates: string[],
    options?: {
        existsSync?: (candidate: string) => boolean;
    },
): string | undefined {
    const existsSync = options?.existsSync ?? fs.existsSync;

    for (const candidate of candidates) {
        if (!candidate || !existsSync(candidate)) {
            continue;
        }

        return candidate;
    }

    return undefined;
}

/** 仅在所需文件全部存在时选择开发构建目录，避免跨目录拼接运行时依赖。
 * @note 用所需文件的最新修改时间比较目录；同分沿用候选顺序。
 */
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

/** 对捆绑候选按声明顺序找完整目录，返回规范化的绝对路径。 */
export function pickFirstExistingDirectoryWithFiles(
    candidates: string[],
    requiredFiles: string[],
    options?: {
        existsSync?: (candidate: string) => boolean;
    },
): string | undefined {
    const existsSync = options?.existsSync ?? fs.existsSync;

    for (const candidate of candidates) {
        if (!candidate || !existsSync(candidate)) {
            continue;
        }

        const normalizedCandidate = path.resolve(candidate);
        if (requiredFiles.every((fileName) => existsSync(path.join(normalizedCandidate, fileName)))) {
            return normalizedCandidate;
        }
    }

    return undefined;
}
