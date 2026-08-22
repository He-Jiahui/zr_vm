const { spawnSync } = require('child_process');
const fs = require('fs');
const os = require('os');
const path = require('path');
const { pathToFileURL, fileURLToPath } = require('url');
const { StdioProtocolClient } = require('./stdio_protocol_client');

const DEFAULT_STDIO_PEAK_MEMORY_LIMIT_BYTES = 512 * 1024 * 1024;

function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

function peakMemoryLimitBytes() {
    const configured = process.env.ZR_LSP_STDIO_PEAK_MEMORY_LIMIT_BYTES;

    if (configured === undefined || configured.length === 0) {
        return DEFAULT_STDIO_PEAK_MEMORY_LIMIT_BYTES;
    }

    assert(/^\d+$/.test(configured),
        'ZR_LSP_STDIO_PEAK_MEMORY_LIMIT_BYTES must be a positive integer');
    const bytes = Number(configured);
    assert(Number.isSafeInteger(bytes) && bytes > 0,
        'ZR_LSP_STDIO_PEAK_MEMORY_LIMIT_BYTES must be a positive safe integer');
    return bytes;
}

function readLinuxProcessPeakMemoryBytes(pid) {
    const statusPath = `/proc/${pid}/status`;
    const status = fs.readFileSync(statusPath, 'utf8');
    const peakMatch = status.match(/^VmHWM:\s+(\d+)\s+kB$/m);

    assert(peakMatch,
        `Unable to read VmHWM for language server process ${pid} from ${statusPath}`);
    return Number(peakMatch[1]) * 1024;
}

function readWindowsProcessPeakMemoryBytes(pid) {
    const probe = spawnSync('powershell.exe', [
        '-NoLogo',
        '-NoProfile',
        '-NonInteractive',
        '-Command',
        `(Get-Process -Id ${pid} -ErrorAction Stop).PeakWorkingSet64`,
    ], {
        encoding: 'utf8',
        windowsHide: true,
    });

    assert(!probe.error && probe.status === 0,
        `Unable to read PeakWorkingSet64 for language server process ${pid}: ` +
        `${probe.error ? probe.error.message : probe.stderr}`);
    const text = probe.stdout.trim();
    assert(/^\d+$/.test(text),
        `Invalid PeakWorkingSet64 for language server process ${pid}: ${text}`);
    const bytes = Number(text);
    assert(Number.isSafeInteger(bytes) && bytes > 0,
        `Invalid PeakWorkingSet64 value for language server process ${pid}: ${text}`);
    return bytes;
}

function readProcessPeakMemoryBytes(pid) {
    assert(Number.isInteger(pid) && pid > 0,
        'language server child process must have a valid pid for peak-memory accounting');

    if (process.platform === 'linux') {
        return readLinuxProcessPeakMemoryBytes(pid);
    }
    if (process.platform === 'win32') {
        return readWindowsProcessPeakMemoryBytes(pid);
    }

    throw new Error(`Peak-memory accounting is unsupported on ${process.platform}`);
}

function formatMemoryMiB(bytes) {
    return (bytes / (1024 * 1024)).toFixed(2);
}

class LspProcessPeakMemory {
    constructor(pid, limitBytes) {
        this.pid = pid;
        this.limitBytes = limitBytes;
        this.peakBytes = 0;
    }

    observe(label) {
        const bytes = readProcessPeakMemoryBytes(this.pid);
        this.peakBytes = Math.max(this.peakBytes, bytes);
        return { bytes, label };
    }

    assertWithinBudget() {
        this.observe('final');
        console.log(
            `LSP stdio peak working set: ${this.peakBytes} bytes ` +
            `(${formatMemoryMiB(this.peakBytes)} MiB), limit ${this.limitBytes} bytes ` +
            `(${formatMemoryMiB(this.limitBytes)} MiB)`);
        assert(this.peakBytes <= this.limitBytes,
            `LSP stdio peak working set must be <= ${this.limitBytes} bytes, got ${this.peakBytes}`);
    }
}

function percentile(samples, percent) {
    const ordered = [...samples].sort((left, right) => left - right);
    const index = Math.min(ordered.length - 1, Math.ceil(ordered.length * percent) - 1);

    return ordered[index];
}

async function measureWarmRequestLatency(client, method, params, sampleCount = 20) {
    const samples = [];

    for (let index = 0; index < sampleCount; index += 1) {
        const startedAt = process.hrtime.bigint();

        await client.request(method, params);
        samples.push(Number(process.hrtime.bigint() - startedAt) / 1e6);
    }

    return {
        p50: percentile(samples, 0.50),
        p95: percentile(samples, 0.95),
        p99: percentile(samples, 0.99),
    };
}

function assertWarmRequestBudget(name, latency, limitMs) {
    assert(latency.p95 <= limitMs,
        name + ' warm p95 must be <= ' + limitMs + 'ms, got ' +
        'p50=' + latency.p50.toFixed(2) + 'ms p95=' + latency.p95.toFixed(2) + 'ms ' +
        'p99=' + latency.p99.toFixed(2) + 'ms');
}

async function measureWarmDiagnosticsLatency(client, uri, baseText, sampleCount = 20) {
    const samples = [];

    for (let index = 0; index < sampleCount; index += 1) {
        const version = index + 2;
        const startedAt = process.hrtime.bigint();

        client.notify('textDocument/didChange', {
            textDocument: { uri, version },
            contentChanges: [{ text: baseText + '// diagnostics warm ' + String(index).padStart(2, '0') + '\n' }],
        });
        const diagnostics = await waitForDiagnosticsUriVersion(
            client,
            uri,
            version,
            'warm diagnostics must publish the edited document version');

        assert(Array.isArray(diagnostics.diagnostics) && diagnostics.diagnostics.length === 0,
            'warm diagnostics latency fixture must remain semantically valid');
        samples.push(Number(process.hrtime.bigint() - startedAt) / 1e6);
    }

    return {
        p50: percentile(samples, 0.50),
        p95: percentile(samples, 0.95),
        p99: percentile(samples, 0.99),
    };
}

function diagnosticRelatedUriMatches(expectedUri, actualUri) {
    if (actualUri === expectedUri) {
        return true;
    }
    if (typeof actualUri !== 'string' || typeof expectedUri !== 'string') {
        return false;
    }
    try {
        const expectedPath = fileURLToPath(expectedUri);
        const actualPath = fileURLToPath(actualUri);
        if (process.platform === 'win32') {
            return expectedPath.toLowerCase() === actualPath.toLowerCase();
        }
        return expectedPath === actualPath;
    } catch {
        return false;
    }
}

function assertDiagnosticIncludes(diagnostics, code, messageFragment, reason) {
    assert(diagnostics && Array.isArray(diagnostics.diagnostics),
        `${reason}: diagnostics must be an array`);

    const matchingDiagnostic = diagnostics.diagnostics.find((diagnostic) =>
        diagnostic &&
        diagnostic.code === code &&
        typeof diagnostic.message === 'string' &&
        diagnostic.message.includes(messageFragment));
    assert(matchingDiagnostic,
        `${reason}: expected diagnostic ${code} containing "${messageFragment}"`);
    return matchingDiagnostic;
}

function uriWithEncodedWindowsDrive(uri) {
    if (process.platform !== 'win32') {
        return uri;
    }
    return uri.replace(/^file:\/\/\/([A-Za-z]):/, (_match, drive) =>
        `file:///${drive.toLowerCase()}%3A`);
}

async function waitForDiagnosticsUri(client, uri, message) {
    for (let attempt = 0; attempt < 16; attempt += 1) {
        const diagnostics = await client.waitForNotification('textDocument/publishDiagnostics');
        if (diagnosticRelatedUriMatches(uri, diagnostics.uri)) {
            return diagnostics;
        }
    }

    throw new Error(message);
}

async function waitForDiagnosticsUriVersion(client, uri, version, message) {
    for (let attempt = 0; attempt < 16; attempt += 1) {
        const diagnostics = await client.waitForNotification('textDocument/publishDiagnostics');
        if (diagnosticRelatedUriMatches(uri, diagnostics.uri) &&
            diagnostics.version === version) {
            return diagnostics;
        }
    }

    throw new Error(message);
}

async function awaitLspRequestOutcome(promise) {
    try {
        return { result: await promise, error: null };
    } catch (error) {
        return { result: null, error: JSON.parse(error.message) };
    }
}

function workspaceDiagnosticsHasUriVersion(result, uri, version) {
    return result && Array.isArray(result.items) && result.items.some((report) =>
        report && diagnosticRelatedUriMatches(uri, report.uri) && report.version === version);
}

function copyPathSync(sourcePath, targetPath) {
    const stats = fs.statSync(sourcePath);

    if (stats.isDirectory()) {
        fs.mkdirSync(targetPath, { recursive: true });
        fs.readdirSync(sourcePath).forEach((entry) => {
            copyPathSync(path.join(sourcePath, entry), path.join(targetPath, entry));
        });
        return;
    }

    fs.copyFileSync(sourcePath, targetPath);
}

function removePathSync(targetPath, options = {}) {
    if (typeof fs.rmSync === 'function') {
        fs.rmSync(targetPath, options);
        return;
    }

    if (!fs.existsSync(targetPath)) {
        return;
    }

    const stats = fs.statSync(targetPath);
    if (stats.isDirectory()) {
        fs.readdirSync(targetPath).forEach((entry) => {
            removePathSync(path.join(targetPath, entry), options);
        });
        fs.rmdirSync(targetPath);
        return;
    }

    fs.unlinkSync(targetPath);
}

function findPosition(text, substring, occurrence = 0, offset = 0) {
    let fromIndex = 0;
    let index = -1;

    for (let current = 0; current <= occurrence; current += 1) {
        index = text.indexOf(substring, fromIndex);
        if (index < 0) {
            throw new Error(`Unable to find substring "${substring}"`);
        }
        fromIndex = index + substring.length;
    }

    const target = index + offset;
    const lines = text.slice(0, target).split('\n');
    return {
        line: lines.length - 1,
        character: lines[lines.length - 1].length,
    };
}

function decodeSemanticTokens(data) {
    let line = 0;
    let character = 0;
    const tokens = [];

    for (let index = 0; index < data.length; index += 5) {
        const lineDelta = data[index];
        const characterDelta = data[index + 1];
        line += lineDelta;
        character = lineDelta === 0 ? character + characterDelta : characterDelta;
        tokens.push({
            line,
            character,
            length: data[index + 2],
            type: data[index + 3],
            modifiers: data[index + 4],
        });
    }

    return tokens;
}

function hasSemanticToken(tokens, position, length, type, modifiers) {
    return tokens.some((token) => token.line === position.line &&
        token.character === position.character &&
        token.length === length &&
        token.type === type &&
        token.modifiers === modifiers);
}

function assertSemanticTokensDoNotOverlap(tokens, reason) {
    let previous;

    for (const token of tokens) {
        if (previous && previous.line === token.line &&
            previous.character + previous.length > token.character) {
            throw new Error(`${reason}: overlapping semantic tokens at line ${token.line}, ` +
                `characters ${previous.character}-${previous.character + previous.length} and ` +
                `${token.character}-${token.character + token.length}`);
        }
        previous = token;
    }
}

function workspaceEditTextEdits(workspaceEdit, uri) {
    const documentChange = workspaceEdit && Array.isArray(workspaceEdit.documentChanges)
        ? workspaceEdit.documentChanges.find((change) => change &&
            change.textDocument && change.textDocument.uri === uri &&
            Array.isArray(change.edits))
        : undefined;

    if (documentChange) {
        return documentChange.edits;
    }
    return workspaceEdit && workspaceEdit.changes && Array.isArray(workspaceEdit.changes[uri])
        ? workspaceEdit.changes[uri]
        : [];
}

function workspaceEditContainsTextEdit(workspaceEdit, uri, start, end, newText) {
    return workspaceEditTextEdits(workspaceEdit, uri).some((edit) => edit && edit.range &&
            edit.range.start.line === start.line &&
            edit.range.start.character === start.character &&
            edit.range.end.line === end.line &&
            edit.range.end.character === end.character &&
            edit.newText === newText);
}

function workspaceEditDocumentVersion(workspaceEdit, uri) {
    const documentChange = workspaceEdit &&
        Array.isArray(workspaceEdit.documentChanges)
        ? workspaceEdit.documentChanges.find((change) => change &&
            change.textDocument && change.textDocument.uri === uri)
        : undefined;
    return documentChange ? documentChange.textDocument.version : undefined;
}

function createWatchedProjectFixture() {
    const rootPath = fs.mkdtempSync(path.join(os.tmpdir(), 'zr-stdio-watch-'));
    const sourcePath = path.join(rootPath, 'src');
    const projectPath = path.join(rootPath, 'watched_refresh.zrp');
    const mainPath = path.join(sourcePath, 'main.zr');

    fs.mkdirSync(sourcePath, { recursive: true });
    fs.writeFileSync(projectPath, JSON.stringify({
        name: 'watched_refresh',
        source: 'src',
        binary: 'bin',
        entry: 'main',
    }, null, 2));
    fs.writeFileSync(mainPath, [
        'module main;',
        '',
        'pub fn watched_before_refresh(): int {',
        '    return 1;',
        '}',
        '',
    ].join('\n'));

    return {
        rootPath,
        projectPath,
        mainPath,
        projectUri: pathToFileURL(projectPath).toString(),
        mainUri: pathToFileURL(mainPath).toString(),
    };
}

function createWorkspaceLatencyFixture() {
    const rootPath = fs.mkdtempSync(path.join(os.tmpdir(), 'zr-stdio-latency-'));
    const sourcePath = path.join(rootPath, 'src');
    const projectPath = path.join(rootPath, 'workspace_latency.zrp');
    const targetPath = path.join(sourcePath, 'target.zr');
    const imports = [];

    for (let index = 0; index < 99; index += 1) {
        const name = 'helper_' + String(index).padStart(2, '0');

        imports.push('var ' + name + ' = import("' + name + '");');
    }
    const targetText = [
        'module target;',
        '',
        ...imports,
        '',
        'pub fn workspace_latency_target(): int {',
        '    return 1;',
        '}',
        '',
    ].join('\n');

    fs.mkdirSync(sourcePath, { recursive: true });
    fs.writeFileSync(projectPath, JSON.stringify({
        name: 'workspace_latency',
        source: 'src',
        binary: 'bin',
        entry: 'target',
    }, null, 2));
    fs.writeFileSync(targetPath, targetText);
    for (let index = 0; index < 99; index += 1) {
        const name = 'helper_' + String(index).padStart(2, '0');

        fs.writeFileSync(path.join(sourcePath, name + '.zr'), [
            'module ' + name + ';',
            'pub fn ' + name + '(): int { return ' + index + '; }',
            '',
        ].join('\n'));
    }

    return {
        rootPath,
        projectUri: pathToFileURL(projectPath).toString(),
        targetUri: pathToFileURL(targetPath).toString(),
        targetText,
    };
}

function createModuleIdentityRenameFixture() {
    const rootPath = fs.mkdtempSync(path.join(os.tmpdir(), 'zr-stdio-module-rename-'));
    const sourcePath = path.join(rootPath, 'src');
    const projectPath = path.join(rootPath, 'module_identity_rename.zrp');
    const oldUserPath = path.join(sourcePath, 'old_user.zr');
    const newUserPath = path.join(sourcePath, 'new_user.zr');
    const oldProviderPath = path.join(sourcePath, 'legacy.zr');
    const newProviderPath = path.join(sourcePath, 'modern.zr');
    const oldUserContent = [
        'var legacy = import("legacy");',
        'var cached = legacy.value();',
        'return cached;',
        '',
    ].join('\n');
    const newUserContent = [
        'var legacy = import("legacy");',
        'var modern = import("modern");',
        'var prior = legacy.value();',
        'var cached = modern.value();',
        'return cached;',
        '',
    ].join('\n');
    const initialProviderContent = [
        'module legacy;',
        'pub fn value(): int {',
        '    return 1;',
        '}',
        '',
    ].join('\n');
    const renamedProviderContent = [
        'module modern;',
        'pub fn value(): float {',
        '    return 1.5;',
        '}',
        '',
    ].join('\n');

    fs.mkdirSync(sourcePath, { recursive: true });
    fs.writeFileSync(projectPath, JSON.stringify({
        name: 'module_identity_rename',
        source: 'src',
        binary: 'bin',
        entry: 'old_user',
    }, null, 2));
    fs.writeFileSync(oldUserPath, oldUserContent);
    fs.writeFileSync(newUserPath, newUserContent);
    fs.writeFileSync(oldProviderPath, initialProviderContent);

    return {
        rootPath,
        oldUserContent,
        newUserContent,
        initialProviderContent,
        renamedProviderContent,
        oldProviderPath,
        newProviderPath,
        oldUserUri: pathToFileURL(oldUserPath).toString(),
        newUserUri: pathToFileURL(newUserPath).toString(),
        oldProviderUri: pathToFileURL(oldProviderPath).toString(),
        newProviderUri: pathToFileURL(newProviderPath).toString(),
    };
}

function regenerateWatchedBinaryMetadataFixture(serverPath, rootPath, cliPathOptional) {
    const cliPath =
        typeof cliPathOptional === 'string' && cliPathOptional.length > 0
            ? cliPathOptional
            : path.join(path.dirname(serverPath), `zr_vm_cli${path.extname(serverPath)}`);
    const projectPath = path.join(rootPath, 'binary_module_graph_pipeline.zrp');
    const tempBinarySourcePath = path.join(rootPath, 'src', 'graph_binary_stage.zr');
    const compileResult = spawnSync(cliPath, [
        '--compile',
        projectPath,
        '--intermediate',
    ], {
        cwd: process.cwd(),
        encoding: 'utf8',
        windowsHide: true,
    });

    removePathSync(tempBinarySourcePath, { force: true });

    if (compileResult.error) {
        throw compileResult.error;
    }

    if (compileResult.status !== 0) {
        const stderr = compileResult.stderr ? compileResult.stderr.trim() : '';
        const stdout = compileResult.stdout ? compileResult.stdout.trim() : '';
        throw new Error([
            `Failed to regenerate watched binary metadata fixture with ${cliPath}`,
            `status=${compileResult.status}`,
            stdout ? `stdout=${stdout}` : '',
            stderr ? `stderr=${stderr}` : '',
        ].filter(Boolean).join('\n'));
    }
}

function createWatchedBinaryMetadataFixture(serverPath, cliPathOptional) {
    const sourceFixtureRoot = path.join(__dirname,
        '..',
        'fixtures',
        'projects',
        'binary_module_graph_pipeline');
    const binarySourceFixturePath = path.join(sourceFixtureRoot,
        'fixtures',
        'graph_binary_stage_source.zr');
    const rootPath = fs.mkdtempSync(path.join(os.tmpdir(), 'zr-stdio-binary-watch-'));
    const projectPath = path.join(rootPath, 'binary_module_graph_pipeline.zrp');
    const mainPath = path.join(rootPath, 'src', 'main.zr');
    const binaryPath = path.join(rootPath, 'bin', 'graph_binary_stage.zro');
    const binaryIntermediatePath = path.join(rootPath, 'bin', 'graph_binary_stage.zri');
    const tempBinarySourcePath = path.join(rootPath, 'src', 'graph_binary_stage.zr');

    copyPathSync(sourceFixtureRoot, rootPath);
    fs.copyFileSync(binarySourceFixturePath, tempBinarySourcePath);
    removePathSync(binaryPath, { force: true });
    removePathSync(binaryIntermediatePath, { force: true });
    regenerateWatchedBinaryMetadataFixture(serverPath, rootPath, cliPathOptional);

    return {
        rootPath,
        projectPath,
        mainPath,
        binaryPath,
        projectUri: pathToFileURL(projectPath).toString(),
        mainUri: pathToFileURL(mainPath).toString(),
        binaryUri: pathToFileURL(binaryPath).toString(),
    };
}

function createImportDiagnosticsFixture() {
    const rootPath = fs.mkdtempSync(path.join(os.tmpdir(), 'zr-stdio-import-diag-'));
    const sourcePath = path.join(rootPath, 'src');
    const projectPath = path.join(rootPath, 'import_diagnostics.zrp');
    const mainPath = path.join(sourcePath, 'main.zr');
    const modulePath = path.join(sourcePath, 'greet.zr');

    fs.mkdirSync(sourcePath, { recursive: true });
    fs.writeFileSync(projectPath, JSON.stringify({
        name: 'import_diagnostics',
        source: 'src',
        binary: 'bin',
        entry: 'main',
    }, null, 2));
    fs.writeFileSync(mainPath, [
        'var greet = import("greet");',
        'var answer = greet.missing;',
        '',
    ].join('\n'));
    fs.writeFileSync(modulePath, [
        'pub var present = 1;',
        '',
    ].join('\n'));

    return {
        rootPath,
        projectPath,
        mainPath,
        modulePath,
        projectUri: pathToFileURL(projectPath).toString(),
        mainUri: pathToFileURL(mainPath).toString(),
        moduleUri: pathToFileURL(modulePath).toString(),
    };
}

function createDescriptorPluginGenericCallableFixture(serverPath) {
    const buildRoot = path.dirname(path.dirname(serverPath));
    const fixtureCandidates = [
        path.join(path.dirname(serverPath), 'zr_vm_descriptor_plugin_fixture_int.dll'),
        path.join(buildRoot, 'lib', 'libzr_vm_descriptor_plugin_fixture_int.so'),
        path.join(buildRoot, 'lib', 'libzr_vm_descriptor_plugin_fixture_int.dylib'),
    ];
    const fixtureSourcePath = fixtureCandidates.find((candidate) => fs.existsSync(candidate));
    assert(fixtureSourcePath,
        `Unable to locate descriptor plugin fixture beside ${serverPath}`);

    const rootPath = fs.mkdtempSync(path.join(os.tmpdir(), 'zr-stdio-generic-plugin-'));
    const sourcePath = path.join(rootPath, 'src');
    const nativePath = path.join(rootPath, 'native');
    const projectPath = path.join(rootPath, 'generic_plugin.zrp');
    const mainPath = path.join(sourcePath, 'main.zr');
    const pluginPath = path.join(
        nativePath,
        `zrvm_native_zr_pluginprobe${path.extname(fixtureSourcePath)}`);
    const content = [
        'var plugin = import("zr.pluginprobe");',
        'var point = plugin.makePoint();',
        'var echoed = point.echo(1);',
        'return echoed;',
        '',
    ].join('\n');

    fs.mkdirSync(sourcePath, { recursive: true });
    fs.mkdirSync(nativePath, { recursive: true });
    fs.writeFileSync(projectPath, JSON.stringify({
        name: 'generic_plugin',
        source: 'src',
        binary: 'bin',
        entry: 'main',
    }, null, 2));
    fs.writeFileSync(mainPath, content);
    fs.copyFileSync(fixtureSourcePath, pluginPath);

    return {
        rootPath,
        content,
        mainUri: pathToFileURL(mainPath).toString(),
    };
}

