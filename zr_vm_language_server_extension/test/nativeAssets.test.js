const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');

const {
    resolvePreferredCliSetting,
} = require('../out/executablePath.js');
const {
    pickLatestExistingPath,
    pickLatestExistingDirectoryWithFiles,
} = require('../out/nativePathSelection.js');

test('resolvePreferredCliSetting prefers zr.executablePath over the legacy debug setting', () => {
    const resolved = resolvePreferredCliSetting({
        executablePath: 'D:/tools/new-zr_vm_cli.exe',
        legacyDebugCliPath: 'D:/tools/old-zr_vm_cli.exe',
    });

    assert.equal(resolved, 'D:/tools/new-zr_vm_cli.exe');
});

test('resolvePreferredCliSetting falls back to zr.debug.cli.path when zr.executablePath is empty', () => {
    const resolved = resolvePreferredCliSetting({
        executablePath: '   ',
        legacyDebugCliPath: 'D:/tools/old-zr_vm_cli.exe',
    });

    assert.equal(resolved, 'D:/tools/old-zr_vm_cli.exe');
});

test('pickLatestExistingPath prefers the newest existing native asset candidate', () => {
    const tempRoot = fs.mkdtempSync(path.join(os.tmpdir(), 'zr-native-assets-'));

    try {
        const olderPath = path.join(tempRoot, 'older.exe');
        const newerPath = path.join(tempRoot, 'newer.exe');
        fs.writeFileSync(olderPath, 'older');
        fs.writeFileSync(newerPath, 'newer');
        fs.utimesSync(olderPath, new Date('2025-01-01T00:00:00Z'), new Date('2025-01-01T00:00:00Z'));
        fs.utimesSync(newerPath, new Date('2026-01-01T00:00:00Z'), new Date('2026-01-01T00:00:00Z'));

        const resolved = pickLatestExistingPath([
            path.join(tempRoot, 'missing.exe'),
            olderPath,
            newerPath,
        ]);

        assert.equal(resolved, newerPath);
    } finally {
        fs.rmSync(tempRoot, { recursive: true, force: true });
    }
});

test('pickLatestExistingDirectoryWithFiles ignores newer incomplete native asset directories', () => {
    const tempRoot = fs.mkdtempSync(path.join(os.tmpdir(), 'zr-native-asset-dir-'));

    try {
        const olderCompleteDir = path.join(tempRoot, 'older-complete');
        const newerIncompleteDir = path.join(tempRoot, 'newer-incomplete');
        const newerCompleteDir = path.join(tempRoot, 'newer-complete');
        fs.mkdirSync(olderCompleteDir, { recursive: true });
        fs.mkdirSync(newerIncompleteDir, { recursive: true });
        fs.mkdirSync(newerCompleteDir, { recursive: true });

        for (const fileName of ['zr_vm_language_server_stdio.exe', 'zr_vm_cli.exe', 'zr_vm_core.dll']) {
            fs.writeFileSync(path.join(olderCompleteDir, fileName), `older-${fileName}`);
        }
        for (const fileName of ['zr_vm_cli.exe', 'zr_vm_core.dll']) {
            fs.writeFileSync(path.join(newerIncompleteDir, fileName), `incomplete-${fileName}`);
        }
        for (const fileName of ['zr_vm_language_server_stdio.exe', 'zr_vm_cli.exe', 'zr_vm_core.dll']) {
            fs.writeFileSync(path.join(newerCompleteDir, fileName), `newer-${fileName}`);
        }

        fs.utimesSync(olderCompleteDir, new Date('2025-01-01T00:00:00Z'), new Date('2025-01-01T00:00:00Z'));
        fs.utimesSync(newerIncompleteDir, new Date('2026-01-01T00:00:00Z'), new Date('2026-01-01T00:00:00Z'));
        fs.utimesSync(newerCompleteDir, new Date('2026-02-01T00:00:00Z'), new Date('2026-02-01T00:00:00Z'));
        for (const fileName of ['zr_vm_language_server_stdio.exe', 'zr_vm_cli.exe', 'zr_vm_core.dll']) {
            fs.utimesSync(
                path.join(olderCompleteDir, fileName),
                new Date('2025-01-01T00:00:00Z'),
                new Date('2025-01-01T00:00:00Z'),
            );
        }
        for (const fileName of ['zr_vm_cli.exe', 'zr_vm_core.dll']) {
            fs.utimesSync(
                path.join(newerIncompleteDir, fileName),
                new Date('2026-01-01T00:00:00Z'),
                new Date('2026-01-01T00:00:00Z'),
            );
        }
        for (const fileName of ['zr_vm_language_server_stdio.exe', 'zr_vm_cli.exe', 'zr_vm_core.dll']) {
            fs.utimesSync(
                path.join(newerCompleteDir, fileName),
                new Date('2026-02-01T00:00:00Z'),
                new Date('2026-02-01T00:00:00Z'),
            );
        }

        const resolved = pickLatestExistingDirectoryWithFiles(
            [
                path.join(tempRoot, 'missing'),
                olderCompleteDir,
                newerIncompleteDir,
                newerCompleteDir,
            ],
            ['zr_vm_language_server_stdio.exe', 'zr_vm_cli.exe', 'zr_vm_core.dll'],
        );

        assert.equal(resolved, newerCompleteDir);
    } finally {
        fs.rmSync(tempRoot, { recursive: true, force: true });
    }
});
