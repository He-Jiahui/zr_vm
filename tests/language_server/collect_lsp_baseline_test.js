const assert = require('assert');
const fs = require('fs');
const path = require('path');
const vm = require('vm');

const collectorPath = path.join(__dirname, 'collect_lsp_baseline.js');
const collectorSource = fs.readFileSync(collectorPath, 'utf8');
const build = '/baseline/build';
const output = '/baseline/output';
const summaryPath = `${output}/summary.json`;
const sourceCommit = '0123456789abcdef0123456789abcdef01234567';

/** 在隔离的 VM 内运行真实采集脚本，把 CTest、构建、进程与文件写入映射到内存。 */
function collect(processResults, existingFiles = [], options = {}) {
    const platform = options.platform || 'linux';
    const paths = platform === 'win32' ? path.win32 : path.posix;
    const buildDirectory = platform === 'win32' ? 'C:\\baseline\\build' : build;
    const outputDirectory = platform === 'win32' ? 'C:\\baseline\\output' : output;
    const summaryFile = paths.join(outputDirectory, 'summary.json');
    const files = new Map(existingFiles);
    const writes = [];
    const executions = [];
    const inventoryCalls = [];
    const buildCalls = [];
    const executableCalls = [];
    const errors = [];
    let inventoryReads = 0;
    const executables = processResults.map((_, index) => paths.join(
        buildDirectory, 'bin', options.config || '', `test_${index}${platform === 'win32' ? '.exe' : ''}`,
    ));
    const collectorProcess = {
        argv: ['node', collectorPath, buildDirectory, outputDirectory, `--source-commit=${sourceCommit}`]
            .concat(options.build ? ['--build'] : [])
            .concat(options.config !== undefined ? [`--config=${options.config}`] : []),
        env: options.environment || {},
        platform,
        hrtime: process.hrtime,
    };
    const dependencies = {
        fs: {
            existsSync: (name) => files.has(name),
            mkdirSync: (name) => writes.push(name),
            openSync: (name) => {
                writes.push(name);
                return name;
            },
            closeSync: () => {},
            writeFileSync: (name, content) => {
                writes.push(name);
                files.set(name, content);
            },
        },
        path: paths,
        child_process: {
            execFileSync: (command, args) => {
                assert.strictEqual(command, 'ctest');
                inventoryCalls.push(Array.from(args));
                inventoryReads++;
                return JSON.stringify({
                    tests: [{
                        name: 'language_server',
                        command: options.missingCommand ? undefined : [`-DEXECUTABLES=${executables.join(';')}`],
                    }],
                });
            },
            spawnSync: (executable, args, spawnOptions) => {
                if (executable === 'cmake') {
                    assert.strictEqual(options.build, true);
                    buildCalls.push(Array.from(args));
                    return { status: 0, signal: null };
                }
                const index = executables.indexOf(executable);
                assert.notStrictEqual(index, -1, 'only inventory members may execute');
                executions.push(executable);
                executableCalls.push({ executable, environment: { ...spawnOptions.env } });
                return {
                    status: 0, signal: null, stdout: '', stderr: '',
                    ...processResults[index],
                };
            },
        },
    };
    // 复用真实 CLI 主流程，但所有副作用留在内存，以验证拒绝分支先于文件写入。
    vm.runInNewContext(collectorSource, {
        require: (name) => {
            assert.ok(dependencies[name], `unexpected dependency: ${name}`);
            return dependencies[name];
        },
        process: collectorProcess,
        console: { log: () => {}, error: (message) => errors.push(message) },
    }, { filename: collectorPath, timeout: 1000 });
    return {
        files, writes, executions, errors, inventoryReads,
        inventoryCalls, buildCalls, executableCalls, buildDirectory,
        exitCode: collectorProcess.exitCode,
        summary: files.has(summaryFile) ? JSON.parse(files.get(summaryFile)) : null,
    };
}

