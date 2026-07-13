// AC Runtime Security Implementation
#include "../include/runtime_security.hpp"
#include <iostream>
#include <algorithm>
#include <thread>

namespace AC_Runtime {

// ============================================================================
// RATE LIMITER IMPLEMENTATION
// ============================================================================

bool RateLimiter::allow(const std::string& action_id) {
    std::lock_guard<std::mutex> lock(mtx);
    auto now = std::chrono::steady_clock::now();
    auto& metrics = metrics[action_id];

    // Clean old timestamps (older than 1 second)
    while (!metrics.timestamps.empty()) {
        auto age_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - metrics.timestamps.front()).count();
        if (age_ms > 1000) {
            metrics.timestamps.pop();
        } else {
            break;
        }
    }

    // Check if currently blocked
    if (metrics.loop_pattern_detected) {
        auto block_age_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - metrics.last_block_time).count();
        if (block_age_ms < BLOCK_DURATION_MS) {
            return false;  // Still blocked
        }
        // Block period expired
        metrics.loop_pattern_detected = false;
        metrics.consecutive_rapid_calls = 0;
    }

    // Count requests in last second
    int requests_this_second = metrics.timestamps.size();

    // Detect loop pattern: more than MAX_REQUESTS_PER_SECOND in < 100ms
    auto recent_count = 0;
    if (!metrics.timestamps.empty()) {
        auto age_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - metrics.timestamps.back()).count();
        if (age_ms < 100) {
            recent_count = requests_this_second;
        }
    }

    if (recent_count >= LOOP_PATTERN_THRESHOLD) {
        // Loop pattern detected!
        metrics.loop_pattern_detected = true;
        metrics.last_block_time = now;
        std::cerr << "[AC SECURITY] Suspicious loop pattern detected: " << action_id
                  << " (" << recent_count << " requests in 100ms)\n";
        return false;
    }

    // Allow if under limit
    if (requests_this_second < MAX_REQUESTS_PER_SECOND) {
        metrics.timestamps.push(now);
        metrics.consecutive_rapid_calls++;
        return true;
    }

    // Rate limited
    metrics.loop_pattern_detected = true;
    metrics.last_block_time = now;
    return false;
}

void RateLimiter::report_loop_pattern(const std::string& action_id) {
    std::lock_guard<std::mutex> lock(mtx);
    auto& metrics = metrics[action_id];
    metrics.loop_pattern_detected = true;
    metrics.last_block_time = std::chrono::steady_clock::now();
    std::cerr << "[AC SECURITY] Loop pattern reported for: " << action_id << "\n";
}

int RateLimiter::get_requests_per_second(const std::string& action_id) {
    std::lock_guard<std::mutex> lock(mtx);
    if (metrics.find(action_id) == metrics.end()) return 0;
    return metrics[action_id].timestamps.size();
}

void RateLimiter::reset() {
    std::lock_guard<std::mutex> lock(mtx);
    metrics.clear();
}

// ============================================================================
// PING FLOOD DETECTOR IMPLEMENTATION
// ============================================================================

bool PingFloodDetector::allow_ping(const std::string& target) {
    std::lock_guard<std::mutex> lock(mtx);
    auto now = std::chrono::steady_clock::now();
    auto& metrics = ping_history[target];

    // Check if currently blocked
    if (metrics.flood_detected) {
        auto block_age_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - metrics.block_until).count();
        if (block_age_ms < 0) {
            return false;  // Still blocked
        }
        metrics.flood_detected = false;
    }

    // Clean old pings (older than 10 seconds)
    auto cutoff = now - std::chrono::seconds(10);
    auto& pings = metrics.pings;
    pings.erase(
        std::remove_if(pings.begin(), pings.end(),
            [&cutoff](const auto& tp) { return tp < cutoff; }),
        pings.end()
    );

    // Check if adding this ping would exceed limit
    if (pings.size() >= MAX_PINGS_PER_10_SECONDS) {
        metrics.flood_detected = true;
        metrics.block_until = now + std::chrono::milliseconds(BLOCK_DURATION_MS);
        std::cerr << "[AC SECURITY] Ping flood detected to: " << target
                  << " (" << pings.size() << " pings in 10 seconds)\n";
        return false;
    }

    // Allow this ping
    pings.push_back(now);
    return true;
}

int PingFloodDetector::get_ping_count(const std::string& target) {
    std::lock_guard<std::mutex> lock(mtx);
    if (ping_history.find(target) == ping_history.end()) return 0;
    return ping_history[target].pings.size();
}

void PingFloodDetector::reset() {
    std::lock_guard<std::mutex> lock(mtx);
    ping_history.clear();
}

// ============================================================================
// CONNECTION POOL IMPLEMENTATION
// ============================================================================

