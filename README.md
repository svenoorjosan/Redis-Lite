# Redis-Lite

A small Redis-compatible in-memory key-value store written in **C++**, with **TTL** and **AOF** persistence, plus a tiny **Node/Express** metrics dashboard.

> Not production software. It’s a learning / demo project that’s great for benchmarks and interviews.

---

## Features

- **RESP** protocol over TCP (works with `redis-cli` for the supported commands)  
- Commands: `PING`, `ECHO`, `SET`, `GET`, `DEL`, `EXISTS`, `EXPIRE`, `TTL`, `INFO`  
- **TTL**: lazy expiration on reads + active sweeper (min-heap)  
- **AOF** persistence: buffered appends, ~100 ms flush, ~1 s fsync; replay on startup  
- **Metrics dashboard** at **<http://localhost:8080>** (uptime, clients, keys, command count, expired keys, AOF size)  
- **Graceful shutdown**: final AOF flush on `SIGINT` / `SIGTERM`

---

## Quick Start

### Local

```bash
# build the C++ server
make

# run the server  (defaults: PORT=6380, AOF_PATH=./data.aof)
PORT=6381 AOF_PATH=data-prod.aof ./redis-lite
```

**New terminal:**

```bash
cd dashboard
npm i                 # first run only
REDIS_PORT=6381 node server.js
# open http://localhost:8080
```

**Docker (one container: server + dashboard)**

```bash
docker compose up --build
# open http://localhost:8080
```

**Verify in 30 seconds**

```bash
# ping
printf '*1
$4
PING
' | nc 127.0.0.1 6381

# basic set / get
printf '*3
$3
SET
$1
a
$1
1
' | nc 127.0.0.1 6381
printf '*2
$3
GET
$1
a
'             | nc 127.0.0.1 6381
```

**TTL demo (key expires after 1 s)**

```python
import socket, time

def enc(*a):
    parts=[f"*{len(a)}
".encode()]
    for x in a:
        x=str(x).encode()
        parts += [f"${len(x)}
".encode(), x, b"
"]
    return b"".join(parts)

s = socket.create_connection(("127.0.0.1", 6381))
s.sendall(enc("SET", "t", "x"))
s.sendall(enc("EXPIRE", "t", "1")); print(s.recv(128))   # -> :1
time.sleep(1.3)
s.sendall(enc("GET", "t"));      print(s.recv(128))       # -> $-1
```

**Metrics JSON**

```bash
curl http://localhost:8080/metrics
# {"uptime":"...","connected_clients":"...","keys":"...","total_commands":"...","expired_keys":"...","aof_bytes":"..."}
```

## Benchmarks (loopback, single client)

| metric          | value                               |
|-----------------|-------------------------------------|
| SET throughput  | ~208 k ops / sec                    |
| GET latency     | p50 ≈ 0.03 ms • p95 ≈ 0.03 ms       |

Numbers from my laptop; they vary by hardware, OS, and buffer sizes.

## Supported Commands

| Command              | Description           | Notes                               |
|---------------------|-----------------------|-------------------------------------|
| `PING`              | Health check          | PONG or echo if arg given           |
| `ECHO <msg>`        | Echo a message        |                                     |
| `SET <key> <val>`   | Set string value      | AOF-logged                          |
| `GET <key>`         | Get string value      | Returns `$-1` if missing            |
| `DEL <k1> [k2 …]`   | Delete keys           | Returns `:<count>`                  |
| `EXISTS <k1> [k2 …]`| Count existing keys   | Returns `:<count>`                  |
| `EXPIRE <key> <sec>`| Set key expiration    | Returns `:1` or `:0`                |
| `TTL <key>`         | Seconds to expire     | `-2` (missing), `-1` (no TTL)       |
| `INFO`              | Metrics snapshot      | Bulk string of `key:value` lines    |

## Configuration

**Server env-vars**

| Var      | Default  | Description   |
|---------|----------|---------------|
| `PORT`  | `6380`   | TCP port      |
| `AOF_PATH` | `data.aof` | AOF file path |

**Dashboard env-vars**

| Var          | Default     | Description          |
|--------------|-------------|----------------------|
| `REDIS_HOST` | `127.0.0.1` | Server host          |
| `REDIS_PORT` | `6380`      | Server port          |
| `PORT`       | `8080`      | Dashboard HTTP port  |

Flush / fsync intervals are hard-coded (~100 ms flush, ~1 s fsync).

## Project Layout

```
.
├─ cpp/
│  ├─ server.cpp        ← epoll loop, connections, periodic tasks
│  ├─ resp.{h,cpp}      ← RESP parser / encoder
│  ├─ commands.{h,cpp}  ← command handlers + TTL logic
│  ├─ aof.{h,cpp}       ← append-only file (flush / replay)
│  └─ metrics.{h,cpp}   ← counters, uptime, clients
├─ dashboard/
│  ├─ server.js         ← Express proxy to INFO
│  └─ public/           ← index.html, app.js, styles.css
├─ bench/               ← simple smoke / load scripts
├─ Dockerfile
├─ docker-compose.yml
└─ README.md
```

## Troubleshooting

- “Makefile: missing separator” → command lines under targets must start with a Tab, not spaces.
- “Port already in use” → change `PORT` or stop the other process.
- Dashboard shows empty metrics → ensure dashboard `REDIS_PORT` matches server port.
- AOF not replayed → give ~1.5 s to flush / fsync before killing the server, or check `AOF_PATH`.

## License

MIT
