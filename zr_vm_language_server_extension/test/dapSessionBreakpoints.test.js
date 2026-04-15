const test = require('node:test');
const assert = require('node:assert/strict');

const {
    PendingSourceBreakpointStore,
} = require('../out/debug/breakpointReplay.js');

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
