# AC Security Features

Built-in protections to prevent misuse and attacks.

---

## 1. Anti-DDoS: Loop Rate Limiting

### The Problem

Attackers could write:
```ac
WHILST true
    web.request_get("https://target.com")
```

This would hammer a target server with thousands of requests per second.

### AC's Solution

**AC automatically detects and blocks repeated rapid requests** from the same code location.

```ac
/* This will NOT spam the target */
WHILST true
    web.request_get("https://example.com")  /* Blocked after ~10 requests */
Make

Term.display "Loop request limit enforced"
```

**Behavior:**
- First ~10 requests: Allowed
- After that: Automatically rate-limited (1 req per second)
- Warnings logged: "Suspicious loop pattern detected"
- Further abuse: Connection dropped / process killed

---

## 2. Anti-Ping Flood

### The Problem

Ping floods (ICMP storms) can take down servers:
```ac
WHILST true
    ping(target)
```

### AC's Solution

**AC detects repeated ping patterns and blocks them.**

```ac
/* Legitimate: Check if server is up */
result = ping("example.com")

/* Malicious: Rapid repeated pings */
WHILST true
    ping("example.com")  /* ← Detected and blocked after ~5 attempts */
```

---

## 3. Connection Pooling Limits

### The Problem

Opening thousands of connections to exhaust resources:
```ac
WHILST i < 10000
    server = web.httpServer("0.0.0.0", random_port)
    i += 1
```

### AC's Solution

**AC limits concurrent connections per process.**

```ac
/* Default: Max 100 concurrent connections per AC process */
server1 = web.httpServer("0.0.0.0", 8000)  /* ✅ Allowed */
server2 = web.httpServer("0.0.0.0", 8001)  /* ✅ Allowed */
server3 = web.httpServer("0.0.0.0", 8002)  /* ... */
/* After ~100: ❌ REJECTED - "Connection limit exceeded" */
```

---

## 4. Resource Exhaustion Protection

### Memory Limits

```ac
/* AC limits memory per process */
buffer = []
WHILST true
    buffer.append("x" * 1000000)  /* Allocate 1MB each iteration */
    /* After ~500MB: ❌ KILLED - "Memory limit exceeded" */
```

### CPU Limits

```ac
/* CPU spinning is detected and throttled */
WHILST true
    x = x + 1  /* Empty loop */
    /* After ~30 seconds at 100% CPU: ⏸️ Process throttled or killed */
```

---

## 5. File Descriptor Limits

### The Problem

Open too many files → exhaust system resources:
```ac
WHILST true
    f = file.open("test.txt")  /* Never close */
```

### AC's Solution

```ac
/* Default: Max 1000 open file descriptors per AC process */
files = []
FOR i in range(500)
    files.append(file.open("test" + i + ".txt"))  /* ✅ Allowed */

FOR i in range(500, 1000)
    files.append(file.open("test" + i + ".txt"))  /* ✅ Allowed */

FOR i in range(1000, 1500)
    files.append(file.open("test" + i + ".txt"))  /* ❌ REJECTED */
```

---

## 6. Network Rate Limiting

### Per-IP Rate Limits

```ac
/* Each IP is limited to prevent abuse */
server = web.httpServer("0.0.0.0", 8080)

/* Default: 100 requests per second per IP */
server.rate_limit_per_ip(100)

/* Custom limits */
server.rate_limit_per_ip(1000)   /* Increase to 1000 req/s */
server.rate_limit_global(10000)  /* Global: 10000 req/s total */
```

### Automatic Blocking

```ac
/* After limit exceeded, attacker is blocked */
/* Default block duration: 60 seconds */
server.rate_limit_block_duration(60)

/* After 3 block violations: IP banned for 24 hours */
```

---

## 7. Loop Detection

### Infinite Loop Protection

```ac
/* AC detects and breaks infinite loops */
WHILST true
    Term.display "Running"
    /* After ~10 seconds: ⏱️ Loop timeout, process killed */
```

### Rapid Request Loop Protection

```ac
/* This is caught immediately */
WHILST true
    web.request_get("https://target.com")
    /* After ~10 requests in < 1 second: ❌ BLOCKED */
```

---

## 8. Security Headers (Automatic)

### HSTS (HTTP Strict Transport Security)

```ac
/* AC servers automatically set HSTS for HTTPS */
server = web.httpsServer("0.0.0.0", 8443)
server.listen()

/* Automatic response header:
   Strict-Transport-Security: max-age=31536000; includeSubDomains
*/
```

### X-Frame-Options

```ac
/* Prevents clickjacking */
/* Automatic header: X-Frame-Options: DENY */
```

### X-Content-Type-Options

```ac
/* Prevents MIME sniffing */
/* Automatic header: X-Content-Type-Options: nosniff */
```

### Content Security Policy (CSP)

```ac
/* AC sets restrictive CSP by default */
/* Automatic header:
   Content-Security-Policy: default-src 'self'; 
   script-src 'self'; style-src 'self' 'unsafe-inline'
*/
```

---

## 9. Input Validation

### SQL Injection Prevention

```ac
/* SAFE: Parameterized queries */
datac result is $TAKE * FROM users IF id = "$ + user_id + $"$
/* AC uses JaSQL which is NOT vulnerable to injection */
/* (JaSQL parses strictly, doesn't eval user input) */

/* Instead of string concatenation, use parameters */
result = web.jasql("TAKE * FROM users IF id = ?", [user_id])
```

