const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');

const extensionOutputRoot = path.resolve(
    process.env.ZR_VM_EXTENSION_TEST_OUTPUT_ROOT || path.join(__dirname, '..', 'out'),
);
const { ZrCliLauncher } = require(path.join(extensionOutputRoot, 'debug', 'cliLauncher.js'));

test('launch receives the private CLI token without forwarding it to DAP output', async () => {
    const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'zr-debug-token-handoff-'));
    const token = '0123456789abcdef'.repeat(4);
    const tokenSplit = token.length / 2;
    const fakeCli = path.join(directory, 'fake-cli.js');
    fs.writeFileSync(fakeCli, [
        `process.stdout.write('ordinary-before-handshake\\n');`,
        `process.stdout.write('debug_endpoint=127.0.0.1:4321 debug_token=${token.slice(0, tokenSplit)}');`,
        `setTimeout(() => { process.stdout.write('${token.slice(tokenSplit)}\\nordinary-cli-output\\nordinary-cli-final-tail', () => process.exit(0)); }, 10);`,
    ].join('\n'));
    const forwarded = [];
    const launcher = new ZrCliLauncher((_channel, text) => forwarded.push(text));
    const exitPromise = new Promise((resolve) => launcher.onExit(resolve));

    try {
        const result = await launcher.launch({
            cliPath: process.execPath,
            project: fakeCli,
            cwd: directory,
        });

        assert.deepEqual(result, { endpoint: '127.0.0.1:4321', authToken: token });
        const child = launcher['child'];
        await exitPromise;
        if (!child.stdout.closed) {
            await new Promise((resolve) => child.stdout.once('close', resolve));
        }
        assert.equal(forwarded.join('').includes(token), false);
        assert.equal(forwarded.join('').includes('ordinary-before-handshake'), true);
        assert.equal(forwarded.join('').includes('ordinary-cli-output'), true);
        assert.equal(forwarded.join('').includes('ordinary-cli-final-tail'), true);
        await launcher.stop();
        assert.ok(child.exitCode !== null || child.signalCode !== null);
    } finally {
        await launcher.stop();
        fs.rmSync(directory, { recursive: true, force: true });
    }
});

test('forwards a complete ordinary stdout line from the same data chunk', () => {
    const forwarded = [];
    const launcher = new ZrCliLauncher((_channel, text) => forwarded.push(text));

    launcher['forwardStdoutWithoutHandshake']('ordinary-complete-line\n');

    assert.deepEqual(forwarded, ['ordinary-complete-line\n']);
});
