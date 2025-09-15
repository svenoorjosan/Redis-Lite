# Redis-Lite

A small Redis-compatible in-memory key-value store written in **C++**, with **TTL** and **AOF** persistence, plus a tiny **Node/Express** metrics dashboard.

> Not production software. It’s a learning project that’s great for demos, benchmarks, and interviews.

---

## Features

- **RESP** protocol over TCP (works with `redis-cli` for the supported commands)
- Commands: `PING`, `ECHO`, `SET`, `GET`, `DEL`, `EXISTS`, `EXPIRE`, `TTL`, `INFO`
- **TTL**: lazy expiration on reads + active sweeper using a min-heap
- **AOF** persistence: buffered appends, ~100 ms flush, ~1 s fsync; replay on startup
- **Metrics dashboard** at `http://localhost:8080` (uptime, clients, keys, command count, expired keys, AOF size)
- **Graceful shutdown**: final AOF flush on SIGINT/SIGTERM

---

## Quick Start

### Local

```bash
# build the C++ server
make

# run the server (defaults: PORT=6380, AOF_PATH=./data.aof)
PORT=6381 AOF_PATH=data-prod.aof ./redis-lite
# ─── new terminal ───
cd dashboard
npm i                 # first run only
REDIS_PORT=6381 node server.js
# open http://localhost:8080


