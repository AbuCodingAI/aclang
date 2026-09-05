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

const os = AcOsNS{}
