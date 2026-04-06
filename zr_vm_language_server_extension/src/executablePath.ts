export interface CliSettingInputs {
    executablePath?: string;
    legacyDebugCliPath?: string;
}

function trimConfiguredPath(value: string | undefined): string {
    return typeof value === 'string' ? value.trim() : '';
}

export function resolvePreferredCliSetting(inputs: CliSettingInputs): string {
    const explicitExecutablePath = trimConfiguredPath(inputs.executablePath);
    if (explicitExecutablePath.length > 0) {
        return explicitExecutablePath;
    }

    return trimConfiguredPath(inputs.legacyDebugCliPath);
}
