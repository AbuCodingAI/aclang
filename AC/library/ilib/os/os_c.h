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
