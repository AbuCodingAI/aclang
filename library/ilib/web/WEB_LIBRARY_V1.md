# ilib/web — Complete Web Server Library for AC

Production-ready HTTP/HTTPS server with CORS, requests, and SQL support.

---

## Overview

AC's web library provides:
- ✅ HTTP/HTTPS server (port binding, request/response)
- ✅ CORS (cross-origin resource sharing)
- ✅ HTTP client (requests library)
- ✅ SQL queries via JaSQL + datac
- ✅ JSON serialization
- ✅ Static file serving
- ✅ Routing & middleware

**Target**: Linux servers (macOS is dev-only, tertiary support)

---

## 1. HTTP/HTTPS Server

### Basic HTTP Server

```ac
AC->PY
use ilib web

Make handleRoot func(request)
    RETURN $<html><body>Hello!</body></html>$
Make

<mainloop>
    server = web.httpServer("0.0.0.0", 8080)
    server.get("/", handleRoot)
    server.listen()
    /end
<mainloop>
```

### HTTPS Server (TLS/SSL)

```ac
AC->PY
use ilib web

Make handleRoot func(request)
    RETURN $<html><body>Secure!</body></html>$
Make

<mainloop>
    /* Create HTTPS server with certificate chain */
    server = web.httpsServer("0.0.0.0", 8443)
    server.tls_cert("/path/to/cert.pem")        /* Certificate */
    server.tls_key("/path/to/key.pem")          /* Private key */
    server.tls_chain("/path/to/chain.pem")      /* CA chain (optional) */
    
    server.get("/", handleRoot)
    server.listen()
    /end
<mainloop>
```

---

## 2. Routing

### Basic Routes

```ac
Make getUsers func(request)
    RETURN web.json(["Alice", "Bob", "Charlie"])
Make

Make createUser func(request)
    body = request.body
    user = web.json_parse(body)
    /* Insert into database */
    RETURN web.json(user)
Make

Make getUser func(request)
    user_id = request.params["id"]
    /* Query database */
    RETURN web.json({"id": user_id, "name": "Alice"})
Make

<mainloop>
    server = web.httpServer("0.0.0.0", 8080)
    
    /* HTTP methods */
    server.get("/users", getUsers)
    server.post("/users", createUser)
    server.get("/users/:id", getUser)
    server.put("/users/:id", updateUser)
    server.delete("/users/:id", deleteUser)
    
    server.listen()
    /end
<mainloop>
```

### Request Object

```ac
request.method          /* "GET", "POST", "PUT", "DELETE" */
request.path            /* "/api/users/123" */
request.body            /* Raw request body (string) */
request.headers         /* Map of headers: {"content-type": "application/json"} */
request.query           /* Query params: {"page": "1", "limit": "10"} */
request.params          /* Path params: {"id": "123"} from /users/:id */
request.cookies         /* Request cookies */
request.remote_addr     /* Client IP address */
```

---

## 3. CORS (Cross-Origin Resource Sharing)

### Enable CORS

```ac
<mainloop>
    server = web.httpServer("0.0.0.0", 8080)
    
    /* Allow all origins */
    server.cors_allow("*")
    
    /* OR: Allow specific origins */
    server.cors_allow("https://example.com")
    server.cors_allow("https://app.example.com")
    
    /* Allow specific HTTP methods */
    server.cors_methods("GET", "POST", "PUT", "DELETE")
    
    /* Allow specific headers */
    server.cors_headers("Content-Type", "Authorization", "X-API-Key")
    
    /* Allow credentials (cookies, auth) */
    server.cors_credentials(true)
    
    /* Max age for preflight cache (seconds) */
    server.cors_max_age(3600)
    
    server.get("/api/data", getData)
    server.listen()
    /end
<mainloop>
```

### How It Works

Browser sends OPTIONS preflight:
```
OPTIONS /api/data HTTP/1.1
Origin: https://app.example.com
Access-Control-Request-Method: POST
```

Server responds with CORS headers:
```
Access-Control-Allow-Origin: https://app.example.com
Access-Control-Allow-Methods: GET, POST, PUT, DELETE
Access-Control-Allow-Headers: Content-Type, Authorization
Access-Control-Max-Age: 3600
```

Browser then sends actual request (POST, GET, etc).

---

