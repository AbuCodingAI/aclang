#pragma once
#ifdef __cplusplus
extern "C" {
#endif

int         ac_os_bash(const char* cmd);
int         ac_os_sbash(const char* cmd);
int         ac_os_app_open(const char* app);
int         ac_os_mkfile(const char* path);
int         ac_os_rmfile(const char* path);
int         ac_os_mkdir(const char* path);
int         ac_os_rmdir(const char* path);
int         ac_os_exists(const char* path);
const char* ac_os_cwd(void);
const char* ac_os_env(const char* key);
int         ac_os_write_to(const char* path, const char* content);
int         ac_os_append_to(const char* path, const char* content);
const char* ac_os_read(const char* path);

#ifdef __cplusplus
}
#endif

/* Call-form shims: the AC compiler emits os_x(...) on C and os.x(...) on C++. */
#ifdef __cplusplus
#include <string>
// Real overloaded static member functions, not raw function-pointer fields — a function
// pointer's signature is fixed at `const char*`, which does NOT implicitly accept a
// std::string argument (no user-defined conversion happens through a function pointer
// call), only a real overloaded FUNCTION does. Every widget's `.get()`/`.find()` returns
// std::string, and piping that straight into `os.write_to(path, content)` etc is a
// completely ordinary AC pattern — was a hard "cannot convert 'std::string' to 'const
// char*'" on every such call (verified: any widget .get() result passed to os.read/
// write_to/bash/append_to). One overload per const-char*/std::string combination per
// parameter.
struct _ac_os_ns {
    static int bash(const char* cmd) { return ac_os_bash(cmd); }
    static int bash(const std::string& cmd) { return ac_os_bash(cmd.c_str()); }
    static int sbash(const char* cmd) { return ac_os_sbash(cmd); }
    static int sbash(const std::string& cmd) { return ac_os_sbash(cmd.c_str()); }
    static int app_open(const char* app) { return ac_os_app_open(app); }
    static int app_open(const std::string& app) { return ac_os_app_open(app.c_str()); }
    static int mkfile(const char* path) { return ac_os_mkfile(path); }
    static int mkfile(const std::string& path) { return ac_os_mkfile(path.c_str()); }
    static int rmfile(const char* path) { return ac_os_rmfile(path); }
    static int rmfile(const std::string& path) { return ac_os_rmfile(path.c_str()); }
    static int mkdir(const char* path) { return ac_os_mkdir(path); }
    static int mkdir(const std::string& path) { return ac_os_mkdir(path.c_str()); }
    static int rmdir(const char* path) { return ac_os_rmdir(path); }
    static int rmdir(const std::string& path) { return ac_os_rmdir(path.c_str()); }
    static int exists(const char* path) { return ac_os_exists(path); }
    static int exists(const std::string& path) { return ac_os_exists(path.c_str()); }
    static const char* cwd() { return ac_os_cwd(); }
    static const char* env(const char* key) { return ac_os_env(key); }
    static const char* env(const std::string& key) { return ac_os_env(key.c_str()); }
    static const char* read(const char* path) { return ac_os_read(path); }
    static const char* read(const std::string& path) { return ac_os_read(path.c_str()); }
    static int write_to(const char* path, const char* content) { return ac_os_write_to(path, content); }
    static int write_to(const std::string& path, const char* content) { return ac_os_write_to(path.c_str(), content); }
    static int write_to(const char* path, const std::string& content) { return ac_os_write_to(path, content.c_str()); }
    static int write_to(const std::string& path, const std::string& content) { return ac_os_write_to(path.c_str(), content.c_str()); }
    static int append_to(const char* path, const char* content) { return ac_os_append_to(path, content); }
    static int append_to(const std::string& path, const char* content) { return ac_os_append_to(path.c_str(), content); }
    static int append_to(const char* path, const std::string& content) { return ac_os_append_to(path, content.c_str()); }
    static int append_to(const std::string& path, const std::string& content) { return ac_os_append_to(path.c_str(), content.c_str()); }
};
static _ac_os_ns os;
#else
#define os_bash ac_os_bash
#define os_sbash ac_os_sbash
#define os_app_open ac_os_app_open
#define os_mkfile ac_os_mkfile
#define os_rmfile ac_os_rmfile
#define os_mkdir ac_os_mkdir
#define os_rmdir ac_os_rmdir
#define os_exists ac_os_exists
#define os_cwd ac_os_cwd
#define os_env ac_os_env
#define os_read ac_os_read
#define os_write_to ac_os_write_to
#define os_append_to ac_os_append_to
#endif
