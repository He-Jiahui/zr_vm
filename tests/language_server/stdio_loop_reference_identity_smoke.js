const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { StdioProtocolClient } = require('./stdio_protocol_client');

const outer = [[15, 8], [16, 11], [17, 12], [22, 25], [23, 8]];
const inner = [[19, 16], [20, 29]];
const uri = 'file:///loop-reference-identity.zr';

function rangeKey(range) {
    return [range.start.line, range.start.character,
        range.end.line, range.end.character].join(':');
}

function expectedKeys(positions, name) {
    return positions.map(([line, character]) =>
        [line, character, line, character + name.length].join(':')).sort();
}

function exactLocations(locations, positions, name) {
    assert(Array.isArray(locations), 'expected canonical reference locations');
    assert(locations.every(location => location.uri === uri), 'unexpected document');
    const actual = locations.map(location => rangeKey(location.range)).sort();
    assert.deepStrictEqual(actual, expectedKeys(positions, name),
        `reference ranges must be exact, unique and scope-separated; received ${actual.join(', ')}`);
}

function exactEdits(edit, positions, oldName, newName, version) {
    assert(edit && Array.isArray(edit.documentChanges), 'expected versioned workspace edit');
    assert.strictEqual(edit.documentChanges.length, 1);
    const change = edit.documentChanges[0];
    assert.strictEqual(change.textDocument.uri, uri);
    assert.strictEqual(change.textDocument.version, version);
    assert(change.edits.every(item => item.newText === newName));
    assert.deepStrictEqual(change.edits.map(item => rangeKey(item.range)).sort(),
        expectedKeys(positions, oldName), 'rename must include the loop condition, not the shadow');
    return change.edits;
}

// Apply only the server's semantic edits, never a textual name replacement.
function applyEdits(text, edits) {
    const lines = text.split('\n');
    const offset = position => {
        let value = 0;
        for (let line = 0; line < position.line; line++) value += lines[line].length + 1;
        return value + position.character; // JS string offsets and LSP positions are both UTF-16.
    };
    const changes = edits.map(edit => ({
        start: offset(edit.range.start), end: offset(edit.range.end), text: edit.newText,
    })).sort((left, right) => right.start - left.start);
    for (let i = 0; i < changes.length; i++) {
        const change = changes[i];
        if (i > 0) assert(change.end <= changes[i - 1].start, 'overlapping rename edits');
        text = text.slice(0, change.start) + change.text + text.slice(change.end);
    }
    return text;
}

async function main() {
    assert(process.argv[2], 'expected a real native stdio server');
    const document = { uri };
    // D6 (t727u): normalize line endings so positions, applyEdits' split('\n') and the
    // includes() checks hold however the fixture was checked out (core.autocrlf, archives).
    let text = fs.readFileSync(path.join(__dirname, 'fixtures/loop_reference_identity.zr'), 'utf8')
        .replace(/\r\n/g, '\n');
    const client = new StdioProtocolClient(process.argv[2]);
    const request = (method, params) => client.requestWithId(method, params, 15000).promise;
    const cleanDiagnostics = async () => {
        const result = await request('textDocument/diagnostic', { textDocument: document });
        assert.strictEqual(result.kind, 'full');
        assert.deepStrictEqual(result.items, [], 'actual renamed source must still analyze cleanly');
    };
    try {
        await request('initialize', {
            capabilities: {}, initializationOptions: { zrDeferDiagnostics: true },
        });
        client.notify('initialized', {});
        client.notify('textDocument/didOpen', {
            textDocument: { uri, languageId: 'zr', version: 1, text },
        });
        await cleanDiagnostics();
        let outerEdits;
        for (const [line, character] of outer) {
            const params = { textDocument: document, position: { line, character } };
            exactLocations(await request('textDocument/references', Object.assign({}, params, {
                context: { includeDeclaration: true },
            })), outer, 'index');
            outerEdits = exactEdits(await request('textDocument/rename', Object.assign({}, params, {
                newName: 'loopIndex',
            })), outer, 'index', 'loopIndex', 1);
        }
        for (const [line, character] of inner) {
            exactEdits(await request('textDocument/rename', {
                textDocument: document, position: { line, character }, newName: 'innerIndex',
            }), inner, 'index', 'innerIndex', 1);
        }
        text = applyEdits(text, outerEdits);
        assert(text.includes('while (loopIndex < values.length)'));
        assert(text.includes('let index = 1;\n            output += lookup(index);'));
        client.notify('textDocument/didChange', {
            textDocument: { uri, version: 2 }, contentChanges: [{ text }],
        });
        await cleanDiagnostics();
        exactLocations(await request('textDocument/references', {
            textDocument: document, position: { line: 16, character: 11 },
            context: { includeDeclaration: true },
        }), outer, 'loopIndex');
        const innerEdits = exactEdits(await request('textDocument/rename', {
            textDocument: document, position: { line: 19, character: 16 }, newName: 'innerIndex',
        }), inner, 'index', 'innerIndex', 2);
        text = applyEdits(text, innerEdits);
        client.notify('textDocument/didChange', {
            textDocument: { uri, version: 3 }, contentChanges: [{ text }],
        });
        await cleanDiagnostics();
        exactLocations(await request('textDocument/references', {
            textDocument: document, position: { line: 20, character: 29 },
            context: { includeDeclaration: true },
        }), inner, 'innerIndex');
        await request('shutdown', {});
        client.notify('exit', {});
        await client.waitForExit(10000);
        assert.strictEqual(client.exitCode, 0, client.stderr());
        console.log('PASS: native loop rename outer=5 inner=2, exact scopes, applied edits analyze cleanly');
    } finally {
        await client.terminate();
    }
}

main().catch(error => { console.error(error); process.exitCode = 1; });
