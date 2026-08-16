// AC ilib: widgets — real-DOM JavaScript implementation.
//
// STATUS: this IS what AC->HTML loads for `use ilib widgets` (HTMLStrategy::emitHeader
// in ir_codegen.cpp special-cases "widgets" to inline this exact file verbatim, rather
// than JS's `ffi/widgets_ffi.js` — that file is a Node-only ffi-napi wrapper into the
// real GTK core, which doesn't exist in a browser at all). Verified live: compiled
// examples/widgets_test2.ac to HTML and rendered it in real headless Chrome — correct
// tab-page isolation, a real <table>, a real styled <canvas>, no console errors.
// AC->JS still uses `ffi/widgets_ffi.js` (real GTK windows) — this file is HTML-only.
//
// Matches the exact calling convention the compiler actually emits (bare/lowercase
// functions, positional args, no `new`, dot-call methods — confirmed by reading
// examples/widgets_test.js and examples/widgets_test.html) and the full widget surface
// + signatures/semantics of library/ilib/widgets/widgets.hpp (the authoritative,
// current, GTK-backed C++ header).
//
// Runs in two contexts:
//   1. A real browser DOM (HTML backend's natural home) — uses real elements.
//   2. Plain Node with no `document` at all — falls back to a minimal internal element
//      shim (_makeEl/_makeCanvasStub below) so constructing/driving widgets (get/set/
//      add/pack) doesn't throw ReferenceError; there's nothing to visually render into,
//      so the shim doesn't try to fake rendering, only the small subset of the Element
//      API the widgets below touch.
// The OLD version of this file assumed `document` unconditionally (would throw
// immediately under plain `node foo.js`) — that assumption is what's fixed here.

'use strict';

const _hasDOM = typeof document !== 'undefined';

// Minimal element stand-in for plain-Node contexts with no DOM. Supports exactly the
// subset of the Element API the widgets below use. No visual output — nothing to
// render into in headless Node — this exists purely so widget logic (get/set/add/pack)
// can be constructed and driven without throwing.
function _makeEl(tag) {
    if (_hasDOM) return document.createElement(tag);
    const listeners = {};
    return {
        tagName: String(tag).toUpperCase(),
        style: {},
        children: [],
        _text: '',
        _value: '',
        _checked: false,
        get textContent() { return this._text; },
        set textContent(v) { this._text = String(v); },
        get value() { return this._value; },
        set value(v) { this._value = String(v); },
        get checked() { return this._checked; },
        set checked(v) { this._checked = !!v; },
        setAttribute(k, v) { this[k] = v; },
        appendChild(c) { this.children.push(c); return c; },
        addEventListener(ev, fn) { (listeners[ev] = listeners[ev] || []).push(fn); },
        removeEventListener(ev, fn) {
            const a = listeners[ev]; if (!a) return;
            const i = a.indexOf(fn); if (i >= 0) a.splice(i, 1);
        },
        click() { (listeners['click'] || []).forEach(fn => fn({ target: this })); },
        getContext() { return _makeCanvasStub(); },
        remove() {},
    };
}

// Stub 2D canvas context for the no-DOM path — records nothing, just doesn't throw.
function _makeCanvasStub() {
    return {
        strokeStyle: '', fillStyle: '', lineWidth: 1, font: '',
        beginPath() {}, moveTo() {}, lineTo() {}, stroke() {}, closePath() {},
        fillRect() {}, clearRect() {}, arc() {}, fill() {}, fillText() {},
    };
}

// `lazy` — pass as the last positional arg to any packable widget to defer auto-pack
// until an explicit `.pack()` call. Plain constant, matches widgets.hpp's `lazy`.
var lazy = 'lazy';

// AC's calling convention has no named/skippable args — a caller wanting `lazy` but not
// an earlier optional positional (e.g. `ask(root, lazy)` instead of `ask(root, 30, lazy)`)
// ends up passing "lazy" into that earlier slot. Detect and re-route it, mirroring the
// same defensive pattern already used by ffi/widgets_ffi.js for this backend's ABI.
function _stripLazy(...args) {
    const isLazy = args.some(a => a === 'lazy');
    return { isLazy, rest: args.filter(a => a !== 'lazy') };
}

