# AC Operations Analysis & Server Features Roadmap

Complete review of what AC has, what's missing, and what servers need.

---

## 1. Operations You HAVE ✅

### **Arithmetic**
- `+` Addition
- `-` Subtraction
- `@` or `*` Multiplication (inside `fn...fn`)
- `/` Division
- `//` Integer division
- `^` Exponentiation
- `+=`, `-=`, `*=`, `/=` Compound assignment

### **Bitwise**
- `&` / `band` Bitwise AND
- `|` Bitwise XOR
- `bor` Bitwise OR
- `~` Bitwise NOT
- `ptm` Power Two Multiply (shift left × 2^n)
- `ptd` Power Two Divide (shift right ÷ 2^n)

### **Logical**
- `and` Logical AND
- `or` Logical OR
- `xor` Logical XOR
- `xnor` Logical XNOR
- `not` Logical NOT
- `#=` Not equal (!=)
- `#>` Not greater (≤)
- `#<` Not less (≥)
- `#|` Logical XNOR (already have)

### **Unique to AC**
- **`xsub`** Inclusive range count: |a - b| + 1

### **Comparisons**
- `<` Less than
- `>` Greater than
- `<=` / `>=` (via `#>` / `#<`)
- `is` Equality (AC's == operator)
- `overlap` Set overlap check

---

## 2. Operations You're MISSING ❌

| Operation | Why It Matters | Example |
|-----------|---|---|
| **Modulo (`%`)** | Remainder, cycling, grid operations | `x % 10` = last digit |
| **Min/Max** | Clamping, comparisons | `min(a, b)`, `max(a, b)` |
| **Ternary (`? :`)** | Conditional inline expression | `x > 0 ? x : -x` |
| **abs() inline** | Absolute value without function call | Game physics, optimization |
| **floor/ceil** | Rounding (may need `math` lib) | UI layout, collisions |
| **String concatenation (`+`)** | Already works with `$` | Direct string math? |

### **Quick Assessment**

- ✅ Modulo: Check if in `math` library
- ✅ Min/Max: Probably in `math` library
- ❌ Ternary: Not standard AC (use `IF/OTHER` instead)
- ⏳ Inline `abs()`: Could add shorthand

---

## 3. Server Features (Non-SQL) 🌐

### **Must Have for v1.0**

| Feature | Status | Priority | Note |
|---------|--------|----------|------|
| **HTTP Server** | ❌ Not implemented | 🔴 Critical | Listen on port, accept connections |
| **Request Parsing** | ❌ Not implemented | 🔴 Critical | GET/POST, headers, body |
| **Response Generation** | ❌ Not implemented | 🔴 Critical | Status codes, content-type, headers |
| **Routing** | ❌ Not implemented | 🔴 Critical | URL pattern matching → function |
| **Static File Serving** | ❌ Not implemented | 🟠 High | Serve HTML, CSS, JS, images |
| **Logging** | ✅ Partial (Term.display) | 🟡 Medium | Better structured logging |
| **Error Handling** | ✅ try/catch | 🟡 Medium | HTTP error responses (404, 500, etc.) |

### **Nice to Have for v1.1+**

| Feature | What It Does | Example |
|---------|---|---|
| **Middleware/Pipeline** | Pre/post request processing | Logging, auth, CORS |
| **Sessions** | Track user state | Shopping cart, login |
| **Cookies** | Client-side storage | Remember preferences |
| **WebSocket** | Real-time bidirectional | Chat, live updates |
| **CORS** | Cross-origin requests | API access from browser |
| **Rate Limiting** | Prevent abuse | Max 100 requests/min |
| **Compression** | Gzip responses | Bandwidth savings |
| **Authentication** | Basic auth, tokens | Secure endpoints |
| **File Upload** | Handle POST files | Image uploads |

---

## 4. Minimal HTTP Server (AC v1.0 Target)

What you NEED for a working web server:

```ac
AC->PY
use ilib web
use ilib os

<mainloop>
    /* 1. Create HTTP server on port 8080 */
    server = web.httpServer("0.0.0.0", 8080)
    
    /* 2. Define routes */
    server.get("/", fn(request)
        RETURN $<html><body>Hello!</body></html>$
    )
    
    server.post("/api/data", fn(request)
        data = request.body
        result = process(data)
        RETURN web.json(result)
    )
    
    /* 3. Serve static files */
    server.static("/public", "./static")
    
    /* 4. Start listening */
    server.listen()
    
    /end
<mainloop>
```

This requires:
- ✅ `web.httpServer()` - Create server
- ✅ `server.get()` / `server.post()` - Define routes
- ✅ `request.body` - Parse request
- ✅ `web.json()` - Generate JSON response
- ✅ `server.static()` - Serve files
- ✅ `server.listen()` - Start server

---

## 5. What `ilib web` Needs to Implement

### **Core Functions**

```ac
/* Server creation */
web.httpServer(host, port) → server object

/* Routing */
server.get(path, handler)
server.post(path, handler)
server.put(path, handler)
server.delete(path, handler)

/* Request object */
request.method       /* GET, POST, PUT, DELETE */
request.path         /* /api/users/123 */
request.body         /* Request body (string) */
request.headers      /* Map of headers */
request.query        /* Query parameters */

/* Response helpers */
web.json(object)     /* Serialize to JSON */
web.redirect(url)    /* HTTP redirect */
web.error(status, message)  /* Error response */

/* Server control */
server.listen()      /* Start listening */
server.static(path, folder)  /* Serve static files */
```

### **Low-Level Network Access**

If AC backend is C/C++:
- Socket creation (TCP)
- Socket binding & listening
- Accept connections
- Read/write HTTP protocol
- Parse HTTP headers

---

## 6. Missing AC Operations (Quick Summary)

### **Essential**
- ❓ **Modulo (`%`)** - Check if exists
- ❓ **Min/Max functions** - Check if in math lib

### **Nice to Have**
- **Ternary operator** - Use IF/OTHER for now
- **Inline abs()** - Use `math.abs()`
- **String concat** - Works with `$string$`

### **Not Critical for v1.0**
- Bitwise rotate (ROL, ROR)
- Saturating arithmetic (checked add/sub)
- SIMD operations

---

## 7. Implementation Roadmap

### **v1.0 (Now - 2 weeks)**
Priority:
1. ✅ Logical operators (have)
2. ✅ Bitwise operators (have + just added ptm/ptd)
3. ✅ Basic HTTP server (need ilib/web)
4. ❌ Routing & request parsing

### **v1.1 (2-4 weeks)**
1. SQL database support
2. Sessions & cookies
3. Middleware pipeline
4. Better error responses

### **v1.2+ (Future)**
1. WebSocket
2. CORS
3. Rate limiting
4. File uploads

---

## 8. Your Operation Set is SOLID ✅

You have:
- ✅ All essential arithmetic
- ✅ Complete bitwise ops (& | bor ~ ptm ptd)
- ✅ Complete logical ops (and or xor xnor)
- ✅ Unique xsub operator
- ✅ String operations (via $$ syntax)

**Missing for servers:** HTTP stack, not language operations.

The language is feature-complete for most programs. Add HTTP server support to ilib/web and you're ready for v1.0! 🚀