function sleepSync(milliseconds) {
    const waitArray = new Int32Array(new SharedArrayBuffer(4));
    Atomics.wait(waitArray, 0, 0, milliseconds);
}

function cleanupPath(targetPath) {
    let lastError = null;

    if (!targetPath) {
        return;
    }

    for (let attempt = 0; attempt < 8; attempt += 1) {
        try {
            removePathSync(targetPath, { recursive: true, force: true });
            return;
        } catch (error) {
            if (!error || (error.code !== 'EBUSY' && error.code !== 'EPERM')) {
                throw error;
            }

            lastError = error;
            sleepSync(25 * (attempt + 1));
        }
    }

    if (lastError) {
        console.warn(`Skipping cleanup for locked path ${targetPath}: ${lastError.code}`);
    }
}

let watchedFixtureRootToCleanup = null;
let watchedBinaryFixtureRootToCleanup = null;
let importDiagnosticsFixtureRootToCleanup = null;
let fileOperationsFixtureRootToCleanup = null;
let moduleIdentityRenameFixtureRootToCleanup = null;
let descriptorPluginGenericFixtureRootToCleanup = null;
let workspaceLatencyFixtureRootToCleanup = null;

class LspClient extends StdioProtocolClient {
    constructor(serverPath) {
        super(serverPath);
    }

    requestWithId(method, params, timeoutMs = 10000) {
        if (this.closed) {
            return {
                id: null,
                promise: Promise.reject(new Error('Server already exited')),
            };
        }

        const id = this.nextId++;
        const promise = super.request(method, params, id, timeoutMs).then((response) => {
            if (response.error) {
                throw new Error(JSON.stringify(response.error));
            }
            return response.result;
        });
        return { id, promise };
    }

    request(method, params, timeoutMs = 10000) {
        return this.requestWithId(method, params, timeoutMs).promise;
    }

    notify(method, params) {
        super.notify(method, params);
    }

    waitForNotification(method, timeoutMs = 10000) {
        return super.waitForNotification(method, timeoutMs);
    }

    waitForExit(timeoutMs = 10000) {
        return super.waitForExit(timeoutMs);
    }
}