// Shared "auto-pack unless lazy" + "pack with optional margin" logic for the widgets
// that support it (display/ask/btn/ckbtn/radbtn/dropdown/advance/slider — the ones
// widgets.hpp gives a `lz` ctor param and a `pack(sx, sy)` method).
function _finishPack(master, el, packedFlag, sx, sy) {
    if (!packedFlag.v) {
        if (master && master._container) master._container.appendChild(el);
        packedFlag.v = true;
    }
    if (sx) { el.style.marginLeft = sx + 'px'; el.style.marginRight = sx + 'px'; }
    if (sy) { el.style.marginTop = sy + 'px'; el.style.marginBottom = sy + 'px'; }
}

// ── Screen ───────────────────────────────────────────────────────────────────
function Screen(title = 'AC App', geometry = '800x600') {
    const parts = String(geometry).split('x');
    const width = parseInt(parts[0]) || 800;
    const height = parseInt(parts[1]) || 600;
    const el = _makeEl('div');
    el.className = 'ac-screen';
    el.style.cssText =
        `width:${width}px;min-height:${height}px;margin:20px auto;border:2px solid #333;` +
        `padding:16px;background:#f5f5f5;font-family:Arial,sans-serif;box-sizing:border-box;`;
    if (_hasDOM) document.title = String(title);

    const o = { _container: el, _mounted: false };
    o.mainloop = () => {
        if (_hasDOM && !o._mounted) { document.body.appendChild(el); o._mounted = true; }
    };
    // No real blocking GTK-style loop makes sense on a JS event loop — the DOM already
    // reflects current widget state continuously, so update() is a deliberate no-op.
    o.update = () => {};
    o.destroy = () => {
        if (_hasDOM && el.parentNode) el.parentNode.removeChild(el);
        o._mounted = false;
    };
    return o;
}

// ── display ──────────────────────────────────────────────────────────────────
function display(master, text = '', lz = null) {
    const el = _makeEl('div');
    el.className = 'ac-display';
    el.textContent = String(text);
    el.style.cssText = 'margin:4px 0;font-size:14px;';
    const packed = { v: false };
    if (lz !== 'lazy') { if (master && master._container) master._container.appendChild(el); packed.v = true; }

    const o = { _el: el };
    o.pack = (sx = 0, sy = 0) => _finishPack(master, el, packed, sx, sy);
    o.set = (v) => { el.textContent = String(v); };
    o.get = () => el.textContent;
    o.config = (text) => { el.textContent = String(text); };
    return o;
}

// ── ask ──────────────────────────────────────────────────────────────────────
function ask(master, width = 20, lz = null) {
    const s = _stripLazy(width);
    if (s.isLazy) { lz = 'lazy'; width = s.rest[0] ?? 20; }
    const el = _makeEl('input');
    el.type = 'text';
    el.size = parseInt(width) || 20;
    el.className = 'ac-ask';
    el.style.cssText = 'margin:4px 0;padding:4px;';
    const packed = { v: false };
    if (lz !== 'lazy') { if (master && master._container) master._container.appendChild(el); packed.v = true; }

    const o = { _el: el };
    o.pack = (sx = 0, sy = 0) => _finishPack(master, el, packed, sx, sy);
    o.get = () => el.value;
    o.set = (v) => { el.value = String(v); };
    return o;
}

// ── btn ──────────────────────────────────────────────────────────────────────
function btn(master, text = 'Button', cmd = null, lz = null) {
    const el = _makeEl('button');
    el.type = 'button';
    el.textContent = String(text);
    el.className = 'ac-btn';
    el.style.cssText = 'margin:4px 2px;padding:6px 14px;cursor:pointer;';
    let _cb = null;
    const packed = { v: false };

    const o = { _el: el };
    o.on_click = (cb) => {
        if (_cb) el.removeEventListener('click', _cb);
        _cb = () => cb();
        el.addEventListener('click', _cb);
    };
    if (cmd) o.on_click(cmd);
    if (lz !== 'lazy') { if (master && master._container) master._container.appendChild(el); packed.v = true; }
    o.pack = (sx = 0, sy = 0) => _finishPack(master, el, packed, sx, sy);
    return o;
}

