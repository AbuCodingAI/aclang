#include "dns_c.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdlib>
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
std::atomic<bool> running{false};
int serverFd = -1;
std::thread serverThread;
bool atexitRegistered = false;
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

// Builds the reply for one query. The reply header is the query's, with QR and AA set,
// RA cleared, and NSCOUNT/ARCOUNT zeroed: the query may carry an EDNS OPT record in its
// additional section that the reply does not echo, so leaving ARCOUNT alone produced a
// packet whose counts disagree with its contents ("malformed message" in dig).
void answerQuery(int fd, const unsigned char* q, size_t n, const sockaddr_in& peer, socklen_t peerLen) {
    if (n < 12) return;
    if (q[2] & 0x80) return;                       // a response, not a query: never answer those
    size_t p = 12; std::string host;
    while (p < n) { unsigned len = q[p++]; if (!len) break; if (len > 63 || p + len > n) return; if (!host.empty()) host += '.'; host.append((const char*)q + p, len); p += len; }
    if (p + 4 > n) return;
    unsigned qtype = (q[p] << 8) | q[p + 1];
    unsigned qclass = (q[p + 2] << 8) | q[p + 3];

    Record rec; bool known;
    { std::lock_guard<std::mutex> lock(recordsMutex); auto it = records.find(keyFor(host.c_str())); known = it != records.end(); if (known) rec = it->second; }

    std::vector<unsigned char> out(q, q + p + 4);
    out[2] |= 0x84; out[2] &= ~0x02;               // QR + AA; the TC bit is never set here
    out[3] = 0;                                    // RA off, rcode NOERROR unless set below
    out[6] = out[7] = out[8] = out[9] = out[10] = out[11] = 0;   // ANCOUNT/NSCOUNT/ARCOUNT

    in_addr addr{};
    if (!known) {
        out[3] = 3;                                // NXDOMAIN
    } else if (qtype != 1 || qclass != 1 || !ipv4For(rec.backend, addr)) {
        if (!ipv4For(rec.backend, addr)) out[3] = 2;   // misconfigured record: SERVFAIL
        // else: a real name asked for a type we don't hold (AAAA, MX, ...) -> NODATA (NOERROR, 0 answers)
    } else {
        out[7] = 1;                                // ANCOUNT = 1
        out.insert(out.end(), {0xC0,0x0C,0,1,0,1,0,0,0,30,0,4});
        auto* b = (unsigned char*)&addr.s_addr; out.insert(out.end(), b, b + 4);
    }
    sendto(fd, out.data(), out.size(), 0, (const sockaddr*)&peer, peerLen);
}

// Takes its socket as an argument: the thread must never read the shared serverFd, which
// stop() clears from another thread.
void serve(int fd) {
    unsigned char buf[512];
    while (running.load()) {
        sockaddr_in peer{}; socklen_t len = sizeof(peer);
        ssize_t n = recvfrom(fd, buf, sizeof(buf), MSG_DONTWAIT, (sockaddr*)&peer, &len);
        if (n > 0) answerQuery(fd, buf, (size_t)n, peer, len);
        else std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

// Joins the server thread so the process never exits (or runs static destructors) while it
// is still joinable. Registered once, on the first ac_dns_start.
void stopLocked() {
    if (!running.load()) return;
    running.store(false);
    if (serverThread.joinable()) serverThread.join();
    close(serverFd);
    serverFd = -1;
}
void stopAtExit() {
    std::lock_guard<std::mutex> lock(serverMutex);
    stopLocked();
}
}

extern "C" {
int ac_dns_start(const char* address, int port) {
    std::lock_guard<std::mutex> lock(serverMutex);
    if (running.load() || !address || port < 1 || port > 65535) return running.load() ? 0 : -1;
    int fd = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0); if (fd < 0) return -1;
    sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, address, &addr.sin_addr) != 1 || bind(fd, (sockaddr*)&addr, sizeof(addr)) != 0) { close(fd); return -1; }
    serverFd = fd;
    running.store(true);
    serverThread = std::thread(serve, fd);
    if (!atexitRegistered) { std::atexit(stopAtExit); atexitRegistered = true; }
    return 0;
}
int ac_dns_url(const char* host, const char* display) { std::string key = keyFor(host); if (key.empty()) return -1; std::lock_guard<std::mutex> lock(recordsMutex); records[key].display = display ? display : ""; return 0; }
int ac_dns_backend(const char* host, const char* address) { std::string key = keyFor(host); if (key.empty() || !address || !*address) return -1; std::lock_guard<std::mutex> lock(recordsMutex); records[key].backend = address; return 0; }
const char* ac_dns_resolve(const char* host) { std::lock_guard<std::mutex> lock(recordsMutex); auto it = records.find(keyFor(host)); result = it == records.end() ? "" : it->second.backend; return result.c_str(); }
int ac_dns_remove(const char* host) { std::lock_guard<std::mutex> lock(recordsMutex); return records.erase(keyFor(host)) ? 0 : -1; }
const char* ac_dns_list(void) { std::lock_guard<std::mutex> lock(recordsMutex); std::ostringstream out; for (const auto& [host, rec] : records) out << host << " -> " << rec.backend << " [" << rec.display << "]\n"; result = out.str(); return result.c_str(); }
int ac_dns_stop(void) { std::lock_guard<std::mutex> lock(serverMutex); if (!running.load()) return 0; stopLocked(); return 0; }
}
