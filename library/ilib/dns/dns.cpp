#include "dns_c.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {
struct Record { std::string display, backend; };
std::map<std::string, Record> records;
std::mutex recordsMutex, serverMutex;
int serverFd = -1;
bool running = false;
std::thread serverThread;
thread_local std::string result;

std::string keyFor(const char* raw) {
    if (!raw) return {};
    std::string s(raw); auto scheme = s.find("://");
    if (scheme != std::string::npos) s.erase(0, scheme + 3);
    auto slash = s.find('/'); if (slash != std::string::npos) s.resize(slash);
    while (!s.empty() && s.back() == '.') s.pop_back();
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

bool ipv4For(std::string address, in_addr& out) {
    auto colon = address.rfind(':');
    if (colon != std::string::npos && address.find(':') == colon) address.resize(colon);
    return inet_pton(AF_INET, address.c_str(), &out) == 1;
}

void answerQuery(const unsigned char* q, size_t n, const sockaddr_in& peer, socklen_t peerLen) {
    if (n < 12) return;
    size_t p = 12; std::string host;
    while (p < n) { unsigned len = q[p++]; if (!len) break; if (len > 63 || p + len > n) return; if (!host.empty()) host += '.'; host.append((const char*)q + p, len); p += len; }
    if (p + 4 > n) return;
    std::string backend; { std::lock_guard<std::mutex> lock(recordsMutex); auto it = records.find(keyFor(host.c_str())); if (it != records.end()) backend = it->second.backend; }
    std::vector<unsigned char> out(q, q + p + 4); out[2] |= 0x84; out[3] = 0;
    in_addr addr{};
    if (backend.empty() || !ipv4For(backend, addr)) { out[3] = 3; out[6] = out[7] = 0; }
    else { out[6] = 0; out[7] = 1; out.insert(out.end(), {0xC0,0x0C,0,1,0,1,0,0,0,30,0,4}); auto* b = (unsigned char*)&addr.s_addr; out.insert(out.end(), b, b + 4); }
    sendto(serverFd, out.data(), out.size(), 0, (const sockaddr*)&peer, peerLen);
}

void serve() {
    unsigned char buf[512];
    while (running) { sockaddr_in peer{}; socklen_t len = sizeof(peer); ssize_t n = recvfrom(serverFd, buf, sizeof(buf), MSG_DONTWAIT, (sockaddr*)&peer, &len); if (n > 0) answerQuery(buf, (size_t)n, peer, len); else std::this_thread::sleep_for(std::chrono::milliseconds(5)); }
}
}

extern "C" {
int ac_dns_start(const char* address, int port) {
    std::lock_guard<std::mutex> lock(serverMutex); if (running || !address || port < 1 || port > 65535) return running ? 0 : -1;
    int fd = socket(AF_INET, SOCK_DGRAM, 0); if (fd < 0) return -1;
    sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, address, &addr.sin_addr) != 1 || bind(fd, (sockaddr*)&addr, sizeof(addr)) != 0) { close(fd); return -1; }
    serverFd = fd; running = true; serverThread = std::thread(serve); return 0;
}
int ac_dns_url(const char* host, const char* display) { std::string key = keyFor(host); if (key.empty()) return -1; std::lock_guard<std::mutex> lock(recordsMutex); records[key].display = display ? display : ""; return 0; }
int ac_dns_backend(const char* host, const char* address) { std::string key = keyFor(host); if (key.empty() || !address || !*address) return -1; std::lock_guard<std::mutex> lock(recordsMutex); records[key].backend = address; return 0; }
const char* ac_dns_resolve(const char* host) { std::lock_guard<std::mutex> lock(recordsMutex); auto it = records.find(keyFor(host)); result = it == records.end() ? "" : it->second.backend; return result.c_str(); }
int ac_dns_remove(const char* host) { std::lock_guard<std::mutex> lock(recordsMutex); return records.erase(keyFor(host)) ? 0 : -1; }
const char* ac_dns_list(void) { std::lock_guard<std::mutex> lock(recordsMutex); std::ostringstream out; for (const auto& [host, rec] : records) out << host << " -> " << rec.backend << " [" << rec.display << "]\n"; result = out.str(); return result.c_str(); }
int ac_dns_stop(void) { std::lock_guard<std::mutex> lock(serverMutex); if (!running) return 0; running = false; close(serverFd); serverFd = -1; if (serverThread.joinable()) serverThread.join(); return 0; }
}
