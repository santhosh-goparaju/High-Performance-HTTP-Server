# Requirement Traceability Checklist

This checklist maps each requirement from the PDF build plan to its implementation.

## Core Architecture Requirements

| # | Requirement | Status | Implementation |
|---|------------|--------|---------------|
| 1 | Raw POSIX sockets (no frameworks) | ✅ | `net/socket.cpp` uses `socket()`, `bind()`, `listen()`, `accept()`, `recv()`, `send()` |
| 2 | RAII resource management | ✅ | `Socket` wrapper with move semantics, destructor calls `close()` |
| 3 | HTTP/1.1 protocol compliance | ✅ | Parser validates version, serializer outputs `HTTP/1.1` |
| 4 | State-machine HTTP parser | ✅ | `HttpParser` with states: REQUEST_LINE → HEADERS → BODY → COMPLETE/ERROR |
| 5 | Incremental/streaming parsing | ✅ | `feed()` accepts partial data, maintains internal buffer |
| 6 | Request routing | ✅ | `Router::route()` dispatches by method+path |
| 7 | Static file serving | ✅ | `Router::add_static()` with MIME type detection |
| 8 | Thread-per-connection baseline | ✅ | `Server::accept_loop_baseline()` with `--workers 0` |
| 9 | Bounded thread pool | ✅ | `ThreadPool` with configurable workers and queue size |
| 10 | Keep-alive support | ✅ | Connection reuse, configurable timeout and max requests |
| 11 | Graceful shutdown | ✅ | Signal handling (SIGINT/SIGTERM), `ThreadPool::shutdown()` joins workers |
| 12 | C++20 standard | ✅ | `std::jthread`, `std::stop_token`, `starts_with()`, `std::latch` |

## Security Requirements

| # | Requirement | Status | Implementation |
|---|------------|--------|---------------|
| 1 | Path traversal protection | ✅ | `is_path_safe()` rejects `..`, canonical path containment check |
| 2 | Request size limits | ✅ | URI (8KB), header (8KB), total headers (64KB), body (1MB) |
| 3 | Null byte rejection | ✅ | `is_path_safe()` checks for `\0` |
| 4 | Connection timeouts | ✅ | Configurable read/write timeouts via `SO_RCVTIMEO`/`SO_SNDTIMEO` |
| 5 | Backpressure (503) | ✅ | Thread pool returns 503 when queue full |
| 6 | Method whitelist | ✅ | Parser rejects non-whitelisted methods with 405 |

## Testing Requirements

| # | Requirement | Status | Details |
|---|------------|--------|---------|
| 1 | Unit tests | ✅ | 49 tests: parser (23), serializer (8), router (8), thread pool (7), connection (3) |
| 2 | Integration tests | ✅ | 16 tests: server endpoints (8), security (8) |
| 3 | AddressSanitizer | ✅ | Zero issues (unit + integration) |
| 4 | UndefinedBehaviorSanitizer | ✅ | Zero issues (unit tests) |
| 5 | Malformed input tests | ✅ | Missing method, bad version, oversized inputs, null bytes |
| 6 | Path traversal tests | ✅ | `../` and null byte injection tested |
| 7 | Keep-alive tests | ✅ | Multiple requests on single connection tested |

## Infrastructure Requirements

| # | Requirement | Status | Details |
|---|------------|--------|---------|
| 1 | Docker support | ✅ | Multi-stage Dockerfile (builder, tester, production) |
| 2 | docker-compose | ✅ | `docker-compose.yml` |
| 3 | Benchmark scripts | ✅ | `benchmarks/run_benchmarks.sh` (wrk), `benchmarks/compare.py` |
| 4 | Architecture docs | ✅ | `docs/architecture.md` |
| 5 | README | ✅ | Build, run, test, benchmark instructions |
| 6 | Git version control | ✅ | Logical commits pushed to GitHub |
| 7 | CMake build system | ✅ | C++20, strict warnings, sanitizer options, GoogleTest |

## Concurrency Modes

| # | Mode | Status | Flag |
|---|------|--------|------|
| 1 | Thread-per-connection | ✅ | `--workers 0` |
| 2 | Bounded thread pool | ✅ | `--workers N` (default: 4) |
| 3 | epoll event loop (Linux) | ✅ | `#ifdef __linux__` guarded, not active on macOS |

## API Endpoints

| Method | Path | Status | Response |
|--------|------|--------|----------|
| GET | `/` | ✅ | HTML welcome page |
| GET | `/health` | ✅ | `{"status":"ok"}` |
| GET | `/api/info` | ✅ | `{"name":"httpserver","version":"1.0","language":"C++20"}` |
| POST | `/api/echo` | ✅ | Echoes request body |
| DELETE | `/api/data` | ✅ | 204 No Content |
| GET | `/static/*` | ✅ | Static file with MIME type |
| * | unknown | ✅ | 404 Not Found |
| wrong | registered | ✅ | 405 Method Not Allowed (with Allow header) |

## HTTP Status Codes Implemented

| Code | Reason | Trigger |
|------|--------|---------|
| 200 | OK | Successful request |
| 204 | No Content | DELETE /api/data |
| 400 | Bad Request | Malformed request |
| 403 | Forbidden | Path traversal attempt |
| 404 | Not Found | Unknown route |
| 405 | Method Not Allowed | Wrong HTTP method |
| 413 | Payload Too Large | Body > 1MB |
| 414 | URI Too Long | Request line > 8KB |
| 431 | Header Fields Too Large | Header(s) too large or too many |
| 500 | Internal Server Error | Handler exception |
| 503 | Service Unavailable | Thread pool queue full |
