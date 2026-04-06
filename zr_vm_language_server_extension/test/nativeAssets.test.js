const test = require('node:test');
const assert = require('node:assert/strict');

const {
    resolvePreferredCliSetting,
} = require('../out/executablePath.js');

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
