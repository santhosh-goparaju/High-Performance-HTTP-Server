# Architecture Documentation

## Overview

This is a production-grade HTTP/1.1 server built from raw POSIX sockets in C++20. Every component — networking, HTTP parsing, routing, and concurrency — is hand-implemented without external frameworks.

## Layered Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                        Application Layer                     │
│  main.cpp — Signal handling, CLI config parsing, startup     │
├─────────────────────────────────────────────────────────────┤
│                         Server Layer                         │
│  server::Server — Accept loop, connection dispatch           │
│  server::EpollServer — Event-driven I/O (Linux only)         │
│  server::ThreadPool — Bounded work queue with jthread         │
│  server::Config — CLI argument parsing                       │
├─────────────────────────────────────────────────────────────┤
│                         Router Layer                         │
│  router::Router — Method+path dispatch, static files         │
├─────────────────────────────────────────────────────────────┤
│                       HTTP Protocol Layer                    │
│  http::HttpParser — State-machine request parser             │
│  http::HttpSerializer — Response serialization               │
│  http::Request / http::Response — Data structures            │
│  http::StatusCode — HTTP status code enum                    │
├─────────────────────────────────────────────────────────────┤
│                       Network Layer                          │
│  net::Socket — RAII file descriptor wrapper                  │
│  net::Listener — TCP bind/listen/accept                      │
│  net::Connection — Buffered read/write with timeouts         │
└─────────────────────────────────────────────────────────────┘
```

## Component Details

### Network Layer (`net::`)

#### Socket (`net/socket.hpp`)
- RAII wrapper around a raw file descriptor
- Move-only semantics (deleted copy constructor/assignment)
- Factory method `Socket::create(domain, type, protocol)`
- Socket options: `set_reuse_addr()`, `set_reuse_port()`, `set_nonblocking()`, `set_recv_timeout()`, `set_send_timeout()`
- Destructor calls `close()` automatically

#### Listener (`net/listener.hpp`)
- Binds to `INADDR_ANY` on the specified port
- Configurable backlog (default 128)
- `accept()` returns a `Socket` object (invalid on failure)
- `stop()` closes the listening socket

#### Connection (`net/connection.hpp`)
- Wraps a connected `Socket` with buffered I/O operations
- `read_some()` — Single `recv()` call, for incremental parsing
- `read_all()` — Reads until buffer full or no more data
- `read_until()` — Reads until delimiter found (with internal buffer)
- `write_all()` — Complete write with partial-write retry loop
- All methods handle `EINTR` transparently

### HTTP Protocol Layer (`http::`)

#### Parser (`http/parser.hpp`)
State-machine parser with four states:

```
REQUEST_LINE → HEADERS → BODY → COMPLETE
                                    ↓
                                  ERROR