bool ConnectionPool::acquire_connection(const std::string& pool_id) {
    std::lock_guard<std::mutex> lock(mtx);
    auto& pool = pools[pool_id];

    if (pool.active_connections >= MAX_CONNECTIONS_PER_POOL) {
        std::cerr << "[AC SECURITY] Connection limit exceeded for pool: " << pool_id
                  << " (" << pool.active_connections << "/" << MAX_CONNECTIONS_PER_POOL << ")\n";
        return false;
    }

    pool.active_connections++;
    return true;
}

void ConnectionPool::release_connection(const std::string& pool_id) {
    std::lock_guard<std::mutex> lock(mtx);
    auto& pool = pools[pool_id];
    if (pool.active_connections > 0) {
        pool.active_connections--;
    }
}

int ConnectionPool::get_connection_count(const std::string& pool_id) {
    std::lock_guard<std::mutex> lock(mtx);
    if (pools.find(pool_id) == pools.end()) return 0;
    return pools[pool_id].active_connections;
}

void ConnectionPool::reset() {
    std::lock_guard<std::mutex> lock(mtx);
    pools.clear();
}

// ============================================================================
// RESOURCE LIMITER IMPLEMENTATION
// ============================================================================

bool ResourceLimiter::allow_memory_allocation(size_t bytes) {
    size_t new_total = current_memory_usage + bytes;
    if (new_total > MAX_MEMORY_PER_PROCESS) {
        std::cerr << "[AC SECURITY] Memory limit exceeded: "
                  << new_total / (1024*1024) << "MB / "
                  << MAX_MEMORY_PER_PROCESS / (1024*1024) << "MB\n";
        return false;
    }
    return true;
}

void ResourceLimiter::track_memory_usage(size_t bytes, bool allocate) {
    if (allocate) {
        current_memory_usage += bytes;
    } else {
        if (current_memory_usage >= bytes) {
            current_memory_usage -= bytes;
        }
    }
}

bool ResourceLimiter::check_cpu_usage() {
    auto now = std::chrono::steady_clock::now();

    // Check if CPU spinning (tight loop)
    cpu_spin_count++;
    if (cpu_spin_count > MAX_CPU_SPIN_ITERATIONS) {
        auto time_since_check_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - last_cpu_check).count();

        if (time_since_check_ms < 1000) {  // 1M iterations in < 1 second = spinning
            std::cerr << "[AC SECURITY] CPU throttling: Excessive spin loop detected\n";
            cpu_spin_count = 0;
            last_cpu_check = now;
            return false;
        }

        cpu_spin_count = 0;
        last_cpu_check = now;
    }

    return true;
}

size_t ResourceLimiter::get_memory_usage() {
    return current_memory_usage;
}

void ResourceLimiter::reset() {
    current_memory_usage = 0;
    cpu_spin_count = 0;
}

// ============================================================================
// SECURITY MANAGER IMPLEMENTATION (Singleton)
// ============================================================================

SecurityManager& SecurityManager::instance() {
    static SecurityManager manager;
    return manager;
}

bool SecurityManager::allow_request(const std::string& endpoint) {
    bool allowed = rate_limiter.allow(endpoint);
    if (!allowed) {
        total_blocks++;
    }
    return allowed;
}

bool SecurityManager::allow_ping(const std::string& target) {
    bool allowed = ping_detector.allow_ping(target);
    if (!allowed) {
        total_pings_blocked++;
    }
    return allowed;
}

bool SecurityManager::acquire_connection() {
    return connection_pool.acquire_connection("default");
}

void SecurityManager::release_connection() {
    connection_pool.release_connection("default");
}

bool SecurityManager::allow_memory_allocation(size_t bytes) {
    return resource_limiter.allow_memory_allocation(bytes);
}

void SecurityManager::track_memory_usage(size_t bytes, bool allocate) {
    resource_limiter.track_memory_usage(bytes, allocate);
}

bool SecurityManager::check_cpu_usage() {
    return resource_limiter.check_cpu_usage();
}

void SecurityManager::report_loop_pattern(const std::string& action) {
    rate_limiter.report_loop_pattern(action);
}

void SecurityManager::report_flood_attempt(const std::string& type, const std::string& target) {
    std::cerr << "[AC SECURITY] Flood attempt reported: " << type << " on " << target << "\n";
}

std::string SecurityManager::get_security_stats() {
    std::string stats;
    stats += "=== AC Runtime Security Stats ===\n";
    stats += "Total requests blocked: " + std::to_string(total_blocks) + "\n";
    stats += "Total pings blocked: " + std::to_string(total_pings_blocked) + "\n";
    stats += "Current memory usage: " + std::to_string(resource_limiter.get_memory_usage() / (1024*1024)) + "MB\n";
    stats += "Active connections: " + std::to_string(connection_pool.get_connection_count("default")) + "\n";
    return stats;
}

}  // namespace AC_Runtime
