const test = require('node:test');
const assert = require('node:assert/strict');

const {
    summarizeRichHover,
    renderRichHoverHtml,
} = require('../out/richHoverShared.js');

test('summarizeRichHover keeps standard hover concise while preserving key semantic fields', () => {
    const summary = summarizeRichHover({
        sections: [
            { role: 'name', label: 'function', value: 'takeFromPoolTest' },
            { role: 'signature', label: 'Signature', value: 'takeFromPoolTest(seed: int): %unique PointSet' },
            { role: 'resolvedType', label: 'Resolved Type', value: '%unique PointSet' },
            { role: 'access', label: 'Access', value: 'public' },
            { role: 'docs', label: 'Documentation', value: 'Allocates and returns a unique point set.' },
            { role: 'source', label: 'Source', value: 'project source' },
        ],
    });

    assert.equal(summary.title, 'takeFromPoolTest');
    assert.deepEqual(summary.lines, [
        '**function**: takeFromPoolTest',
        'Signature: `takeFromPoolTest(seed: int): %unique PointSet`',
        'Resolved Type: `%unique PointSet`',
        'Access: `public`',
        'Source: `project source`',
    ]);
});

test('renderRichHoverHtml renders colored role sections and escapes HTML-sensitive text', () => {
    const html = renderRichHoverHtml({
        title: 'Rich Hover',
        subtitle: 'file:///demo.zr:12:8',
        sections: [
            { role: 'name', label: 'Meta Method', value: '@constructor' },
            { role: 'category', label: 'Category', value: 'lifecycle' },
            { role: 'applicableTo', label: 'Applicable To', value: 'class/struct meta function' },
            { role: 'detail', label: 'Detail', value: 'constructor <initializer>' },
        ],
    });

    assert.match(html, /class="section role-name"/);
    assert.match(html, /class="section role-category"/);
    assert.match(html, /Meta Method/);
    assert.match(html, /@constructor/);
    assert.match(html, /constructor &lt;initializer&gt;/);
    assert.match(html, /file:\/\/\/demo\.zr:12:8/);
});
