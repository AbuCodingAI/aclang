// AC ilib: regex — JavaScript native implementation (no .so dependency)
// AC-generated JS uses regex.match(s, p), regex.find_all(s, p), etc.

function regex_match(s, pat) {
    try { return new RegExp('^(?:' + pat + ')$').test(s); } catch { return false; }
}
function regex_test(s, pat) {
    try { return new RegExp(pat).test(s); } catch { return false; }
}
function regex_search(s, pat) {
    try {
        const m = s.match(new RegExp(pat));
        return m ? m[0] : '';
    } catch { return ''; }
}
function regex_replace(s, pat, repl) {
    try { return s.replace(new RegExp(pat), repl); } catch { return s; }
}
function regex_replace_all(s, pat, repl) {
    try { return s.replace(new RegExp(pat, 'g'), repl); } catch { return s; }
}
function regex_count(s, pat) {
    try { return (s.match(new RegExp(pat, 'g')) || []).length; } catch { return 0; }
}
function regex_escape(s) {
    return s.replace(/[\\^$.|?*+()[\]{}]/g, '\\$&');
}
function regex_find_all(s, pat) {
    try { return [...s.matchAll(new RegExp(pat, 'g'))].map(m => m[0]); } catch { return []; }
}
function regex_split(s, pat) {
    try { return s.split(new RegExp(pat)); } catch { return [s]; }
}
function regex_groups(s, pat) {
    try {
        const m = s.match(new RegExp(pat));
        return m ? m.slice(1).map(g => g ?? '') : [];
    } catch { return []; }
}

// regex namespace object — AC-generated JS uses regex.match(s, p), etc.
const regex = {
    match:       regex_match,
    test:        regex_test,
    search:      regex_search,
    replace:     regex_replace,
    replace_all: regex_replace_all,
    count:       regex_count,
    escape:      regex_escape,
    find_all:    regex_find_all,
    split:       regex_split,
    groups:      regex_groups,
};
