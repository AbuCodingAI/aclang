#ifndef AC_WEBSERVER_HPP
#define AC_WEBSERVER_HPP
// The C++ backend expects `<lib>.hpp`; the C backend expects `<lib>_c.h`. Both are
// satisfied by the same declarations (web-server_c.h already handles the C++-only
// `server` namespace object via #ifdef __cplusplus), so this just forwards.
#include "web-server_c.h"
#endif // AC_WEBSERVER_HPP
