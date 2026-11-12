const test = require('node:test');
const assert = require('node:assert/strict');

const {
    PendingSourceBreakpointStore,
} = require('../out/debug/breakpointReplay.js');

// 覆盖 launch 前保存编辑器绝对路径、模块加载后才获知 runtime 相对路径的衔接。
// Match resolved paths using the host platform's case rules.
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

    if (process.platform !== 'win32') {
        assert.deepEqual(
            store.replayBindingsForResolvedSource('src/main.zr', 'c:/repo/src/main.zr'),
            [],
            'POSIX source matching remains case-sensitive',
        );
    }

    const resolvedEditorPath = process.platform === 'win32'
        ? 'c:/repo/src/main.zr'
        : 'C:/Repo/src/main.zr';
    assert.deepEqual(
        store.replayBindingsForResolvedSource('src/main.zr', resolvedEditorPath),
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
