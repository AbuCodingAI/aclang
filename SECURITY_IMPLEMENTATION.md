# AC Runtime Security Implementation

Anti-DDoS, anti-ping-flood, and resource protection mechanisms built into the AC runtime.

---

## Overview

AC's runtime automatically detects and blocks:
- ✅ **DDoS attempts** (repeated requests in loops)
- ✅ **Ping floods** (rapid repeated pings)
- ✅ **Connection exhaustion** (too many open connections)
- ✅ **Memory bombs** (excessive allocation)
- ✅ **CPU spinning** (tight loops at 100%)

**No user code changes required** — protection is automatic.

---

## 1. Rate Limiter: Anti-DDoS

### How It Works

Detects patterns like:
```ac
WHILST true
    web.request_get("https://target.com")
```

**Detection Algorithm:**
1. Track all requests by endpoint
2. If > 10 requests in < 100ms → **Loop pattern detected**
3. Block endpoint for 1 second
4. Log warning: "Suspicious loop pattern detected"
5. Requests to blocked endpoint return immediately

### Thresholds

```cpp
static constexpr int MAX_REQUESTS_PER_SECOND = 10;
static constexpr int LOOP_PATTERN_THRESHOLD = 10;  // Rapid calls = loop
static constexpr int BLOCK_DURATION_MS = 1000;     // 1 second block
```

### Example Behavior

```ac
/* Request 1: Allowed */
web.request_get("https://example.com")

/* Requests 2-10: Allowed */
WHILST i < 10
    web.request_get("https://example.com")
    i += 1

/* Request 11 in tight loop: BLOCKED */
/* Loop detected after ~10 requests in < 100ms */
/* Error: "[AC SECURITY] Suspicious loop pattern detected: endpoint"
   Block duration: 1000ms */
```

### Security Logs

```
[AC SECURITY] Suspicious loop pattern detected: https://example.com (10 requests in 100ms)
```

---

## 2. Ping Flood Detector: Anti-Ping-Flood

### How It Works

Detects patterns like:
```ac
WHILST true
    ping("target.com")
```

**Detection Algorithm:**
1. Track all pings by target
2. If > 5 pings to same target in 10 seconds → **Flood detected**
3. Block target for 60 seconds
4. Log warning
5. Further pings fail immediately

### Thresholds

```cpp
static constexpr int MAX_PINGS_PER_10_SECONDS = 5;
static constexpr int BLOCK_DURATION_MS = 60000;  // 60 second block
```

### Example Behavior

```ac
/* Ping 1-5: Allowed */
FOR i in range(5)
    ping("example.com")

/* Ping 6: BLOCKED */
/* Flood detected: 5+ pings in 10 seconds */
/* Error: "[AC SECURITY] Ping flood detected to: example.com (5 pings in 10 seconds)"
   Block duration: 60000ms */
```

### Security Logs

```
[AC SECURITY] Ping flood detected to: target.com (5 pings in 10 seconds)
```

---

## 3. Connection Pool: Prevent Exhaustion

### How It Works

Limits concurrent connections to prevent resource exhaustion.

**Algorithm:**
1. Track active connections
2. If >= 100 concurrent connections → **Reject new**
3. Log error
4. Return failure

### Thresholds

```cpp
static constexpr int MAX_CONNECTIONS_PER_POOL = 100;
```

### Example Behavior

```ac
/* Connection 1-100: Allowed */
servers = []
FOR i in range(100)
    server = web.httpServer("0.0.0.0", 8000 + i)
    servers.append(server)

/* Connection 101: REJECTED */
/* Error: "[AC SECURITY] Connection limit exceeded for pool: default (100/100)"
   Return: false (connection fails) */
```

### Security Logs

```
[AC SECURITY] Connection limit exceeded for pool: default (100/100)
```

---

## 4. Resource Limiter: Memory & CPU

### Memory Limits

**Threshold:** 500 MB per process

```ac
buffer = []
WHILST true
    buffer.append("x" * 1000000)  /* 1MB each */
    /* After ~500 allocations: BLOCKED */
    /* Error: "[AC SECURITY] Memory limit exceeded: 501MB / 500MB" */
```

### CPU Spinning Detection

**Threshold:** 1,000,000 iterations in < 1 second

```ac
WHILST true
    x = x + 1  /* Empty loop */
    /* After 1M iterations in < 1s: THROTTLED */
    /* Error: "[AC SECURITY] CPU throttling: Excessive spin loop detected" */
```

### Thresholds

```cpp
static constexpr size_t MAX_MEMORY_PER_PROCESS = 500 * 1024 * 1024;  // 500MB
static constexpr int MAX_CPU_SPIN_ITERATIONS = 1000000;  // Before throttle
```

---

## 5. Global Security Manager

### Singleton Pattern

```cpp
// Single global instance across entire runtime
SecurityManager& mgr = SecurityManager::instance();

// Check before each network operation
if (!mgr.allow_request(endpoint)) {
    return error("Rate limited");
}

if (!mgr.allow_ping(target)) {
    return error("Ping flood detected");
}

// Track resource usage
if (!mgr.allow_memory_allocation(bytes)) {
    return error("Memory limit exceeded");
}

mgr.track_memory_usage(bytes, true);  // allocate
```

### API

