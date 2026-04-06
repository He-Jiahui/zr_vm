const test = require('node:test');
const assert = require('node:assert/strict');

const {
    getBuiltinModuleSnapshot,
} = require('../out/structure/builtinModules.js');

test('zr.system exposes built-in child modules', () => {
    const systemModule = getBuiltinModuleSnapshot('zr.system');
    const childNames = (systemModule?.modules ?? []).map((entry) => entry.name);

    assert.deepEqual(
        childNames,
        ['console', 'fs', 'env', 'process', 'gc', 'exception', 'vm'],
    );
});

test('zr.math exposes representative constants and types', () => {
    const mathModule = getBuiltinModuleSnapshot('zr.math');
    const constantNames = (mathModule?.symbols ?? [])
        .filter((entry) => entry.kind === 'constant')
        .map((entry) => entry.name);
    const typeNames = (mathModule?.symbols ?? [])
        .filter((entry) => entry.kind === 'type')
        .map((entry) => entry.name);

    assert(constantNames.includes('PI'));
    assert(constantNames.includes('EPSILON'));
    assert(typeNames.includes('Vector3'));
    assert(typeNames.includes('Tensor'));
});
