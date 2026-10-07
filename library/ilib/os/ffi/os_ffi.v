// AC ilib: os — V C interop (libacoos.so)
// Inlined by AC->V compiler when "use ilib os" is declared.
#flag -L @AC_LIBDIR@ -lacoos
#flag -Wl,-rpath,@AC_LIBDIR@
#include "@AC_LIBDIR@/os_c.h"

fn C.ac_os_bash(cmd &char) int
fn C.ac_os_sbash(cmd &char) int
fn C.ac_os_app_open(app &char) int
fn C.ac_os_mkfile(path &char) int
fn C.ac_os_rmfile(path &char) int
fn C.ac_os_mkdir(path &char) int
fn C.ac_os_rmdir(path &char) int
fn C.ac_os_exists(path &char) int
fn C.ac_os_cwd() &char
fn C.ac_os_env(key &char) &char
fn C.ac_os_write_to(path &char, content &char) int
fn C.ac_os_append_to(path &char, content &char) int
fn C.ac_os_read(path &char) &char
fn C.ac_os_isdir(path &char) int
fn C.ac_os_isfile(path &char) int
fn C.ac_os_size(path &char) i64
fn C.ac_os_mtime(path &char) i64
fn C.ac_os_copy(src &char, dst &char) int
fn C.ac_os_move(src &char, dst &char) int
fn C.ac_os_listdir(path &char, out_count &int) &&char
fn C.ac_os_free_list(list &&char, count int)
fn C.ac_os_tmpdir() &char
fn C.ac_os_tmpfile(suffix &char) &char
fn C.ac_os_mktmpdir() &char
fn C.ac_os_chdir(path &char) int
fn C.ac_os_join(a &char, b &char) &char
fn C.ac_os_basename(path &char) &char
fn C.ac_os_dirname(path &char) &char
fn C.ac_os_homedir() &char

// V struct names must begin with a capital letter (unlike function/field names, which
// must NOT) — "_AcOsNS" hard-errored ("struct name must begin with capital letter"), and
// separately every method here was missing its receiver VARIABLE name (V requires
// `fn (recv Type)`, not a bare `fn (Type)` — "expecting type declaration").
struct AcOsNS {}
fn (o AcOsNS) bash(cmd string) int      { return C.ac_os_bash(cmd.str) }
fn (o AcOsNS) sbash(cmd string) int     { return C.ac_os_sbash(cmd.str) }
fn (o AcOsNS) app_open(app string) int  { return C.ac_os_app_open(app.str) }
fn (o AcOsNS) mkfile(p string) int      { return C.ac_os_mkfile(p.str) }
fn (o AcOsNS) rmfile(p string) int      { return C.ac_os_rmfile(p.str) }
fn (o AcOsNS) mkdir(p string) int       { return C.ac_os_mkdir(p.str) }
fn (o AcOsNS) rmdir(p string) int       { return C.ac_os_rmdir(p.str) }
fn (o AcOsNS) exists(p string) bool     { return C.ac_os_exists(p.str) != 0 }
fn (o AcOsNS) cwd() string              { return unsafe { cstring_to_vstring(C.ac_os_cwd()) } }
fn (o AcOsNS) env(k string) string      { return unsafe { cstring_to_vstring(C.ac_os_env(k.str)) } }
fn (o AcOsNS) write_to(p string, c string) int  { return C.ac_os_write_to(p.str, c.str) }
fn (o AcOsNS) append_to(p string, c string) int { return C.ac_os_append_to(p.str, c.str) }
fn (o AcOsNS) read(p string) string     { return unsafe { cstring_to_vstring(C.ac_os_read(p.str)) } }
fn (o AcOsNS) isdir(p string) int                 { return C.ac_os_isdir(p.str) }
fn (o AcOsNS) isfile(p string) int                { return C.ac_os_isfile(p.str) }
fn (o AcOsNS) size(p string) i64                  { return C.ac_os_size(p.str) }
fn (o AcOsNS) mtime(p string) i64                 { return C.ac_os_mtime(p.str) }
fn (o AcOsNS) copy(src string, dst string) int    { return C.ac_os_copy(src.str, dst.str) }
fn (o AcOsNS) move(src string, dst string) int    { return C.ac_os_move(src.str, dst.str) }
fn (o AcOsNS) listdir(p string) []string {
    mut n := 0
    arr := C.ac_os_listdir(p.str, &n)
    if arr == unsafe { nil } { return []string{} }
    mut result := []string{cap: n}
    for i in 0 .. n {
        result << unsafe { cstring_to_vstring(*arr[i]) }
    }
    C.ac_os_free_list(arr, n)
    return result
}
fn (o AcOsNS) tmpdir() string                     { return unsafe { cstring_to_vstring(C.ac_os_tmpdir()) } }
fn (o AcOsNS) tmpfile(suffix string) string       { return unsafe { cstring_to_vstring(C.ac_os_tmpfile(suffix.str)) } }
fn (o AcOsNS) mktmpdir() string                   { return unsafe { cstring_to_vstring(C.ac_os_mktmpdir()) } }
fn (o AcOsNS) chdir(p string) int                 { return C.ac_os_chdir(p.str) }
fn (o AcOsNS) join(a string, b string) string     { return unsafe { cstring_to_vstring(C.ac_os_join(a.str, b.str)) } }
fn (o AcOsNS) basename(p string) string           { return unsafe { cstring_to_vstring(C.ac_os_basename(p.str)) } }
fn (o AcOsNS) dirname(p string) string            { return unsafe { cstring_to_vstring(C.ac_os_dirname(p.str)) } }
fn (o AcOsNS) homedir() string                    { return unsafe { cstring_to_vstring(C.ac_os_homedir()) } }

const os = AcOsNS{}