// ── ckbtn ────────────────────────────────────────────────────────────────────
function ckbtn(master, text = '', lz = null) {
    const wrap = _makeEl('label');
    wrap.className = 'ac-ckbtn';
    wrap.style.cssText = 'display:inline-flex;align-items:center;gap:4px;margin:4px 0;';
    const el = _makeEl('input');
    el.type = 'checkbox';
    wrap.appendChild(el);
    const span = _makeEl('span');
    span.textContent = String(text);
    wrap.appendChild(span);
    const packed = { v: false };
    if (lz !== 'lazy') { if (master && master._container) master._container.appendChild(wrap); packed.v = true; }

    const o = { _el: wrap };
    o.pack = (sx = 0, sy = 0) => _finishPack(master, wrap, packed, sx, sy);
    o.get = () => !!el.checked;
    o.set = (v) => { el.checked = !!v; };
    return o;
}

// ── radbtn ───────────────────────────────────────────────────────────────────
// Matches widgets.hpp exactly: currently just re-uses ckbtn's backend (checkbox) and
// only exposes .get() — a pre-existing simplification in the C++ core, not "fixed" here.
function radbtn(master, text = '', lz = null) {
    const wrap = _makeEl('label');
    wrap.className = 'ac-radbtn';
    wrap.style.cssText = 'display:inline-flex;align-items:center;gap:4px;margin:4px 0;';
    const el = _makeEl('input');
    el.type = 'checkbox';
    wrap.appendChild(el);
    const span = _makeEl('span');
    span.textContent = String(text);
    wrap.appendChild(span);
    const packed = { v: false };
    if (lz !== 'lazy') { if (master && master._container) master._container.appendChild(wrap); packed.v = true; }

    const o = { _el: wrap };
    o.pack = (sx = 0, sy = 0) => _finishPack(master, wrap, packed, sx, sy);
    o.get = () => !!el.checked;
    return o;
}

// ── dropdown ─────────────────────────────────────────────────────────────────
function dropdown(master, values = [], lz = null) {
    const s = _stripLazy(values);
    if (s.isLazy) { lz = 'lazy'; values = s.rest[0] ?? []; }
    const el = _makeEl('select');
    el.className = 'ac-dropdown';
    el.style.cssText = 'margin:4px 0;padding:4px;';
    const vals = typeof values === 'string'
        ? (values ? values.split(',').map(v => v.trim()) : [])
        : (values || []);
    vals.forEach(v => {
        const opt = _makeEl('option');
        opt.value = String(v); opt.textContent = String(v);
        el.appendChild(opt);
        if (!el.value) el.value = String(v);
    });
    const packed = { v: false };
    if (lz !== 'lazy') { if (master && master._container) master._container.appendChild(el); packed.v = true; }

    const o = { _el: el };
    o.pack = (sx = 0, sy = 0) => _finishPack(master, el, packed, sx, sy);
    o.add = (v) => {
        const opt = _makeEl('option');
        opt.value = String(v); opt.textContent = String(v);
        el.appendChild(opt);
        if (!el.value) el.value = String(v);
    };
    o.get = () => el.value;
    o.set = (v) => { el.value = String(v); };
    return o;
}

// ── advance ──────────────────────────────────────────────────────────────────
function advance(master, length = 200, lz = null) {
    const s = _stripLazy(length);
    if (s.isLazy) { lz = 'lazy'; length = s.rest[0] ?? 200; }
    const len = parseInt(length) || 200;
    const el = _makeEl('progress');
    el.max = 100; el.value = 0;
    el.className = 'ac-advance';
    el.style.cssText = `width:${len}px;`;
    const packed = { v: false };
    if (lz !== 'lazy') { if (master && master._container) master._container.appendChild(el); packed.v = true; }

    const o = { _el: el };
    o.pack = (sx = 0, sy = 0) => _finishPack(master, el, packed, sx, sy);
    o.set = (v) => { el.value = Number(v); };
    o.get = () => Number(el.value) || 0;
    return o;
}

