/* Execute the complete actual inline page script, with browser boundary stubs.
 * No private viewer functions are exposed or duplicated. Input/output are JSON. */
"use strict";
const fs = require("node:fs");
const vm = require("node:vm");
const { TextDecoder } = require("node:util");
const input = JSON.parse(fs.readFileSync(0, "utf8"));
const elements = new Map();
for (const match of input.html.matchAll(/\bid="([^"]+)"/g)) {
  elements.set(match[1], {
    innerHTML: "", textContent: "", hidden: false, listeners: {},
    addEventListener(name, callback) { this.listeners[name] = callback; },
  });
}
const scripts = [...input.html.matchAll(/<script\b([^>]*)>([\s\S]*?)<\/script>/gi)];
const embedded = scripts.find(match => /\bid="zr-bench-embed"/.test(match[1]));
if (embedded) elements.get("zr-bench-embed").textContent = embedded[2];
const encodings = [];
class FileReader {
  readAsText(file, encoding) {
    encodings.push(encoding);
    this.result = file.text;
    this.onload();
  }
}
const context = vm.createContext({
  document: { getElementById: id => elements.get(id) || null },
  FileReader, TextDecoder, Uint8Array,
  // Browser atob returns one character per byte, not decoded Unicode.
  atob: text => Buffer.from(text, "base64").toString("latin1"),
});
for (const script of scripts) {
  if (/\btype="text\/plain"/.test(script[1])) continue;
  vm.runInContext(script[2], context, { timeout: 2000, filename: "actual-viewer-inline.js" });
}
function snapshot() {
  return Object.fromEntries([...elements].map(([id, el]) => [id, {
    html: el.innerHTML, text: el.textContent, hidden: el.hidden,
  }]));
}
const states = [snapshot()];
for (const text of input.files || []) {
  const callback = elements.get("file-input").listeners.change;
  if (typeof callback !== "function") throw new Error("actual file change listener missing");
  callback({ target: { files: [{ text }] } });
  states.push(snapshot());
}
process.stdout.write(JSON.stringify({ states, encodings }));
