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

struct _AcOsNS {}
fn (_AcOsNS) bash(cmd string) int      { return C.ac_os_bash(cmd.str) }
fn (_AcOsNS) sbash(cmd string) int     { return C.ac_os_sbash(cmd.str) }
fn (_AcOsNS) app_open(app string) int  { return C.ac_os_app_open(app.str) }
fn (_AcOsNS) mkfile(p string) int      { return C.ac_os_mkfile(p.str) }
fn (_AcOsNS) rmfile(p string) int      { return C.ac_os_rmfile(p.str) }
fn (_AcOsNS) mkdir(p string) int       { return C.ac_os_mkdir(p.str) }
fn (_AcOsNS) rmdir(p string) int       { return C.ac_os_rmdir(p.str) }
fn (_AcOsNS) exists(p string) bool     { return C.ac_os_exists(p.str) != 0 }
fn (_AcOsNS) cwd() string              { return unsafe { cstring_to_vstring(C.ac_os_cwd()) } }
fn (_AcOsNS) env(k string) string      { return unsafe { cstring_to_vstring(C.ac_os_env(k.str)) } }
fn (_AcOsNS) write_to(p string, c string) int  { return C.ac_os_write_to(p.str, c.str) }
fn (_AcOsNS) append_to(p string, c string) int { return C.ac_os_append_to(p.str, c.str) }
fn (_AcOsNS) read(p string) string     { return unsafe { cstring_to_vstring(C.ac_os_read(p.str)) } }

const os = _AcOsNS{}
