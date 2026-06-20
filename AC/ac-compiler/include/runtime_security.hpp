// AC Runtime Security: Anti-DDoS, Anti-Ping-Flood, Rate Limiting
// Prevents malicious patterns that could be used to attack servers

#pragma once
#include <string>
#include <unordered_map>
#include <queue>
#include <chrono>
#include <mutex>
#include <atomic>
#include <vector>

namespace AC_Runtime {

// ============================================================================
// RATE LIMITING & LOOP DETECTION
// ============================================================================

class RateLimiter {
public:
    // Check if action (request, ping, etc.) is allowed
    // Returns: true if allowed, false if rate-limited
    bool allow(const std::string& action_id);

    // Report suspicious loop pattern (repeated action in tight loop)
    void report_loop_pattern(const std::string& action_id);

    // Get current request count per second
    int get_requests_per_second(const std::string& action_id);

    // Reset (for testing)
    void reset();

private:
    struct ActionMetrics {
        std::queue<std::chrono::steady_clock::time_point> timestamps;
        int consecutive_rapid_calls = 0;
        bool loop_pattern_detected = false;
        std::chrono::steady_clock::time_point last_block_time;
    };

    std::unordered_map<std::string, ActionMetrics> metrics;
    std::mutex mtx;

    static constexpr int MAX_REQUESTS_PER_SECOND = 10;
    static constexpr int LOOP_PATTERN_THRESHOLD = 10;  // 10 rapid calls = loop
    static constexpr int BLOCK_DURATION_MS = 1000;      // Block for 1 second
};

// ============================================================================
// PING FLOOD DETECTION
// ============================================================================

class PingFloodDetector {
public:
    // Check if ping is allowed to target
    // Returns: true if allowed, false if flood detected
    bool allow_ping(const std::string& target);

    // Get ping count to target in last 10 seconds
    int get_ping_count(const std::string& target);

    // Reset (for testing)
    void reset();

private:
    struct PingMetrics {
        std::vector<std::chrono::steady_clock::time_point> pings;
        bool flood_detected = false;
        std::chrono::steady_clock::time_point block_until;
    };

    std::unordered_map<std::string, PingMetrics> ping_history;
    std::mutex mtx;

    static constexpr int MAX_PINGS_PER_10_SECONDS = 5;
    static constexpr int BLOCK_DURATION_MS = 60000;  // Block for 60 seconds
};

// ============================================================================
// CONNECTION POOLING LIMITS
// ============================================================================

class ConnectionPool {
public:
    // Try to acquire a connection slot
    // Returns: true if acquired, false if limit reached
    bool acquire_connection(const std::string& pool_id);

    // Release a connection slot
    void release_connection(const std::string& pool_id);

    // Get current connection count
    int get_connection_count(const std::string& pool_id);

    // Reset (for testing)
    void reset();

private:
    struct PoolMetrics {
        std::atomic<int> active_connections = 0;
    };

    std::unordered_map<std::string, PoolMetrics> pools;
    std::mutex mtx;

    static constexpr int MAX_CONNECTIONS_PER_POOL = 100;
};

// ============================================================================
// RESOURCE LIMITS
// ============================================================================

class ResourceLimiter {
public:
    // Check if memory allocation is allowed
    // Returns: true if allowed, false if would exceed limit
    bool allow_memory_allocation(size_t bytes);

    // Track actual memory usage
    void track_memory_usage(size_t bytes, bool allocate);

    // Check if CPU time is excessive
    // Returns: true if normal, false if should throttle
    bool check_cpu_usage();

    // Get current memory usage
    size_t get_memory_usage();

    // Reset (for testing)
    void reset();

private:
    std::atomic<size_t> current_memory_usage = 0;
    std::chrono::steady_clock::time_point last_cpu_check;
    std::atomic<int> cpu_spin_count = 0;
    std::mutex mtx;

    static constexpr size_t MAX_MEMORY_PER_PROCESS = 500 * 1024 * 1024;  // 500MB
    static constexpr int MAX_CPU_SPIN_ITERATIONS = 1000000;  // Before throttle
};

// ============================================================================
// GLOBAL SECURITY MANAGER
// ============================================================================

class SecurityManager {
public:
    static SecurityManager& instance();

    // Anti-DDoS: Check request is allowed
    bool allow_request(const std::string& endpoint);

    // Anti-Ping-Flood: Check ping is allowed
    bool allow_ping(const std::string& target);

    // Connection management
    bool acquire_connection();
    void release_connection();

    // Resource management
    bool allow_memory_allocation(size_t bytes);
    void track_memory_usage(size_t bytes, bool allocate);
    bool check_cpu_usage();

    // Reporting
    void report_loop_pattern(const std::string& action);
    void report_flood_attempt(const std::string& type, const std::string& target);

    // Statistics
    std::string get_security_stats();

private:
    SecurityManager() = default;

    RateLimiter rate_limiter;
    PingFloodDetector ping_detector;
    ConnectionPool connection_pool;
    ResourceLimiter resource_limiter;

    std::atomic<int> total_blocks = 0;
    std::atomic<int> total_pings_blocked = 0;
};

}  // namespace AC_Runtime
