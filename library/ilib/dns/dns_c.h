#pragma once
#ifdef __cplusplus
extern "C" {
#endif
int ac_dns_start(const char*, int);
int ac_dns_url(const char*, const char*);
int ac_dns_backend(const char*, const char*);
const char* ac_dns_resolve(const char*);
int ac_dns_remove(const char*);
const char* ac_dns_list(void);
int ac_dns_stop(void);
#ifdef __cplusplus
}
struct _ac_dns_ns {
    static int start(const char* a, int p) { return ac_dns_start(a, p); }
    static int url(const char* h, const char* d) { return ac_dns_url(h, d); }
    static int backend(const char* h, const char* a) { return ac_dns_backend(h, a); }
    static const char* resolve(const char* h) { return ac_dns_resolve(h); }
    static int remove(const char* h) { return ac_dns_remove(h); }
    static const char* list() { return ac_dns_list(); }
    static int stop() { return ac_dns_stop(); }
};
static _ac_dns_ns dns;
#else
#define dns_start ac_dns_start
#define dns_url ac_dns_url
#define dns_backend ac_dns_backend
#define dns_resolve ac_dns_resolve
#define dns_remove ac_dns_remove
#define dns_list ac_dns_list
#define dns_stop ac_dns_stop
#endif