const cases = [];
// 进程成功退出仍可能在 stdout/stderr 中报告测试失败，基线不能只看退出码。
for (const stream of ['stdout', 'stderr']) {
    cases.push([`exit-zero printed failure on ${stream}`, () => {
        const block = 'FAIL: named fixture\nexpected canonical fact; actual null';
        const run = collect([{ [stream]: `${block}\n` }]);
        assert.strictEqual(run.exitCode, 1);
        assert.strictEqual(run.summary.failed, 1);
        assert.strictEqual(run.summary.results[0].passed, false);
        assert.strictEqual(run.summary.results[0].exitCode, 0);
        assert.deepStrictEqual(run.summary.results[0].failures, [block]);
        assert.ok(run.files.get(`${output}/test_0.log`).includes(block));
    }]);
}

// 一个失败样例不应阻断后续二进制的证据采集。
cases.push(['collection continues after a failed executable', () => {
    const run = collect([{ stdout: 'FAIL: first fixture\n' }, {}, {}]);
    assert.strictEqual(run.exitCode, 1);
    assert.deepStrictEqual(run.executions, [
        `${build}/bin/test_0`, `${build}/bin/test_1`, `${build}/bin/test_2`,
    ]);
    assert.strictEqual(run.summary.passed, 2);
    assert.strictEqual(run.summary.failed, 1);
    assert.strictEqual(run.summary.results.length, 3);
    assert.ok(run.files.has(`${output}/test_2.log`));
}]);

// 超时和非零退出的原始信号与部分输出仍属于可审计的失败证据。
cases.push(['timeout and nonzero exits retain failure evidence', () => {
    const run = collect([
        { status: null, signal: 'SIGTERM', error: new Error('spawnSync ETIMEDOUT'), stdout: 'partial output' },
        { status: 7, stderr: 'fixture setup failed' },
        {},
    ]);
    assert.strictEqual(run.exitCode, 1);
    assert.strictEqual(run.summary.failed, 2);
    assert.strictEqual(run.summary.passed, 1);
    assert.strictEqual(run.summary.results[0].exitCode, null);
    assert.strictEqual(run.summary.results[0].signal, 'SIGTERM');
    assert.strictEqual(run.summary.results[0].error, 'spawnSync ETIMEDOUT');
    assert.strictEqual(run.summary.results[1].exitCode, 7);
    assert.strictEqual(run.summary.results[1].passed, false);
    assert.strictEqual(run.files.get(`${output}/test_0.log`), 'partial output');
    assert.ok(run.files.get(`${output}/test_1.log`).includes('fixture setup failed'));
    assert.strictEqual(run.executions.length, 3);
}]);

// spawn 的 error 优先于 status，防止异常路径被误记为通过。
cases.push(['timeout error fails even when spawn status is zero', () => {
    const run = collect([{
        status: 0, signal: null, error: new Error('spawnSync ETIMEDOUT'),
        stdout: 'partial output',
    }]);
    assert.strictEqual(run.exitCode, 1);
    assert.strictEqual(run.summary.failed, 1);
    assert.strictEqual(run.summary.results[0].passed, false);
    assert.strictEqual(run.summary.results[0].exitCode, 0);
    assert.strictEqual(run.summary.results[0].signal, null);
    assert.strictEqual(run.summary.results[0].error, 'spawnSync ETIMEDOUT');
}]);

// 多配置构建与动态库搜索路径在 Linux/Windows 上均须指向同一配置。
for (const platform of ['linux', 'win32']) {
    cases.push([`configured ${platform} collection selects inventory, build, and libraries`, () => {
        const paths = platform === 'win32' ? path.win32 : path.posix;
        const variable = platform === 'win32' ? 'Path' : 'LD_LIBRARY_PATH';
        const separator = platform === 'win32' ? ';' : ':';
        const environment = { [variable]: 'existing-libraries' };
        const run = collect([{}, {}], [], {
            build: true, config: 'Debug', platform, environment,
        });
        assert.strictEqual(run.exitCode, 0, run.errors.join('\n'));
        assert.strictEqual(run.summary.passed, 2);
        assert.strictEqual(run.summary.failed, 0);
        assert.strictEqual(run.summary.config, 'Debug');
        assert.strictEqual(run.inventoryCalls.length, 1);
        const inventoryArgs = run.inventoryCalls[0];
        assert.ok(inventoryArgs.includes('-C'), 'CTest must select the requested configuration');
        assert.strictEqual(inventoryArgs[inventoryArgs.indexOf('-C') + 1], 'Debug');
        assert.strictEqual(run.buildCalls.length, 1);
        const buildArgs = run.buildCalls[0];
        assert.strictEqual(buildArgs[buildArgs.indexOf('--build') + 1], run.buildDirectory);
        assert.ok(buildArgs.includes('--config'), 'CMake must select the requested configuration');
        assert.strictEqual(buildArgs[buildArgs.indexOf('--config') + 1], 'Debug');
        assert.ok(buildArgs.includes('--target'));
        assert.deepStrictEqual(buildArgs.slice(buildArgs.indexOf('--target') + 1)
            .filter((argument) => /^test_/.test(argument)), ['test_0', 'test_1']);
        const expectedLibraries = [
            paths.join(run.buildDirectory, 'lib', 'Debug'),
            paths.join(run.buildDirectory, 'lib'),
            'existing-libraries',
        ].join(separator);
        assert.strictEqual(run.executableCalls.length, 2);
        for (const call of run.executableCalls) {
            assert.strictEqual(call.environment[variable], expectedLibraries);
            assert.ok(call.executable.includes(paths.join('bin', 'Debug')));
        }
        assert.strictEqual(environment[variable], 'existing-libraries');
    }]);
}

