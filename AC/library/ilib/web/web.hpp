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
const char* ac_web_help();

#ifdef __cplusplus
}
#endif

#endif // AC_WEB_HPP
