const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const test = require('node:test');
const vm = require('node:vm');

function runBuild(platform, { cache = false } = {}) {
    const calls = [];
    const deleted = [];
    const repositoryRoot = platform === 'win32' ? "E:\\work\\dev's project" : '/work/zr';
    const buildDir = platform === 'win32' ? "D:\\cargo-targets\\dev's wasm" : '/outputs/wasm';
    const source = fs.readFileSync(path.join(__dirname, '../scripts/build-wasm-server.js'), 'utf8');
    const mockProcess = {
        platform,
        env: { ZR_WASM_EMSDK_ENV_WSL: "/mnt/e/dev's sdk/emsdk_env.sh" },
        exit(code) { throw new Error(`exit:${code}`); },
    };
    vm.runInNewContext(source, {
        process: mockProcess,
        console,
        __dirname,
        require(name) {
            if (name === 'node:child_process') {
                return { spawnSync(program, args) { calls.push({ program, args }); return { status: 0 }; } };
            }
            if (name === 'node:fs') {
                return {
                    existsSync: () => cache,
                    readFileSync: () => 'CMAKE_GENERATOR:INTERNAL=Visual Studio 17 2022\n',
                    mkdirSync() {},
                    rmSync(value) { deleted.push(value); },
                };
            }
            if (name === 'node:path') return platform === 'win32' ? path.win32 : path.posix;
            if (name === './artifact-layout') {
                return { createArtifactLayout: () => ({ repositoryRoot, wasm: { buildDir, sourceCandidates: [] } }) };
            }
            throw new Error(`unexpected require: ${name}`);
        },
    });
    return { calls, deleted };
}

test('Windows wasm commands use absolute WSL paths and quote apostrophes', () => {
    const { calls } = runBuild('win32');
    const configure = calls.find(({ args }) => args.some((arg) => arg.includes('emcmake cmake')));
    const build = calls.find(({ args }) => args.some((arg) => arg.includes('cmake --build')));
    assert.ok(configure.args[2].includes("'/mnt/d/cargo-targets/dev'\\''s wasm'"));
    assert.ok(build.args[2].includes("'/mnt/d/cargo-targets/dev'\\''s wasm'"));
    assert.ok(configure.args[2].includes("'/mnt/e/work/dev'\\''s project'"));
    assert.ok(configure.args[2].includes("'/mnt/e/dev'\\''s sdk/emsdk_env.sh'"));
});

test('wasm configure disables host-only Rust and network/debug entry points', () => {
    const { calls } = runBuild('linux');
    const configure = calls.find(({ program, args }) => program === 'emcmake' && args.includes('cmake'));
    for (const flag of ['-DBUILD_RUST_BINDING=OFF', '-DBUILD_NETWORK_LIB=OFF',
        '-DBUILD_DEBUG_LIB=OFF', '-DBUILD_THREAD_LIB=OFF', '-DBUILD_CLI=OFF',
        '-DBUILD_LANGUAGE_SERVER_STDIO=OFF']) assert.ok(configure.args.includes(flag), flag);
});

test('an incompatible cache is rejected without deleting an arbitrary build tree', () => {
    assert.throws(() => runBuild('linux', { cache: true }), /incompatible|dedicated|cache/i);
});
