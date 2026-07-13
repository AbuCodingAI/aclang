// AC ilib: string-cheese — JavaScript FFI
// Inlined by AC->JS compiler when "use ilib string-cheese" is declared.
'use strict';

const _WS = " \t\n\r";

function _isWs(s) { return s === _WS; }

function stringm_f(template, ...args)  { return String(template); }
function stringm_t(s)                  { return String(s).trim(); }
function stringm_b(s)                  { return Buffer.from(String(s), 'utf8'); }
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

const stringm = {
    f: stringm_f, t: stringm_t, b: stringm_b,
    upper: stringm_upper, lower: stringm_lower,
    find: stringm_find, strip: stringm_strip,
    replace: stringm_replace, split: stringm_split,
    join: stringm_join, length: stringm_len,
    startswith: stringm_startswith, endswith: stringm_endswith,
    count: stringm_count,
};
