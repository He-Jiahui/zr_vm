const test = require('node:test');
const assert = require('node:assert/strict');

// BUG: test:unit 未预先编译 TypeScript；out 目录若是旧构建，测试会验证
// 旧快照而非当前 builtinModules.ts，产生误报通过。运行前须先编译。
const {
    getBuiltinModuleSnapshot,
} = require('../out/structure/builtinModules.js');

// 固定系统根目录的子模块入口，防止离线结构视图把运行时常用库遗漏。
test('zr.system exposes built-in child modules', () => {
    const systemModule = getBuiltinModuleSnapshot('zr.system');
    const childNames = (systemModule?.modules ?? []).map((entry) => entry.name);

    assert.deepEqual(
        childNames,
        ['console', 'fs', 'env', 'process', 'gc', 'exception', 'vm'],
    );
});

// 只检查跨符号种类的代表项；完整性仍需与运行时官方清单另行核对。
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
