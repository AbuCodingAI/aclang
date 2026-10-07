#pragma once
#ifdef __cplusplus
extern "C" {
#endif

int         ac_os_bash(const char* cmd);
int         ac_os_wait(int pid);
int         ac_os_sbash(const char* cmd);
int         ac_os_app_open(const char* app);
int         ac_os_mkfile(const char* path);
int         ac_os_rmfile(const char* path);
int         ac_os_mkdir(const char* path);
int         ac_os_rmdir(const char* path);
int         ac_os_exists(const char* path);
int         ac_os_pid(int status);
const char* ac_os_cwd(void);
const char* ac_os_env(const char* key);
int         ac_os_write_to(const char* path, const char* content);
int         ac_os_append_to(const char* path, const char* content);
const char* ac_os_read(const char* path);

/* files, directories, temp paths (see os_c.cpp) */
int         ac_os_isdir(const char* path);
int         ac_os_isfile(const char* path);
long long   ac_os_size(const char* path);
long long   ac_os_mtime(const char* path);
int         ac_os_copy(const char* src, const char* dst);
int         ac_os_move(const char* src, const char* dst);
char**      ac_os_listdir(const char* path, int* out_count);
void        ac_os_free_list(char** list, int count);
const char* ac_os_tmpdir(void);
const char* ac_os_tmpfile(const char* suffix);
const char* ac_os_mktmpdir(void);
int         ac_os_chdir(const char* path);
const char* ac_os_join(const char* a, const char* b);
const char* ac_os_basename(const char* path);
const char* ac_os_dirname(const char* path);
const char* ac_os_homedir(void);

#ifdef __cplusplus
}
#endif

/* Call-form shims: the AC compiler emits os_x(...) on C and os.x(...) on C++. */
#ifdef __cplusplus
#include <string>
#include <vector>
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
    static int wait(int pid) { return ac_os_wait(pid); }
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
    static int pid(int status) { return ac_os_pid(status); }
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
    static int isdir(const char* path) { return ac_os_isdir(path); }
    static int isdir(const std::string& path) { return ac_os_isdir(path.c_str()); }
    static int isfile(const char* path) { return ac_os_isfile(path); }
    static int isfile(const std::string& path) { return ac_os_isfile(path.c_str()); }
    static long long size(const char* path) { return ac_os_size(path); }
    static long long size(const std::string& path) { return ac_os_size(path.c_str()); }
    static long long mtime(const char* path) { return ac_os_mtime(path); }
    static long long mtime(const std::string& path) { return ac_os_mtime(path.c_str()); }
    static int copy(const char* src, const char* dst) { return ac_os_copy(src, dst); }
    static int copy(const std::string& src, const std::string& dst) { return ac_os_copy(src.c_str(), dst.c_str()); }
    static int move(const char* src, const char* dst) { return ac_os_move(src, dst); }
    static int move(const std::string& src, const std::string& dst) { return ac_os_move(src.c_str(), dst.c_str()); }
    static std::vector<std::string> listdir(const char* path) {
        int n = 0;
        char** raw = ac_os_listdir(path, &n);
        std::vector<std::string> out;
        for (int i = 0; i < n; i++) out.push_back(raw[i]);
        ac_os_free_list(raw, n);
        return out;
    }
    static std::vector<std::string> listdir(const std::string& path) { return listdir(path.c_str()); }
    static const char* tmpdir() { return ac_os_tmpdir(); }
    static const char* tmpfile(const char* suffix) { return ac_os_tmpfile(suffix); }
    static const char* tmpfile(const std::string& suffix) { return ac_os_tmpfile(suffix.c_str()); }
    static const char* mktmpdir() { return ac_os_mktmpdir(); }
    static int chdir(const char* path) { return ac_os_chdir(path); }
    static int chdir(const std::string& path) { return ac_os_chdir(path.c_str()); }
    static const char* join(const char* a, const char* b) { return ac_os_join(a, b); }
    static const char* join(const std::string& a, const std::string& b) { return ac_os_join(a.c_str(), b.c_str()); }
    static const char* basename(const char* path) { return ac_os_basename(path); }
    static const char* basename(const std::string& path) { return ac_os_basename(path.c_str()); }
    static const char* dirname(const char* path) { return ac_os_dirname(path); }
    static const char* dirname(const std::string& path) { return ac_os_dirname(path.c_str()); }
    static const char* homedir() { return ac_os_homedir(); }
};
static _ac_os_ns os;
#else
#define os_bash ac_os_bash
#define os_wait ac_os_wait
#define os_sbash ac_os_sbash
#define os_app_open ac_os_app_open
#define os_mkfile ac_os_mkfile
#define os_rmfile ac_os_rmfile
#define os_mkdir ac_os_mkdir
#define os_rmdir ac_os_rmdir
#define os_exists ac_os_exists
#define os_pid ac_os_pid
#define os_cwd ac_os_cwd
#define os_env ac_os_env
#define os_read ac_os_read
#define os_write_to ac_os_write_to
#define os_append_to ac_os_append_to
#define os_isdir ac_os_isdir
#define os_isfile ac_os_isfile
#define os_size ac_os_size
#define os_mtime ac_os_mtime
#define os_copy ac_os_copy
#define os_move ac_os_move
#define os_listdir ac_os_listdir
#define os_tmpdir ac_os_tmpdir
#define os_tmpfile ac_os_tmpfile
#define os_mktmpdir ac_os_mktmpdir
#define os_chdir ac_os_chdir
#define os_join ac_os_join
#define os_basename ac_os_basename
#define os_dirname ac_os_dirname
#define os_homedir ac_os_homedir
#endif
