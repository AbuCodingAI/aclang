// AC ilib: string-cheese — JavaScript FFI
// Inlined by AC->JS compiler when "use ilib string-cheese" is declared.
'use strict';

const _WS = " \t\n\r";

function _isWs(s) { return s === _WS; }

function stringm_f(template, ...args)  { return String(template); }   // f-string (compiler interpolates {})
function stringm_t(template, ...args)  { return String(template); }   // t-string (template, resolved at IR level)
function stringm_b(s)                  { return Buffer.from(String(s), 'utf8'); }
// bytes -> integer, 'little' or 'big' endian (first 8 bytes, wrapped to signed 64-bit like the C ilib)
function stringm_endian(b, order) {
    const buf = Buffer.isBuffer(b) ? b : Buffer.from(String(b), 'utf8');
    const little = !order || String(order).toLowerCase().startsWith('l');
    const n = Math.min(buf.length, 8);
    let v = 0n;
    for (let i = 0; i < n; i++) v = (v << 8n) | BigInt(buf[little ? (n - 1 - i) : i]);
    return Number(BigInt.asIntN(64, v));
}
function stringm_upper(s)              { return String(s).toUpperCase(); }
function stringm_lower(s)              { return String(s).toLowerCase(); }
function stringm_find(s, pattern) {
    if (_isWs(String(pattern))) {
        const m = String(s).match(/\s/);
        return m ? m.index : -1;
    }
    return String(s).indexOf(String(pattern));
}
function stringm_strip(s, chars) {
    const str = String(s);
    if (chars === undefined || chars === null || _isWs(String(chars)))
        return str.trim();
    const esc = String(chars).replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
    return str.replace(new RegExp(`^[${esc}]+|[${esc}]+$`, 'g'), '');
}
function stringm_replace(s, old, nw) {
    if (_isWs(String(old))) return String(s).replace(/\s+/g, String(nw));
    return String(s).split(String(old)).join(String(nw));
}
function stringm_split(s, sep) {
    if (sep === undefined || sep === null || _isWs(String(sep))) return String(s).split(/\s+/).filter(Boolean);
    return String(s).split(String(sep));
}
function stringm_join(sep, parts)       { return Array.from(parts).map(String).join(String(sep)); }
function stringm_len(s)                 { return String(s).length; }
function stringm_startswith(s, prefix)  { return String(s).startsWith(String(prefix)) ? 1 : 0; }
function stringm_endswith(s, suffix)    { return String(s).endsWith(String(suffix)) ? 1 : 0; }
function stringm_count(s, sub) {
    if (_isWs(String(sub))) return (String(s).match(/\s/g) || []).length;
    return String(s).split(String(sub)).length - 1;
}

function stringm_trim(s) { return String(s).replace(/^[ \t\n\r\f\v]+|[ \t\n\r\f\v]+$/g, ''); }
function stringm_strip_clause(mode, clause, s) {
    const text = String(s), key = String(clause), at = text.indexOf(key);
    if (at < 0) return text;
    if (String(mode) === 'before') return text.slice(at + key.length);
    if (String(mode) === 'after')  return text.slice(0, at);
    return text;
}
function stringm_stripln(s, needle) {
    for (const line of String(s).split('\n')) if (line.includes(String(needle))) return line;
    return '';
}
function stringm_split_nth(s, sep, n) {
    const parts = stringm_split(s, sep), k = Number(n);
    return k >= 0 && k < parts.length ? parts[k] : '';
}
function stringm_format(t) { return String(t); }
function stringm_ischar(s) { s = String(s); return s.length > 0 && /^[A-Za-z]+$/.test(s) ? 1 : 0; }
function stringm_isws(s)   { return /^[ \t\n\r\f\v]*$/.test(String(s)) ? 1 : 0; }
// Stdin, one byte at a time, so a program that asks a question doesn't wait for end-of-file.
function _readStdinLine() {
    const fs = require('fs'), buf = Buffer.alloc(1);
    let line = '', got = false;
    while (true) {
        let n;
        try { n = fs.readSync(0, buf, 0, 1, null); }
        catch (e) { if (e.code === 'EAGAIN') continue; break; }
        if (n === 0) break;
        got = true;
        const c = buf.toString('utf8');
        if (c === '\n') break;
        line += c;
    }
    return { line, eof: !got };
}
function stringm_getline() { return _readStdinLine().line; }
function stringm_scan(needle) {
    const r = _readStdinLine();
    if (r.eof) return 0;
    return r.line.includes(String(needle)) ? 1 : 0;
}

const stringm = {
    f: stringm_f, t: stringm_t, b: stringm_b, endian: stringm_endian,
    upper: stringm_upper, lower: stringm_lower,
    find: stringm_find, strip: stringm_strip,
    replace: stringm_replace, split: stringm_split,
    join: stringm_join, length: stringm_len, len: stringm_len,
    trim: stringm_trim, strip_clause: stringm_strip_clause, stripln: stringm_stripln,
    split_nth: stringm_split_nth, format: stringm_format,
    ischar: stringm_ischar, isws: stringm_isws, getline: stringm_getline, scan: stringm_scan,
    startswith: stringm_startswith, endswith: stringm_endswith,
    count: stringm_count,
};