async function main() {
    const serverPath = process.argv[2];
    const cliPathOptional = process.argv[3];
    assert(serverPath, 'Expected server executable path as argv[2]');
    const watchedFixture = createWatchedProjectFixture();
    const watchedBinaryFixture = createWatchedBinaryMetadataFixture(serverPath, cliPathOptional);
    const importDiagnosticsFixture = createImportDiagnosticsFixture();
    const fileOperationsFixture = createWatchedProjectFixture();
    const moduleIdentityRenameFixture = createModuleIdentityRenameFixture();
    const descriptorPluginGenericFixture =
        createDescriptorPluginGenericCallableFixture(serverPath);
    const workspaceLatencyFixture = createWorkspaceLatencyFixture();
    watchedFixtureRootToCleanup = watchedFixture.rootPath;
    watchedBinaryFixtureRootToCleanup = watchedBinaryFixture.rootPath;
    importDiagnosticsFixtureRootToCleanup = importDiagnosticsFixture.rootPath;
    fileOperationsFixtureRootToCleanup = fileOperationsFixture.rootPath;
    moduleIdentityRenameFixtureRootToCleanup = moduleIdentityRenameFixture.rootPath;
    descriptorPluginGenericFixtureRootToCleanup =
        descriptorPluginGenericFixture.rootPath;
    workspaceLatencyFixtureRootToCleanup = workspaceLatencyFixture.rootPath;

    const client = new LspClient(serverPath);
    const documentUri = 'file:///c%3A/Users/test/workspace/%2Bzr_vm%2B/stdio-smoke.zr';
    const docsUri = 'file:///c%3A/Users/test/workspace/%2Bzr_vm%2B/stdio-docs.zr';
    const testCodeLensUri =
        'file:///c%3A/Users/test/workspace/%2Bzr_vm%2B/stdio-test-code-lens.zr';
    const propertyContractUri =
        'file:///c%3A/Users/test/workspace/%2Bzr_vm%2B/stdio-property-contract.zr';
    const genericUri = 'file:///c%3A/Users/test/workspace/%2Bzr_vm%2B/stdio-generic.zr';
    const canonicalDisplayUri =
        'file:///c%3A/Users/test/workspace/%2Bzr_vm%2B/stdio-canonical-display.zr';
    const nativeCallableUri =
        'file:///c%3A/Users/test/workspace/%2Bzr_vm%2B/stdio-native-callable.zr';
    const parserDiagnosticUri =
        'file:///c%3A/Users/test/workspace/%2Bzr_vm%2B/stdio-parser-diagnostic.zr';
    const missingConditionUri =
        'file:///c%3A/Users/test/workspace/%2Bzr_vm%2B/stdio-missing-condition.zr';
    const diagnosticsLatencyUri =
        'file:///c%3A/Users/test/workspace/%2Bzr_vm%2B/stdio-diagnostics-latency.zr';
    const formatEditUri = 'file:///c%3A/Users/test/workspace/%2Bzr_vm%2B/stdio-format-edit.zr';
    const noopFormatUri = 'file:///c%3A/Users/test/workspace/%2Bzr_vm%2B/stdio-format-noop.zr';
    const importFoldingUri = 'file:///c%3A/Users/test/workspace/%2Bzr_vm%2B/stdio-import-folding.zr';
    const moduleImportsUri = 'file:///c%3A/Users/test/workspace/%2Bzr_vm%2B/stdio-module-imports.zr';
    const legacySemanticUri =
        'file:///c%3A/Users/test/workspace/%2Bzr_vm%2B/stdio-legacy-semantic.zr';
    const unresolvedSemanticTokenUri =
        'file:///c%3A/Users/test/workspace/%2Bzr_vm%2B/stdio-unresolved-semantic-token.zr';
    const semanticDeltaUri = 'file:///c%3A/Users/test/workspace/%2Bzr_vm%2B/stdio-semantic-delta.zr';
    const colorUri = 'file:///c%3A/Users/test/workspace/%2Bzr_vm%2B/stdio-colors.zr';
    const inlineCompletionUri =
        'file:///c%3A/Users/test/workspace/%2Bzr_vm%2B/stdio-inline-completion.zr';
    const inlineReturnUri =
        'file:///c%3A/Users/test/workspace/%2Bzr_vm%2B/stdio-inline-return.zr';
    const inlineExpressionUri =
        'file:///c%3A/Users/test/workspace/%2Bzr_vm%2B/stdio-inline-expression.zr';
    const documentHighlightFilterUri =
        'file:///c%3A/Users/test/workspace/%2Bzr_vm%2B/stdio-document-highlight-filter.zr';
    const linkedEditingFilterUri =
        'file:///c%3A/Users/test/workspace/%2Bzr_vm%2B/stdio-linked-editing-filter.zr';
    const monikerFilterUri =
        'file:///c%3A/Users/test/workspace/%2Bzr_vm%2B/stdio-moniker-filter.zr';
    const initialText = 'var x = 10; var y = x; var flag = true || false;';
    const parserDiagnosticText = 'var x = ;\n';
    const missingConditionText = 'if () { return 1; }\n';
    const colorText = [
        'var accent = "#336699";',
        '// "#112233" is only a comment color',
        '/* "#445566" is only a block comment color */',
        '',
    ].join('\n');
    const inlineCompletionText = [
        'fn main(): int {',
        '    ret',
        '    // ret',
        '    var label = "ret";',
        '    /* ret */',
        '}',
        '',
    ].join('\n');
    const inlineReturnText = [
        'fn main(): int {',
        '    return 1 + 2;',
        '}',
        '',
    ].join('\n');
    const inlineExpressionText = [
        'fn main(): void {',
        '    1 + 2;',
        '    true || false;',
        '}',
        '',
    ].join('\n');
    const documentHighlightFilterText = [
        'var highlightOnly = 1;',
        '// highlightOnly appears in a comment',
        'var label = "highlightOnly";',
        '/* highlightOnly appears in a block comment */',
        '',
    ].join('\n');
    const linkedEditingFilterText = [
        'var linkedOnly = 1;',
        '// linkedOnly appears in a comment',
        'var label = "linkedOnly";',
        '/* linkedOnly appears in a block comment */',
        '',
    ].join('\n');
    const monikerFilterText = [
        'var real = 1;',
        '// commentOnly symbol',
        'var label = "stringOnly";',
        '/* blockOnly symbol */',
        '',
    ].join('\n');
    const documentationText = [
        'module documentation;',
        '',
        'class ScoreBoard {',
        '    pri static var _bonus: int = 5;',
        '',
        '    // Shared bonus exposed through get/set.',
        '    pub static property bonus: int {',
        '        get { return ScoreBoard._bonus; }',
        '        set { ScoreBoard._bonus = value; }',
        '    }',
        '}',
        '',
        'fn documentationValue(): int {',
        '    return ScoreBoard.bonus;',
        '}',
        '',
        'let currentModule = import("zr.math");',
        'let scoreBoard = new ScoreBoard();',
        '',
    ].join('\n');
    const testCodeLensText = [
        '#zr.testing.test#',
        'fn codeLensTest(): void {',
        '}',
        '',
    ].join('\n');
    const legacySemanticText = [
        '%import("zr.system");',
        'let remainder = rate % divisor;',
        'using (resource) { }',
        '',
    ].join('\n');
    const unresolvedSemanticTokenText = [
        'class Device {',
        '    fn resolved(): int { return 1; }',
        '}',
        'fn run() {',
        '    var target = new Device();',
        '    target.resolved();',
        '    target.unresolved();',
        '}',
        '',
    ].join('\n');
    const propertyContractText = [
        'class Meter {',
        '    pri var stored: int = 7;',
        '    pub property value: int {',
        '        get { return this.stored; }',
        '        set { this.stored = value; }',
        '    }',
        '}',
        'fn read(meter: Meter): int { return meter.value; }',
        '',
    ].join('\n');
    const genericText = [
        'class Item {',
        '    pub @constructor() { }',
        '}',
        'class Derived<T, const N: int> {',
        '}',
        'class Matrix<T, const N: int> { }',
        'class Box<T> {',
        '    fn shape<const N: int>(value: Matrix<T, N>): Matrix<T, N> { return value; }',
        '}',
        'fn pick(value: int, flag: bool): int {',
        '    return value;',
        '}',
        'fn inferNumber() {',
        '    return 42;',
        '}',
        'fn use(): void {',
        '    var value: Derived<Item, 2 + 2> = null;',
        '    var box = new Box<int>();',
        '    var m = new Matrix<int, 2 + 2>();',
        '    value;',
        '    box.shape(m);',
        '    pick(1 + 2, true || false);',
        '}',
        '',
    ].join('\n');
    const canonicalDisplayText = [
        'fn redact(value: MissingType): MissingType {',
        '    return value;',
        '}',
        'fn use(): void {',
        '    redact(null);',
        '}',
        '',
    ].join('\n');
    const diagnosticsLatencyBaseText = [
        'fn diagnostics_latency(): int {',
        '    return 1;',
        '}',
        '',
    ].join('\n');
    const nativeCallableText = [
        'var gc = import("zr.system.gc");',
        'gc.set_budget(2000);',
        'var {LinkedList} = import("zr.container");',
        'var list: LinkedList<int> = null;',
        'list.addLast(1);',
        '',
    ].join('\n');
    const noopFormatText = [
        'class Sample {',
        '    pub fn run(value: int): int {',
        '        return value;',
        '    }',
        '}',
        '',
    ].join('\n');
    const formatEditText = [
        'class Sample {',
        'pub fn run(value: int): int {',
        'return value;',
        '}',
        '}',
        '',
    ].join('\n');
    const importFoldingText = [
        'let system = import("zr.system");',
        'let math = import("zr.math");',
        'let container = import("zr.container");',
        '',
        '// first note',
        '// second note',
        '',
        '//#region setup',
        'fn main(): int {',
        '    return 0;',
        '}',
        '//#endregion',
        '',
    ].join('\n');
    const moduleImportsText = [
        'module stdio;',
        '',
        'let system = import("zr.system");',
        'let math = import("zr.math");',
        '',
        'fn main(): int { return 0; }',
        '',
    ].join('\n');
    const semanticDeltaText = [
        'fn alpha(): int {',
        '    var value = 1;',
        '    return value;',
        '}',
        '',
        'fn omega(): int {',
        '    return alpha();',
        '}',
        '',
    ].join('\n');
    const semanticDeltaUpdatedText = semanticDeltaText
        .replace('value = 1', 'valueName = 1')
        .replace('return value;', 'return valueName;');

    const initializeResult = await client.request('initialize', {
        processId: null,
        rootUri: null,
        capabilities: {},
        clientInfo: {
            name: 'stdio-smoke',
            version: '0.0.1',
        },
    });

    assert(initializeResult, 'initialize returned null');
    const peakMemory = new LspProcessPeakMemory(
        client.child.pid,
        peakMemoryLimitBytes());
    peakMemory.observe('initialized');
    assert(initializeResult.capabilities, 'initialize missing capabilities');
    assert(initializeResult.capabilities.textDocumentSync.change === 2,
        'server must advertise incremental sync');
    assert(initializeResult.capabilities.signatureHelpProvider &&
        Array.isArray(initializeResult.capabilities.signatureHelpProvider.triggerCharacters) &&
        initializeResult.capabilities.signatureHelpProvider.triggerCharacters.includes('('),
        'signatureHelpProvider must advertise trigger characters');
    assert(initializeResult.capabilities.definitionProvider === true,
        'definitionProvider must be enabled');
    assert(initializeResult.capabilities.renameProvider.prepareProvider === true,
        'renameProvider.prepareProvider must be enabled');
    assert(initializeResult.capabilities.documentSymbolProvider === true,
        'documentSymbolProvider must be enabled');
    assert(initializeResult.capabilities.workspaceSymbolProvider &&
        initializeResult.capabilities.workspaceSymbolProvider.resolveProvider === true,
        'workspaceSymbolProvider resolveProvider must be enabled');
    assert(initializeResult.capabilities.semanticTokensProvider &&
        initializeResult.capabilities.semanticTokensProvider.full &&
        initializeResult.capabilities.semanticTokensProvider.full.delta === true,
        'semanticTokensProvider.full.delta must be enabled');
    assert(initializeResult.capabilities.semanticTokensProvider.range === true,
        'semanticTokensProvider.range must be enabled');
    const semanticTokensProvider = initializeResult.capabilities.semanticTokensProvider;
    const semanticTokenTypes = semanticTokensProvider &&
        semanticTokensProvider.legend &&
        semanticTokensProvider.legend.tokenTypes;
    const semanticTokenModifiers = semanticTokensProvider &&
        semanticTokensProvider.legend &&
        semanticTokensProvider.legend.tokenModifiers;
    assert(Array.isArray(semanticTokenTypes) &&
        semanticTokenTypes.includes('keyword'),
        'semantic token legend must include keyword');
    assert(Array.isArray(semanticTokenModifiers) &&
        !semanticTokenModifiers.includes('deprecated'),
        'semantic token legend must not retain removed syntax modifiers');
    assert(initializeResult.capabilities.inlayHintProvider &&
        initializeResult.capabilities.inlayHintProvider.resolveProvider === true,
        'inlayHintProvider resolveProvider must be enabled');
    assert(initializeResult.capabilities.codeActionProvider &&
        Array.isArray(initializeResult.capabilities.codeActionProvider.codeActionKinds) &&
        initializeResult.capabilities.codeActionProvider.codeActionKinds.includes('source.organizeImports'),
        'codeActionProvider must advertise organize imports');
    assert(initializeResult.capabilities.codeActionProvider.resolveProvider === true,
        'codeActionProvider resolveProvider must be enabled');
    assert(initializeResult.capabilities.codeActionProvider.codeActionKinds.includes('quickfix'),
        'codeActionProvider must advertise quick fixes');
    assert(initializeResult.capabilities.codeActionProvider.codeActionKinds.includes('source.removeUnused'),
        'codeActionProvider must advertise remove-unused source actions');
    assert(initializeResult.capabilities.documentFormattingProvider === true,
        'documentFormattingProvider must be enabled');
    assert(initializeResult.capabilities.documentRangeFormattingProvider === true,
        'documentRangeFormattingProvider must be enabled');
    assert(initializeResult.capabilities.textDocumentSync &&
        initializeResult.capabilities.textDocumentSync.willSaveWaitUntil === true,
        'textDocumentSync.willSaveWaitUntil must be enabled');
    assert(initializeResult.capabilities.documentOnTypeFormattingProvider &&
        initializeResult.capabilities.documentOnTypeFormattingProvider.firstTriggerCharacter === '}',
        'documentOnTypeFormattingProvider must be enabled');
    assert(initializeResult.capabilities.foldingRangeProvider === true,
        'foldingRangeProvider must be enabled');
    assert(initializeResult.capabilities.selectionRangeProvider === true,
        'selectionRangeProvider must be enabled');
    assert(initializeResult.capabilities.linkedEditingRangeProvider === true,
        'linkedEditingRangeProvider must be enabled');
    assert(initializeResult.capabilities.monikerProvider === true,
        'monikerProvider must be enabled');
    assert(initializeResult.capabilities.inlineValueProvider === true,
        'inlineValueProvider must be enabled');
    assert(initializeResult.capabilities.colorProvider === true,
        'colorProvider must be enabled');
    assert(initializeResult.capabilities.inlineCompletionProvider === true,
        'inlineCompletionProvider must be enabled');
    assert(initializeResult.capabilities.documentLinkProvider &&
        initializeResult.capabilities.documentLinkProvider.resolveProvider === true,
        'documentLinkProvider resolveProvider must be enabled');
    assert(initializeResult.capabilities.declarationProvider === true,
        'declarationProvider must be enabled');
    assert(initializeResult.capabilities.typeDefinitionProvider === true,
        'typeDefinitionProvider must be enabled');
    assert(initializeResult.capabilities.implementationProvider === true,
        'implementationProvider must be enabled');
    assert(initializeResult.capabilities.codeLensProvider &&
        initializeResult.capabilities.codeLensProvider.resolveProvider === true,
        'codeLensProvider resolveProvider must be enabled');
    assert(!initializeResult.capabilities.executeCommandProvider ||
        !Array.isArray(initializeResult.capabilities.executeCommandProvider.commands) ||
        (!initializeResult.capabilities.executeCommandProvider.commands.includes('zr.runCurrentProject') &&
            !initializeResult.capabilities.executeCommandProvider.commands.includes('zr.showReferences')),
    'executeCommandProvider must not advertise client-owned ZR commands');
    assert(initializeResult.capabilities.callHierarchyProvider === true,
        'callHierarchyProvider must be enabled');
    assert(initializeResult.capabilities.typeHierarchyProvider === true,
        'typeHierarchyProvider must be enabled');
    assert(initializeResult.capabilities.diagnosticProvider &&
        initializeResult.capabilities.diagnosticProvider.workspaceDiagnostics === true,
        'diagnosticProvider must support workspace diagnostics');
    assert(initializeResult.capabilities.completionProvider.resolveProvider === true,
        'completionProvider.resolveProvider must be enabled');
    assert(Array.isArray(initializeResult.capabilities.completionProvider.allCommitCharacters) &&
        initializeResult.capabilities.completionProvider.allCommitCharacters.includes(';') &&
        initializeResult.capabilities.completionProvider.allCommitCharacters.includes(',') &&
        initializeResult.capabilities.completionProvider.allCommitCharacters.includes('.') &&
        initializeResult.capabilities.completionProvider.allCommitCharacters.includes('('),
    'completionProvider must advertise all commit characters');
    assert(initializeResult.capabilities.workspace &&
        initializeResult.capabilities.workspace.fileOperations &&
        initializeResult.capabilities.workspace.fileOperations.willCreate &&
        initializeResult.capabilities.workspace.fileOperations.didCreate &&
        initializeResult.capabilities.workspace.fileOperations.willRename &&
        initializeResult.capabilities.workspace.fileOperations.didRename &&
        initializeResult.capabilities.workspace.fileOperations.willDelete &&
        initializeResult.capabilities.workspace.fileOperations.didDelete &&
        initializeResult.capabilities.workspace.fileOperations.didRename,
    'workspace.fileOperations must advertise create/delete/rename requests and notifications');

    client.notify('initialized', {});
    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: documentUri,
            languageId: 'zr',
            version: 0,
            text: initialText,
        },
    });

    const openDiagnostics = await client.waitForNotification('textDocument/publishDiagnostics');
    assert(openDiagnostics.uri === documentUri, 'didOpen diagnostics uri mismatch');
    assert(Array.isArray(openDiagnostics.diagnostics), 'didOpen diagnostics must be an array');

    const versionZeroRename = await client.request('textDocument/rename', {
        textDocument: { uri: documentUri },
        position: { line: 0, character: 4 },
        newName: 'versionZeroX',
    });
    assert(workspaceEditDocumentVersion(versionZeroRename, documentUri) === 0,
        'rename must serialize the captured version-zero opened document provenance');

    client.notify('textDocument/didChange', {
        textDocument: {
            uri: documentUri,
            version: 2,
        },
        contentChanges: [
            {
                range: {
                    start: { line: 0, character: 8 },
                    end: { line: 0, character: 10 },
                },
                text: '20',
            },
        ],
    });

    const changeDiagnostics = await client.waitForNotification('textDocument/publishDiagnostics');
    assert(changeDiagnostics.uri === documentUri, 'didChange diagnostics uri mismatch');
    assert(changeDiagnostics.version === 2, 'didChange diagnostics version mismatch');

    const definition = await client.request('textDocument/definition', {
        textDocument: { uri: documentUri },
        position: { line: 0, character: 4 },
    });
    assert(Array.isArray(definition) && definition.length > 0, 'definition must return at least one location');
    const usageDefinition = await client.request('textDocument/definition', {
        textDocument: { uri: documentUri },
        position: { line: 0, character: 20 },
    });
    assert(Array.isArray(usageDefinition) && usageDefinition.length > 0,
        'definition must resolve local identifier usages');

    const references = await client.request('textDocument/references', {
        textDocument: { uri: documentUri },
        position: { line: 0, character: 4 },
        context: { includeDeclaration: true },
    });
    assert(Array.isArray(references) && references.length > 0, 'references must return at least one location');
    const linkedEditing = await client.request('textDocument/linkedEditingRange', {
        textDocument: { uri: documentUri },
        position: { line: 0, character: 4 },
    });
    assert(linkedEditing &&
        Array.isArray(linkedEditing.ranges) &&
        linkedEditing.ranges.some((range) =>
            range && range.start && range.start.line === 0 && range.start.character === 4) &&
        linkedEditing.ranges.some((range) =>
            range && range.start && range.start.line === 0 && range.start.character === 20) &&
        linkedEditing.wordPattern,
    'linkedEditingRange must return same-document ranges for the edited symbol');
    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: linkedEditingFilterUri,
            languageId: 'zr',
            version: 1,
            text: linkedEditingFilterText,
        },
    });
    const linkedEditingFilterDiagnostics = await client.waitForNotification('textDocument/publishDiagnostics');
    assert(linkedEditingFilterDiagnostics.uri === linkedEditingFilterUri,
        'linked editing filter diagnostics uri mismatch');
    const linkedEditingFiltered = await client.request('textDocument/linkedEditingRange', {
        textDocument: { uri: linkedEditingFilterUri },
        position: findPosition(linkedEditingFilterText, 'linkedOnly'),
    });
    assert(linkedEditingFiltered === null,
        'linkedEditingRange fallback must ignore identifiers inside comments and strings');
    const monikers = await client.request('textDocument/moniker', {
        textDocument: { uri: documentUri },
        position: { line: 0, character: 4 },
    });
    assert(Array.isArray(monikers) &&
        monikers.some((moniker) =>
            moniker &&
            moniker.scheme === 'zr' &&
            moniker.identifier.endsWith('#x') &&
            moniker.unique === 'document' &&
            moniker.kind === 'local'),
    'textDocument/moniker must return a document-scoped symbol identity');
    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: monikerFilterUri,
            languageId: 'zr',
            version: 1,
            text: monikerFilterText,
        },
    });
    const monikerFilterDiagnostics = await client.waitForNotification('textDocument/publishDiagnostics');
    assert(monikerFilterDiagnostics.uri === monikerFilterUri,
        'moniker filter diagnostics uri mismatch');
    const lineCommentMonikers = await client.request('textDocument/moniker', {
        textDocument: { uri: monikerFilterUri },
        position: findPosition(monikerFilterText, 'commentOnly'),
    });
    const stringMonikers = await client.request('textDocument/moniker', {
        textDocument: { uri: monikerFilterUri },
        position: findPosition(monikerFilterText, 'stringOnly'),
    });
    const blockCommentMonikers = await client.request('textDocument/moniker', {
        textDocument: { uri: monikerFilterUri },
        position: findPosition(monikerFilterText, 'blockOnly'),
    });
    assert(Array.isArray(lineCommentMonikers) && lineCommentMonikers.length === 0 &&
        Array.isArray(stringMonikers) && stringMonikers.length === 0 &&
        Array.isArray(blockCommentMonikers) && blockCommentMonikers.length === 0,
    'textDocument/moniker must ignore identifiers inside comments and strings');
    const inlineValues = await client.request('textDocument/inlineValue', {
        textDocument: { uri: documentUri },
        range: {
            start: { line: 0, character: 0 },
            end: { line: 0, character: initialText.length },
        },
        context: {
            frameId: 1,
            stoppedLocation: {
                start: { line: 0, character: 0 },
                end: { line: 0, character: initialText.length },
            },
        },
    });
    assert(Array.isArray(inlineValues) &&
        inlineValues.some((value) =>
            value &&
            value.variableName === 'x' &&
            value.caseSensitiveLookup === true &&
            value.range &&
            value.range.start.line === 0 &&
            value.range.start.character === 4) &&
        inlineValues.some((value) =>
            value &&
            value.variableName === 'y' &&
            value.caseSensitiveLookup === true &&
            value.range &&
            value.range.start.line === 0 &&
            value.range.start.character === 16),
    'textDocument/inlineValue must expose local variable lookups in the requested range');
    assert(inlineValues.some((value) =>
            value &&
            typeof value.text === 'string' &&
            value.text.includes('range 20..20') &&
            value.range &&
            value.range.start.line === 0 &&
            value.range.start.character === 4),
    'textDocument/inlineValue must expose semantic numeric facts for local initializers');
    assert(inlineValues.some((value) =>
            value &&
            value.text === 'logical true, short-circuits' &&
            value.range &&
            value.range.start.line === 0 &&
            value.range.start.character === 27),
    'textDocument/inlineValue must expose semantic logical facts for local initializers');
    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: inlineReturnUri,
            languageId: 'zr',
            version: 1,
            text: inlineReturnText,
        },
    });
    const inlineReturnDiagnostics = await client.waitForNotification('textDocument/publishDiagnostics');
    assert(inlineReturnDiagnostics.uri === inlineReturnUri, 'inline return diagnostics uri mismatch');
    const returnInlineValues = await client.request('textDocument/inlineValue', {
        textDocument: { uri: inlineReturnUri },
        range: {
            start: { line: 1, character: 0 },
            end: { line: 1, character: 17 },
        },
        context: {
            frameId: 1,
            stoppedLocation: {
                start: { line: 1, character: 0 },
                end: { line: 1, character: 17 },
            },
        },
    });
    assert(Array.isArray(returnInlineValues) &&
        returnInlineValues.some((value) =>
            value &&
            typeof value.text === 'string' &&
            value.text.includes('range 3..3') &&
            value.range &&
            value.range.start.line === 1 &&
            value.range.start.character === 11 &&
            value.range.end.line === 1 &&
            value.range.end.character === 16),
    'textDocument/inlineValue must expose semantic numeric facts for return expressions');
    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: inlineExpressionUri,
            languageId: 'zr',
            version: 1,
            text: inlineExpressionText,
        },
    });
    const inlineExpressionDiagnostics = await client.waitForNotification('textDocument/publishDiagnostics');
    assert(inlineExpressionDiagnostics.uri === inlineExpressionUri,
        'inline expression diagnostics uri mismatch');
    const expressionInlineValues = await client.request('textDocument/inlineValue', {
        textDocument: { uri: inlineExpressionUri },
        range: {
            start: { line: 1, character: 0 },
            end: { line: 2, character: 18 },
        },
        context: {
            frameId: 1,
            stoppedLocation: {
                start: { line: 1, character: 0 },
                end: { line: 2, character: 18 },
            },
        },
    });
    assert(Array.isArray(expressionInlineValues) &&
        expressionInlineValues.some((value) =>
            value &&
            typeof value.text === 'string' &&
            value.text.includes('range 3..3') &&
            value.range &&
            value.range.start.line === 1 &&
            value.range.start.character === 4 &&
            value.range.end.line === 1 &&
            value.range.end.character === 9),
    'textDocument/inlineValue must expose semantic numeric facts for expression statements');
    assert(expressionInlineValues.some((value) =>
            value &&
            value.text === 'logical true, short-circuits' &&
            value.range &&
            value.range.start.line === 2 &&
            value.range.start.character === 4 &&
            value.range.end.line === 2 &&
            value.range.end.character === 17),
    'textDocument/inlineValue must expose semantic logical facts for expression statements');
    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: colorUri,
            languageId: 'zr',
            version: 1,
            text: colorText,
        },
    });
    const colorDiagnostics = await client.waitForNotification('textDocument/publishDiagnostics');
    assert(colorDiagnostics.uri === colorUri, 'color didOpen diagnostics uri mismatch');
    const documentColors = await client.request('textDocument/documentColor', {
        textDocument: { uri: colorUri },
    });
    assert(Array.isArray(documentColors) &&
        documentColors.some((entry) =>
            entry &&
            entry.range &&
            entry.range.start.line === 0 &&
            entry.range.start.character === 14 &&
            Math.abs(entry.color.red - 0.2) < 0.001 &&
            Math.abs(entry.color.green - 0.4) < 0.001 &&
            Math.abs(entry.color.blue - 0.6) < 0.001 &&
            entry.color.alpha === 1),
    'textDocument/documentColor must expose hex color literals');
    assert(!documentColors.some((entry) =>
        entry &&
        entry.range &&
        (entry.range.start.line === 1 || entry.range.start.line === 2)),
    'textDocument/documentColor must ignore hex colors inside comments');
    const colorPresentation = await client.request('textDocument/colorPresentation', {
        textDocument: { uri: colorUri },
        color: { red: 0.2, green: 0.4, blue: 0.6, alpha: 1 },
        range: {
            start: { line: 0, character: 14 },
            end: { line: 0, character: 21 },
        },
    });
    assert(Array.isArray(colorPresentation) &&
        colorPresentation.some((presentation) =>
            presentation &&
            presentation.label === '#336699' &&
            presentation.textEdit &&
            presentation.textEdit.newText === '#336699'),
    'textDocument/colorPresentation must format the selected color as a hex edit');
    const commentColorPresentation = await client.request('textDocument/colorPresentation', {
        textDocument: { uri: colorUri },
        color: { red: 0x11 / 255, green: 0x22 / 255, blue: 0x33 / 255, alpha: 1 },
        range: {
            start: { line: 1, character: 4 },
            end: { line: 1, character: 11 },
        },
    });
    assert(Array.isArray(commentColorPresentation) && commentColorPresentation.length === 0,
        'textDocument/colorPresentation must ignore comment-only hex colors');
    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: inlineCompletionUri,
            languageId: 'zr',
            version: 1,
            text: inlineCompletionText,
        },
    });
    const inlineCompletionDiagnostics = await client.waitForNotification('textDocument/publishDiagnostics');
    assert(inlineCompletionDiagnostics.uri === inlineCompletionUri,
        'inline completion didOpen diagnostics uri mismatch');
    const functionInlineCompletions = await client.request('textDocument/inlineCompletion', {
        textDocument: { uri: inlineCompletionUri },
        position: { line: 0, character: 2 },
        context: {
            triggerKind: 1,
            selectedCompletionInfo: null,
        },
    });
    assert(Array.isArray(functionInlineCompletions) &&
        functionInlineCompletions.some((item) =>
            item &&
            item.insertText === 'fn ' &&
            item.filterText === 'fn' &&
            item.range &&
            item.range.start.line === 0 &&
            item.range.start.character === 0 &&
            item.range.end.character === 2),
    'textDocument/inlineCompletion must suggest the current fn declaration keyword');
    const inlineCompletions = await client.request('textDocument/inlineCompletion', {
        textDocument: { uri: inlineCompletionUri },
        position: { line: 1, character: 7 },
        context: {
            triggerKind: 1,
            selectedCompletionInfo: null,
        },
    });
    assert(Array.isArray(inlineCompletions) &&
        inlineCompletions.some((item) =>
            item &&
            item.insertText === 'return ' &&
            item.filterText === 'return' &&
            item.range &&
            item.range.start.line === 1 &&
            item.range.start.character === 4 &&
            item.range.end.character === 7),
    'textDocument/inlineCompletion must expand statement prefixes with filter text');
    const commentInlineCompletions = await client.request('textDocument/inlineCompletion', {
        textDocument: { uri: inlineCompletionUri },
        position: { line: 2, character: 10 },
        context: {
            triggerKind: 1,
            selectedCompletionInfo: null,
        },
    });
    assert(Array.isArray(commentInlineCompletions) && commentInlineCompletions.length === 0,
        'textDocument/inlineCompletion must ignore prefixes inside line comments');
    const stringInlineCompletions = await client.request('textDocument/inlineCompletion', {
        textDocument: { uri: inlineCompletionUri },
        position: { line: 3, character: 20 },
        context: {
            triggerKind: 1,
            selectedCompletionInfo: null,
        },
    });
    assert(Array.isArray(stringInlineCompletions) && stringInlineCompletions.length === 0,
        'textDocument/inlineCompletion must ignore prefixes inside strings');
    const blockCommentInlineCompletions = await client.request('textDocument/inlineCompletion', {
        textDocument: { uri: inlineCompletionUri },
        position: { line: 4, character: 10 },
        context: {
            triggerKind: 1,
            selectedCompletionInfo: null,
        },
    });
    assert(Array.isArray(blockCommentInlineCompletions) &&
        blockCommentInlineCompletions.length === 0,
        'textDocument/inlineCompletion must ignore prefixes inside block comments');
    const inlineCompletionUpdatedText = inlineCompletionText.replace('    ret', '    retu');
    client.notify('textDocument/didChange', {
        textDocument: {
            uri: inlineCompletionUri,
            version: 2,
        },
        contentChanges: [
            {
                text: inlineCompletionUpdatedText,
            },
        ],
    });
    const inlineCompletionChangeDiagnostics = await client.waitForNotification('textDocument/publishDiagnostics');
    assert(inlineCompletionChangeDiagnostics.uri === inlineCompletionUri,
        'inline completion didChange diagnostics uri mismatch');
    const extendedInlineCompletions = await client.request('textDocument/inlineCompletion', {
        textDocument: { uri: inlineCompletionUri },
        position: { line: 1, character: 8 },
        context: {
            triggerKind: 1,
            selectedCompletionInfo: null,
        },
    });
    assert(Array.isArray(extendedInlineCompletions) &&
        extendedInlineCompletions.some((item) =>
            item &&
            item.insertText === 'return ' &&
            item.filterText === 'return' &&
            item.range &&
            item.range.start.line === 1 &&
            item.range.start.character === 4 &&
            item.range.end.character === 8),
    'textDocument/inlineCompletion must keep keyword completions active for longer typed prefixes');

    const hover = await client.request('textDocument/hover', {
        textDocument: { uri: documentUri },
        position: { line: 0, character: 4 },
    });
    assert(hover && hover.contents, 'hover must return contents');

    const completions = await client.request('textDocument/completion', {
        textDocument: { uri: documentUri },
        position: { line: 0, character: 0 },
    });
    assert(Array.isArray(completions), 'completion must return an array');
    const localCompletionPosition = { line: 0, character: initialText.length };
    const localCompletions = await client.request('textDocument/completion', {
        textDocument: { uri: documentUri },
        position: localCompletionPosition,
    });
    assert(Array.isArray(localCompletions), 'local completion must return an array');
    const xCompletion = localCompletions.find((item) => item && item.label === 'x');
    assert(xCompletion &&
        typeof xCompletion.detail === 'string' &&
        xCompletion.detail.includes('range 20..20') &&
        xCompletion.data && xCompletion.data.uri === documentUri,
    'local completion detail should include numeric initializer semantic facts and resolve data');
    const resolvedXCompletion = await client.request('completionItem/resolve', {
        label: xCompletion.label,
        kind: xCompletion.kind,
        insertText: xCompletion.insertText,
        insertTextFormat: xCompletion.insertTextFormat,
        data: xCompletion.data,
    });
    assert(resolvedXCompletion &&
        typeof resolvedXCompletion.detail === 'string' &&
        resolvedXCompletion.detail.includes('range 20..20') &&
        resolvedXCompletion.labelDetails &&
        typeof resolvedXCompletion.labelDetails.detail === 'string' &&
        resolvedXCompletion.labelDetails.detail.includes('range 20..20'),
    'completionItem/resolve must preserve local numeric semantic facts and labelDetails');
    const flagCompletion = localCompletions.find((item) => item && item.label === 'flag');
    assert(flagCompletion &&
        typeof flagCompletion.detail === 'string' &&
        flagCompletion.detail.includes('logical true') &&
        flagCompletion.detail.includes('short-circuits') &&
        flagCompletion.data && flagCompletion.data.uri === documentUri,
    'local completion detail should include logical initializer semantic facts and resolve data');
    const resolvedFlagCompletion = await client.request('completionItem/resolve', {
        label: flagCompletion.label,
        kind: flagCompletion.kind,
        insertText: flagCompletion.insertText,
        insertTextFormat: flagCompletion.insertTextFormat,
        data: flagCompletion.data,
    });
    assert(resolvedFlagCompletion &&
        typeof resolvedFlagCompletion.detail === 'string' &&
        resolvedFlagCompletion.detail.includes('logical true') &&
        resolvedFlagCompletion.detail.includes('short-circuits') &&
        resolvedFlagCompletion.labelDetails &&
        typeof resolvedFlagCompletion.labelDetails.detail === 'string' &&
        resolvedFlagCompletion.labelDetails.detail.includes('logical true') &&
        resolvedFlagCompletion.labelDetails.detail.includes('short-circuits'),
    'completionItem/resolve must preserve local logical semantic facts and labelDetails');

    const documentSymbols = await client.request('textDocument/documentSymbol', {
        textDocument: { uri: documentUri },
    });
    assert(Array.isArray(documentSymbols) && documentSymbols.length > 0,
        'documentSymbol must return at least one symbol');

    const workspaceSymbols = await client.request('workspace/symbol', {
        query: 'x',
    });
    assert(Array.isArray(workspaceSymbols) && workspaceSymbols.length > 0,
        'workspace/symbol must return at least one symbol');
    const resolvedWorkspaceSymbol = await client.request('workspaceSymbol/resolve', workspaceSymbols[0]);
    assert(resolvedWorkspaceSymbol &&
        resolvedWorkspaceSymbol.name === workspaceSymbols[0].name &&
        resolvedWorkspaceSymbol.location &&
        resolvedWorkspaceSymbol.location.uri === workspaceSymbols[0].location.uri,
    'workspaceSymbol/resolve must preserve resolved workspace symbols');

    const highlights = await client.request('textDocument/documentHighlight', {
        textDocument: { uri: documentUri },
        position: { line: 0, character: 4 },
    });
    assert(Array.isArray(highlights) && highlights.length > 0,
        'documentHighlight must return at least one highlight');
    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: documentHighlightFilterUri,
            languageId: 'zr',
            version: 1,
            text: documentHighlightFilterText,
        },
    });
    const documentHighlightFilterDiagnostics = await client.waitForNotification('textDocument/publishDiagnostics');
    assert(documentHighlightFilterDiagnostics.uri === documentHighlightFilterUri,
        'document highlight filter diagnostics uri mismatch');
    const lineCommentHighlights = await client.request('textDocument/documentHighlight', {
        textDocument: { uri: documentHighlightFilterUri },
        position: findPosition(documentHighlightFilterText, 'highlightOnly', 1),
    });
    const stringHighlights = await client.request('textDocument/documentHighlight', {
        textDocument: { uri: documentHighlightFilterUri },
        position: findPosition(documentHighlightFilterText, 'highlightOnly', 2),
    });
    const blockCommentHighlights = await client.request('textDocument/documentHighlight', {
        textDocument: { uri: documentHighlightFilterUri },
        position: findPosition(documentHighlightFilterText, 'highlightOnly', 3),
    });
    assert(Array.isArray(lineCommentHighlights) && lineCommentHighlights.length === 0 &&
        Array.isArray(stringHighlights) && stringHighlights.length === 0 &&
        Array.isArray(blockCommentHighlights) && blockCommentHighlights.length === 0,
    'documentHighlight must ignore identifiers inside comments and strings');

    const prepareRename = await client.request('textDocument/prepareRename', {
        textDocument: { uri: documentUri },
        position: { line: 0, character: 4 },
    });
    assert(prepareRename && prepareRename.range && prepareRename.placeholder === 'x',
        'prepareRename must return range and placeholder');

    const usagePrepareRename = await client.request('textDocument/prepareRename', {
        textDocument: { uri: documentUri },
        position: { line: 0, character: 20 },
    });
    assert(usagePrepareRename &&
        usagePrepareRename.range &&
        usagePrepareRename.placeholder === 'x' &&
        usagePrepareRename.range.start.line === 0 &&
        usagePrepareRename.range.start.character === 20 &&
        usagePrepareRename.range.end.character === 21,
    'prepareRename must resolve local identifier usages');

    const rename = await client.request('textDocument/rename', {
        textDocument: { uri: documentUri },
        position: { line: 0, character: 4 },
        newName: 'renamedX',
    });
    assert(rename && !rename.changes &&
        Array.isArray(rename.documentChanges) &&
        rename.documentChanges.some((documentChange) =>
            documentChange &&
            documentChange.textDocument &&
            documentChange.textDocument.uri === documentUri &&
            documentChange.textDocument.version === 2 &&
            Array.isArray(documentChange.edits) &&
            documentChange.edits.length > 0),
        'rename must return versioned workspace edits for the document');

    const usageRename = await client.request('textDocument/rename', {
        textDocument: { uri: documentUri },
        position: { line: 0, character: 20 },
        newName: 'renamedFromUsageX',
    });
    assert(usageRename && !usageRename.changes &&
        workspaceEditTextEdits(usageRename, documentUri).length > 0 &&
        workspaceEditTextEdits(usageRename, documentUri).some((edit) =>
            edit &&
            edit.range &&
            edit.range.start &&
            edit.range.start.line === 0 &&
            edit.range.start.character === 4) &&
        workspaceEditTextEdits(usageRename, documentUri).some((edit) =>
            edit &&
            edit.range &&
            edit.range.start &&
            edit.range.start.line === 0 &&
            edit.range.start.character === 20),
        'rename must resolve local identifier usages and include declaration plus usage edits');

    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: docsUri,
            languageId: 'zr',
            version: 1,
            text: documentationText,
        },
    });

    const docsDiagnostics = await client.waitForNotification('textDocument/publishDiagnostics');
    assert(docsDiagnostics.uri === docsUri, 'documentation diagnostics uri mismatch');
    assert(Array.isArray(docsDiagnostics.diagnostics) && docsDiagnostics.diagnostics.length === 0,
        'documentation fixture should open without diagnostics');

    const docsHoverPosition = findPosition(documentationText, 'ScoreBoard.bonus;', 0, 11);
    const docsCompletionPosition = findPosition(documentationText, 'ScoreBoard.bonus;', 0, 11);

    const docsHover = await client.request('textDocument/hover', {
        textDocument: { uri: docsUri },
        position: docsHoverPosition,
    });
    assert(docsHover && docsHover.contents && !Array.isArray(docsHover.contents),
        'hover.contents must be a MarkupContent object');
    assert(docsHover.contents.kind === 'markdown',
        'hover.contents.kind must be markdown');
    assert(typeof docsHover.contents.value === 'string' &&
        docsHover.contents.value.includes('Shared bonus exposed through get/set.'),
        'hover markdown should include the leading property comment');

    const docsCompletions = await client.request('textDocument/completion', {
        textDocument: { uri: docsUri },
        position: docsCompletionPosition,
    });
    assert(Array.isArray(docsCompletions), 'documentation completion must return an array');
    const bonusCompletion = docsCompletions.find((item) => item && item.label === 'bonus');
    assert(bonusCompletion, 'documentation completion must include bonus');
    assert(bonusCompletion.documentation && bonusCompletion.documentation.kind === 'markdown',
        'completion documentation must use markdown');
    assert(typeof bonusCompletion.documentation.value === 'string' &&
        bonusCompletion.documentation.value.includes('Shared bonus exposed through get/set.'),
        'completion documentation should include the leading property comment');
    assert(bonusCompletion.filterText === 'bonus' && bonusCompletion.sortText === 'bonus',
        'completion items must expose stable filterText and sortText');
    assert(Array.isArray(bonusCompletion.commitCharacters) &&
        bonusCompletion.commitCharacters.includes(';') &&
        bonusCompletion.commitCharacters.includes(',') &&
        bonusCompletion.commitCharacters.includes('.'),
    'completion items must expose common commit characters');
    assert(bonusCompletion.textEdit &&
        bonusCompletion.textEdit.newText === 'bonus' &&
        bonusCompletion.textEdit.range &&
        bonusCompletion.textEdit.range.start.line === docsCompletionPosition.line &&
        bonusCompletion.textEdit.range.start.character === docsCompletionPosition.character &&
        bonusCompletion.textEdit.range.end.line === docsCompletionPosition.line &&
        bonusCompletion.textEdit.range.end.character === docsCompletionPosition.character,
    'completion items must expose a stable textEdit insertion range');
    assert(bonusCompletion.data && bonusCompletion.data.uri === docsUri && bonusCompletion.data.position,
        'completion items must include resolve data');
    const resolvedBonusCompletion = await client.request('completionItem/resolve', {
        label: bonusCompletion.label,
        kind: bonusCompletion.kind,
        insertText: bonusCompletion.insertText,
        insertTextFormat: bonusCompletion.insertTextFormat,
        data: bonusCompletion.data,
    });
    assert(resolvedBonusCompletion &&
        resolvedBonusCompletion.documentation &&
        resolvedBonusCompletion.documentation.kind === 'markdown' &&
        resolvedBonusCompletion.documentation.value.includes('Shared bonus exposed through get/set.'),
    'completionItem/resolve must restore markdown documentation from resolve data');

    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: propertyContractUri,
            languageId: 'zr',
            version: 1,
            text: propertyContractText,
        },
    });
    const propertyContractDiagnostics =
        await client.waitForNotification('textDocument/publishDiagnostics');
    assert(propertyContractDiagnostics.uri === propertyContractUri &&
        Array.isArray(propertyContractDiagnostics.diagnostics) &&
        propertyContractDiagnostics.diagnostics.length === 0,
    'unified property contract fixture must open without diagnostics');
    const propertyUsagePosition = findPosition(propertyContractText, 'meter.value', 0, 6);
    const propertyHover = await client.request('textDocument/hover', {
        textDocument: { uri: propertyContractUri },
        position: propertyUsagePosition,
    });
    assert(propertyHover && propertyHover.contents &&
        typeof propertyHover.contents.value === 'string' &&
        propertyHover.contents.value.includes('property value: int') &&
        !propertyHover.contents.value.includes('__get_') &&
        !propertyHover.contents.value.includes('__set_'),
    'unified property hover must expose one canonical visible property contract');
    const propertyCompletions = await client.request('textDocument/completion', {
        textDocument: { uri: propertyContractUri },
        position: propertyUsagePosition,
    });
    assert(Array.isArray(propertyCompletions) &&
        propertyCompletions.filter((item) => item && item.label === 'value').length === 1 &&
        !propertyCompletions.some((item) => item &&
            (item.label === '__get_value' || item.label === '__set_value')),
    'unified property completion must emit one visible property and no hidden accessors');
    const propertyDefinitions = await client.request('textDocument/definition', {
        textDocument: { uri: propertyContractUri },
        position: propertyUsagePosition,
    });
    assert(Array.isArray(propertyDefinitions) && propertyDefinitions.some((location) =>
        location && location.uri === propertyContractUri && location.range &&
        location.range.start.line === 2 && location.range.start.character === 17 &&
        location.range.end.line === 2 && location.range.end.character === 22),
    'unified property definition must target the canonical property selection range');
    const propertyPrepareRename = await client.request('textDocument/prepareRename', {
        textDocument: { uri: propertyContractUri },
        position: propertyUsagePosition,
    });
    assert(propertyPrepareRename && propertyPrepareRename.placeholder === 'value' &&
        propertyPrepareRename.range.start.line === 7 &&
        propertyPrepareRename.range.start.character === 42 &&
        propertyPrepareRename.range.end.line === 7 &&
        propertyPrepareRename.range.end.character === 47,
    'unified property prepareRename must preserve the usage selection range');

    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: testCodeLensUri,
            languageId: 'zr',
            version: 1,
            text: testCodeLensText,
        },
    });
    const testCodeLensDiagnostics =
        await client.waitForNotification('textDocument/publishDiagnostics');
    assert(testCodeLensDiagnostics.uri === testCodeLensUri &&
        Array.isArray(testCodeLensDiagnostics.diagnostics) &&
        testCodeLensDiagnostics.diagnostics.length === 0,
    'typed test CodeLens fixture must open without diagnostics');
    const testCodeLenses = await client.request('textDocument/codeLens', {
        textDocument: { uri: testCodeLensUri },
    });
    const runTestCodeLens = Array.isArray(testCodeLenses) ? testCodeLenses.find((lens) =>
        lens &&
        lens.command &&
        lens.command.command === 'zr.runCurrentProject' &&
        lens.data &&
        lens.data.command === lens.command.command &&
        lens.data.range) : undefined;
    assert(runTestCodeLens,
        `textDocument/codeLens must expose a run command with resolve data for test attributes: ${JSON.stringify(testCodeLenses)}`);
    const resolvedTestCodeLens = await client.request('codeLens/resolve', runTestCodeLens);
    assert(resolvedTestCodeLens &&
        resolvedTestCodeLens.command &&
        resolvedTestCodeLens.command.command === runTestCodeLens.command.command &&
        resolvedTestCodeLens.range &&
        resolvedTestCodeLens.range.start &&
        resolvedTestCodeLens.range.start.line === runTestCodeLens.range.start.line,
    'codeLens/resolve must preserve resolved command lenses');
    const runCommandResult = await client.request('workspace/executeCommand', {
        command: 'zr.runCurrentProject',
        arguments: [testCodeLensUri],
    });
    assert(runCommandResult === null,
        'workspace/executeCommand must acknowledge legacy run command requests');

    const importDiagnosticsText = fs.readFileSync(importDiagnosticsFixture.mainPath, 'utf8');
    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: importDiagnosticsFixture.mainUri,
            languageId: 'zr',
            version: 1,
            text: importDiagnosticsText,
        },
    });

    const importDiagnostics = await client.waitForNotification('textDocument/publishDiagnostics');
    assert(diagnosticRelatedUriMatches(importDiagnosticsFixture.mainUri, importDiagnostics.uri),
        'import diagnostics uri mismatch');
    assert(Array.isArray(importDiagnostics.diagnostics) && importDiagnostics.diagnostics.length > 0,
        'import diagnostics fixture should publish at least one diagnostic');
    const missingImportDiagnostic = importDiagnostics.diagnostics.find((diagnostic) =>
        diagnostic &&
        typeof diagnostic.message === 'string' &&
        diagnostic.message.includes("Import member 'greet.missing' could not be resolved"));
    assert(missingImportDiagnostic,
        'import diagnostics fixture should publish the missing imported member diagnostic');
    assert(missingImportDiagnostic.code === 'plugin_unknown_export',
        `import diagnostics should use plugin_unknown_export code, got ${missingImportDiagnostic.code}`);
    assert(missingImportDiagnostic.data &&
        diagnosticRelatedUriMatches(importDiagnosticsFixture.mainUri, missingImportDiagnostic.data.uri) &&
        missingImportDiagnostic.data.range &&
        missingImportDiagnostic.data.range.start &&
        missingImportDiagnostic.data.range.start.line === missingImportDiagnostic.range.start.line &&
        (!missingImportDiagnostic.code || missingImportDiagnostic.data.code === missingImportDiagnostic.code),
    'diagnostics must carry stable uri/range/code data for follow-up actions');
    assert(Array.isArray(missingImportDiagnostic.relatedInformation) &&
        missingImportDiagnostic.relatedInformation.some((item) =>
            item &&
            item.location &&
            diagnosticRelatedUriMatches(importDiagnosticsFixture.mainUri, item.location.uri)) &&
        missingImportDiagnostic.relatedInformation.some((item) =>
            item &&
            item.location &&
            diagnosticRelatedUriMatches(importDiagnosticsFixture.moduleUri, item.location.uri)),
    'import diagnostics should serialize cross-file relatedInformation trace locations');

    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: parserDiagnosticUri,
            languageId: 'zr',
            version: 1,
            text: parserDiagnosticText,
        },
    });

    const parserDiagnostics = await client.waitForNotification('textDocument/publishDiagnostics');
    assert(parserDiagnostics.uri === parserDiagnosticUri, 'parser diagnostics uri mismatch');
    const missingExpressionDiagnostic = assertDiagnosticIncludes(
        parserDiagnostics,
        'missing_expression_after_assignment',
        "Missing expression after '='",
        'structured parser diagnostic should reach stdio'
    );
    assert(missingExpressionDiagnostic.message.includes("Add an expression before ';'"),
        'structured parser diagnostic should serialize the parser suggestion');

    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: missingConditionUri,
            languageId: 'zr',
            version: 1,
            text: missingConditionText,
        },
    });

    const missingConditionDiagnostics = await client.waitForNotification('textDocument/publishDiagnostics');
    assert(missingConditionDiagnostics.uri === missingConditionUri, 'missing condition diagnostics uri mismatch');
    const missingConditionDiagnostic = assertDiagnosticIncludes(
        missingConditionDiagnostics,
        'missing_condition',
        "Missing condition inside 'if'",
        'structured missing-condition diagnostic should reach stdio'
    );
    assert(missingConditionDiagnostic.message.includes("Add a boolean expression between '(' and ')'"),
        'structured missing-condition diagnostic should serialize the parser suggestion');

    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: genericUri,
            languageId: 'zr',
            version: 1,
            text: genericText,
        },
    });

    const genericDiagnostics = await client.waitForNotification('textDocument/publishDiagnostics');
    assert(genericDiagnostics.uri === genericUri, 'generic diagnostics uri mismatch');
    const genericShortCircuitDiagnostic = assertDiagnosticIncludes(
        genericDiagnostics,
        'short_circuit_unreachable',
        'Right-hand branch is unreachable due to deterministic short-circuit',
        'generic fixture should publish its intentional short-circuit warning'
    );
    assert(genericShortCircuitDiagnostic.severity === 2,
        'generic short-circuit diagnostic should remain a warning');

    const genericTypePosition = findPosition(genericText, 'Derived<Item, 2 + 2>', 0, 0);
    const genericDefinitionPosition = findPosition(genericText, 'class Derived<T, const N: int>', 0, 6);
    const genericCallPosition = findPosition(genericText, 'box.shape(m);', 0, 10);
    const signatureNumericFactPosition = findPosition(genericText, 'pick(1 + 2, true || false);', 0, 7);
    const signatureLogicalFactPosition = findPosition(genericText, 'true || false', 0, 5);

    const genericCompletions = await client.request('textDocument/completion', {
        textDocument: { uri: genericUri },
        position: genericTypePosition,
    });
    assert(Array.isArray(genericCompletions), 'generic completion must return an array');
    const derivedCompletion = genericCompletions.find((item) => item && item.label === 'Derived');
    assert(derivedCompletion && typeof derivedCompletion.detail === 'string' &&
        derivedCompletion.detail.includes('Resolved Type: Derived<Item, 4>'),
        'generic completion detail should include the normalized closed instantiation');
    assert(derivedCompletion.labelDetails &&
        typeof derivedCompletion.labelDetails.detail === 'string' &&
        derivedCompletion.labelDetails.detail.includes('Resolved Type: Derived<Item, 4>'),
    'completion items with detail must expose labelDetails for modern completion UIs');
    assert(derivedCompletion.data && derivedCompletion.data.uri === genericUri && derivedCompletion.data.position,
        'generic completion items with semantic detail must include resolve data');
    const resolvedDerivedCompletion = await client.request('completionItem/resolve', {
        label: derivedCompletion.label,
        kind: derivedCompletion.kind,
        insertText: derivedCompletion.insertText,
        insertTextFormat: derivedCompletion.insertTextFormat,
        data: derivedCompletion.data,
    });
    assert(resolvedDerivedCompletion &&
        typeof resolvedDerivedCompletion.detail === 'string' &&
        resolvedDerivedCompletion.detail.includes('Resolved Type: Derived<Item, 4>') &&
        resolvedDerivedCompletion.labelDetails &&
        typeof resolvedDerivedCompletion.labelDetails.detail === 'string' &&
        resolvedDerivedCompletion.labelDetails.detail.includes('Resolved Type: Derived<Item, 4>'),
    'completionItem/resolve must preserve semantic detail and labelDetails for generic completions');

    const genericDefinition = await client.request('textDocument/definition', {
        textDocument: { uri: genericUri },
        position: genericTypePosition,
    });
    assert(Array.isArray(genericDefinition) && genericDefinition.length > 0,
        'generic definition must return at least one location');
    assert(genericDefinition.some((location) =>
        location &&
        location.uri === genericUri &&
        location.range &&
        location.range.start &&
        location.range.start.line === genericDefinitionPosition.line &&
        location.range.start.character === genericDefinitionPosition.character),
        'generic definition should jump from closed use to open generic declaration');

    const signatureHelp = await client.request('textDocument/signatureHelp', {
        textDocument: { uri: genericUri },
        position: genericCallPosition,
    });
    assert(signatureHelp && Array.isArray(signatureHelp.signatures) && signatureHelp.signatures.length > 0,
        'signatureHelp must return at least one signature');
    assert(signatureHelp.activeParameter === 0,
        'signatureHelp activeParameter must resolve to the current argument index');
    assert(signatureHelp.signatures.some((signature) =>
        signature &&
        typeof signature.label === 'string' &&
        signature.label.includes('shape<const N: int>(value: Matrix<int, 4>): Matrix<int, 4>')),
        'signatureHelp should show the closed generic method signature with normalized const generics');

    const numericFactSignatureHelp = await client.request('textDocument/signatureHelp', {
        textDocument: { uri: genericUri },
        position: signatureNumericFactPosition,
    });
    assert(numericFactSignatureHelp &&
        Array.isArray(numericFactSignatureHelp.signatures) &&
        numericFactSignatureHelp.signatures.some((signature) =>
            signature &&
            Array.isArray(signature.parameters) &&
            signature.parameters[0] &&
            signature.parameters[0].documentation &&
            typeof signature.parameters[0].documentation.value === 'string' &&
            signature.parameters[0].documentation.value.includes('range 3..3')),
    'signatureHelp parameter documentation should serialize argument numeric semantic facts');

    const logicalFactSignatureHelp = await client.request('textDocument/signatureHelp', {
        textDocument: { uri: genericUri },
        position: signatureLogicalFactPosition,
    });
    assert(logicalFactSignatureHelp &&
        Array.isArray(logicalFactSignatureHelp.signatures) &&
        logicalFactSignatureHelp.signatures.some((signature) =>
            signature &&
            Array.isArray(signature.parameters) &&
            signature.parameters[1] &&
            signature.parameters[1].documentation &&
            typeof signature.parameters[1].documentation.value === 'string' &&
            signature.parameters[1].documentation.value.includes('logical true') &&
            signature.parameters[1].documentation.value.includes('short-circuits')),
    'signatureHelp parameter documentation should serialize argument logical semantic facts');

    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: canonicalDisplayUri,
            languageId: 'zr',
            version: 1,
            text: canonicalDisplayText,
        },
    });
    const canonicalDisplayDiagnostics =
        await client.waitForNotification('textDocument/publishDiagnostics');
    assert(canonicalDisplayDiagnostics.uri === canonicalDisplayUri,
        'canonical display diagnostics uri mismatch');
    const canonicalDisplayPosition = findPosition(canonicalDisplayText, 'redact(null)', 0, 0);
    const canonicalDisplayCompletions = await client.request('textDocument/completion', {
        textDocument: { uri: canonicalDisplayUri },
        position: canonicalDisplayPosition,
    });
    const redactCompletion = Array.isArray(canonicalDisplayCompletions) ?
        canonicalDisplayCompletions.find((item) => item && item.label === 'redact') : undefined;
    assert(redactCompletion && typeof redactCompletion.detail === 'string' &&
        redactCompletion.detail.includes('value: cannot infer exact type') &&
        redactCompletion.detail.includes('): cannot infer exact type') &&
        !redactCompletion.detail.includes('MissingType'),
    `completion must fail closed when an explicit declaration type lacks a resolved canonical fact: ${JSON.stringify(redactCompletion)}`);
    const canonicalDisplayHover = await client.request('textDocument/hover', {
        textDocument: { uri: canonicalDisplayUri },
        position: canonicalDisplayPosition,
    });
    assert(canonicalDisplayHover && canonicalDisplayHover.contents &&
        typeof canonicalDisplayHover.contents.value === 'string' &&
        canonicalDisplayHover.contents.value.includes('value: cannot infer exact type') &&
        canonicalDisplayHover.contents.value.includes('): cannot infer exact type') &&
        !canonicalDisplayHover.contents.value.includes('MissingType'),
    `hover must fail closed when an explicit declaration type lacks a resolved canonical fact: ${JSON.stringify(canonicalDisplayHover)}`);

    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: nativeCallableUri,
            languageId: 'zr',
            version: 1,
            text: nativeCallableText,
        },
    });

    const nativeCallableDiagnostics =
        await client.waitForNotification('textDocument/publishDiagnostics');
    assert(nativeCallableDiagnostics.uri === nativeCallableUri,
        'native callable diagnostics uri mismatch');
    assert(Array.isArray(nativeCallableDiagnostics.diagnostics) &&
        nativeCallableDiagnostics.diagnostics.length === 0,
    'native callable fixture should open without diagnostics');

    const nativeCallableLabel = 'set_budget(microseconds: int): null';
    const nativeCallableSignature = await client.request('textDocument/signatureHelp', {
        textDocument: { uri: nativeCallableUri },
        position: findPosition(nativeCallableText, 'gc.set_budget(2000)', 0, 17),
    });
    assert(nativeCallableSignature && nativeCallableSignature.activeParameter === 0 &&
        Array.isArray(nativeCallableSignature.signatures) &&
        nativeCallableSignature.signatures.some((signature) =>
            signature && signature.label === nativeCallableLabel &&
            Array.isArray(signature.parameters) &&
            signature.parameters.length === 1 &&
            signature.parameters[0].label === 'microseconds: int' &&
            signature.parameters[0].documentation &&
            typeof signature.parameters[0].documentation.value === 'string' &&
            signature.parameters[0].documentation.value.includes(
                'Pause budget in microseconds used for both pause and remark slices.')),
    'native callable signatureHelp should preserve the structured builtin descriptor contract');

    const nativeCallableHover = await client.request('textDocument/hover', {
        textDocument: { uri: nativeCallableUri },
        position: findPosition(nativeCallableText, 'gc.set_budget(2000)', 0, 5),
    });
    assert(nativeCallableHover && nativeCallableHover.contents &&
        typeof nativeCallableHover.contents.value === 'string' &&
        nativeCallableHover.contents.value.includes(nativeCallableLabel) &&
        nativeCallableHover.contents.value.includes('Source: native builtin'),
    'native callable hover should reuse the same structured builtin descriptor contract');

    const nativeReceiverCallableLabel = 'fn addLast(value: int): LinkedNode<int>';
    const nativeReceiverCallableSignature =
        await client.request('textDocument/signatureHelp', {
            textDocument: { uri: nativeCallableUri },
            position: findPosition(nativeCallableText, 'list.addLast(1)', 0, 13),
        });
    assert(nativeReceiverCallableSignature &&
        nativeReceiverCallableSignature.activeParameter === 0 &&
        Array.isArray(nativeReceiverCallableSignature.signatures) &&
        nativeReceiverCallableSignature.signatures.some((signature) =>
            signature && signature.label === nativeReceiverCallableLabel &&
            Array.isArray(signature.parameters) &&
            signature.parameters.length === 1 &&
            signature.parameters[0].label === 'value: int'),
    'native receiver signatureHelp should merge descriptor names with canonical closed types');

    const nativeReceiverCallableHover = await client.request('textDocument/hover', {
        textDocument: { uri: nativeCallableUri },
        position: findPosition(nativeCallableText, 'list.addLast(1)', 0, 7),
    });
    assert(nativeReceiverCallableHover && nativeReceiverCallableHover.contents &&
        typeof nativeReceiverCallableHover.contents.value === 'string' &&
        nativeReceiverCallableHover.contents.value.includes(nativeReceiverCallableLabel) &&
        nativeReceiverCallableHover.contents.value.includes('Source: native builtin'),
    'native receiver hover should reuse the same canonical closed descriptor contract');

    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: descriptorPluginGenericFixture.mainUri,
            languageId: 'zr',
            version: 1,
            text: descriptorPluginGenericFixture.content,
        },
    });
    const genericPluginDiagnostics =
        await client.waitForNotification('textDocument/publishDiagnostics');
    assert(genericPluginDiagnostics.uri === descriptorPluginGenericFixture.mainUri &&
        Array.isArray(genericPluginDiagnostics.diagnostics) &&
        genericPluginDiagnostics.diagnostics.length === 0,
    `native generic receiver fixture should open without diagnostics: ${JSON.stringify(genericPluginDiagnostics)}`);

    const nativeGenericReceiverLabel = 'fn echo<T>(value: int): int';
    const nativeGenericReceiverSignature =
        await client.request('textDocument/signatureHelp', {
            textDocument: { uri: descriptorPluginGenericFixture.mainUri },
            position: findPosition(
                descriptorPluginGenericFixture.content,
                'point.echo(1)',
                0,
                11),
        });
    assert(nativeGenericReceiverSignature &&
        nativeGenericReceiverSignature.activeParameter === 0 &&
        Array.isArray(nativeGenericReceiverSignature.signatures) &&
        nativeGenericReceiverSignature.signatures.some((signature) =>
            signature && signature.label === nativeGenericReceiverLabel &&
            Array.isArray(signature.parameters) &&
            signature.parameters.length === 1 &&
            signature.parameters[0].label === 'value: int'),
    'native generic receiver signatureHelp should preserve the structured generic clause and closed type');

    const nativeGenericReceiverHover = await client.request('textDocument/hover', {
        textDocument: { uri: descriptorPluginGenericFixture.mainUri },
        position: findPosition(
            descriptorPluginGenericFixture.content,
            'point.echo(1)',
            0,
            8),
    });
    assert(nativeGenericReceiverHover && nativeGenericReceiverHover.contents &&
        typeof nativeGenericReceiverHover.contents.value === 'string' &&
        nativeGenericReceiverHover.contents.value.includes(nativeGenericReceiverLabel) &&
        nativeGenericReceiverHover.contents.value.includes(
            'Returns a value using an unconstrained method generic.'),
    'native generic receiver hover should share the structured generic contract');

    const warmHoverLatency = await measureWarmRequestLatency(client, 'textDocument/hover', {
        textDocument: { uri: nativeCallableUri },
        position: findPosition(nativeCallableText, 'gc.set_budget(2000)', 0, 5),
    });
    const warmCompletionLatency = await measureWarmRequestLatency(client, 'textDocument/completion', {
        textDocument: { uri: genericUri },
        position: genericTypePosition,
    });
    const warmSignatureLatency = await measureWarmRequestLatency(client, 'textDocument/signatureHelp', {
        textDocument: { uri: genericUri },
        position: genericCallPosition,
    });
    assertWarmRequestBudget('hover', warmHoverLatency, 50);
    assertWarmRequestBudget('completion', warmCompletionLatency, 100);
    assertWarmRequestBudget('signatureHelp', warmSignatureLatency, 100);
    console.log(
        'LSP warm request latency ms: hover p50=' + warmHoverLatency.p50.toFixed(2) +
        ' p95=' + warmHoverLatency.p95.toFixed(2) +
        ' p99=' + warmHoverLatency.p99.toFixed(2) +
        '; completion p50=' + warmCompletionLatency.p50.toFixed(2) +
        ' p95=' + warmCompletionLatency.p95.toFixed(2) +
        ' p99=' + warmCompletionLatency.p99.toFixed(2) +
        '; signatureHelp p50=' + warmSignatureLatency.p50.toFixed(2) +
        ' p95=' + warmSignatureLatency.p95.toFixed(2) +
        ' p99=' + warmSignatureLatency.p99.toFixed(2));

    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: diagnosticsLatencyUri,
            languageId: 'zr',
            version: 1,
            text: diagnosticsLatencyBaseText + '// diagnostics warm 00\n',
        },
    });
    const diagnosticsLatencyInitial = await waitForDiagnosticsUriVersion(
        client,
        diagnosticsLatencyUri,
        1,
        'warm diagnostics fixture must publish version one');
    assert(Array.isArray(diagnosticsLatencyInitial.diagnostics) &&
        diagnosticsLatencyInitial.diagnostics.length === 0,
    'warm diagnostics fixture must open without diagnostics');
    const warmDiagnosticsLatency = await measureWarmDiagnosticsLatency(
        client,
        diagnosticsLatencyUri,
        diagnosticsLatencyBaseText);
    assertWarmRequestBudget('diagnostics', warmDiagnosticsLatency, 250);
    console.log(
        'LSP warm diagnostics latency ms: p50=' + warmDiagnosticsLatency.p50.toFixed(2) +
        ' p95=' + warmDiagnosticsLatency.p95.toFixed(2) +
        ' p99=' + warmDiagnosticsLatency.p99.toFixed(2));
    client.notify('textDocument/didClose', {
        textDocument: { uri: diagnosticsLatencyUri },
    });
    const diagnosticsLatencyClosed = await waitForDiagnosticsUri(
        client,
        diagnosticsLatencyUri,
        'warm diagnostics fixture didClose diagnostics uri mismatch');
    assert(Array.isArray(diagnosticsLatencyClosed.diagnostics) &&
        diagnosticsLatencyClosed.diagnostics.length === 0,
    'warm diagnostics fixture didClose must clear diagnostics');

    client.notify('workspace/didChangeWatchedFiles', {
        changes: [
            { uri: workspaceLatencyFixture.projectUri, type: 1 },
        ],
    });
    const workspaceLatencySymbols = await client.request('workspace/symbol', {
        query: 'workspace_latency_target',
    });
    assert(Array.isArray(workspaceLatencySymbols) && workspaceLatencySymbols.some((item) =>
        item &&
        item.location &&
        diagnosticRelatedUriMatches(workspaceLatencyFixture.targetUri, item.location.uri) &&
        item.name === 'workspace_latency_target'),
    '100-file workspace fixture must index the target source before latency sampling');
    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: workspaceLatencyFixture.targetUri,
            languageId: 'zr',
            version: 1,
            text: workspaceLatencyFixture.targetText + '// workspace latency 00\n',
        },
    });
    const workspaceLatencyInitial = await waitForDiagnosticsUriVersion(
        client,
        workspaceLatencyFixture.targetUri,
        1,
        '100-file workspace latency fixture must publish version one');
    assert(Array.isArray(workspaceLatencyInitial.diagnostics) &&
        workspaceLatencyInitial.diagnostics.length === 0,
    '100-file workspace latency fixture must open without diagnostics');
    const workspaceIncrementalDiagnosticsLatency = await measureWarmDiagnosticsLatency(
        client,
        workspaceLatencyFixture.targetUri,
        workspaceLatencyFixture.targetText);
    assertWarmRequestBudget(
        '100-file workspace incremental diagnostics',
        workspaceIncrementalDiagnosticsLatency,
        500);
    console.log(
        'LSP 100-file workspace incremental diagnostics latency ms: p50=' +
        workspaceIncrementalDiagnosticsLatency.p50.toFixed(2) +
        ' p95=' + workspaceIncrementalDiagnosticsLatency.p95.toFixed(2) +
        ' p99=' + workspaceIncrementalDiagnosticsLatency.p99.toFixed(2));
    client.notify('textDocument/didClose', {
        textDocument: { uri: workspaceLatencyFixture.targetUri },
    });
    const workspaceLatencyClosed = await waitForDiagnosticsUri(
        client,
        workspaceLatencyFixture.targetUri,
        '100-file workspace latency fixture didClose diagnostics uri mismatch');
    assert(Array.isArray(workspaceLatencyClosed.diagnostics) &&
        workspaceLatencyClosed.diagnostics.length === 0,
    '100-file workspace latency fixture didClose must clear diagnostics');

    client.notify('textDocument/didClose', {
        textDocument: { uri: descriptorPluginGenericFixture.mainUri },
    });
    const genericPluginCloseDiagnostics = await waitForDiagnosticsUri(
        client,
        descriptorPluginGenericFixture.mainUri,
        'native generic receiver didClose diagnostics uri mismatch');
    assert(Array.isArray(genericPluginCloseDiagnostics.diagnostics) &&
        genericPluginCloseDiagnostics.diagnostics.length === 0,
    'native generic receiver didClose must clear diagnostics');

    client.notify('textDocument/didClose', {
        textDocument: { uri: nativeCallableUri },
    });
    const nativeCallableCloseDiagnostics = await waitForDiagnosticsUri(
        client,
        nativeCallableUri,
        'native callable didClose diagnostics uri mismatch');
    assert(Array.isArray(nativeCallableCloseDiagnostics.diagnostics) &&
        nativeCallableCloseDiagnostics.diagnostics.length === 0,
    'native callable didClose must clear diagnostics');

    const genericInlayHints = await client.request('textDocument/inlayHint', {
        textDocument: { uri: genericUri },
        range: {
            start: { line: 0, character: 0 },
            end: { line: genericText.split('\n').length, character: 0 },
        },
    });
    assert(Array.isArray(genericInlayHints),
        'textDocument/inlayHint must return an array');
    assert(genericInlayHints.some((hint) => hint && hint.label === ': Box<int>'),
        'inlay hints should include the exact inferred closed generic type for box');
    assert(genericInlayHints.some((hint) => hint && hint.label === ': Matrix<int, 4>'),
        'inlay hints should include the normalized exact inferred closed generic type for m');
    assert(genericInlayHints.some((hint) => hint && hint.label === ': int'),
        'inlay hints should include the exact inferred return type for inferNumber');
    const boxInlayHint = genericInlayHints.find((hint) => hint && hint.label === ': Box<int>');
    const resolvedBoxInlayHint = await client.request('inlayHint/resolve', boxInlayHint);
    assert(resolvedBoxInlayHint &&
        resolvedBoxInlayHint.label === boxInlayHint.label &&
        resolvedBoxInlayHint.position &&
        resolvedBoxInlayHint.position.line === boxInlayHint.position.line,
    'inlayHint/resolve must preserve resolved hints');

    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: formatEditUri,
            languageId: 'zr',
            version: 1,
            text: formatEditText,
        },
    });
    await waitForDiagnosticsUri(client, formatEditUri, 'format edit diagnostics uri mismatch');
    const formatted = await client.request('textDocument/formatting', {
        textDocument: { uri: formatEditUri },
        options: { tabSize: 4, insertSpaces: true },
    });
    assert(Array.isArray(formatted) && formatted.length === 1 &&
        formatted[0].newText.includes('    pub fn run') &&
        formatted[0].newText.includes('        return value;'),
        'textDocument/formatting must return a full-document indented edit');
    const willSaveEdits = await client.request('textDocument/willSaveWaitUntil', {
        textDocument: { uri: formatEditUri },
        reason: 1,
    });
    assert(Array.isArray(willSaveEdits) && willSaveEdits.length === 1 &&
        willSaveEdits[0].newText.includes('    pub fn run') &&
        willSaveEdits[0].newText.includes('        return value;'),
        'textDocument/willSaveWaitUntil must return save-time formatting edits');

    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: noopFormatUri,
            languageId: 'zr',
            version: 1,
            text: noopFormatText,
        },
    });
    await waitForDiagnosticsUri(client, noopFormatUri, 'noop formatting diagnostics uri mismatch');
    const noopFormatted = await client.request('textDocument/formatting', {
        textDocument: { uri: noopFormatUri },
        options: { tabSize: 4, insertSpaces: true },
    });
    assert(Array.isArray(noopFormatted) && noopFormatted.length === 0,
        'textDocument/formatting must skip already formatted documents');
    const noopRangeFormatted = await client.request('textDocument/rangeFormatting', {
        textDocument: { uri: noopFormatUri },
        range: { start: { line: 1, character: 0 }, end: { line: 3, character: 0 } },
        options: { tabSize: 4, insertSpaces: true },
    });
    assert(Array.isArray(noopRangeFormatted) && noopRangeFormatted.length === 0,
        'textDocument/rangeFormatting must skip already formatted ranges');
    const noopRangesFormatted = await client.request('textDocument/rangesFormatting', {
        textDocument: { uri: noopFormatUri },
        ranges: [
            { start: { line: 1, character: 0 }, end: { line: 2, character: 0 } },
            { start: { line: 2, character: 0 }, end: { line: 3, character: 0 } },
        ],
        options: { tabSize: 4, insertSpaces: true },
    });
    assert(Array.isArray(noopRangesFormatted) && noopRangesFormatted.length === 0,
        'textDocument/rangesFormatting must return aggregated range edits');

    const onTypeFormatted = await client.request('textDocument/onTypeFormatting', {
        textDocument: { uri: genericUri },
        position: { line: 8, character: 1 },
        ch: '}',
        options: { tabSize: 4, insertSpaces: true },
    });
    assert(Array.isArray(onTypeFormatted),
        'textDocument/onTypeFormatting must return an edit array');

    const folds = await client.request('textDocument/foldingRange', {
        textDocument: { uri: genericUri },
    });
    assert(Array.isArray(folds) && folds.length > 0,
        'textDocument/foldingRange must return structural ranges');

    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: importFoldingUri,
            languageId: 'zr',
            version: 1,
            text: importFoldingText,
        },
    });
    await waitForDiagnosticsUri(client, importFoldingUri, 'import folding diagnostics uri mismatch');
    const importFolds = await client.request('textDocument/foldingRange', {
        textDocument: { uri: importFoldingUri },
    });
    assert(Array.isArray(importFolds) && importFolds.some((range) =>
        range &&
        range.kind === 'imports' &&
        range.startLine === 0 &&
        range.endLine === 2) &&
        importFolds.some((range) =>
            range &&
        range.kind === 'comment' &&
        range.startLine === 4 &&
        range.endLine === 5) &&
        importFolds.some((range) =>
            range &&
            range.kind === 'region' &&
            range.startLine === 7 &&
            range.endLine === 11),
    'textDocument/foldingRange must include import, comment, and explicit marker regions');

    const selections = await client.request('textDocument/selectionRange', {
        textDocument: { uri: genericUri },
        positions: [genericDefinitionPosition],
    });
    assert(Array.isArray(selections) &&
        selections.length === 1 &&
        selections[0].range &&
        selections[0].parent &&
        selections[0].parent.parent,
    'textDocument/selectionRange must return word, line, and block parent ranges');

    const importLinks = await client.request('textDocument/documentLink', {
        textDocument: { uri: importFoldingUri },
    });
    assert(Array.isArray(importLinks),
        'textDocument/documentLink must return an array');
    assert(importLinks.some((link) =>
        link && link.target === 'zr-decompiled:/zr.system.zr'),
    'textDocument/documentLink must expose native import virtual declaration links');

    const zrpLinksPath = path.join(watchedFixture.rootPath, 'linked_paths.zrp');
    const zrpLinksUri = pathToFileURL(zrpLinksPath).toString();
    const zrpLinksText = [
        '{',
        '  "name": "linked_paths",',
        '  "source": "src",',
        '  "binary": "bin",',
        '  "entry": "main",',
        '  "dependency": "deps",',
        '  "local": "local_modules"',
        '}',
        '',
    ].join('\n');
    fs.writeFileSync(zrpLinksPath, zrpLinksText);
    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: zrpLinksUri,
            languageId: 'json',
            version: 1,
            text: zrpLinksText,
        },
    });
    await waitForDiagnosticsUri(client, zrpLinksUri, 'zrp documentLink diagnostics uri mismatch');
    const zrpLinks = await client.request('textDocument/documentLink', {
        textDocument: { uri: zrpLinksUri },
    });
    assert(Array.isArray(zrpLinks) &&
        zrpLinks.some((link) =>
            link &&
            typeof link.target === 'string' &&
            link.target.endsWith('/src') &&
            link.data &&
            link.data.target === link.target &&
            link.data.range) &&
        zrpLinks.some((link) => link && typeof link.target === 'string' && link.target.endsWith('/bin')) &&
        zrpLinks.some((link) => link && typeof link.target === 'string' && link.target.endsWith('/src/main.zr')) &&
        zrpLinks.some((link) => link && typeof link.target === 'string' && link.target.endsWith('/deps')) &&
        zrpLinks.some((link) => link && typeof link.target === 'string' && link.target.endsWith('/local_modules')),
    'textDocument/documentLink must expose all zrp project path fields');
    const resolvedZrpLink = await client.request('documentLink/resolve', zrpLinks[0]);
    assert(resolvedZrpLink &&
        resolvedZrpLink.target === zrpLinks[0].target &&
        resolvedZrpLink.range &&
        resolvedZrpLink.range.start &&
        resolvedZrpLink.range.start.line === zrpLinks[0].range.start.line,
    'documentLink/resolve must preserve resolved target links');

    const virtualNetworkUri = 'zr-decompiled:/zr.network.zr';
    const virtualNetworkText = await client.request('zr/nativeDeclarationDocument', {
        uri: virtualNetworkUri,
    });
    assert(typeof virtualNetworkText === 'string' && virtualNetworkText.includes('pub module tcp: zr.network.tcp;'),
        'zr/nativeDeclarationDocument must render native module links');
    const virtualNetworkLinks = await client.request('textDocument/documentLink', {
        textDocument: { uri: virtualNetworkUri },
    });
    assert(Array.isArray(virtualNetworkLinks) && virtualNetworkLinks.some((link) =>
        link && link.target === 'zr-decompiled:/zr.network.tcp.zr'),
    'textDocument/documentLink must expose virtual native module links');

    const codeActions = await client.request('textDocument/codeAction', {
        textDocument: { uri: genericUri },
        range: { start: { line: 0, character: 0 }, end: { line: genericText.split('\n').length, character: 0 } },
        context: { diagnostics: [], only: ['source.organizeImports'] },
    });
    assert(Array.isArray(codeActions),
        'textDocument/codeAction must return an array');

    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: moduleImportsUri,
            languageId: 'zr',
            version: 0,
            text: moduleImportsText,
        },
    });
    await waitForDiagnosticsUri(client, moduleImportsUri, 'module imports diagnostics uri mismatch');
    const moduleImportActions = await client.request('textDocument/codeAction', {
        textDocument: { uri: moduleImportsUri },
        range: { start: { line: 0, character: 0 }, end: { line: moduleImportsText.split('\n').length, character: 0 } },
        context: { diagnostics: [], only: ['source.organizeImports'] },
    });
    assert(Array.isArray(moduleImportActions) && moduleImportActions.some((action) =>
        action &&
        action.kind === 'source.organizeImports' &&
        action.edit &&
        !action.edit.changes &&
        Array.isArray(action.edit.documentChanges) &&
        action.edit.documentChanges.some((documentChange) =>
            documentChange &&
            documentChange.textDocument &&
            documentChange.textDocument.uri === moduleImportsUri &&
            documentChange.textDocument.version === 0 &&
            Array.isArray(documentChange.edits) &&
            documentChange.edits.some((edit) =>
                edit.newText.includes('let math = import("zr.math");\nlet system = import("zr.system");')))),
    'textDocument/codeAction must serialize current imports with one versioned edit representation');
    const organizeImportAction = moduleImportActions.find((action) =>
        action && action.kind === 'source.organizeImports' && action.edit);
    assert(organizeImportAction &&
        organizeImportAction.data &&
        organizeImportAction.data.uri === moduleImportsUri &&
        organizeImportAction.data.kind === organizeImportAction.kind &&
        organizeImportAction.data.title === organizeImportAction.title &&
        organizeImportAction.data.snapshot &&
        organizeImportAction.data.snapshot.version === 0 &&
        organizeImportAction.data.snapshot.isOpenDocument === true &&
        Number.isInteger(organizeImportAction.data.snapshot.contentGeneration) &&
        Number.isInteger(organizeImportAction.data.snapshot.contentLength) &&
        /^[0-9a-f]{16}$/.test(organizeImportAction.data.snapshot.contentHash),
    'textDocument/codeAction must attach exact version-zero snapshot resolve data');
    const resolvedOrganizeImportAction = await client.request('codeAction/resolve', organizeImportAction);
    assert(resolvedOrganizeImportAction &&
        resolvedOrganizeImportAction.title === organizeImportAction.title &&
        resolvedOrganizeImportAction.edit &&
        !resolvedOrganizeImportAction.edit.changes &&
        workspaceEditTextEdits(resolvedOrganizeImportAction.edit, moduleImportsUri).length > 0 &&
        resolvedOrganizeImportAction.data &&
        resolvedOrganizeImportAction.data.uri === moduleImportsUri,
    'codeAction/resolve must preserve resolved edits');

    client.notify('textDocument/didChange', {
        textDocument: { uri: moduleImportsUri, version: 1 },
        contentChanges: [{ text: moduleImportsText }],
    });
    const staleResolvedOrganizeImportAction =
        await client.request('codeAction/resolve', organizeImportAction);
    assert(staleResolvedOrganizeImportAction &&
        !staleResolvedOrganizeImportAction.edit &&
        staleResolvedOrganizeImportAction.disabled &&
        staleResolvedOrganizeImportAction.disabled.reason ===
            'Document changed since this code action was computed',
    'codeAction/resolve must disable stale workspace edits instead of replaying them');

    const freshModuleImportActions = await client.request('textDocument/codeAction', {
        textDocument: { uri: moduleImportsUri },
        range: { start: { line: 0, character: 0 }, end: { line: moduleImportsText.split('\n').length, character: 0 } },
        context: { diagnostics: [], only: ['source.organizeImports'] },
    });
    const freshOrganizeImportAction = freshModuleImportActions.find((action) =>
        action && action.kind === 'source.organizeImports' && action.edit);
    assert(freshOrganizeImportAction &&
        freshOrganizeImportAction.data &&
        freshOrganizeImportAction.data.snapshot &&
        freshOrganizeImportAction.data.snapshot.version === 1 &&
        !freshOrganizeImportAction.edit.changes &&
        freshOrganizeImportAction.edit.documentChanges.some((documentChange) =>
            documentChange &&
            documentChange.textDocument &&
            documentChange.textDocument.version === 1),
    'fresh code actions must serialize the recaptured document version');
    const resolvedFreshOrganizeImportAction =
        await client.request('codeAction/resolve', freshOrganizeImportAction);
    assert(resolvedFreshOrganizeImportAction &&
        resolvedFreshOrganizeImportAction.edit &&
        !resolvedFreshOrganizeImportAction.disabled,
    'codeAction/resolve must preserve edits while the captured snapshot remains current');

    const aliasImportsUri = 'file:///c%3A/Users/test/workspace/%2Bzr_vm%2B/stdio-alias-imports.zr';
    const aliasImportsText = [
        'let system = import("zr.system");',
        'let math = import("zr.math");',
        'let system = import("zr.system");',
        '',
        'fn main(): int { return 0; }',
        '',
    ].join('\n');
    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: aliasImportsUri,
            languageId: 'zr',
            version: 1,
            text: aliasImportsText,
        },
    });
    await waitForDiagnosticsUri(client, aliasImportsUri, 'alias import diagnostics uri mismatch');
    const aliasImportActions = await client.request('textDocument/codeAction', {
        textDocument: { uri: aliasImportsUri },
        range: { start: { line: 0, character: 0 }, end: { line: 5, character: 0 } },
        context: { diagnostics: [], only: ['source.organizeImports'] },
    });
    assert(Array.isArray(aliasImportActions) && aliasImportActions.some((action) =>
        action &&
        action.kind === 'source.organizeImports' &&
        action.edit &&
        !action.edit.changes &&
        workspaceEditTextEdits(action.edit, aliasImportsUri).some((edit) =>
            edit.newText.includes('let math = import("zr.math");\nlet system = import("zr.system");'))),
    'textDocument/codeAction must organize alias imports');

    const duplicateAliasImportsUri = 'file:///c%3A/Users/test/workspace/%2Bzr_vm%2B/stdio-duplicate-alias-imports.zr';
    const duplicateAliasImportsText = [
        'let system = import("zr.system");',
        'let math = import("zr.math");',
        'let system = import("zr.network");',
        '',
        'fn main(): int { return 0; }',
        '',
    ].join('\n');
    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: duplicateAliasImportsUri,
            languageId: 'zr',
            version: 1,
            text: duplicateAliasImportsText,
        },
    });
    await waitForDiagnosticsUri(client,
        duplicateAliasImportsUri,
        'duplicate alias imports diagnostics uri mismatch');
    const duplicateAliasImportActions = await client.request('textDocument/codeAction', {
        textDocument: { uri: duplicateAliasImportsUri },
        range: { start: { line: 0, character: 0 }, end: { line: 5, character: 0 } },
        context: { diagnostics: [], only: ['source.organizeImports'] },
    });
    assert(Array.isArray(duplicateAliasImportActions) && duplicateAliasImportActions.some((action) =>
        action &&
        action.kind === 'source.organizeImports' &&
        action.edit &&
        !action.edit.changes &&
        workspaceEditTextEdits(action.edit, duplicateAliasImportsUri).some((edit) =>
            edit.newText.includes('let math = import("zr.math");\nlet system = import("zr.system");') &&
            !edit.newText.includes('zr.network'))),
    'textDocument/codeAction must remove duplicate alias imports while preserving the first alias binding');

    const unusedAliasImportsUri = 'file:///c%3A/Users/test/workspace/%2Bzr_vm%2B/stdio-unused-alias-imports.zr';
    const unusedAliasImportsText = [
        'let math = import("zr.math");',
        'let system = import("zr.system");',
        '',
        'fn main(value: int): int {',
        '    return math.abs(value);',
        '}',
        '',
    ].join('\n');
    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: unusedAliasImportsUri,
            languageId: 'zr',
            version: 1,
            text: unusedAliasImportsText,
        },
    });
    await waitForDiagnosticsUri(client,
        unusedAliasImportsUri,
        'unused alias imports diagnostics uri mismatch');
    const removeUnusedImportActions = await client.request('textDocument/codeAction', {
        textDocument: { uri: unusedAliasImportsUri },
        range: { start: { line: 0, character: 0 }, end: { line: 6, character: 0 } },
        context: { diagnostics: [], only: ['source.removeUnused'] },
    });
    assert(Array.isArray(removeUnusedImportActions) && removeUnusedImportActions.some((action) =>
        action &&
        action.kind === 'source.removeUnused' &&
        action.edit &&
        !action.edit.changes &&
        workspaceEditTextEdits(action.edit, unusedAliasImportsUri).some((edit) =>
            edit.newText === '' &&
            edit.range &&
            edit.range.start.line === 1 &&
            edit.range.end.line === 2) &&
        !workspaceEditTextEdits(action.edit, unusedAliasImportsUri).some((edit) =>
            edit.range &&
            edit.range.start.line === 0)),
    'textDocument/codeAction must remove unused alias imports without deleting used aliases');

    const semicolonFixturePath = path.join(watchedFixture.rootPath, 'semicolon_action.zr');
    const semicolonFixtureUri = pathToFileURL(semicolonFixturePath).toString();
    const semicolonFixtureText = 'var answer = 42\n';
    fs.writeFileSync(semicolonFixturePath, semicolonFixtureText);
    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: semicolonFixtureUri,
            languageId: 'zr',
            version: 1,
            text: semicolonFixtureText,
        },
    });
    const semicolonDiagnostics = await waitForDiagnosticsUri(
        client,
        semicolonFixtureUri,
        'semicolon quickfix diagnostics uri mismatch');
    assert(Array.isArray(semicolonDiagnostics.diagnostics) &&
        semicolonDiagnostics.diagnostics.some((diagnostic) =>
            diagnostic && diagnostic.code === 'missing_statement_semicolon'),
    'EOF variable declaration must publish missing_statement_semicolon before offering a quickfix');

    const quickFixActions = await client.request('textDocument/codeAction', {
        textDocument: { uri: semicolonFixtureUri },
        range: { start: { line: 0, character: 0 }, end: { line: 0, character: 0 } },
        context: { diagnostics: [], only: ['quickfix'] },
    });
    const semicolonAction = Array.isArray(quickFixActions)
        ? quickFixActions.find((action) =>
            action &&
            action.title === 'Insert missing semicolon' &&
            action.kind === 'quickfix')
        : null;
    assert(semicolonAction &&
        semicolonAction.data &&
        semicolonAction.data.snapshot &&
        semicolonAction.data.snapshot.version === 1 &&
        semicolonAction.data.snapshot.isOpenDocument === true &&
        semicolonAction.edit &&
        !semicolonAction.edit.changes &&
        workspaceEditTextEdits(semicolonAction.edit, semicolonFixtureUri).some((edit) =>
            edit.newText === ';' &&
            edit.range.start.line === 0 &&
            edit.range.start.character === 15 &&
            edit.range.end.line === 0 &&
            edit.range.end.character === 15),
    'textDocument/codeAction must serialize the exact structured semicolon fix and captured snapshot');

    const resolvedSemicolonAction = await client.request(
        'codeAction/resolve', semicolonAction);
    assert(resolvedSemicolonAction &&
        resolvedSemicolonAction.edit &&
        !resolvedSemicolonAction.disabled,
    'codeAction/resolve must preserve the semicolon edit while its snapshot is current');

    client.notify('textDocument/didChange', {
        textDocument: { uri: semicolonFixtureUri, version: 2 },
        contentChanges: [{ text: semicolonFixtureText }],
    });
    const staleResolvedSemicolonAction = await client.request(
        'codeAction/resolve', semicolonAction);
    assert(staleResolvedSemicolonAction &&
        !staleResolvedSemicolonAction.edit &&
        staleResolvedSemicolonAction.disabled &&
        staleResolvedSemicolonAction.disabled.reason ===
            'Document changed since this code action was computed',
    'codeAction/resolve must disable a stale structured semicolon fix');

    const freshSemicolonActions = await client.request('textDocument/codeAction', {
        textDocument: { uri: semicolonFixtureUri },
        range: { start: { line: 0, character: 0 }, end: { line: 0, character: 0 } },
        context: { diagnostics: [], only: ['quickfix'] },
    });
    const freshSemicolonAction = Array.isArray(freshSemicolonActions)
        ? freshSemicolonActions.find((action) =>
            action &&
            action.title === 'Insert missing semicolon' &&
            action.kind === 'quickfix')
        : null;
    assert(freshSemicolonAction &&
        freshSemicolonAction.data &&
        freshSemicolonAction.data.snapshot &&
        freshSemicolonAction.data.snapshot.version === 2,
    'fresh semicolon code action must recapture the changed document version');

    const fixedSemicolonFixtureText = 'var answer = 42;\n';
    client.notify('textDocument/didChange', {
        textDocument: { uri: semicolonFixtureUri, version: 3 },
        contentChanges: [{ text: fixedSemicolonFixtureText }],
    });
    const fixedSemicolonDiagnostics = await waitForDiagnosticsUriVersion(
        client,
        semicolonFixtureUri,
        3,
        'fixed semicolon diagnostics uri mismatch');
    assert(Array.isArray(fixedSemicolonDiagnostics.diagnostics) &&
        !fixedSemicolonDiagnostics.diagnostics.some((diagnostic) =>
            diagnostic && diagnostic.code === 'missing_statement_semicolon'),
    'applying the exact semicolon edit must clear the parser diagnostic after rebind');

    const missingImportFixturePath = path.join(watchedFixture.rootPath, 'missing_import_action.zr');
    const missingImportFixtureUri = pathToFileURL(missingImportFixturePath).toString();
    const missingImportFixtureText = [
        'fn main(value: int): int {',
        '    return math.abs(value);',
        '}',
        '',
    ].join('\n');
    fs.writeFileSync(missingImportFixturePath, missingImportFixtureText);
    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: missingImportFixtureUri,
            languageId: 'zr',
            version: 1,
            text: missingImportFixtureText,
        },
    });
    await waitForDiagnosticsUri(client, missingImportFixtureUri, 'missing import quickfix diagnostics uri mismatch');

    const missingImportActions = await client.request('textDocument/codeAction', {
        textDocument: { uri: missingImportFixtureUri },
        range: { start: { line: 1, character: 11 }, end: { line: 1, character: 15 } },
        context: { diagnostics: [], only: ['quickfix'] },
    });
    assert(Array.isArray(missingImportActions) && missingImportActions.some((action) =>
        action &&
        action.kind === 'quickfix' &&
        action.title === 'Import zr.math as math' &&
        action.edit &&
        !action.edit.changes &&
        workspaceEditTextEdits(action.edit, missingImportFixtureUri).some((edit) =>
            edit.newText === 'let math = import("zr.math");\n')),
    'textDocument/codeAction must return a missing native import quickfix edit');

    const rangeMissingImportFixturePath =
        path.join(importDiagnosticsFixture.rootPath, 'src', 'range_missing_import_action.zr');
    const rangeMissingImportFixtureUri = pathToFileURL(rangeMissingImportFixturePath).toString();
    const rangeMissingImportFixtureText = [
        'fn main(value: int): int {',
        '    return system.print(math.abs(value));',
        '}',
        '',
    ].join('\n');
    fs.writeFileSync(rangeMissingImportFixturePath, rangeMissingImportFixtureText);
    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: rangeMissingImportFixtureUri,
            languageId: 'zr',
            version: 1,
            text: rangeMissingImportFixtureText,
        },
    });
    await waitForDiagnosticsUri(client,
        rangeMissingImportFixtureUri,
        'range missing import quickfix diagnostics uri mismatch');

    const rangeMissingImportActions = await client.request('textDocument/codeAction', {
        textDocument: { uri: rangeMissingImportFixtureUri },
        range: { start: { line: 1, character: 24 }, end: { line: 1, character: 28 } },
        context: { diagnostics: [], only: ['quickfix'] },
    });
    assert(Array.isArray(rangeMissingImportActions) && rangeMissingImportActions.some((action) =>
        action &&
        action.kind === 'quickfix' &&
        action.title === 'Import zr.math as math' &&
        action.edit &&
        !action.edit.changes &&
        workspaceEditTextEdits(action.edit, rangeMissingImportFixtureUri).some((edit) =>
            edit.newText === 'let math = import("zr.math");\n')) &&
        !rangeMissingImportActions.some((action) =>
            action && action.title === 'Import zr.system as system'),
    'textDocument/codeAction must use the requested range for missing import quickfixes');

    const missingProjectImportModulePath = path.join(importDiagnosticsFixture.rootPath, 'src', 'helper.zr');
    const missingProjectImportFixturePath =
        path.join(importDiagnosticsFixture.rootPath, 'src', 'missing_project_import_action.zr');
    const missingProjectImportFixtureUri = pathToFileURL(missingProjectImportFixturePath).toString();
    const missingProjectImportFixtureText = [
        'fn main(): int {',
        '    return helper.present;',
        '}',
        '',
    ].join('\n');
    fs.writeFileSync(missingProjectImportModulePath, [
        'pub var present = 1;',
        '',
    ].join('\n'));
    fs.writeFileSync(missingProjectImportFixturePath, missingProjectImportFixtureText);
    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: missingProjectImportFixtureUri,
            languageId: 'zr',
            version: 1,
            text: missingProjectImportFixtureText,
        },
    });
    await waitForDiagnosticsUri(client,
        missingProjectImportFixtureUri,
        'missing project import quickfix diagnostics uri mismatch');

    const missingProjectImportActions = await client.request('textDocument/codeAction', {
        textDocument: { uri: missingProjectImportFixtureUri },
        range: { start: { line: 1, character: 11 }, end: { line: 1, character: 17 } },
        context: { diagnostics: [], only: ['quickfix'] },
    });
    assert(Array.isArray(missingProjectImportActions) && missingProjectImportActions.some((action) =>
        action &&
        action.kind === 'quickfix' &&
        action.title === 'Import helper as helper' &&
        action.edit &&
        !action.edit.changes &&
        workspaceEditTextEdits(action.edit, missingProjectImportFixtureUri).some((edit) =>
            edit.newText === 'let helper = import("helper");\n')),
    'textDocument/codeAction must return a missing project import quickfix edit');

    const sourceOnlyActions = await client.request('textDocument/codeAction', {
        textDocument: { uri: semicolonFixtureUri },
        range: { start: { line: 0, character: 0 }, end: { line: 0, character: 0 } },
        context: { diagnostics: [], only: ['source.organizeImports'] },
    });
    assert(Array.isArray(sourceOnlyActions) &&
        !sourceOnlyActions.some((action) => action && action.kind === 'quickfix'),
    'textDocument/codeAction must honor context.only filters');

    const declaration = await client.request('textDocument/declaration', {
        textDocument: { uri: genericUri },
        position: genericDefinitionPosition,
    });
    assert(Array.isArray(declaration),
        'textDocument/declaration must return an array');

    const callHierarchyItems = await client.request('textDocument/prepareCallHierarchy', {
        textDocument: { uri: genericUri },
        position: genericCallPosition,
    });
    assert(Array.isArray(callHierarchyItems),
        'textDocument/prepareCallHierarchy must return an array');
    const outgoingCalls = await client.request('callHierarchy/outgoingCalls', {
        item: callHierarchyItems[0] || {
            name: 'shape',
            kind: 6,
            uri: genericUri,
            range: { start: genericCallPosition, end: genericCallPosition },
            selectionRange: { start: genericCallPosition, end: genericCallPosition },
        },
    });
    assert(Array.isArray(outgoingCalls),
        'callHierarchy/outgoingCalls must return an array');
    const incomingCalls = await client.request('callHierarchy/incomingCalls', {
        item: callHierarchyItems[0] || {
            name: 'shape',
            kind: 6,
            uri: genericUri,
            range: { start: genericCallPosition, end: genericCallPosition },
            selectionRange: { start: genericCallPosition, end: genericCallPosition },
        },
    });
    assert(Array.isArray(incomingCalls),
        'callHierarchy/incomingCalls must return an array');

    const hierarchyFixturePath = path.join(watchedFixture.rootPath, 'src', 'call_hierarchy.zr');
    const hierarchyFixtureUri = pathToFileURL(hierarchyFixturePath).toString();
    const hierarchyFixtureText = [
        'fn helper(value: int): int {',
        '    return value;',
        '}',
        '',
        'fn run(value: int): int {',
        '    return helper(value);',
        '}',
        '',
    ].join('\n');
    fs.writeFileSync(hierarchyFixturePath, hierarchyFixtureText);
    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: hierarchyFixtureUri,
            languageId: 'zr',
            version: 1,
            text: hierarchyFixtureText,
        },
    });
    await waitForDiagnosticsUri(client, hierarchyFixtureUri, 'call hierarchy fixture diagnostics uri mismatch');
    const hierarchyCodeLens = await client.request('textDocument/codeLens', {
        textDocument: { uri: hierarchyFixtureUri },
    });
    assert(Array.isArray(hierarchyCodeLens) && hierarchyCodeLens.some((lens) =>
        lens &&
        lens.command &&
        lens.command.title === '1 reference' &&
        lens.command.command === 'zr.showReferences' &&
        Array.isArray(lens.command.arguments) &&
        lens.command.arguments[0] === hierarchyFixtureUri &&
        lens.command.arguments[1] &&
        lens.command.arguments[1].line === 0 &&
        lens.command.arguments[1].character === 3),
    'textDocument/codeLens must expose callable reference counts with a reference command');
    const hierarchyRunPosition = findPosition(hierarchyFixtureText, 'run(value', 0, 1);
    const preparedRunItems = await client.request('textDocument/prepareCallHierarchy', {
        textDocument: { uri: hierarchyFixtureUri },
        position: hierarchyRunPosition,
    });
    assert(Array.isArray(preparedRunItems),
        'textDocument/prepareCallHierarchy must return an array in the call fixture');
    const preparedRunOutgoing = await client.request('callHierarchy/outgoingCalls', {
        item: preparedRunItems[0] || {
            name: 'run',
            kind: 12,
            uri: hierarchyFixtureUri,
            range: {
                start: { line: 4, character: 0 },
                end: { line: 6, character: 1 },
            },
            selectionRange: {
                start: { line: 4, character: 5 },
                end: { line: 4, character: 8 },
            },
        },
    });
    assert(Array.isArray(preparedRunOutgoing) && preparedRunOutgoing.some((call) =>
        call &&
        call.to &&
        call.to.name === 'helper' &&
        Array.isArray(call.fromRanges) &&
        call.fromRanges.length > 0),
    'callHierarchy/outgoingCalls must return direct helper() calls over stdio');

    const hierarchyHelperPosition = findPosition(hierarchyFixtureText, 'helper(value', 0, 1);
    const preparedHelperItems = await client.request('textDocument/prepareCallHierarchy', {
        textDocument: { uri: hierarchyFixtureUri },
        position: hierarchyHelperPosition,
    });
    assert(Array.isArray(preparedHelperItems),
        'textDocument/prepareCallHierarchy must return helper in the call fixture');
    const preparedHelperIncoming = await client.request('callHierarchy/incomingCalls', {
        item: preparedHelperItems[0] || {
            name: 'helper',
            kind: 12,
            uri: hierarchyFixtureUri,
            range: {
                start: { line: 0, character: 0 },
                end: { line: 2, character: 1 },
            },
            selectionRange: {
                start: { line: 0, character: 5 },
                end: { line: 0, character: 11 },
            },
        },
    });
    assert(Array.isArray(preparedHelperIncoming) && preparedHelperIncoming.some((call) =>
        call &&
        call.from &&
        call.from.name === 'run' &&
        Array.isArray(call.fromRanges) &&
        call.fromRanges.length > 0),
    'callHierarchy/incomingCalls must return direct run() callers over stdio');

    const typeHierarchyItems = await client.request('textDocument/prepareTypeHierarchy', {
        textDocument: { uri: genericUri },
        position: genericDefinitionPosition,
    });
    assert(Array.isArray(typeHierarchyItems),
        'textDocument/prepareTypeHierarchy must return an array');
    const supertypes = await client.request('typeHierarchy/supertypes', {
        item: typeHierarchyItems[0] || {
            name: 'Derived',
            kind: 5,
            uri: genericUri,
            range: { start: genericDefinitionPosition, end: genericDefinitionPosition },
            selectionRange: { start: genericDefinitionPosition, end: genericDefinitionPosition },
        },
    });
    assert(Array.isArray(supertypes),
        'typeHierarchy/supertypes must return an array');
    const subtypes = await client.request('typeHierarchy/subtypes', {
        item: typeHierarchyItems[0] || {
            name: 'Derived',
            kind: 5,
            uri: genericUri,
            range: { start: genericDefinitionPosition, end: genericDefinitionPosition },
            selectionRange: { start: genericDefinitionPosition, end: genericDefinitionPosition },
        },
    });
    assert(Array.isArray(subtypes),
        'typeHierarchy/subtypes must return an array');

    const pullDiagnostics = await client.request('textDocument/diagnostic', {
        textDocument: { uri: genericUri },
    });
    assert(pullDiagnostics &&
        pullDiagnostics.kind === 'full' &&
        Array.isArray(pullDiagnostics.items) &&
        typeof pullDiagnostics.resultId === 'string' &&
        pullDiagnostics.resultId.length > 0,
    'textDocument/diagnostic must return a full diagnostic report with resultId');
    const unchangedDiagnostics = await client.request('textDocument/diagnostic', {
        textDocument: { uri: genericUri },
        previousResultId: pullDiagnostics.resultId,
    });
    assert(unchangedDiagnostics &&
        unchangedDiagnostics.kind === 'unchanged' &&
        unchangedDiagnostics.resultId === pullDiagnostics.resultId &&
        !Object.prototype.hasOwnProperty.call(unchangedDiagnostics, 'items'),
    'textDocument/diagnostic must return unchanged reports for matching previousResultId');

    const workspaceDiagnostics = await client.request('workspace/diagnostic', {});
    assert(workspaceDiagnostics && Array.isArray(workspaceDiagnostics.items),
        'workspace/diagnostic must return a workspace diagnostic report');
    assert(workspaceDiagnostics.items.some((report) =>
        report &&
        report.kind === 'full' &&
        report.version === 1 &&
        typeof report.resultId === 'string' &&
        report.resultId.length > 0 &&
        diagnosticRelatedUriMatches(genericUri, report.uri) &&
        Array.isArray(report.items)),
    'workspace/diagnostic must include opened document diagnostic reports');
    const genericWorkspaceReport = workspaceDiagnostics.items.find((report) =>
        report &&
        report.kind === 'full' &&
        diagnosticRelatedUriMatches(genericUri, report.uri) &&
        typeof report.resultId === 'string');
    assert(genericWorkspaceReport && genericWorkspaceReport.version === 1,
        'workspace/diagnostic full reports must include the document version');

    const cancellationStartedAt = process.hrtime.bigint();
    const cancelledWorkspaceDiagnostics = client.requestWithId('workspace/diagnostic', {});
    const queuedWorkspaceDiagnostics = client.requestWithId('workspace/diagnostic', {});
    client.notify('$/cancelRequest', { id: queuedWorkspaceDiagnostics.id });
    client.notify('$/cancelRequest', { id: cancelledWorkspaceDiagnostics.id });
    const cancellationErrors = await Promise.all([
        cancelledWorkspaceDiagnostics.promise,
        queuedWorkspaceDiagnostics.promise,
    ].map(async (promise) => {
        try {
            await promise;
            return null;
        } catch (error) {
            return JSON.parse(error.message);
        }
    }));
    assert(cancellationErrors.every((error) => error && error.code === -32800),
        'cancelled workspace/diagnostic requests must return RequestCancelled without stale success results');
    const cancellationElapsedMs = Number(process.hrtime.bigint() - cancellationStartedAt) / 1e6;
    assert(cancellationElapsedMs < 50,
        `workspace/diagnostic cancellation must be observed within 50ms, took ${cancellationElapsedMs.toFixed(2)}ms`);

    const unchangedWorkspaceDiagnostics = await client.request('workspace/diagnostic', {
        previousResultIds: [
            { uri: genericWorkspaceReport.uri, value: genericWorkspaceReport.resultId },
        ],
    });
    assert(unchangedWorkspaceDiagnostics &&
        Array.isArray(unchangedWorkspaceDiagnostics.items) &&
        unchangedWorkspaceDiagnostics.items.some((report) =>
            report &&
            report.uri === genericWorkspaceReport.uri &&
            report.kind === 'unchanged' &&
            report.version === genericWorkspaceReport.version &&
            report.resultId === genericWorkspaceReport.resultId &&
            !Object.prototype.hasOwnProperty.call(report, 'items')),
    'workspace/diagnostic must return unchanged reports for matching previousResultIds');

    const staleWorkspaceDiagnostics = client.requestWithId('workspace/diagnostic', {});
    client.notify('textDocument/didChange', {
        textDocument: { uri: genericUri, version: 2 },
        contentChanges: [{ text: `${genericText}\n// generation two\n` }],
    });
    let contentModifiedError = null;
    try {
        await staleWorkspaceDiagnostics.promise;
    } catch (error) {
        contentModifiedError = JSON.parse(error.message);
    }
    assert(contentModifiedError && contentModifiedError.code === -32801,
        'workspace/diagnostic must reject a response made stale by didChange');
    const generationTwoDiagnostics = await waitForDiagnosticsUriVersion(
        client,
        genericUri,
        2,
        'didChange after an active workspace request must publish diagnostics for version two');
    assert(Array.isArray(generationTwoDiagnostics.diagnostics),
        'generation two diagnostics must remain a structured array');
    const generationTwoWorkspaceDiagnostics = await client.request('workspace/diagnostic', {});
    assert(generationTwoWorkspaceDiagnostics &&
        Array.isArray(generationTwoWorkspaceDiagnostics.items) &&
        generationTwoWorkspaceDiagnostics.items.some((report) =>
            report && diagnosticRelatedUriMatches(genericUri, report.uri) && report.version === 2),
    'workspace/diagnostic after didChange must only publish the current document version');

    const rapidStaleUri =
        'file:///c%3A/Users/test/workspace/%2Bzr_vm%2B/stdio-rapid-stale-churn.zr';
    const rapidStaleText = [
        'fn rapid_stale(value: int): int {',
        '    return value;',
        '}',
        '',
    ].join('\n');
    let rapidStaleVersion = 1;
    for (let iteration = 0; iteration < 100; iteration += 1) {
        const openedVersion = rapidStaleVersion;
        const changedVersion = openedVersion + 1;
        client.notify('textDocument/didOpen', {
            textDocument: {
                uri: rapidStaleUri,
                languageId: 'zr',
                version: openedVersion,
                text: rapidStaleText + '// open generation ' + openedVersion + '\n',
            },
        });
        await waitForDiagnosticsUriVersion(
            client,
            rapidStaleUri,
            openedVersion,
            'rapid stale churn didOpen must publish diagnostics for its exact version');

        const cancelledDiagnostics = client.requestWithId('workspace/diagnostic', {});
        const queuedCancelledDiagnostics = client.requestWithId('workspace/diagnostic', {});
        client.notify('$/cancelRequest', { id: queuedCancelledDiagnostics.id });
        client.notify('$/cancelRequest', { id: cancelledDiagnostics.id });
        const rapidCancellationOutcomes = await Promise.all([
            cancelledDiagnostics.promise,
            queuedCancelledDiagnostics.promise,
        ].map(awaitLspRequestOutcome));
        assert(rapidCancellationOutcomes.every((outcome) =>
            (outcome.error && outcome.error.code === -32800) ||
            workspaceDiagnosticsHasUriVersion(outcome.result, rapidStaleUri, openedVersion)),
        'rapid stale churn cancellation must return RequestCancelled or the exact open snapshot');

        const staleAfterChange = client.requestWithId('workspace/diagnostic', {});
        client.notify('textDocument/didChange', {
            textDocument: { uri: rapidStaleUri, version: changedVersion },
            contentChanges: [{ text: rapidStaleText + '// changed generation ' + changedVersion + '\n' }],
        });
        const staleChangeOutcome = await awaitLspRequestOutcome(staleAfterChange.promise);
        assert((staleChangeOutcome.error && staleChangeOutcome.error.code === -32801) ||
            workspaceDiagnosticsHasUriVersion(
                staleChangeOutcome.result,
                rapidStaleUri,
                openedVersion),
        'rapid stale churn didChange must reject the request or return its exact open snapshot');
        await waitForDiagnosticsUriVersion(
            client,
            rapidStaleUri,
            changedVersion,
            'rapid stale churn didChange must publish diagnostics for its exact replacement version');

        const staleAfterClose = client.requestWithId('workspace/diagnostic', {});
        client.notify('textDocument/didClose', {
            textDocument: { uri: rapidStaleUri },
        });
        const staleCloseOutcome = await awaitLspRequestOutcome(staleAfterClose.promise);
        assert((staleCloseOutcome.error && staleCloseOutcome.error.code === -32801) ||
            workspaceDiagnosticsHasUriVersion(
                staleCloseOutcome.result,
                rapidStaleUri,
                changedVersion),
        'rapid stale churn didClose must reject the request or return its exact changed snapshot');
        const rapidCloseDiagnostics = await waitForDiagnosticsUri(
            client,
            rapidStaleUri,
            'rapid stale churn didClose diagnostics uri mismatch');
        assert(Array.isArray(rapidCloseDiagnostics.diagnostics) && rapidCloseDiagnostics.diagnostics.length === 0,
            'rapid stale churn didClose must clear diagnostics before the next open generation');
        rapidStaleVersion = changedVersion + 1;
    }

    client.notify('workspace/didChangeWatchedFiles', {
        changes: [
            { uri: watchedBinaryFixture.binaryUri, type: 2 },
        ],
    });

    const watchedBootstrapSymbols = await client.request('workspace/symbol', {
        query: 'merged',
    });
    assert(Array.isArray(watchedBootstrapSymbols) && watchedBootstrapSymbols.some((item) =>
        item &&
        item.location &&
        diagnosticRelatedUriMatches(watchedBinaryFixture.mainUri, item.location.uri) &&
        item.name === 'merged'),
    'workspace/didChangeWatchedFiles binary metadata change must bootstrap unopened project indexes');

    const watchedBinaryText = fs.readFileSync(watchedBinaryFixture.mainPath, 'utf8');
    const watchedBinaryImportDefinitionPosition = findPosition(watchedBinaryText, '"graph_binary_stage"', 0, 1);
    const watchedBinaryHoverPosition = findPosition(watchedBinaryText, 'binarySeed', 0, 0);

    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: watchedBinaryFixture.mainUri,
            languageId: 'zr',
            version: 1,
            text: watchedBinaryText,
        },
    });

    const watchedBinaryDiagnostics = await waitForDiagnosticsUri(
        client,
        watchedBinaryFixture.mainUri,
        'binary watched metadata diagnostics uri mismatch');
    assert(Array.isArray(watchedBinaryDiagnostics.diagnostics) && watchedBinaryDiagnostics.diagnostics.length === 0,
        'binary watched metadata fixture should open without diagnostics');

    const watchedBinaryImportDefinition = await client.request('textDocument/definition', {
        textDocument: { uri: watchedBinaryFixture.mainUri },
        position: watchedBinaryImportDefinitionPosition,
    });
    assert(Array.isArray(watchedBinaryImportDefinition) && watchedBinaryImportDefinition.some((location) =>
        location &&
        diagnosticRelatedUriMatches(watchedBinaryFixture.binaryUri, location.uri) &&
        location.range &&
        location.range.start &&
        location.range.start.line === 0 &&
        location.range.start.character === 0),
    'binary import literal definition should navigate to the binary metadata file entry');

    const watchedBinaryMemberDefinition = await client.request('textDocument/definition', {
        textDocument: { uri: watchedBinaryFixture.mainUri },
        position: watchedBinaryHoverPosition,
    });
    assert(Array.isArray(watchedBinaryMemberDefinition) && watchedBinaryMemberDefinition.some((location) =>
        location &&
        diagnosticRelatedUriMatches(watchedBinaryFixture.binaryUri, location.uri) &&
        location.range),
    'binary imported member definition should navigate to the binary metadata declaration');

    const watchedBinaryMemberReferences = await client.request('textDocument/references', {
        textDocument: { uri: watchedBinaryFixture.mainUri },
        position: watchedBinaryHoverPosition,
        context: { includeDeclaration: true },
    });
    assert(Array.isArray(watchedBinaryMemberReferences) && watchedBinaryMemberReferences.some((location) =>
        location &&
        diagnosticRelatedUriMatches(watchedBinaryFixture.mainUri, location.uri) &&
        location.range &&
        location.range.start &&
        location.range.end &&
        location.range.start.line === watchedBinaryHoverPosition.line &&
        location.range.start.character === watchedBinaryHoverPosition.character &&
        location.range.end.line === watchedBinaryHoverPosition.line &&
        location.range.end.character === watchedBinaryHoverPosition.character + 'binarySeed'.length) &&
        watchedBinaryMemberReferences.some((location) =>
            location &&
            diagnosticRelatedUriMatches(watchedBinaryFixture.binaryUri, location.uri) &&
            location.range),
    'binary imported member references should include the current project usage and the binary metadata declaration');

    const watchedBinaryHighlights = await client.request('textDocument/documentHighlight', {
        textDocument: { uri: watchedBinaryFixture.mainUri },
        position: watchedBinaryHoverPosition,
    });
    assert(Array.isArray(watchedBinaryHighlights) && watchedBinaryHighlights.some((highlight) =>
        highlight &&
        highlight.range &&
        highlight.range.start &&
        highlight.range.end &&
        highlight.range.start.line === watchedBinaryHoverPosition.line &&
        highlight.range.start.character === watchedBinaryHoverPosition.character &&
        highlight.range.end.line === watchedBinaryHoverPosition.line &&
        highlight.range.end.character === watchedBinaryHoverPosition.character + 'binarySeed'.length),
    'binary imported member documentHighlight should include the current document usage');

    const watchedBinaryHoverBefore = await client.request('textDocument/hover', {
        textDocument: { uri: watchedBinaryFixture.mainUri },
        position: watchedBinaryHoverPosition,
    });
    assert(watchedBinaryHoverBefore &&
        watchedBinaryHoverBefore.contents &&
        watchedBinaryHoverBefore.contents.value.includes('Type: int') &&
        watchedBinaryHoverBefore.contents.value.includes('Source: binary metadata'),
    'binary metadata hover should resolve the initial inferred return type');

    fs.writeFileSync(watchedBinaryFixture.binaryPath, fs.readFileSync(watchedBinaryFixture.binaryPath));
    client.notify('workspace/didChangeWatchedFiles', {
        changes: [
            { uri: watchedBinaryFixture.binaryUri, type: 2 },
        ],
    });

    const watchedBinaryHoverAfter = await client.request('textDocument/hover', {
        textDocument: { uri: watchedBinaryFixture.mainUri },
        position: watchedBinaryHoverPosition,
    });
    assert(watchedBinaryHoverAfter &&
        watchedBinaryHoverAfter.contents &&
        watchedBinaryHoverAfter.contents.value.includes('Type: int') &&
        watchedBinaryHoverAfter.contents.value.includes('Source: binary metadata'),
    'workspace/didChangeWatchedFiles binary refresh must keep open analyzers usable for imported binary metadata');

    const staleCloseWorkspaceDiagnostics = client.requestWithId('workspace/diagnostic', {});
    client.notify('textDocument/didClose', {
        textDocument: {
            uri: genericUri,
        },
    });

    let closeContentModifiedError = null;
    try {
        await staleCloseWorkspaceDiagnostics.promise;
    } catch (error) {
        closeContentModifiedError = JSON.parse(error.message);
    }
    assert(closeContentModifiedError && closeContentModifiedError.code === -32801,
        'workspace/diagnostic must reject a response made stale by didClose');

    const genericCloseDiagnostics = await client.waitForNotification('textDocument/publishDiagnostics');
    assert(genericCloseDiagnostics.uri === genericUri, 'generic didClose diagnostics uri mismatch');
    assert(Array.isArray(genericCloseDiagnostics.diagnostics) && genericCloseDiagnostics.diagnostics.length === 0,
        'generic didClose must clear diagnostics');

    client.notify('workspace/didChangeWatchedFiles', {
        changes: [
            { uri: watchedFixture.projectUri, type: 1 },
        ],
    });

    const watchedCreateSymbols = await client.request('workspace/symbol', {
        query: 'watched_before_refresh',
    });
    assert(Array.isArray(watchedCreateSymbols) && watchedCreateSymbols.some((item) =>
        item &&
        item.location &&
        diagnosticRelatedUriMatches(watchedFixture.mainUri, item.location.uri) &&
        item.name === 'watched_before_refresh'),
    'workspace/didChangeWatchedFiles create must index unopened project sources');

    const watchedProjectLinks = await client.request('textDocument/documentLink', {
        textDocument: { uri: watchedFixture.projectUri },
    });
    assert(Array.isArray(watchedProjectLinks) &&
        watchedProjectLinks.some((link) => link && typeof link.target === 'string' && link.target.endsWith('/src')) &&
        watchedProjectLinks.some((link) => link && typeof link.target === 'string' && link.target.endsWith('/src/main.zr')),
    'workspace/didChangeWatchedFiles create must make unopened project document links available');

    const watchedOpenedPath = path.join(watchedFixture.rootPath, 'src', 'opened_after_project.zr');
    const watchedOpenedUri = pathToFileURL(watchedOpenedPath).toString();
    const watchedOpenedText = [
        'module opened_after_project;',
        '',
        'fn opened_project_helper(value: int): int {',
        '    return value;',
        '}',
        '',
        'pub fn opened_project_entry(): int {',
        '    return opened_project_helper(7);',
        '}',
        '',
    ].join('\n');
    fs.writeFileSync(watchedOpenedPath, watchedOpenedText);
    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: watchedOpenedUri,
            languageId: 'zr',
            version: 1,
            text: watchedOpenedText,
        },
    });
    await waitForDiagnosticsUri(
        client,
        watchedOpenedUri,
        'watched project opened source diagnostics uri mismatch');
    const watchedOpenedSymbols = await client.request('workspace/symbol', {
        query: 'opened_project_entry',
    });
    assert(Array.isArray(watchedOpenedSymbols) && watchedOpenedSymbols.some((item) =>
        item &&
        item.location &&
        diagnosticRelatedUriMatches(watchedOpenedUri, item.location.uri) &&
        item.name === 'opened_project_entry'),
    'workspace/symbol must include project source opened after watched project indexing');

    const watchedOpenedCodeLens = await client.request('textDocument/codeLens', {
        textDocument: { uri: watchedOpenedUri },
    });
    assert(Array.isArray(watchedOpenedCodeLens) && watchedOpenedCodeLens.some((lens) =>
        lens &&
        lens.command &&
        lens.command.title === '1 reference' &&
        lens.command.command === 'zr.showReferences' &&
        Array.isArray(lens.command.arguments) &&
        lens.command.arguments[0] === watchedOpenedUri),
    'textDocument/codeLens must work for project source opened after watched project indexing');

    const watchedOpenedEntryPosition = findPosition(watchedOpenedText, 'opened_project_entry', 0, 1);
    const watchedOpenedCallItems = await client.request('textDocument/prepareCallHierarchy', {
        textDocument: { uri: watchedOpenedUri },
        position: watchedOpenedEntryPosition,
    });
    assert(Array.isArray(watchedOpenedCallItems) && watchedOpenedCallItems.length > 0,
        'textDocument/prepareCallHierarchy must return an item for project source opened after watched project indexing');
    const watchedOpenedOutgoing = await client.request('callHierarchy/outgoingCalls', {
        item: watchedOpenedCallItems[0],
    });
    assert(Array.isArray(watchedOpenedOutgoing) && watchedOpenedOutgoing.some((call) =>
        call &&
        call.to &&
        call.to.name === 'opened_project_helper' &&
        Array.isArray(call.fromRanges) &&
        call.fromRanges.length > 0),
    'callHierarchy/outgoingCalls must work for project source opened after watched project indexing');

    const watchedOpenOverlayPath = path.join(watchedFixture.rootPath, 'src', 'opened_overlay.zr');
    const watchedOpenOverlayRequestUri = pathToFileURL(watchedOpenOverlayPath).toString();
    const watchedOpenOverlayUri = uriWithEncodedWindowsDrive(watchedOpenOverlayRequestUri);
    const watchedOpenOverlayText = [
        'module opened_overlay;',
        '',
        'class WatchedOpenOverlay {',
        '    pub @constructor() {',
        '    }',
        '',
        '    pub fn total(): int {',
        '        return 42;',
        '    }',
        '}',
        '',
        'fn watched_open_overlay_entry(): int {',
        '    let overlay: WatchedOpenOverlay = new WatchedOpenOverlay();',
        '    return overlay.total();',
        '}',
        '',
    ].join('\n');
    fs.writeFileSync(watchedOpenOverlayPath, watchedOpenOverlayText);
    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: watchedOpenOverlayUri,
            languageId: 'zr',
            version: 1,
            text: watchedOpenOverlayText,
        },
    });
    await waitForDiagnosticsUri(
        client,
        watchedOpenOverlayUri,
        'watched open overlay diagnostics uri mismatch');
    client.notify('workspace/didChangeWatchedFiles', {
        changes: [
            { uri: watchedOpenOverlayUri, type: 2 },
        ],
    });
    const watchedOpenOverlayDefinition = await client.request('textDocument/definition', {
        textDocument: { uri: watchedOpenOverlayRequestUri },
        position: findPosition(watchedOpenOverlayText, 'overlay.total()', 0, 'overlay.'.length),
    });
    const watchedOpenOverlayTotalPosition = findPosition(watchedOpenOverlayText, 'total()', 0);
    assert(Array.isArray(watchedOpenOverlayDefinition) && watchedOpenOverlayDefinition.some((item) =>
        item &&
        diagnosticRelatedUriMatches(watchedOpenOverlayUri, item.uri) &&
        item.range &&
        item.range.start.line === watchedOpenOverlayTotalPosition.line &&
        item.range.start.character === watchedOpenOverlayTotalPosition.character &&
        item.range.end.line === watchedOpenOverlayTotalPosition.line &&
        item.range.end.character === watchedOpenOverlayTotalPosition.character + 'total'.length),
    'workspace watcher updates must preserve an open document overlay and its member definitions');
    const watchedOpenOverlaySymbols = await client.request('textDocument/documentSymbol', {
        textDocument: { uri: watchedOpenOverlayRequestUri },
    });
    assert(Array.isArray(watchedOpenOverlaySymbols) && watchedOpenOverlaySymbols.some((item) =>
        item && item.name === 'WatchedOpenOverlay'),
    'document symbols must resolve encoded Windows document URIs through their native file path');

    fs.writeFileSync(watchedFixture.mainPath, [
        'module main;',
        '',
        'pub fn watched_after_refresh(): int {',
        '    return 2;',
        '}',
        '',
    ].join('\n'));
    client.notify('workspace/didChangeWatchedFiles', {
        changes: [
            { uri: watchedFixture.mainUri, type: 2 },
        ],
    });

    const watchedChangeDiagnostics = await waitForDiagnosticsUri(
        client,
        watchedFixture.mainUri,
        'workspace/didChangeWatchedFiles source change diagnostics uri mismatch');
    assert(Array.isArray(watchedChangeDiagnostics.diagnostics) && watchedChangeDiagnostics.diagnostics.length === 0,
        'workspace/didChangeWatchedFiles source change should publish empty diagnostics');

    const watchedUpdatedSymbols = await client.request('workspace/symbol', {
        query: 'watched_after_refresh',
    });
    assert(Array.isArray(watchedUpdatedSymbols) && watchedUpdatedSymbols.some((item) =>
        item &&
        item.location &&
        diagnosticRelatedUriMatches(watchedFixture.mainUri, item.location.uri) &&
        item.name === 'watched_after_refresh'),
    'workspace/didChangeWatchedFiles change must refresh unopened source analyzers');

    const watchedRemovedOldSymbols = await client.request('workspace/symbol', {
        query: 'watched_before_refresh',
    });
    assert(Array.isArray(watchedRemovedOldSymbols) && watchedRemovedOldSymbols.length === 0,
        'workspace/didChangeWatchedFiles change must replace stale unopened source symbols');

    fs.unlinkSync(watchedFixture.projectPath);
    client.notify('workspace/didChangeWatchedFiles', {
        changes: [
            { uri: watchedFixture.projectUri, type: 3 },
        ],
    });

    const watchedDeleteDiagnostics = await waitForDiagnosticsUri(
        client,
        watchedFixture.projectUri,
        'workspace/didChangeWatchedFiles project delete diagnostics uri mismatch');
    assert(Array.isArray(watchedDeleteDiagnostics.diagnostics) && watchedDeleteDiagnostics.diagnostics.length === 0,
        'workspace/didChangeWatchedFiles project delete must clear diagnostics');

    const watchedDeletedSymbols = await client.request('workspace/symbol', {
        query: 'watched_after_refresh',
    });
    assert(Array.isArray(watchedDeletedSymbols) && watchedDeletedSymbols.length === 0,
        'workspace/didChangeWatchedFiles delete must clear the removed project index');

    const willCreateFiles = await client.request('workspace/willCreateFiles', {
        files: [
            { uri: fileOperationsFixture.projectUri },
        ],
    });
    assert(willCreateFiles === null, 'workspace/willCreateFiles must return null when no edits are needed');
    client.notify('workspace/didCreateFiles', {
        files: [
            { uri: fileOperationsFixture.projectUri },
        ],
    });
    const fileOperationCreateDiagnostics = await waitForDiagnosticsUri(
        client,
        fileOperationsFixture.projectUri,
        'workspace/didCreateFiles project diagnostics uri mismatch');
    assert(Array.isArray(fileOperationCreateDiagnostics.diagnostics),
        'workspace/didCreateFiles must publish project diagnostics');
    const fileOperationCreateSymbols = await client.request('workspace/symbol', {
        query: 'watched_before_refresh',
    });
    assert(Array.isArray(fileOperationCreateSymbols) && fileOperationCreateSymbols.some((item) =>
        item &&
        item.location &&
        diagnosticRelatedUriMatches(fileOperationsFixture.mainUri, item.location.uri) &&
        item.name === 'watched_before_refresh'),
    'workspace/didCreateFiles must index newly created unopened project sources');

    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: moduleIdentityRenameFixture.oldUserUri,
            languageId: 'zr',
            version: 1,
            text: moduleIdentityRenameFixture.oldUserContent,
        },
    });
    await waitForDiagnosticsUri(
        client,
        moduleIdentityRenameFixture.oldUserUri,
        'module identity old importer diagnostics uri mismatch');
    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: moduleIdentityRenameFixture.newUserUri,
            languageId: 'zr',
            version: 1,
            text: moduleIdentityRenameFixture.newUserContent,
        },
    });
    await waitForDiagnosticsUri(
        client,
        moduleIdentityRenameFixture.newUserUri,
        'module identity new importer diagnostics uri mismatch');

    const moduleIdentityWillRename = await client.request('workspace/willRenameFiles', {
        files: [
            {
                oldUri: moduleIdentityRenameFixture.oldProviderUri,
                newUri: moduleIdentityRenameFixture.newProviderUri,
            },
        ],
    });
    const oldUserImportStart = findPosition(
        moduleIdentityRenameFixture.oldUserContent, 'legacy', 1);
    const newUserImportStart = findPosition(
        moduleIdentityRenameFixture.newUserContent, 'legacy', 1);
    const providerModuleStart = findPosition(
        moduleIdentityRenameFixture.initialProviderContent, 'legacy');
    assert(workspaceEditContainsTextEdit(
        moduleIdentityWillRename,
        moduleIdentityRenameFixture.oldUserUri,
        oldUserImportStart,
        { line: oldUserImportStart.line, character: oldUserImportStart.character + 6 },
        'modern'),
    'workspace/willRenameFiles must edit the opened old-edge import specifier');
    assert(workspaceEditContainsTextEdit(
        moduleIdentityWillRename,
        moduleIdentityRenameFixture.newUserUri,
        newUserImportStart,
        { line: newUserImportStart.line, character: newUserImportStart.character + 6 },
        'modern'),
    'workspace/willRenameFiles must edit the overlapping importer old-edge specifier');
    assert(workspaceEditContainsTextEdit(
        moduleIdentityWillRename,
        moduleIdentityRenameFixture.oldProviderUri,
        providerModuleStart,
        { line: providerModuleStart.line, character: providerModuleStart.character + 6 },
        'modern'),
    'workspace/willRenameFiles must edit the explicit provider module declaration');
    assert(Array.isArray(moduleIdentityWillRename.documentChanges) &&
        moduleIdentityWillRename.documentChanges.length === 3,
    'workspace/willRenameFiles must publish versioned documentChanges for all edited source files');
    assert(workspaceEditDocumentVersion(
        moduleIdentityWillRename,
        moduleIdentityRenameFixture.oldUserUri) === 1,
    'workspace/willRenameFiles must serialize the captured opened old-edge importer version');
    assert(workspaceEditDocumentVersion(
        moduleIdentityWillRename,
        moduleIdentityRenameFixture.newUserUri) === 1,
    'workspace/willRenameFiles must serialize the captured opened new-edge importer version');
    assert(workspaceEditDocumentVersion(
        moduleIdentityWillRename,
        moduleIdentityRenameFixture.oldProviderUri) === null,
    'workspace/willRenameFiles must keep unopened provider documentChanges unversioned');

    fs.renameSync(
        moduleIdentityRenameFixture.oldProviderPath,
        moduleIdentityRenameFixture.newProviderPath);
    fs.writeFileSync(
        moduleIdentityRenameFixture.newProviderPath,
        moduleIdentityRenameFixture.renamedProviderContent);
    client.notify('workspace/didRenameFiles', {
        files: [
            {
                oldUri: moduleIdentityRenameFixture.oldProviderUri,
                newUri: moduleIdentityRenameFixture.newProviderUri,
            },
        ],
    });
    await waitForDiagnosticsUri(
        client,
        moduleIdentityRenameFixture.newProviderUri,
        'workspace/didRenameFiles renamed provider diagnostics uri mismatch');

    const renamedImporterHover = await client.request('textDocument/hover', {
        textDocument: { uri: moduleIdentityRenameFixture.newUserUri },
        position: findPosition(moduleIdentityRenameFixture.newUserContent, 'cached', 1),
    });
    assert(renamedImporterHover && renamedImporterHover.contents &&
        typeof renamedImporterHover.contents.value === 'string' &&
        renamedImporterHover.contents.value.includes('float'),
    'workspace/didRenameFiles must refresh semantic facts on the added ModuleIdentity edge');
    const renamedProviderDefinition = await client.request('textDocument/definition', {
        textDocument: { uri: moduleIdentityRenameFixture.newUserUri },
        position: findPosition(moduleIdentityRenameFixture.newUserContent, 'value', 1),
    });
    assert(Array.isArray(renamedProviderDefinition) && renamedProviderDefinition.some((location) =>
        location && diagnosticRelatedUriMatches(
            moduleIdentityRenameFixture.newProviderUri, location.uri)),
    `workspace/didRenameFiles must resolve the added ModuleIdentity edge to the renamed source: ${JSON.stringify(renamedProviderDefinition)}`);

    const willRenameFiles = await client.request('workspace/willRenameFiles', {
        files: [
            {
                oldUri: fileOperationsFixture.mainUri,
                newUri: fileOperationsFixture.mainUri,
            },
        ],
    });
    assert(willRenameFiles === null, 'workspace/willRenameFiles must return null when no edits are needed');
    const willDeleteFiles = await client.request('workspace/willDeleteFiles', {
        files: [
            { uri: fileOperationsFixture.projectUri },
        ],
    });
    assert(willDeleteFiles === null, 'workspace/willDeleteFiles must return null when no edits are needed');
    fs.unlinkSync(fileOperationsFixture.projectPath);
    client.notify('workspace/didDeleteFiles', {
        files: [
            { uri: fileOperationsFixture.projectUri },
        ],
    });
    const fileOperationDeleteDiagnostics = await waitForDiagnosticsUri(
        client,
        fileOperationsFixture.projectUri,
        'workspace/didDeleteFiles project delete diagnostics uri mismatch');
    assert(Array.isArray(fileOperationDeleteDiagnostics.diagnostics) &&
        fileOperationDeleteDiagnostics.diagnostics.length === 0,
    'workspace/didDeleteFiles must clear diagnostics for deleted projects');
    const fileOperationDeletedSymbols = await client.request('workspace/symbol', {
        query: 'watched_before_refresh',
    });
    assert(Array.isArray(fileOperationDeletedSymbols) && fileOperationDeletedSymbols.length === 0,
        'workspace/didDeleteFiles must clear deleted project indexes');

    const semanticTokens = await client.request('textDocument/semanticTokens/full', {
        textDocument: { uri: docsUri },
    });
    assert(semanticTokens &&
        Array.isArray(semanticTokens.data) &&
        typeof semanticTokens.resultId === 'string' &&
        semanticTokens.resultId.length > 0,
    'semanticTokens/full must return a data array with a resultId');
    const decodedSemanticTokens = decodeSemanticTokens(semanticTokens.data);
    assertSemanticTokensDoNotOverlap(decodedSemanticTokens,
        'semanticTokens/full must not return overlapping spans');
    const keywordTokenType = semanticTokenTypes.indexOf('keyword');
    assert(hasSemanticToken(decodedSemanticTokens,
        findPosition(documentationText, 'module'),
        'module'.length,
        keywordTokenType,
        0),
    'semanticTokens/full must classify module as a current keyword');
    assert(hasSemanticToken(decodedSemanticTokens,
        findPosition(documentationText, 'import'),
        'import'.length,
        keywordTokenType,
        0),
    'semanticTokens/full must classify import as a current keyword');
    assert(hasSemanticToken(decodedSemanticTokens,
        findPosition(documentationText, 'new'),
        'new'.length,
        keywordTokenType,
        0),
    'semanticTokens/full must classify new class construction as current');
    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: legacySemanticUri,
            languageId: 'zr',
            version: 1,
            text: legacySemanticText,
        },
    });
    const legacySemanticDiagnostics = await waitForDiagnosticsUri(
        client,
        legacySemanticUri,
        'legacy semantic diagnostics uri mismatch');
    assertDiagnosticIncludes(
        legacySemanticDiagnostics,
        'legacy_syntax_removed',
        '%import',
        'legacy semantic fixture must remain a migration diagnostic');
    const legacySemanticTokens = await client.request('textDocument/semanticTokens/full', {
        textDocument: { uri: legacySemanticUri },
    });
    const decodedLegacySemanticTokens = decodeSemanticTokens(legacySemanticTokens.data);
    assert(!hasSemanticToken(decodedLegacySemanticTokens,
        findPosition(legacySemanticText, '%import'),
        '%import'.length,
        keywordTokenType,
        0),
    'semanticTokens/full must not classify the removed percent prefix as a keyword');
    assert(!hasSemanticToken(decodedLegacySemanticTokens,
        findPosition(legacySemanticText, 'import'),
        'import'.length,
        keywordTokenType,
        0),
    'semanticTokens/full must not classify a keyword embedded in removed prefix syntax');
    assert(!hasSemanticToken(decodedLegacySemanticTokens,
        findPosition(legacySemanticText, '%', 1),
        '%'.length,
        keywordTokenType,
        0),
    'semanticTokens/full must not classify modulo as removed syntax');
    assert(!hasSemanticToken(decodedLegacySemanticTokens,
        findPosition(legacySemanticText, 'using'),
        'using'.length,
        keywordTokenType,
        0),
    'semanticTokens/full must not classify a removed using form as a current keyword');
    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: unresolvedSemanticTokenUri,
            languageId: 'zr',
            version: 1,
            text: unresolvedSemanticTokenText,
        },
    });
    await waitForDiagnosticsUri(
        client,
        unresolvedSemanticTokenUri,
        'unresolved semantic token didOpen diagnostics uri mismatch');
    const unresolvedSemanticTokens = await client.request('textDocument/semanticTokens/full', {
        textDocument: { uri: unresolvedSemanticTokenUri },
    });
    const decodedUnresolvedSemanticTokens = decodeSemanticTokens(unresolvedSemanticTokens.data);
    const resolvedPosition = findPosition(unresolvedSemanticTokenText, 'resolved', 1);
    const unresolvedPosition = findPosition(unresolvedSemanticTokenText, 'unresolved');
    assert(hasSemanticToken(decodedUnresolvedSemanticTokens,
        resolvedPosition,
        'resolved'.length,
        semanticTokenTypes.indexOf('method'),
        0) &&
        !hasSemanticToken(decodedUnresolvedSemanticTokens,
        unresolvedPosition,
        'unresolved'.length,
        semanticTokenTypes.indexOf('namespace'),
        0) &&
        !hasSemanticToken(decodedUnresolvedSemanticTokens,
            unresolvedPosition,
            'unresolved'.length,
            semanticTokenTypes.indexOf('method'),
            0) &&
        !hasSemanticToken(decodedUnresolvedSemanticTokens,
            unresolvedPosition,
            'unresolved'.length,
            semanticTokenTypes.indexOf('property'),
            0),
    'semanticTokens/full must not infer an unresolved member token from punctuation');
    const staleSemanticResultId = `zr-semantic:${semanticTokens.data.length}:stale`;
    const semanticDeltaTokens = await client.request('textDocument/semanticTokens/full/delta', {
        textDocument: { uri: docsUri },
        previousResultId: staleSemanticResultId,
    });
    assert(semanticDeltaTokens &&
        typeof semanticDeltaTokens.resultId === 'string' &&
        Array.isArray(semanticDeltaTokens.edits) &&
        semanticDeltaTokens.edits.some((edit) =>
            edit &&
            edit.start === 0 &&
            edit.deleteCount === semanticTokens.data.length &&
            Array.isArray(edit.data) &&
            edit.data.length === semanticTokens.data.length),
    'semanticTokens/full/delta must return a full replacement delta edit');
    const unchangedSemanticDeltaTokens = await client.request('textDocument/semanticTokens/full/delta', {
        textDocument: { uri: docsUri },
        previousResultId: semanticDeltaTokens.resultId,
    });
    assert(unchangedSemanticDeltaTokens &&
        unchangedSemanticDeltaTokens.resultId === semanticDeltaTokens.resultId &&
        Array.isArray(unchangedSemanticDeltaTokens.edits) &&
        unchangedSemanticDeltaTokens.edits.length === 0,
    'semanticTokens/full/delta must return empty edits when the token result is unchanged');
    const semanticRangeTokens = await client.request('textDocument/semanticTokens/range', {
        textDocument: { uri: docsUri },
        range: {
            start: { line: 0, character: 0 },
            end: { line: 6, character: 0 },
        },
    });
    assert(semanticRangeTokens &&
        Array.isArray(semanticRangeTokens.data) &&
        semanticRangeTokens.data.length > 0 &&
        semanticRangeTokens.data.length <= semanticTokens.data.length,
    'semanticTokens/range must return filtered semantic token data');
    client.notify('textDocument/didOpen', {
        textDocument: {
            uri: semanticDeltaUri,
            languageId: 'zr',
            version: 1,
            text: semanticDeltaText,
        },
    });
    const semanticDeltaDiagnostics = await client.waitForNotification('textDocument/publishDiagnostics');
    assert(semanticDeltaDiagnostics.uri === semanticDeltaUri,
        'semantic delta didOpen diagnostics uri mismatch');
    const semanticDeltaBaseline = await client.request('textDocument/semanticTokens/full', {
        textDocument: { uri: semanticDeltaUri },
    });
    client.notify('textDocument/didChange', {
        textDocument: {
            uri: semanticDeltaUri,
            version: 2,
        },
        contentChanges: [
            {
                text: semanticDeltaUpdatedText,
            },
        ],
    });
    const semanticDeltaChangeDiagnostics = await client.waitForNotification('textDocument/publishDiagnostics');
    assert(semanticDeltaChangeDiagnostics.uri === semanticDeltaUri,
        'semantic delta didChange diagnostics uri mismatch');
    const minimalSemanticDelta = await client.request('textDocument/semanticTokens/full/delta', {
        textDocument: { uri: semanticDeltaUri },
        previousResultId: semanticDeltaBaseline.resultId,
    });
    assert(minimalSemanticDelta &&
        minimalSemanticDelta.resultId !== semanticDeltaBaseline.resultId &&
        Array.isArray(minimalSemanticDelta.edits) &&
        minimalSemanticDelta.edits.length === 1 &&
        minimalSemanticDelta.edits[0].start > 0 &&
        minimalSemanticDelta.edits[0].deleteCount < semanticDeltaBaseline.data.length &&
        Array.isArray(minimalSemanticDelta.edits[0].data) &&
        minimalSemanticDelta.edits[0].data.length < semanticDeltaBaseline.data.length,
    'semanticTokens/full/delta must return a minimal cached edit for changed token data');

    client.notify('textDocument/didClose', {
        textDocument: {
            uri: watchedBinaryFixture.mainUri,
        },
    });

    client.notify('textDocument/didClose', {
        textDocument: {
            uri: documentUri,
        },
    });

    client.notify('textDocument/didClose', {
        textDocument: {
            uri: docsUri,
        },
    });
    client.notify('textDocument/didClose', {
        textDocument: {
            uri: unresolvedSemanticTokenUri,
        },
    });
    client.notify('textDocument/didClose', {
        textDocument: {
            uri: canonicalDisplayUri,
        },
    });
    client.notify('textDocument/didClose', {
        textDocument: {
            uri: testCodeLensUri,
        },
    });
    client.notify('textDocument/didClose', {
        textDocument: {
            uri: propertyContractUri,
        },
    });
    client.notify('textDocument/didClose', {
        textDocument: {
            uri: parserDiagnosticUri,
        },
    });
    client.notify('textDocument/didClose', {
        textDocument: {
            uri: missingConditionUri,
        },
    });
    client.notify('textDocument/didClose', {
        textDocument: {
            uri: documentHighlightFilterUri,
        },
    });
    client.notify('textDocument/didClose', {
        textDocument: {
            uri: linkedEditingFilterUri,
        },
    });
    client.notify('textDocument/didClose', {
        textDocument: {
            uri: monikerFilterUri,
        },
    });
    client.notify('textDocument/didClose', {
        textDocument: {
            uri: semanticDeltaUri,
        },
    });

    client.notify('textDocument/didClose', {
        textDocument: {
            uri: colorUri,
        },
    });

    client.notify('textDocument/didClose', {
        textDocument: {
            uri: inlineCompletionUri,
        },
    });

    client.notify('textDocument/didClose', {
        textDocument: {
            uri: moduleIdentityRenameFixture.oldUserUri,
        },
    });
    client.notify('textDocument/didClose', {
        textDocument: {
            uri: moduleIdentityRenameFixture.newUserUri,
        },
    });

    const expectedClosedUris = new Set([
        watchedBinaryFixture.mainUri,
        documentUri,
        docsUri,
        unresolvedSemanticTokenUri,
        canonicalDisplayUri,
        testCodeLensUri,
        propertyContractUri,
        colorUri,
        inlineCompletionUri,
        importDiagnosticsFixture.mainUri,
        parserDiagnosticUri,
        missingConditionUri,
        documentHighlightFilterUri,
        linkedEditingFilterUri,
        monikerFilterUri,
        moduleIdentityRenameFixture.oldUserUri,
        moduleIdentityRenameFixture.newUserUri,
    ]);
    let clearedCloseCount = 0;
    while (clearedCloseCount < expectedClosedUris.size) {
        const closeDiagnostics = await client.waitForNotification('textDocument/publishDiagnostics');
        assert(expectedClosedUris.has(closeDiagnostics.uri),
            `didClose diagnostics uri mismatch: ${closeDiagnostics.uri}`);
        assert(Array.isArray(closeDiagnostics.diagnostics) && closeDiagnostics.diagnostics.length === 0,
            'didClose must clear diagnostics');
        expectedClosedUris.delete(closeDiagnostics.uri);
        clearedCloseCount += 1;
    }

    const shutdown = await client.request('shutdown', undefined);
    assert(shutdown === null, 'shutdown must return null');
    peakMemory.assertWithinBudget();

    client.notify('exit', undefined);
    const exitCode = await client.waitForExit();
    assert(exitCode === 0, `server exited with ${exitCode}. stderr=${client.stderr()}`);
    assert(client.stderr().trim() === '', `language server stderr must stay empty during stdio smoke. stderr=${client.stderr()}`);
    cleanupPath(watchedFixtureRootToCleanup);
    cleanupPath(watchedBinaryFixtureRootToCleanup);
    cleanupPath(importDiagnosticsFixtureRootToCleanup);
    cleanupPath(fileOperationsFixtureRootToCleanup);
    cleanupPath(moduleIdentityRenameFixtureRootToCleanup);
    cleanupPath(descriptorPluginGenericFixtureRootToCleanup);
    cleanupPath(workspaceLatencyFixtureRootToCleanup);
    watchedFixtureRootToCleanup = null;
    watchedBinaryFixtureRootToCleanup = null;
    importDiagnosticsFixtureRootToCleanup = null;
    fileOperationsFixtureRootToCleanup = null;
    moduleIdentityRenameFixtureRootToCleanup = null;
    descriptorPluginGenericFixtureRootToCleanup = null;
    workspaceLatencyFixtureRootToCleanup = null;
}

main().catch((error) => {
    cleanupPath(watchedFixtureRootToCleanup);
    cleanupPath(watchedBinaryFixtureRootToCleanup);
    cleanupPath(importDiagnosticsFixtureRootToCleanup);
    cleanupPath(fileOperationsFixtureRootToCleanup);
    cleanupPath(moduleIdentityRenameFixtureRootToCleanup);
    cleanupPath(descriptorPluginGenericFixtureRootToCleanup);
    cleanupPath(workspaceLatencyFixtureRootToCleanup);
    watchedFixtureRootToCleanup = null;
    watchedBinaryFixtureRootToCleanup = null;
    importDiagnosticsFixtureRootToCleanup = null;
    fileOperationsFixtureRootToCleanup = null;
    moduleIdentityRenameFixtureRootToCleanup = null;
    descriptorPluginGenericFixtureRootToCleanup = null;
    workspaceLatencyFixtureRootToCleanup = null;
    console.error(error.stack || String(error));
    process.exit(1);
});
