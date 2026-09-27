const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');

// 从待发布的扩展清单读取贡献项，验证用户可见入口与命令实现约定的 ID 保持一致。
function readPackageJson() {
    return JSON.parse(fs.readFileSync(path.resolve(__dirname, '..', 'package.json'), 'utf8'));
}

// 防止桌面或 Web 虽已实现处理器，却因清单遗漏激活事件、命令或 Zr 编辑器菜单而无法触达。
// 命令执行和编辑结果由两端的 smoke 测试验证；此处专门约束静态贡献契约。
test('import cleanup commands are exposed through activation, commands, and editor context menu', () => {
    const manifest = readPackageJson();
    const commands = manifest.contributes?.commands ?? [];
    const editorContext = manifest.contributes?.menus?.['editor/context'] ?? [];

    assert(manifest.activationEvents.includes('onCommand:zr.organizeImports'));
    assert(manifest.activationEvents.includes('onCommand:zr.removeUnusedImports'));
    assert(commands.some((entry) =>
        entry.command === 'zr.organizeImports' &&
        entry.title === 'Organize Imports' &&
        entry.category === 'Zr'));
    assert(commands.some((entry) =>
        entry.command === 'zr.removeUnusedImports' &&
        entry.title === 'Remove Unused Imports' &&
        entry.category === 'Zr'));
    assert(editorContext.some((entry) =>
        entry.command === 'zr.organizeImports' &&
        entry.when === 'editorLangId == zr'));
    assert(editorContext.some((entry) =>
        entry.command === 'zr.removeUnusedImports' &&
        entry.when === 'editorLangId == zr'));
});