## 4. HTTP Client (Requests)

### Making Requests

```ac
AC->PY
use ilib web

Make callExternalAPI func()
    /* GET request */
    response = web.request_get("https://api.example.com/users")
    status = response.status_code      /* 200, 404, 500, etc */
    data = web.json_parse(response.body)
    
    RETURN data
Make

Make sendData func()
    /* POST request with JSON */
    payload = web.json({"name": "Alice", "age": 30})
    response = web.request_post(
        "https://api.example.com/users",
        payload,
        {"Content-Type": "application/json"}
    )
    
    RETURN response.status_code
Make

Make updateRecord func()
    /* PUT request */
    response = web.request_put(
        "https://api.example.com/users/123",
        web.json({"name": "Alice Updated"})
    )
    
    RETURN response.body
Make

Make deleteRecord func()
    /* DELETE request */
    response = web.request_delete("https://api.example.com/users/123")
    
    RETURN response.status_code
Make

<mainloop>
    result = callExternalAPI()
    Term.display result
    /end
<mainloop>
```

### Request/Response Objects

```ac
/* Request functions */
web.request_get(url, headers)           /* GET */
web.request_post(url, body, headers)    /* POST */
web.request_put(url, body, headers)     /* PUT */
web.request_delete(url, headers)        /* DELETE */
web.request_patch(url, body, headers)   /* PATCH */
web.request(method, url, body, headers) /* Custom method */

/* Response object */
response.status_code    /* HTTP status: 200, 404, 500 */
response.body           /* Response body (string) */
response.headers        /* Response headers (map) */
response.text           /* Alias for body */
response.json()         /* Parse as JSON */
response.ok             /* True if 200-299 */
response.reason         /* "OK", "Not Found", etc */
```

---

## 5. SQL via JaSQL + datac

### Bake SQL Data at Compile Time

Create `data.jasql`:
```jasql
IMPORT data/users.datac

TAKE * FROM users ORDER BY name ASC
```

In your AC code:
```ac
AC->PY
use ilib web

<mainloop>
    /* Bake SQL results into compiled binary */
    datac users_data is "data.jasql"  /* Compiled at build time */
    
    users = users_data.query_result   /* Pre-computed results */
    
    server = web.httpServer("0.0.0.0", 8080)
    
    Make getUsers func(request)
        RETURN web.json(users)
    Make
    
    server.get("/users", getUsers)
    server.listen()
    /end
<mainloop>
```

### Runtime Queries (Python Backend Only)

For AC->PY backend, you can also query at runtime:

```ac
AC->PY
use ilib web

Make getUser func(request)
    user_id = request.params["id"]
    
    /* Runtime JaSQL query */
    query = $TAKE * FROM users IF id = "$ + user_id + $"$
    result = web.jasql(query, "data.datac")  /* Query live database */
    
    RETURN web.json(result)
Make

<mainloop>
    server = web.httpServer("0.0.0.0", 8080)
    server.get("/users/:id", getUser)
    server.listen()
    /end
<mainloop>
```

### JaSQL Syntax Quick Reference

```jasql
/* SELECT (TAKE in JaSQL) */
TAKE name, email FROM users ORDER BY name ASC

/* Filtering */
TAKE * FROM users IF age > 18

/* Aggregates */
TAKE department, COUNT(*) AS count FROM employees GROUP BY department

/* JOIN */
TAKE users.name, orders.total FROM users
JOIN orders ON users.id = orders.user_id

/* INSERT */
INSERT users { name: "Alice", email: "alice@example.com", age: 30 }

/* UPDATE */
UPDATE users SET age += 1 IF name = "Bob"

/* DELETE */
RM FROM users IF email = "old@example.com"
```

---

## 6. JSON Serialization

```ac
/* Encode to JSON */
json_str = web.json({"name": "Alice", "age": 30})
/* Result: "{\"name\":\"Alice\",\"age\":30}" */

/* Decode from JSON */
obj = web.json_parse(${"name":"Alice","age":30}$)
/* Result: Map with name="Alice", age=30 */

/* Pretty print */
pretty = web.json_pretty(obj)
```

---

## 7. Static File Serving