// ── slider ───────────────────────────────────────────────────────────────────
function slider(master, from_val = 0, to_val = 100, orient = 'horizontal', lz = null) {
    const s = _stripLazy(from_val, to_val, orient);
    if (s.isLazy) { lz = 'lazy'; [from_val, to_val, orient] = [s.rest[0] ?? 0, s.rest[1] ?? 100, s.rest[2] ?? 'horizontal']; }
    const el = _makeEl('input');
    el.type = 'range';
    el.min = Number(from_val); el.max = Number(to_val); el.value = Number(from_val);
    el.className = 'ac-slider';
    const vertical = String(orient).toLowerCase().startsWith('v');
    el.style.cssText = vertical
        ? 'writing-mode:vertical-lr;direction:rtl;height:120px;'
        : 'width:150px;';
    const packed = { v: false };
    if (lz !== 'lazy') { if (master && master._container) master._container.appendChild(el); packed.v = true; }

    const o = { _el: el };
    o.pack = (sx = 0, sy = 0) => _finishPack(master, el, packed, sx, sy);
    o.get = () => Number(el.value);
    o.set = (v) => { el.value = Number(v); };
    return o;
}

// ── group ────────────────────────────────────────────────────────────────────
// A labelled container that is itself a usable master — anything constructed with a
// `group` instance as its master parents visually inside it. Not auto-packed (matches
// widgets.hpp: no `lz` ctor param, no auto-pack call — only `.pack()` shows it).
function group(master, text = '') {
    const el = _makeEl('fieldset');
    el.className = 'ac-group';
    el.style.cssText = 'margin:8px 0;padding:8px;border:1px solid #999;border-radius:4px;';
    if (text) {
        const legend = _makeEl('legend');
        legend.textContent = String(text);
        el.appendChild(legend);
    }
    const o = { _container: el };
    o.pack = () => { if (master && master._container) master._container.appendChild(el); };
    return o;
}

// ── tabs ─────────────────────────────────────────────────────────────────────
// Real tab switching: each add_tab(name) creates its own page container and returns a
// master-like object wrapping it, so `display(page, "hi")` nests into that specific tab.
function tabs(master) {
    const wrap = _makeEl('div');
    wrap.className = 'ac-tabs';
    const bar = _makeEl('div');
    bar.className = 'ac-tabs-bar';
    bar.style.cssText = 'display:flex;gap:2px;border-bottom:1px solid #999;margin-bottom:6px;';
    const pagesWrap = _makeEl('div');
    pagesWrap.className = 'ac-tabs-pages';
    wrap.appendChild(bar);
    wrap.appendChild(pagesWrap);

    const pages = [];
    function _select(idx) {
        pages.forEach((p, i) => {
            p.page.style.display = (i === idx) ? 'block' : 'none';
            p.tabBtn.style.fontWeight = (i === idx) ? 'bold' : 'normal';
        });
    }

    const o = { _container: wrap };
    o.pack = () => { if (master && master._container) master._container.appendChild(wrap); };
    o.add_tab = (name) => {
        const idx = pages.length;
        const tabBtn = _makeEl('button');
        tabBtn.type = 'button';
        tabBtn.textContent = String(name);
        tabBtn.className = 'ac-tab-btn';
        tabBtn.addEventListener('click', () => _select(idx));
        bar.appendChild(tabBtn);

        const page = _makeEl('div');
        page.className = 'ac-tab-page';
        page.style.display = (idx === 0) ? 'block' : 'none';
        pagesWrap.appendChild(page);

        pages.push({ tabBtn, page });
        if (idx === 0) tabBtn.style.fontWeight = 'bold';
        return { _container: page };
    };
    return o;
}

// ── scroller ─────────────────────────────────────────────────────────────────
// A standalone scrollbar widget (matches the Python reference — not a scrollable
// container). Not auto-packed; only `.pack()` (no sx/sy, no get/set — matches
// widgets.hpp exactly).
function scroller(master, orient = 'vertical') {
    const el = _makeEl('input');
    el.type = 'range';
    el.min = 0; el.max = 100; el.value = 0;
    el.className = 'ac-scroller';
    const vertical = String(orient).toLowerCase().startsWith('v');
    el.style.cssText = vertical
        ? 'writing-mode:vertical-lr;direction:rtl;width:16px;height:150px;'
        : 'width:150px;height:16px;';
    const o = { _container: el };
    o.pack = () => { if (master && master._container) master._container.appendChild(el); };
    return o;
}

