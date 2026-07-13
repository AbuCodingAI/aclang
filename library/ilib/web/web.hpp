#ifndef AC_WEB_HPP
#define AC_WEB_HPP

#ifdef __cplusplus
extern "C" {
#endif

void ac_web_open(const char* link);
void ac_web_file_open(const char* file);
void ac_web_popen(const char* raw_link);
void ac_web_ropen(const char* identifier);
void ac_web_browser();
int ac_web_pdf(const char* pdf);
int ac_web_text(const char* text);
int ac_web_inspect(const char* program);
void ac_web_ac_page();
const char* ac_web_page_get(const char* url);
const char* ac_web_help();

static inline void web_open(const char* link) { ac_web_open(link); }
static inline void web_file_open(const char* file) { ac_web_file_open(file); }
static inline void web_popen(const char* raw_link) { ac_web_popen(raw_link); }
static inline void web_ropen(const char* identifier) { ac_web_ropen(identifier); }
static inline void web_browser() { ac_web_browser(); }
static inline int web_pdf(const char* pdf) { return ac_web_pdf(pdf); }
static inline int web_text(const char* text) { return ac_web_text(text); }
static inline int web_inspect(const char* program) { return ac_web_inspect(program); }
static inline void web_ac_page() { ac_web_ac_page(); }
static inline const char* web_page_get(const char* url) { return ac_web_page_get(url); }
static inline const char* web_help() { return ac_web_help(); }

#ifdef __cplusplus
}

struct ACWebNamespace {
    void open(const char* link) const { ac_web_open(link); }
    void file_open(const char* file) const { ac_web_file_open(file); }
    void popen(const char* raw_link) const { ac_web_popen(raw_link); }
    void ropen(const char* identifier) const { ac_web_ropen(identifier); }
    void browser() const { ac_web_browser(); }
    int pdf(const char* pdf) const { return ac_web_pdf(pdf); }
    int text(const char* text) const { return ac_web_text(text); }
    int inspect(const char* program) const { return ac_web_inspect(program); }
    void ac_page() const { ac_web_ac_page(); }
    const char* page_get(const char* url) const { return ac_web_page_get(url); }
    const char* help() const { return ac_web_help(); }
};

static const ACWebNamespace web;
#endif

#endif // AC_WEB_HPP