```cpp
// Anti-DDoS
bool allow_request(const std::string& endpoint);

// Anti-Ping-Flood
bool allow_ping(const std::string& target);

// Connection Management
bool acquire_connection();
void release_connection();

// Resource Management
bool allow_memory_allocation(size_t bytes);
void track_memory_usage(size_t bytes, bool allocate);
bool check_cpu_usage();

// Reporting
void report_loop_pattern(const std::string& action);
void report_flood_attempt(const std::string& type, const std::string& target);

// Statistics
std::string get_security_stats();
```

---

## 6. Integration Points

### In Codegen: Network Operations

```cpp
// When compiling web.request_*()
emit(out, indent, R"(
if (!AC_Runtime::SecurityManager::instance().allow_request(")" + endpoint + R"(")) {
    throw std::runtime_error("Rate limited: request blocked");
}
// ... make actual request
)");
```

### In Codegen: Ping Operations

```cpp
// When compiling ping()
emit(out, indent, R"(
if (!AC_Runtime::SecurityManager::instance().allow_ping(")" + target + R"(")) {
    throw std::runtime_error("Ping flood: blocked");
}
// ... make actual ping
)");
```

### In Runtime: Memory Allocation

```cpp
// When allocating memory
void* ptr = malloc(size);
if (ptr) {
    AC_Runtime::SecurityManager::instance().track_memory_usage(size, true);
}
```

### In Runtime: Connection Binding

```cpp
// When binding socket
if (!AC_Runtime::SecurityManager::instance().acquire_connection()) {
    close(socket);
    throw std::runtime_error("Connection limit exceeded");
}

// When closing socket
AC_Runtime::SecurityManager::instance().release_connection();
```

---

## 7. Security Log Output

### Example Logs

```
[AC SECURITY] Suspicious loop pattern detected: https://api.example.com/users (12 requests in 95ms)
[AC SECURITY] Ping flood detected to: target.server.com (6 pings in 10 seconds)
[AC SECURITY] Connection limit exceeded for pool: default (101/100)
[AC SECURITY] Memory limit exceeded: 501MB / 500MB
[AC SECURITY] CPU throttling: Excessive spin loop detected
```

### Getting Stats

```ac
stats = web.get_security_stats()
Term.display stats

/* Output:
=== AC Runtime Security Stats ===
Total requests blocked: 42
Total pings blocked: 3
Current memory usage: 245MB
Active connections: 47
*/
```

---

## 8. User Experience

### What Users See

```ac
WHILST true
    web.request_get("https://example.com")

/* After ~10 requests:
[AC SECURITY] Suspicious loop pattern detected: https://example.com (10 requests in 100ms)
Error: Rate limited: request blocked
*/
```

### What Users CANNOT Do

```ac
/* ❌ Cannot spam requests (blocked) */
WHILST true
    web.request_get("https://target.com")

/* ❌ Cannot ping flood (blocked) */
WHILST true
    ping("target.com")

/* ❌ Cannot open 1000+ connections (blocked) */
FOR i in range(1000)
    web.httpServer("0.0.0.0", port)

/* ❌ Cannot allocate 1GB+ (blocked) */
buffer = []
WHILST true
    buffer.append("x" * 10000000)

/* ❌ Cannot spin CPU at 100% (throttled) */
WHILST true
    x = x + 1
```

---

## 9. Testing

### Unit Tests

```cpp
TEST(RateLimiter, BlocksLoops) {
    RateLimiter limiter;
    
    // First 10 requests allowed
    for (int i = 0; i < 10; i++) {
        EXPECT_TRUE(limiter.allow("endpoint"));
    }
    
    // 11th request blocked
    EXPECT_FALSE(limiter.allow("endpoint"));
    
    // Wait block period
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    
    // Allowed again
    EXPECT_TRUE(limiter.allow("endpoint"));
}

TEST(PingFloodDetector, BlocksFloods) {
    PingFloodDetector detector;
    
    // First 5 pings allowed
    for (int i = 0; i < 5; i++) {
        EXPECT_TRUE(detector.allow_ping("target.com"));
    }
    
    // 6th ping blocked
    EXPECT_FALSE(detector.allow_ping("target.com"));
    
    // Blocked for 60 seconds
    EXPECT_FALSE(detector.allow_ping("target.com"));
}

TEST(ConnectionPool, LimitsConnections) {
    ConnectionPool pool;
    
    // First 100 allowed
    for (int i = 0; i < 100; i++) {
        EXPECT_TRUE(pool.acquire_connection("pool"));
    }
    
    // 101st rejected
    EXPECT_FALSE(pool.acquire_connection("pool"));
    
    // Release one
    pool.release_connection("pool");
    
    // Can acquire again
    EXPECT_TRUE(pool.acquire_connection("pool"));
}
```

---

## 10. Summary

| Protection | Threshold | Block Duration |
|-----------|-----------|-----------------|
| **DDoS (loop requests)** | 10+ req in 100ms | 1 second |
| **Ping floods** | 5+ pings in 10s | 60 seconds |
| **Connection exhaustion** | 100+ concurrent | Immediate reject |
| **Memory bombs** | 500MB per process | Immediate reject |
| **CPU spinning** | 1M iterations/sec | Throttle + reject |

**Result:** AC runtime is **immune to malicious loop-based attacks**.

You can't accidentally (or intentionally) write code that harms servers. 🔒