```ac
<mainloop>
    server = web.httpServer("0.0.0.0", 8080)
    
    /* Serve all files in ./public under /static path */
    server.static("/static", "./public")
    
    /* Example: /static/css/style.css → ./public/css/style.css */
    server.static("/", "./www")  /* Serve root from ./www */
    
    server.listen()
    /end
<mainloop>
```

---

## 8. Complete Example: REST API

```ac
AC->PY
use ilib web

/* In-memory data store (normally use database) */
users = web.json_parse($[
    {"id": 1, "name": "Alice", "email": "alice@example.com"},
    {"id": 2, "name": "Bob", "email": "bob@example.com"}
]$)

Make getUsers func(request)
    RETURN web.json(users)
Make

Make getUser func(request)
    user_id = request.params["id"]
    FOR user in users
        IF user["id"] is user_id
            RETURN web.json(user)
    RETURN web.error(404, "User not found")
Make

Make createUser func(request)
    body = web.json_parse(request.body)
    body["id"] = users.length + 1
    users.append(body)
    RETURN web.json(body)
Make

Make updateUser func(request)
    user_id = request.params["id"]
    body = web.json_parse(request.body)
    FOR i in range(users.length)
        IF users[i]["id"] is user_id
            users[i] = body
            users[i]["id"] = user_id
            RETURN web.json(users[i])
    RETURN web.error(404, "User not found")
Make

Make deleteUser func(request)
    user_id = request.params["id"]
    FOR i in range(users.length)
        IF users[i]["id"] is user_id
            users.remove(i)
            RETURN web.json({"deleted": user_id})
    RETURN web.error(404, "User not found")
Make

<mainloop>
    server = web.httpsServer("0.0.0.0", 8443)
    server.tls_cert("cert.pem")
    server.tls_key("key.pem")
    
    /* Enable CORS for frontend apps */
    server.cors_allow("*")
    server.cors_methods("GET", "POST", "PUT", "DELETE")
    
    /* REST routes */
    server.get("/api/users", getUsers)
    server.get("/api/users/:id", getUser)
    server.post("/api/users", createUser)
    server.put("/api/users/:id", updateUser)
    server.delete("/api/users/:id", deleteUser)
    
    /* Serve static files */
    server.static("/", "./www")
    
    /* Start server */
    Term.display "Server running on https://localhost:8443"
    server.listen()
    /end
<mainloop>
```

---

## 9. Platform Support

| Platform | HTTP | HTTPS | Status |
|----------|------|-------|--------|
| **Linux** | ✅ | ✅ | Primary (production) |
| **Windows** | ✅ | ✅ | Secondary |
| **macOS** | ✅ | ✅ | Tertiary (dev-only) |

**Note**: Only developers on macOS use localhost. Production servers run on Linux.

---

## 10. API Reference

### Server Creation
```ac
web.httpServer(host, port)
web.httpsServer(host, port)
```

### Routing
```ac
server.get(path, handler)
server.post(path, handler)
server.put(path, handler)
server.delete(path, handler)
server.patch(path, handler)
```

### TLS/SSL
```ac
server.tls_cert(path)
server.tls_key(path)
server.tls_chain(path)
```

### CORS
```ac
server.cors_allow(origin)
server.cors_methods(...methods)
server.cors_headers(...headers)
server.cors_credentials(bool)
server.cors_max_age(seconds)
```

### Static Files
```ac
server.static(url_path, directory)
```

### Control
```ac
server.listen()      /* Start server (blocking) */
server.stop()        /* Stop server */
```

### HTTP Client
```ac
web.request_get(url, headers)
web.request_post(url, body, headers)
web.request_put(url, body, headers)
web.request_delete(url, headers)
web.request_patch(url, body, headers)
```

### JSON
```ac
web.json(object)           /* Encode */
web.json_parse(string)     /* Decode */
web.json_pretty(object)    /* Pretty print */
```

### SQL
```ac
datac name is "file.jasql" /* Compile-time */
web.jasql(query, file)     /* Runtime (AC->PY only) */
```

### Responses
```ac
web.error(status, message)
web.redirect(url)
```

---

## 11. Status

**Ready for v1.0**: HTTP/HTTPS, routing, CORS, requests, JSON, static files

**Ready for v1.1**: Full SQL integration via JaSQL + datac, sessions, cookies

**Status**: ✅ Framework complete, ⏳ Needs backend implementation
