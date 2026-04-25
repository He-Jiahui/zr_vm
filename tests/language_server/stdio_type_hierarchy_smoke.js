const { spawn } = require('child_process');
const fs = require('fs');
const os = require('os');
const path = require('path');
const { pathToFileURL } = require('url');

function assert(condition, message) {
    if (!condition) {
        throw new Error(message);
    }
}

function createMessage(payload) {
    const body = Buffer.from(JSON.stringify(payload), 'utf8');
    return Buffer.concat([
        Buffer.from(`Content-Length: ${body.length}\r\n\r\n`, 'ascii'),
        body,
    ]);
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

class LspClient {
    constructor(serverPath) {
        this.nextId = 1;
        this.pending = new Map();
        this.notifications = [];
        this.waitingNotifications = [];
        this.buffer = Buffer.alloc(0);
        this.stderrChunks = [];
        this.process = spawn(serverPath, [], {
            stdio: ['pipe', 'pipe', 'pipe'],
            windowsHide: true,
        });

        this.process.stdout.on('data', (chunk) => this.handleData(chunk));
        this.process.stderr.on('data', (chunk) => this.stderrChunks.push(chunk));
    }

    stderr() {
        return Buffer.concat(this.stderrChunks).toString('utf8');
    }

    request(method, params) {
        const id = this.nextId++;
        const payload = {
            jsonrpc: '2.0',
            id,
            method,
            params,
        };

        this.process.stdin.write(createMessage(payload));
        return new Promise((resolve, reject) => {
            this.pending.set(id, { resolve, reject });
            setTimeout(() => {
                if (this.pending.has(id)) {
                    this.pending.delete(id);
                    reject(new Error(`Timed out waiting for ${method}`));
                }
            }, 10000);
        });
    }

    notify(method, params) {
        this.process.stdin.write(createMessage({
            jsonrpc: '2.0',
            method,
            params,
        }));
    }

    waitForNotification(method) {
        const index = this.notifications.findIndex((notification) => notification.method === method);
        if (index >= 0) {
            const [notification] = this.notifications.splice(index, 1);
            return Promise.resolve(notification.params);
        }

        return new Promise((resolve) => {
            this.waitingNotifications.push({ method, resolve });
        });
    }

    handleData(chunk) {
        this.buffer = Buffer.concat([this.buffer, chunk]);
        while (true) {
            const headerEnd = this.buffer.indexOf('\r\n\r\n');
            if (headerEnd < 0) {
                return;
            }

            const header = this.buffer.slice(0, headerEnd).toString('ascii');
            const match = /Content-Length: (\d+)/i.exec(header);
            if (!match) {
                throw new Error(`Missing Content-Length header: ${header}`);
            }

            const length = Number(match[1]);
            const messageStart = headerEnd + 4;
            const messageEnd = messageStart + length;
            if (this.buffer.length < messageEnd) {
                return;
            }

            const message = JSON.parse(this.buffer.slice(messageStart, messageEnd).toString('utf8'));
            this.buffer = this.buffer.slice(messageEnd);
            this.handleMessage(message);
        }
    }

    handleMessage(message) {
        if (Object.prototype.hasOwnProperty.call(message, 'id')) {
            const pending = this.pending.get(message.id);
            if (pending) {
                this.pending.delete(message.id);
                if (message.error) {
                    pending.reject(new Error(message.error.message || 'LSP request failed'));
                } else {
                    pending.resolve(message.result);
                }
            }
            return;
        }

        const waitingIndex = this.waitingNotifications.findIndex((entry) => entry.method === message.method);
        if (waitingIndex >= 0) {
            const [entry] = this.waitingNotifications.splice(waitingIndex, 1);
            entry.resolve(message.params);
            return;
        }
        this.notifications.push({ method: message.method, params: message.params });
    }

    waitForExit() {
        return new Promise((resolve) => {
            this.process.on('exit', (code) => resolve(code));
        });
    }
}

function removePathSync(targetPath) {
    if (typeof fs.rmSync === 'function') {
        fs.rmSync(targetPath, { recursive: true, force: true });
        return;
    }
    if (fs.existsSync(targetPath)) {
        fs.rmdirSync(targetPath, { recursive: true });
    }
}

async function main() {
    const serverPath = process.argv[2];
    assert(serverPath, 'Usage: node stdio_type_hierarchy_smoke.js <serverPath>');

    const rootPath = fs.mkdtempSync(path.join(os.tmpdir(), 'zr-stdio-type-hierarchy-'));
    const sourcePath = path.join(rootPath, 'src');
    const documentPath = path.join(sourcePath, 'type_hierarchy.zr');
    const documentUri = pathToFileURL(documentPath).toString();
    const text = [
        'class Base {',
        '}',
        '',
        'class Derived : Base {',
        '}',
        '',
    ].join('\n');
    const client = new LspClient(serverPath);

    try {
        fs.mkdirSync(sourcePath, { recursive: true });
        fs.writeFileSync(documentPath, text);

        const initializeResult = await client.request('initialize', {
            processId: process.pid,
            rootUri: pathToFileURL(rootPath).toString(),
            capabilities: {},
        });
        assert(initializeResult.capabilities.typeHierarchyProvider === true,
            'typeHierarchyProvider must be enabled');

        client.notify('initialized', {});
        client.notify('textDocument/didOpen', {
            textDocument: {
                uri: documentUri,
                languageId: 'zr',
                version: 1,
                text,
            },
        });
        await client.waitForNotification('textDocument/publishDiagnostics');

        const derivedPosition = findPosition(text, 'Derived', 0, 1);
        const basePosition = findPosition(text, 'Base', 0, 1);
        const derivedItems = await client.request('textDocument/prepareTypeHierarchy', {
            textDocument: { uri: documentUri },
            position: derivedPosition,
        });
        assert(Array.isArray(derivedItems) && derivedItems.length > 0,
            'prepareTypeHierarchy must return Derived');

        const supertypes = await client.request('typeHierarchy/supertypes', {
            item: derivedItems[0],
        });
        assert(Array.isArray(supertypes) && supertypes.some((item) => item && item.name === 'Base'),
            'typeHierarchy/supertypes must return direct Base');

        const baseItems = await client.request('textDocument/prepareTypeHierarchy', {
            textDocument: { uri: documentUri },
            position: basePosition,
        });
        assert(Array.isArray(baseItems) && baseItems.length > 0,
            'prepareTypeHierarchy must return Base');

        const subtypes = await client.request('typeHierarchy/subtypes', {
            item: baseItems[0],
        });
        assert(Array.isArray(subtypes) && subtypes.some((item) => item && item.name === 'Derived'),
            'typeHierarchy/subtypes must return direct Derived');

        const shutdown = await client.request('shutdown', null);
        assert(shutdown === null, 'shutdown must return null');
        client.notify('exit', null);
        const exitCode = await client.waitForExit();
        assert(exitCode === 0, `server exited with ${exitCode}. stderr=${client.stderr()}`);
        assert(client.stderr().trim() === '', `language server stderr must stay empty. stderr=${client.stderr()}`);
    } finally {
        removePathSync(rootPath);
    }
}

main().catch((error) => {
    console.error(error.stack || String(error));
    process.exit(1);
});
