/** DAP 编辑器断点的期望状态；只保留 zrdbg/1 可重新绑定的条件与日志选项。 */
export type DesiredSourceBreakpoint = {
    line: number;
    condition?: string;
    hitCondition?: string;
    logMessage?: string;
};

/** 一次延迟绑定同时携带编辑器路径和运行时路径，避免把运行时路径回写给用户断点。 */
export type PendingSourceBreakpointReplay = {
    sourcePath: string;
    runtimeSourcePath: string;
    breakpoints: DesiredSourceBreakpoint[];
};

/** 会话内每个编辑器文件的快照及去重标识；输入数组不得在记录后影响它。 */
type DesiredSourceBreakpointEntry = {
    sourcePath: string;
    breakpoints: DesiredSourceBreakpoint[];
    fingerprint: string;
};

/** 与 DAP 会话的路径键保持一致，使 Windows 编辑器路径的大小写差异不拆分记录。 */
function canonicalSourcePath(sourceFile: string): string {
    const normalized = sourceFile.replace(/[\\/]+/g, '/');
    return process.platform === 'win32' ? normalized.toLowerCase() : normalized;
}

/** 复制 VS Code 交来的断点，防止后续编辑器更新悄悄改变待重放的快照。 */
function cloneBreakpoints(breakpoints: DesiredSourceBreakpoint[]): DesiredSourceBreakpoint[] {
    return breakpoints.map((breakpoint) => ({ ...breakpoint }));
}

/** 仅在影响 zrdbg/1 绑定的选项改变时重放，避免重复通知运行时。 */
function breakpointFingerprint(breakpoints: DesiredSourceBreakpoint[]): string {
    return JSON.stringify(
        breakpoints.map((breakpoint) => ({
            line: breakpoint.line,
            condition: breakpoint.condition ?? '',
            hitCondition: breakpoint.hitCondition ?? '',
            logMessage: breakpoint.logMessage ?? '',
        })),
    );
}

/** 去重范围是编辑器文件与运行时文件的配对；同一文件可映射到多个运行时模块。 */
function replayKey(sourcePath: string, runtimeSourcePath: string): string {
    return `${canonicalSourcePath(sourcePath)}=>${canonicalSourcePath(runtimeSourcePath)}`;
}

/** 保存 DAP 先于运行时源码解析到来的断点；实例只属于一个内联调试会话。
 * desiredBySourcePath 保存编辑器最新目标，replayedFingerprints 按编辑器/运行时路径配对
 * 记录已应用版本；两者必须随绑定成功时点同步，才可安全跳过重复请求。
 */
export class PendingSourceBreakpointStore {
    private readonly desiredBySourcePath = new Map<string, DesiredSourceBreakpointEntry>();
    private readonly replayedFingerprints = new Map<string, string>();

    /** 记录编辑器的完整目标集合，包括用于清除旧断点的空集合。 */
    rememberDesiredBreakpoints(sourcePath: string, breakpoints: DesiredSourceBreakpoint[]): void {
        if (sourcePath.trim().length === 0) {
            return;
        }

        const desiredBreakpoints = cloneBreakpoints(breakpoints);
        this.desiredBySourcePath.set(canonicalSourcePath(sourcePath), {
            sourcePath,
            breakpoints: desiredBreakpoints,
            fingerprint: breakpointFingerprint(desiredBreakpoints),
        });
    }

    /** 连接建立或运行时重新初始化时枚举快照，调用者需逐个重新绑定。 */
    getDesiredBreakpoints(): Array<{ sourcePath: string; breakpoints: DesiredSourceBreakpoint[] }> {
        return Array.from(this.desiredBySourcePath.values(), (entry) => ({
            sourcePath: entry.sourcePath,
            breakpoints: cloneBreakpoints(entry.breakpoints),
        }));
    }

    /** 仅在 zrdbg/1 确认绑定后登记已应用状态。 */
    markBindingApplied(sourcePath: string, runtimeSourcePath: string): void {
        const entry = this.desiredBySourcePath.get(canonicalSourcePath(sourcePath));
        if (entry === undefined) {
            return;
        }

        this.replayedFingerprints.set(replayKey(sourcePath, runtimeSourcePath), entry.fingerprint);
    }

    /** 运行时报告源码映射后，找出尚未绑定到该运行时路径的编辑器断点。 */
    replayBindingsForResolvedSource(
        runtimeSourcePath: string,
        resolvedPath: string | undefined,
    ): PendingSourceBreakpointReplay[] {
        if (resolvedPath === undefined || resolvedPath.trim().length === 0) {
            return [];
        }

        const entry = this.desiredBySourcePath.get(canonicalSourcePath(resolvedPath));
        if (entry === undefined) {
            return [];
        }

        const key = replayKey(entry.sourcePath, runtimeSourcePath);
        if (this.replayedFingerprints.get(key) === entry.fingerprint) {
            return [];
        }

        // BUG: 这里在 dapSession.bindDesiredSourceBreakpoints 成功前就标记为已重放；
        // 若 zrdbg/1 请求失败，下一次同一路径解析会返回空集合，断点不会自动重试。
        this.replayedFingerprints.set(key, entry.fingerprint);
        return [
            {
                sourcePath: entry.sourcePath,
                runtimeSourcePath,
                breakpoints: cloneBreakpoints(entry.breakpoints),
            },
        ];
    }
}
