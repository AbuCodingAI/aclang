// AC ilib: os — C implementation (libacoos.so)
// Exposes os.bash, os.sbash, os.mkfile, os.rmfile, etc. via extern "C".
#include "os_c.h"
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <fstream>
#include <string>
#include <sstream>

static thread_local char _os_buf[65536];

static const char* _ret(const std::string& s) {
    size_t n = s.size();
    if (n >= sizeof(_os_buf)) n = sizeof(_os_buf) - 1;
    memcpy(_os_buf, s.data(), n);
    _os_buf[n] = '\0';
    return _os_buf;
}

// sbash forbidden patterns (simplified — no regex, just string search)
static const char* _SBASH_BLOCKED[] = {
    "sudo", "su ", "function ", "nohup", "disown", nullptr
};

static int _sbash_check(const char* cmd) {
    for (int i = 0; _SBASH_BLOCKED[i]; i++)
        if (strstr(cmd, _SBASH_BLOCKED[i])) return 0;
    // block background (&) and tmux/screen
    if (strchr(cmd, '&')) return 0;
    if (strstr(cmd, "tmux") || strstr(cmd, "screen")) return 0;
    return 1;
}

static int _rmdir_r(const char* path) {
    DIR* d = opendir(path);
    if (!d) return -1;
    struct dirent* e;
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        std::string child = std::string(path) + "/" + e->d_name;
        struct stat st{};
        if (stat(child.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
            _rmdir_r(child.c_str());
        else
            unlink(child.c_str());
    }
    closedir(d);
    return rmdir(path);
}

extern "C" {

int ac_os_bash(const char* cmd) {
    if (!cmd) return -1;
    return system(cmd);
}

int ac_os_sbash(const char* cmd) {
    if (!cmd || !_sbash_check(cmd)) {
        fprintf(stderr, "[os.sbash] blocked\n");
        return -1;
    }
    return system(cmd);
}

int ac_os_app_open(const char* app) {
    if (!app) return -1;
    std::string cmd;
    if (system("which xdg-open > /dev/null 2>&1") == 0) cmd = "xdg-open ";
    else if (system("which open > /dev/null 2>&1") == 0) cmd = "open ";
    else cmd = "";
    cmd += std::string(app) + " &";
    return system(cmd.c_str()) == 0 ? 0 : -1;
}

int ac_os_mkfile(const char* path) {
    if (!path) return -1;
    FILE* f = fopen(path, "a");
    if (!f) { fprintf(stderr, "[os.mkfile] %s: %s\n", path, strerror(errno)); return -1; }
    fclose(f);
    return 0;
}

int ac_os_rmfile(const char* path) {
    if (!path) return -1;
    if (unlink(path) != 0) { fprintf(stderr, "[os.rmfile] %s: %s\n", path, strerror(errno)); return -1; }
    return 0;
}

int ac_os_mkdir(const char* path) {
    if (!path) return -1;
    std::string p(path);
    for (size_t i = 1; i < p.size(); i++) {
        if (p[i] == '/') {
            p[i] = '\0';
            mkdir(p.c_str(), 0755);
            p[i] = '/';
        }
    }
    if (mkdir(p.c_str(), 0755) != 0 && errno != EEXIST) {
        fprintf(stderr, "[os.mkdir] %s: %s\n", path, strerror(errno)); return -1;
    }
    return 0;
}

int ac_os_rmdir(const char* path) {
    if (!path) return -1;
    if (_rmdir_r(path) != 0) { fprintf(stderr, "[os.rmdir] %s: %s\n", path, strerror(errno)); return -1; }
    return 0;
}

int ac_os_exists(const char* path) {
    if (!path) return 0;
    struct stat st{};
    return stat(path, &st) == 0 ? 1 : 0;
}

const char* ac_os_cwd() {
    char buf[4096] = {};
    getcwd(buf, sizeof(buf));
    return _ret(buf);
}

const char* ac_os_env(const char* key) {
    if (!key) return "";
    const char* v = getenv(key);
    return v ? v : "";
}

int ac_os_write_to(const char* path, const char* content) {
    if (!path || !content) return -1;
    FILE* f = fopen(path, "w");
    if (!f) { fprintf(stderr, "[os.write_to] %s: %s\n", path, strerror(errno)); return -1; }
    fputs(content, f);
    fclose(f);
    return 0;
}

int ac_os_append_to(const char* path, const char* content) {
    if (!path || !content) return -1;
    FILE* f = fopen(path, "a");
    if (!f) { fprintf(stderr, "[os.append_to] %s: %s\n", path, strerror(errno)); return -1; }
    fputs(content, f);
    size_t len = strlen(content);
    if (len == 0 || content[len-1] != '\n') fputc('\n', f);
    fclose(f);
    return 0;
}

const char* ac_os_read(const char* path) {
    if (!path) return "";
    std::ifstream f(path);
    if (!f) { fprintf(stderr, "[os.read] %s: %s\n", path, strerror(errno)); return ""; }
    std::ostringstream ss;
    ss << f.rdbuf();
    return _ret(ss.str());
}

} // extern "C"