// 无效配置不能进入 CTest 枚举，更不能创建或覆盖基线目录。
cases.push(['invalid configurations fail before inventory or filesystem mutation', () => {
    for (const config of ['', '../Debug']) {
        const run = collect([{}], [], { config });
        assert.strictEqual(run.exitCode, 1);
        assert.strictEqual(run.inventoryReads, 0);
        assert.deepStrictEqual(run.writes, []);
        assert.ok(run.errors.some((message) => /usage:/.test(message)));
    }
}]);

// CTest 缺少可执行清单时要给出配置提示，而非猜测目标路径。
cases.push(['missing multi-config command gives configuration guidance', () => {
    const run = collect([{}], [], { missingCommand: true });
    assert.strictEqual(run.exitCode, 1);
    assert.deepStrictEqual(run.writes, []);
    assert.deepStrictEqual(run.executions, []);
    assert.ok(run.errors.some((message) => /inventory is missing.*--config/.test(message)));
}]);

// 完整摘要存在时不得重用输出目录；这里只覆盖已有 summary 的情形。
cases.push(['existing summaries and logs are preserved without running children', () => {
    const original = '{"capturedAt":"historical evidence"}\n';
    const run = collect([{}], [
        [summaryPath, original], [`${output}/test_0.log`, 'historical log'],
    ]);
    assert.strictEqual(run.exitCode, 1);
    assert.strictEqual(run.files.get(summaryPath), original);
    assert.strictEqual(run.files.get(`${output}/test_0.log`), 'historical log');
    assert.deepStrictEqual(run.writes, []);
    assert.deepStrictEqual(run.executions, []);
    assert.strictEqual(run.inventoryReads, 0);
    assert.ok(run.errors.some((message) => /baseline already exists/.test(message)));
}]);

// 正常采集须在摘要中保留调用方提交标签与执行环境，供后续比较使用。
cases.push(['successful inventory records caller provenance and exits zero', () => {
    const run = collect([{ stdout: 'PASS: fixture\n' }]);
    assert.strictEqual(run.exitCode, 0);
    assert.strictEqual(run.summary.passed, 1);
    assert.strictEqual(run.summary.failed, 0);
    assert.strictEqual(run.summary.sourceCommit, sourceCommit);
    assert.strictEqual(run.summary.config, undefined);
    assert.ok(!run.inventoryCalls[0].includes('-C'));
    assert.deepStrictEqual(run.buildCalls, []);
    assert.strictEqual(run.executableCalls[0].environment.LD_LIBRARY_PATH, `${build}/lib:`);
    assert.deepStrictEqual(run.summary.results[0].failures, []);
    assert.deepStrictEqual(run.errors, []);
}]);

let failed = 0;
for (const [name, test] of cases) {
    try {
        test();
        console.log(`PASS: ${name}`);
    } catch (error) {
        failed++;
        console.error(`FAIL: ${name}\n${error.stack || error}`);
    }
}
console.log(`Baseline collector tests: ${cases.length - failed} passed, ${failed} failed`);
process.exitCode = failed === 0 ? 0 : 1;
