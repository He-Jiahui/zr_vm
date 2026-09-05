const fs = require('fs');
const path = require('path');
const cp = require('child_process');

function main() {
    const [buildArgument, outputArgument, ...options] = process.argv.slice(2);
    const commitOption = options.find((option) => option.startsWith('--source-commit='));
    const sourceCommit = commitOption && commitOption.slice('--source-commit='.length);
    const configOption = options.find((option) => option.startsWith('--config='));
    const config = configOption && configOption.slice('--config='.length);
    if (!buildArgument || !outputArgument || !/^[0-9a-f]{40}$/.test(sourceCommit || '') ||
        (configOption && !/^[A-Za-z0-9_+-]+$/.test(config)) ||
        options.some((option) => option !== '--build' && option !== commitOption && option !== configOption)) {
        throw new Error('usage: node collect_lsp_baseline.js <build-dir> <output-dir> --source-commit=<40-hex> [--build] [--config=<name>]');
    }
    const build = path.resolve(buildArgument);
    const output = path.resolve(outputArgument);
    if (fs.existsSync(path.join(output, 'summary.json'))) {
        throw new Error(`A baseline already exists in ${output}; use a new output directory`);
    }
    const inventory = JSON.parse(cp.execFileSync('ctest', [
        '--test-dir', build, '--show-only=json-v1', '-R', '^language_server$',
        ...(config ? ['-C', config] : []),
    ], { encoding: 'utf8', maxBuffer: 16 * 1024 * 1024, windowsHide: true }));
    const suite = inventory.tests.find((test) => test.name === 'language_server');
    const executableArgument = suite && Array.isArray(suite.command) && suite.command.find((argument) =>
        argument.startsWith('-DEXECUTABLES='));
    if (!executableArgument) {
        throw new Error('CTest language_server executable inventory is missing; multi-config builds require --config=<name>');
    }
    const executables = executableArgument.slice('-DEXECUTABLES='.length).split(';').filter(Boolean);
    if (executables.length === 0 || new Set(executables).size !== executables.length) {
        throw new Error('CTest language_server inventory is empty or contains duplicate executables');
    }
    for (const executable of executables) {
        const relative = path.relative(build, executable);
        if (path.isAbsolute(relative) || relative === '..' || relative.startsWith(`..${path.sep}`)) {
            throw new Error(`Suite executable is outside the requested build: ${executable}`);
        }
    }
    fs.mkdirSync(output, { recursive: true });
    if (options.includes('--build')) {
        const log = fs.openSync(path.join(output, 'build.log'), 'w');
        const targets = executables.map((executable) => path.basename(executable).replace(/\.exe$/i, ''));
        const result = cp.spawnSync('cmake', [
            '--build', build, '--target', ...targets, '--parallel', '4',
            ...(config ? ['--config', config] : []),
        ], { stdio: ['ignore', log, log], windowsHide: true });
        fs.closeSync(log);
        if (result.error || result.signal || result.status !== 0) {
            throw new Error(`Suite build failed; see ${path.join(output, 'build.log')}`);
        }
    }

    const environment = { ...process.env };
    const libraryDirectory = path.join(build, 'lib');
    const libraryDirectories = config ? [path.join(libraryDirectory, config), libraryDirectory] : [libraryDirectory];
    if (process.platform === 'linux') {
        environment.LD_LIBRARY_PATH = [...libraryDirectories, environment.LD_LIBRARY_PATH || ''].join(':');
    } else if (process.platform === 'win32') {
        const pathKey = Object.keys(environment).find((key) => key.toLowerCase() === 'path') || 'Path';
        environment[pathKey] = [...libraryDirectories, environment[pathKey] || ''].join(';');
    }
    const results = [];
    for (const executable of executables) {
        const name = path.basename(executable);
        const started = process.hrtime.bigint();
        const result = cp.spawnSync(executable, [], {
            cwd: build, env: environment, encoding: 'utf8', windowsHide: true,
            timeout: 120000, maxBuffer: 32 * 1024 * 1024,
        });
        const stdout = result.stdout || '';
        const stderr = result.stderr || '';
        const log = `${stdout}${stderr ? `\n[stderr]\n${stderr}` : ''}`;
        fs.writeFileSync(path.join(output, `${name}.log`), log);
        const failures = [];
        const lines = log.split(/\r?\n/);
        for (let index = 0; index < lines.length; index++) {
            if (/^Fail\s*-|^FAIL:|:FAIL:/.test(lines[index])) {
                let next = index + 1;
                while (next < lines.length &&
                       !/^-{3,}|^={3,}|^Unit Test -|^(?:Fail|Pass)\s*-|^(?:FAIL|PASS):/.test(lines[next])) {
                    next++;
                }
                failures.push(lines.slice(index, next).join('\n').trimEnd());
            }
        }
        const item = {
            executable, exitCode: result.status, signal: result.signal,
            error: result.error && result.error.message,
            elapsedMs: Number(process.hrtime.bigint() - started) / 1000000,
            passed: !result.error && !result.signal && result.status === 0 && failures.length === 0,
            failures, log: `${name}.log`,
        };
        results.push(item);
        console.log(`${item.passed ? 'PASS' : 'FAIL'} ${name}: exit=${result.status}, failure-blocks=${failures.length}`);
    }
    const summary = {
        capturedAt: new Date().toISOString(), sourceCommit, build, config, suite: suite.name,
        passed: results.filter((result) => result.passed).length,
        failed: results.filter((result) => !result.passed).length,
        results,
    };
    fs.writeFileSync(path.join(output, 'summary.json'), `${JSON.stringify(summary, null, 2)}\n`);
    console.log(`Suite baseline: ${summary.passed} passed, ${summary.failed} failed; ${output}`);
    process.exitCode = summary.failed === 0 ? 0 : 1;
}

try {
    main();
} catch (error) {
    console.error(error.stack || String(error));
    process.exitCode = 1;
}
