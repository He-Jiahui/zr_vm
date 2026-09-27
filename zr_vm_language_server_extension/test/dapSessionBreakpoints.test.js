const test = require('node:test');
const assert = require('node:assert/strict');

const {
    PendingSourceBreakpointStore,
} = require('../out/debug/breakpointReplay.js');

// 覆盖 launch 前保存编辑器绝对路径、模块加载后才获知 runtime 相对路径的衔接。
// BUG: 测试把 C:/Repo 和 c:/repo 当同一路径，但 store 仅在 win32 折叠大小写；
// Linux/macOS 上此处期望非空重放会失败，测试不能作为跨平台回归门禁。
test('PendingSourceBreakpointStore replays pre-launch editor breakpoints after runtime source resolution', () => {
    const store = new PendingSourceBreakpointStore();
    const desiredBreakpoints = [
        { line: 12 },
        { line: 18, condition: 'seed > 3' },
    ];

    store.rememberDesiredBreakpoints('C:/Repo/src/main.zr', desiredBreakpoints);

    assert.deepEqual(
        store.replayBindingsForResolvedSource('src/main.zr', undefined),
        [],
    );

    assert.deepEqual(
        store.replayBindingsForResolvedSource('src/main.zr', 'c:/repo/src/main.zr'),
        [
            {
                sourcePath: 'C:/Repo/src/main.zr',
                runtimeSourcePath: 'src/main.zr',
                breakpoints: desiredBreakpoints,
            },
        ],
    );
});

// runtime 可能从 moduleLoaded、stopped、stackTrace 重复提供同一路径；只在用户断点意图变化后再重放。
test('PendingSourceBreakpointStore suppresses duplicate replays until the desired breakpoint set changes', () => {
    const store = new PendingSourceBreakpointStore();

    store.rememberDesiredBreakpoints('D:/workspace/app/main.zr', [{ line: 7 }]);

    assert.deepEqual(
        store.replayBindingsForResolvedSource('app/main.zr', 'D:/workspace/app/main.zr'),
        [
            {
                sourcePath: 'D:/workspace/app/main.zr',
                runtimeSourcePath: 'app/main.zr',
                breakpoints: [{ line: 7 }],
            },
        ],
    );

    assert.deepEqual(
        store.replayBindingsForResolvedSource('app/main.zr', 'D:/workspace/app/main.zr'),
        [],
    );

    store.rememberDesiredBreakpoints('D:/workspace/app/main.zr', [{ line: 9 }]);

    assert.deepEqual(
        store.replayBindingsForResolvedSource('app/main.zr', 'D:/workspace/app/main.zr'),
        [
            {
                sourcePath: 'D:/workspace/app/main.zr',
                runtimeSourcePath: 'app/main.zr',
                breakpoints: [{ line: 9 }],
            },
        ],
    );
});