```

- **Incremental feed**: `feed(data, len)` returns `ParseResult::INCOMPLETE/COMPLETE/ERROR`
- **Request line validation**: Method whitelist (GET, POST, DELETE, HEAD), HTTP/1.1 only, target must start with `/`
- **Header processing**: Case-insensitive storage (lowercased keys), colon-separated, whitespace trimmed
- **Size limits**:
  - Request line: 8,192 bytes → 414 URI Too Long
  - Single header: 8,192 bytes → 431 Header Fields Too Large
  - Total headers: 65,536 bytes → 431
  - Header count: 100 → 431
  - Body: 1,048,576 bytes (1 MB) → 413 Payload Too Large

#### Serializer (`http/serializer.hpp`)
- Static `serialize(Response)` method
- Generates: status line, Server header, Date header (RFC 7231), Content-Length, Connection, custom headers, body
- Automatically sets `Connection: keep-alive` or `Connection: close`

#### Request (`http/request.hpp`)
Public struct with helper methods:
- Fields: `method`, `target`, `version`, `headers`, `body`
- `header(name)` → `std::optional<std::string_view>`
- `should_keep_alive()` — Checks `Connection` header
- `content_length()` — Parses `Content-Length` header

#### Response (`http/response.hpp`)
Public struct with factory methods:
- `Response::ok(body, content_type)`, `Response::error(code, message)`, `Response::json(json_string)`
- `Response::not_found()`, `Response::bad_request()`, `Response::method_not_allowed(allowed)`
- `Response::make_response(StatusCode)` — Generic factory
- `set_header()` returns `Response&` for chaining

### Router Layer (`router::`)

#### Router (`router/router.hpp`)
- `add_route(method, path, handler)` — Register a handler function
- `add_static(prefix, directory)` — Mount a static file directory
- `route(request)` — Dispatch request to matching handler

**Static file security**:
1. Path safety check: rejects `..` and null bytes
2. Canonical path resolution with `std::filesystem::canonical()`
3. Containment verification: resolved path must be within mount directory
4. MIME type detection by file extension

### Server Layer (`server::`)

#### ThreadPool (`server/thread_pool.hpp`)
- Workers created in constructor using `std::jthread`
- Each worker accepts a `std::stop_token` for cooperative cancellation
- `enqueue(task)` → `bool` (false if queue full or stopped)
- `shutdown()` — Notifies all workers, waits for completion via jthread join
- Bounded queue with configurable `max_queue_size` for backpressure

#### Server (`server/server.hpp`)
Two concurrency modes:
1. **Thread-per-connection** (`--workers 0`): Creates a new `std::thread` for each accepted connection
2. **Thread pool** (`--workers N`): Enqueues connections to the bounded pool; returns 503 when queue full

Connection handling:
1. Set read/write timeouts
2. Read loop with `read_some()` + `parser.feed()` until COMPLETE or ERROR
3. Route request through `router::Router`
4. Serialize response and write
5. If keep-alive, reset parser and loop

#### EpollServer (`server/epoll_server.hpp`) — Linux only
- Single-threaded event loop using `epoll_wait()`
- Edge-triggered mode (`EPOLLET`) for maximum performance
- Non-blocking sockets with per-connection state machine
- States: `READING → WRITING → (READING | CLOSING)`

## Concurrency Model

### Thread Pool Architecture

```
                    ┌──────────────┐
                    │   Listener   │
                    │   accept()   │
                    └──────┬───────┘
                           │
                    ┌──────▼───────┐
                    │  Task Queue  │ ← bounded (backpressure)
                    │  (std::queue)│
                    └──────┬───────┘
                           │
              ┌────────────┼────────────┐
              │            │            │
        ┌─────▼─────┐┌────▼────┐┌─────▼─────┐
        │  Worker 1  ││ Worker 2││  Worker N  │
        │  (jthread) ││(jthread)││  (jthread) │
        └────────────┘└─────────┘└────────────┘
```

### Shutdown Sequence
1. `Server::stop()` sets `running_ = false`, closes listener
2. `ThreadPool::shutdown()` sets `stop_ = true`, notifies CV
3. `workers_.clear()` → jthread destructors request stop + join
4. All in-flight tasks complete before shutdown returns

## Security Measures

| Threat | Protection |
|--------|-----------|
| Path traversal (`../`) | Pre-check for `..` + canonical path containment |
| Null byte injection | Reject paths containing `\0` |
| Request line overflow | 8 KB limit → 414 URI Too Long |
| Header overflow | 8 KB per header, 64 KB total, 100 max → 431 |
| Body overflow | 1 MB limit → 413 Payload Too Large |
| Invalid HTTP method | Whitelist (GET/POST/DELETE/HEAD) → 405 |
| Wrong HTTP version | Only HTTP/1.1 accepted → 400 |
| Connection exhaustion | Bounded thread pool queue → 503 |
| Slowloris | Read/write timeouts (default 30s) |

## Build Configuration

| CMake Option | Default | Description |
|-------------|---------|-------------|
| `CMAKE_BUILD_TYPE` | — | Debug, Release, RelWithDebInfo |
| `BUILD_TESTS` | ON | Build unit and integration tests |
| `ENABLE_ASAN` | OFF | AddressSanitizer |
| `ENABLE_UBSAN` | OFF | UndefinedBehaviorSanitizer |
| `ENABLE_TSAN` | OFF | ThreadSanitizer |