### Path Traversal Prevention

```ac
/* AC blocks path traversal */
server = web.httpServer("0.0.0.0", 8080)
server.static("/", "./public")

/* Safe: /index.html → ./public/index.html */
web_request = "/index.html"

/* Blocked: /../../../etc/passwd → ❌ REJECTED */
/* Attack blocked, returns 403 Forbidden */
web_request = "/../../../etc/passwd"
```

### Command Injection Prevention

```ac
/* SAFE: Use os.sbash (sandboxed bash) */
result = os.sbash("ls -la")  /* Allowed - safe */

/* BLOCKED: os.sbash with dangerous commands */
result = os.sbash("sudo rm -rf /")  /* ❌ REJECTED - "sudo not allowed" */
result = os.sbash("nohup bad_script &")  /* ❌ REJECTED - "nohup blocked" */
```

---

## 10. Sandboxing (`os.sbash`)

### What's Blocked

```ac
/* All blocked in sbash: */
os.sbash("sudo ...")           /* ❌ Privilege escalation */
os.sbash("su ...")             /* ❌ User switch */
os.sbash("function ...")       /* ❌ Function definition */
os.sbash("nohup ...")          /* ❌ Detached process */
os.sbash("disown ...")         /* ❌ Process orphaning */
os.sbash("command &")          /* ❌ Background execution */
os.sbash("tmux ...")           /* ❌ Terminal multiplexing */
os.sbash("screen ...")         /* ❌ Screen sessions */
```

### What's Allowed

```ac
/* Safe commands are allowed */
os.sbash("ls -la")             /* ✅ List files */
os.sbash("grep pattern file")  /* ✅ Search */
os.sbash("cat file.txt")       /* ✅ Read file */
os.sbash("echo hello")         /* ✅ Output */
os.sbash("date")               /* ✅ Get time */
```

---

## 11. Configuration for Servers

### Set Security Limits

```ac
AC->PY
use ilib web

<mainloop>
    server = web.httpsServer("0.0.0.0", 8443)
    server.tls_cert("cert.pem")
    
    /* Security configuration */
    server.rate_limit_per_ip(100)       /* 100 req/s per IP */
    server.rate_limit_global(10000)     /* 10k req/s total */
    server.rate_limit_block_duration(60) /* 60s block time */
    
    /* Timeouts */
    server.request_timeout(30)          /* 30 second timeout */
    server.connection_timeout(300)      /* 5 minute idle timeout */
    
    /* Resource limits */
    server.max_connections(1000)        /* Max concurrent */
    server.max_request_size(10000000)   /* 10MB max body */
    
    /* CORS security */
    server.cors_allow("https://trusted.example.com")
    server.cors_credentials(true)
    server.cors_max_age(3600)
    
    server.listen()
    /end
<mainloop>
```

---

## 12. Monitoring & Alerts

### Log Security Events

```ac
/* AC logs all security violations */
logs = os.bash("tail -f /var/log/ac/security.log")

/* Typical entries:
   2024-06-18 10:23:45 | RATE_LIMIT_EXCEEDED | IP: 192.168.1.100 | Endpoint: /api/users
   2024-06-18 10:24:12 | LOOP_DETECTED | Pattern: Repeated requests in while loop
   2024-06-18 10:25:00 | PATH_TRAVERSAL_BLOCKED | Attempted: /../../../etc/passwd
   2024-06-18 10:26:30 | SBASH_BLOCKED | Command: sudo rm -rf / (sudo not allowed)
*/
```

---

## 13. Summary: What's Protected

| Attack Type | Protection | Status |
|-----------|-----------|--------|
| **DDoS (loop requests)** | Rate limiting + detection | ✅ Automatic |
| **Ping floods** | Connection limit + detection | ✅ Automatic |
| **Memory bombs** | Memory limit per process | ✅ Automatic |
| **CPU spinning** | CPU throttling | ✅ Automatic |
| **File descriptor exhaustion** | FD limit per process | ✅ Automatic |
| **SQL injection** | Parameterized JaSQL | ✅ Built-in |
| **Path traversal** | Path validation | ✅ Automatic |
| **Command injection** | sbash sandboxing | ✅ Automatic |
| **Privilege escalation** | No sudo in sbash | ✅ Automatic |
| **MIME sniffing** | X-Content-Type-Options header | ✅ Automatic |
| **Clickjacking** | X-Frame-Options header | ✅ Automatic |
| **XSS** | CSP headers | ✅ Automatic |

---

## 14. For Developers

### Best Practices

```ac
/* ✅ DO: Use safe APIs */
result = os.sbash("ls -la")              /* Safe sandboxed bash */
query = web.jasql(sql, file)             /* Safe parameterized SQL */
server.rate_limit_per_ip(100)            /* Enable rate limiting */

/* ❌ DON'T: Bypass protections */
/* Can't: No way to disable security checks */
/* Can't: No way to access dangerous commands from sbash */
/* Can't: No way to run DDoS loops (detected automatically) */
```

### Monitoring

```ac
/* Check security logs */
logs = os.bash("grep SECURITY_EVENT /var/log/ac/security.log")

/* Set up alerts for:
   - Multiple rate limit violations from same IP
   - Suspicious loop patterns
   - Path traversal attempts
   - Command injection attempts
*/
```

---

## Verdict

**AC is built for production.** You can't accidentally (or intentionally) write malicious code that harms others.

Security is not optional—it's built-in. 🔒
