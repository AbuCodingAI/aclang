// AC ilib: os — C implementation (libacoos.so)
// Exposes os.bash, os.sbash, os.mkfile, os.rmfile, etc. via extern "C".
#include "os_c.h"
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#include <direct.h>
#include <unordered_map>
#else
#include <unistd.h>
#include <sys/wait.h>
#include <dirent.h>
#endif
#include <errno.h>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>

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

#ifdef _WIN32
static int _rmdir_r(const char* path) {
    std::string pattern = std::string(path) + "/*";
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return -1;
    do {
        if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, "..")) continue;
        std::string child = std::string(path) + "/" + fd.cFileName;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            _rmdir_r(child.c_str());
        else
            DeleteFileA(child.c_str());
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    return RemoveDirectoryA(path) ? 0 : -1;
}
#else
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
#endif

extern "C" {

#ifdef _WIN32
// fork()+exec() has no Windows equivalent; CreateProcess gives back a HANDLE, not the
// pid alone, so ac_os_wait needs somewhere to find it again by pid — this registry is
// that (a fork/waitpid pair never needed one, since waitpid works from the pid alone).
static std::unordered_map<DWORD, HANDLE> _win_procs;

int ac_os_bash(const char* cmd) {
    if (!cmd) return -1;
    std::string full = std::string("cmd.exe /c ") + cmd;
    std::vector<char> buf(full.begin(), full.end());
    buf.push_back('\0');
    STARTUPINFOA si{}; si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessA(NULL, buf.data(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
        return -1;
    CloseHandle(pi.hThread);
    _win_procs[pi.dwProcessId] = pi.hProcess;
    return (int)pi.dwProcessId;
}

int ac_os_wait(int pid) {
    if (pid <= 0) return -1;
    auto it = _win_procs.find((DWORD)pid);
    if (it == _win_procs.end()) return -1;
    WaitForSingleObject(it->second, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(it->second, &code);
    CloseHandle(it->second);
    _win_procs.erase(it);
    return (int)code;
}
#else
int ac_os_bash(const char* cmd) {
    if (!cmd) return -1;
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        execl("/bin/sh", "sh", "-c", cmd, (char*)nullptr);
        _exit(127);
    }
    return (int)pid;
}

int ac_os_wait(int pid) {
    if (pid <= 0) return -1;
    int status = 0;
    if (waitpid((pid_t)pid, &status, 0) < 0) return -1;
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}
#endif

int ac_os_sbash(const char* cmd) {
    if (!cmd || !_sbash_check(cmd)) {
        fprintf(stderr, "[os.sbash] The p in bash stands for protection\n");
        return -1;
    }
    return system(cmd);
}

#ifdef _WIN32
int ac_os_app_open(const char* app) {
    if (!app) return -1;
    // ShellExecute hands the launch to whatever the OS has registered for this file/URL/
    // app — the same role xdg-open/open play on Linux/macOS, minus the manual probing.
    HINSTANCE r = ShellExecuteA(NULL, "open", app, NULL, NULL, SW_SHOWNORMAL);
    return ((INT_PTR)r > 32) ? 0 : -1;
}
#else
int ac_os_app_open(const char* app) {
    if (!app) return -1;
    // Detect an opener via FIXED-string probes (no injection possible).
    const char* opener = nullptr;
    if (system("which xdg-open > /dev/null 2>&1") == 0) opener = "xdg-open";
    else if (system("which open > /dev/null 2>&1") == 0) opener = "open";
    // Launch WITHOUT a shell (execvp with `app` as a distinct argv element), detached in
    // the background. The old `system(app + " &")` let `app` inject shell commands.
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid == 0) {
        setsid();  // detach so it keeps running in the background
        if (opener) { char* av[] = { (char*)opener, (char*)app, nullptr }; execvp(opener, av); }
        else        { char* av[] = { (char*)app, nullptr };                execvp(app, av); }
        _exit(127);
    }
    return 0;  // launched
}
#endif

int ac_os_mkfile(const char* path) {
    if (!path) return -1;
    FILE* f = fopen(path, "a");
    if (!f) { fprintf(stderr, "[os.mkfile] %s: %s\n", path, strerror(errno)); return -1; }
    fclose(f);
    return 0;
}

int ac_os_rmfile(const char* path) {
    // remove() is plain ISO C — identical behavior to unlink() for a regular file on
    // POSIX, and the one call that actually exists on both platforms.
    if (!path) return -1;
    if (remove(path) != 0) { fprintf(stderr, "[os.rmfile] %s: %s\n", path, strerror(errno)); return -1; }
    return 0;
}

#ifdef _WIN32
static int _mkdir_one(const char* p) { return _mkdir(p); }
#else
static int _mkdir_one(const char* p) { return mkdir(p, 0755); }
#endif

int ac_os_mkdir(const char* path) {
    if (!path) return -1;
    std::string p(path);
    for (size_t i = 1; i < p.size(); i++) {
        if (p[i] == '/') {
            p[i] = '\0';
            _mkdir_one(p.c_str());
            p[i] = '/';
        }
    }
    if (_mkdir_one(p.c_str()) != 0 && errno != EEXIST) {
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

int ac_os_pid(int status) {
    return status > 0 ? status : -1;
}

const char* ac_os_cwd() {
    char buf[4096] = {};
#ifdef _WIN32
    _getcwd(buf, sizeof(buf));
#else
    getcwd(buf, sizeof(buf));
#endif
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
