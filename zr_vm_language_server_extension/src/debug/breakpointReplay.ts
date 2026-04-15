export type DesiredSourceBreakpoint = {
    line: number;
    condition?: string;
    hitCondition?: string;
    logMessage?: string;
};

export type PendingSourceBreakpointReplay = {
    sourcePath: string;
    runtimeSourcePath: string;
    breakpoints: DesiredSourceBreakpoint[];
};

type DesiredSourceBreakpointEntry = {
    sourcePath: string;
    breakpoints: DesiredSourceBreakpoint[];
    fingerprint: string;
};

function canonicalSourcePath(sourceFile: string): string {
    const normalized = sourceFile.replace(/[\\/]+/g, '/');
    return process.platform === 'win32' ? normalized.toLowerCase() : normalized;
}

function cloneBreakpoints(breakpoints: DesiredSourceBreakpoint[]): DesiredSourceBreakpoint[] {
    return breakpoints.map((breakpoint) => ({ ...breakpoint }));
}

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

function replayKey(sourcePath: string, runtimeSourcePath: string): string {
    return `${canonicalSourcePath(sourcePath)}=>${canonicalSourcePath(runtimeSourcePath)}`;
}

export class PendingSourceBreakpointStore {
    private readonly desiredBySourcePath = new Map<string, DesiredSourceBreakpointEntry>();
    private readonly replayedFingerprints = new Map<string, string>();

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

    getDesiredBreakpoints(): Array<{ sourcePath: string; breakpoints: DesiredSourceBreakpoint[] }> {
        return Array.from(this.desiredBySourcePath.values(), (entry) => ({
            sourcePath: entry.sourcePath,
            breakpoints: cloneBreakpoints(entry.breakpoints),
        }));
    }

    markBindingApplied(sourcePath: string, runtimeSourcePath: string): void {
        const entry = this.desiredBySourcePath.get(canonicalSourcePath(sourcePath));
        if (entry === undefined) {
            return;
        }

        this.replayedFingerprints.set(replayKey(sourcePath, runtimeSourcePath), entry.fingerprint);
    }

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