// ── listbox ──────────────────────────────────────────────────────────────────
function listbox(master, width = 30, height = 5) {
    const el = _makeEl('div');
    el.className = 'ac-listbox';
    el.style.cssText =
        `overflow-y:auto;border:1px solid #999;width:${width * 8}px;height:${height * 20}px;` +
        `background:#fff;font-family:monospace;font-size:13px;`;
    const items = [];
    const o = { _container: el };
    o.pack = () => { if (master && master._container) master._container.appendChild(el); };
    o.add = (s) => {
        items.push(String(s));
        const row = _makeEl('div');
        row.textContent = String(s);
        row.style.cssText = 'padding:2px 4px;border-bottom:1px solid #eee;';
        el.appendChild(row);
    };
    o.get = () => items.slice();
    return o;
}

// ── table ────────────────────────────────────────────────────────────────────
// Real multi-column table with visible headers, matching the just-implemented GTK
// semantics: `columns` is a comma-separated header list ("Name,Age"), `add(row)` a
// comma-separated value list ("Alice,30"). No quoting/escaping — matches the
// documented limitation.
function table(master, columns = '', height = 10) {
    const el = _makeEl('table');
    el.className = 'ac-table';
    el.style.cssText = `border-collapse:collapse;width:100%;max-height:${height * 20}px;`;
    const cols = String(columns).split(',').map(s => s.trim()).filter(s => s.length);
    if (cols.length) {
        const thead = _makeEl('thead');
        const hr = _makeEl('tr');
        cols.forEach(c => {
            const th = _makeEl('th');
            th.textContent = c;
            th.style.cssText = 'border:1px solid #999;padding:4px 8px;background:#eee;text-align:left;';
            hr.appendChild(th);
        });
        thead.appendChild(hr);
        el.appendChild(thead);
    }
    const tbody = _makeEl('tbody');
    el.appendChild(tbody);
    const rows = [];

    const o = { _container: el };
    o.pack = () => { if (master && master._container) master._container.appendChild(el); };
    o.add = (row) => {
        rows.push(String(row));
        const cells = String(row).split(',').map(s => s.trim());
        const tr = _makeEl('tr');
        cells.forEach(c => {
            const td = _makeEl('td');
            td.textContent = c;
            td.style.cssText = 'border:1px solid #ccc;padding:4px 8px;';
            tr.appendChild(td);
        });
        tbody.appendChild(tr);
    };
    o.get = () => rows.slice();
    return o;
}

// ── sketch ───────────────────────────────────────────────────────────────────
// Real drawing canvas — `<canvas>` + 2D context in a browser. Colors are 0-255 RGB
// per widgets.hpp's uint8_t params, converted to CSS rgb(r,g,b).
function sketch(master, width = 400, height = 300) {
    const el = _makeEl('canvas');
    el.width = width; el.height = height;
    el.className = 'ac-sketch';
    el.style.cssText = 'border:1px solid #999;background:#fff;';
    const ctx = (typeof el.getContext === 'function') ? el.getContext('2d') : _makeCanvasStub();

    const o = { _container: el };
    o.pack = () => { if (master && master._container) master._container.appendChild(el); };
    o.clear = () => { ctx.clearRect(0, 0, width, height); };
    o.line = (x1, y1, x2, y2, r = 0, g = 0, b = 0) => {
        ctx.strokeStyle = `rgb(${r},${g},${b})`;
        ctx.beginPath(); ctx.moveTo(x1, y1); ctx.lineTo(x2, y2); ctx.stroke();
    };
    o.rect = (x1, y1, x2, y2, r = 0, g = 0, b = 0) => {
        ctx.fillStyle = `rgb(${r},${g},${b})`;
        ctx.fillRect(x1, y1, x2 - x1, y2 - y1);
    };
    o.circle = (cx, cy, rad, r = 0, g = 0, b = 0) => {
        ctx.fillStyle = `rgb(${r},${g},${b})`;
        ctx.beginPath(); ctx.arc(cx, cy, rad, 0, Math.PI * 2); ctx.fill();
    };
    o.text_at = (x, y, t, r = 0, g = 0, b = 0) => {
        ctx.fillStyle = `rgb(${r},${g},${b})`;
        ctx.fillText(String(t), x, y);
    };
    return o;
}

// Export for use in a Node require()-based test harness. In a plain <script> (non-
// module) browser context, the `function` declarations above are already global —
// this block is additive, not required for that path.
if (typeof module !== 'undefined' && module.exports) {
    module.exports = {
        lazy, Screen, display, ask, btn, ckbtn, radbtn, dropdown, advance, slider,
        group, tabs, scroller, listbox, table, sketch,
    };
}
