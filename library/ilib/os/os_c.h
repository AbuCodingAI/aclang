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
struct _ac_os_ns {
    int (*bash)(const char*) = ac_os_bash;
    int (*sbash)(const char*) = ac_os_sbash;
    int (*app_open)(const char*) = ac_os_app_open;
    int (*mkfile)(const char*) = ac_os_mkfile;
    int (*rmfile)(const char*) = ac_os_rmfile;
    int (*mkdir)(const char*) = ac_os_mkdir;
    int (*rmdir)(const char*) = ac_os_rmdir;
    int (*exists)(const char*) = ac_os_exists;
    const char* (*cwd)() = ac_os_cwd;
    const char* (*env)(const char*) = ac_os_env;
    const char* (*read)(const char*) = ac_os_read;
    int (*write_to)(const char*, const char*) = ac_os_write_to;
    int (*append_to)(const char*, const char*) = ac_os_append_to;
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
