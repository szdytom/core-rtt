# Core RTT Architecture

This document describes the system architecture of Core RTT, a programming game platform where user‑submitted strategies compete in head‑to‑head matches.

## High‑Level Architecture

The platform follows a **control‑plane / worker** pattern with a clear separation between metadata management and binary asset storage.  
All components are designed to scale horizontally while keeping initial deployment simple.

| Component | Responsibility |
|-----------|----------------|
| **Next.js App** | Serves the frontend, exposes REST API, handles authentication, matchmaking, result processing, and worker coordination. |
| **Database** | Stores metadata: users, strategies, matches, rankings. |
| **Object Storage (S3‑compatible)** | Stores strategy binaries (ELF files) and compressed match replays. |
| **Game Engine Workers** | Executes matches by pulling strategies from S3, invoking the C++ game engine, and uploading results. |

```
        ┌──────────────────────────────────────────────────────────────────────┐
        │                          Internet                                    │
        └──────────────────────────────────────────────────────────────────────┘
                                        │
                    ┌───────────────────┼───────────────────┐
                    │                   │                   │
                    ▼                   ▼                   ▼
        ┌───────────────────┐ ┌───────────────────┐ ┌───────────────────┐
        │      Browser      │ │      Browser      │ │      Browser      │
        │      (User)       │ │      (User)       │ │      (User)       │
        └─────────┬─────────┘ └─────────┬─────────┘ └─────────┬─────────┘
                  │                     │                     │
                  └─────────────────────┼─────────────────────┘
                                        │ HTTPS (REST)
                                        ▼
        ┌─────────────────────────────────────────────────────────────────────┐
        │                         Control Plane                               │
        │  ┌─────────────────────────────────────────────────────────────┐    │
        │  │                    Next.js Application                      │    │
        │  │  • User auth (GitHub)      • Matchmaking                    │    │
        │  │  • Strategy management     • Result processing              │    │
        │  │  • Map management          • Worker coordination            │    │
        │  └───────────────────┬─────────────────┬───────────────────────┘    │
        │                      │                 │                            │
        │          ┌───────────┘                 └───────────┐                │
        │          │ SQL (Prisma)                            │ WebSocket      │
        │          ▼                                         ▼              │
        │  ┌───────────────────┐                  ┌───────────────────────┐   │
        │  │     Database      │                  │   Worker Registry     │   │
        │  │  (SQLite / PG)    │                  │   & Task Dispatcher   │   │
        │  └───────────────────┘                  └───────────┬───────────┘   │
        └─────────────────────────────────────────────────────┼───────────────┘
                                                              │
                                        ┌─────────────────────┼─────────────────────┐
                                        │                     │                     │
                                        ▼                     ▼                     ▼
                              ┌─────────────────┐   ┌─────────────────┐   ┌─────────────────┐
                              │    Worker 1     │   │    Worker 2     │   │    Worker N     │
                              │  (NAS / Laptop) │   │  (NAS / Laptop) │   │   (Cloud VM)    │
                              │                 │   │                 │   │                 │
                              │ • WebSocket     │   │ • WebSocket     │   │ • WebSocket     │
                              │ • LRU cache     │   │ • LRU cache     │   │ • LRU cache     │
                              │ • C++ engine    │   │ • C++ engine    │   │ • C++ engine    │
                              └────────┬────────┘   └────────┬────────┘   └────────┬────────┘
                                       │                     │                     │
                                       └─────────────────────┼─────────────────────┘
                                                             │ S3 API (pre‑signed URLs)
                                                             ▼
                                        ┌─────────────────────────────────────────────────┐
                                        │           Object Storage (S3‑compatible)        │
                                        │  ┌─────────────────┐  ┌─────────────────────┐   │
                                        │  │ Strategy ELFs   │  │ Compressed Replays  │   │
                                        │  │ (per user/ver)  │  │   (zstd)            │   │
                                        │  └─────────────────┘  └─────────────────────┘   │
                                        └─────────────────────────────────────────────────┘
```


## Control Plane: Next.js Application

A single Next.js instance runs the entire control plane. It is the only component directly exposed to the internet.

### Responsibilities
- **User authentication** – GitHub OAuth.
- **Strategy management** – upload, versioning, metadata.
- **Map management** – store map definitions and metadata.
- **Matchmaking** – generate match plans (ELO‑based).
- **Match results** – ingest results, update ELO, serve replay metadata.
- **Worker coordination** – manage WebSocket connections to workers, dispatch tasks, handle result callbacks.

### Technology Stack
- **Runtime**: Node.js (LTS)
- **Framework**: Next.js (App Router) – provides both API routes and static frontend hosting.
- **ORM**: Prisma (supports SQLite and PostgreSQL with the same schema).
- **Authentication**: NextAuth.js / Auth.js with GitHub provider.

## Database

The database stores **only metadata**. All large binary files are kept in object storage.

### Technology Choice
- **Development / small scale**: SQLite (file‑based, zero configuration).
- **Production / scaling**: PostgreSQL (managed or self‑hosted).

Prisma abstracts the database layer, making migration straightforward.

### Key Entities
- `User` – GitHub id, name, ELO rating.
- `Strategy` – owner, version, S3 path, hash, creation time.
- `Match` – players, map, status, result, replay S3 path.
- `MatchPlan` – scheduled matches waiting for execution.

## Object Storage & CDN

An S3‑compatible service is used to store:

- **Strategy binaries** – uploaded by users, stored under `strategies/{userId}/{version}/strategy.elf`.
- **Match replays** – compressed with **zstd**, stored under `replays/{matchId}.zst`.

A CDN (or the object storage’s native CDN) is enabled to accelerate replay downloads.  
Pre‑signed URLs are generated by the control plane to grant temporary upload/download permissions.

## Game Engine Workers

Workers are stateless processes that execute matches. They connect to the control plane via **WebSocket** and remain idle until a task is assigned.

### Responsibilities
- Register with the control plane on startup.
- Maintain a WebSocket connection and respond to ping/pong heartbeats.
- When a match is assigned:
  1. Download both players’ strategy binaries from S3 (using pre‑signed URLs).
  2. Cache binaries locally with an LRU eviction policy.
  3. Invoke the C++ game engine CLI (provided as a separate binary) with the strategies and map.
  4. Compress the resulting replay using zstd.
  5. Upload the compressed replay to S3 (using a pre‑signed PUT URL provided by the control plane).
  6. Notify the control plane of the match outcome (success or failure).

### Technology Stack
- **Language**: Node.js (TypeScript)
- **Engine invocation**: `child_process.execFile` or `spawn`
- **Caching**: simple in‑memory LRU cache with size limits (configurable)
- **Concurrency**: each worker can process one match at a time (configurable via `MAX_CONCURRENT_TASKS`)

### Deployment
Workers can run anywhere with network access to S3 and the control plane (no public IP needed).  
They are ideal for deployment on:
- Local NAS (e.g., N200 Linux box)
- Spare laptops
- Cloud VMs (if scaling up)

## Task Dispatch Mechanism

The control plane maintains a list of idle workers. When a new match is ready:

1. The match is stored in the database with status `pending`.
2. The control plane selects an idle worker (round‑robin or least‑loaded).
3. It sends a JSON message over the WebSocket containing:
   - `matchId`
   - `map` definition (or reference)
   - Strategy S3 paths (or pre‑signed download URLs)
   - Pre‑signed upload URL for the replay
4. The worker acknowledges receipt and starts processing.
5. After completion, the worker sends a final message with the result (`success`/`failure`).

If a worker fails to respond within a timeout, the match is reassigned.

## Security Considerations

| Area | Measure |
|------|---------|
| **Worker authentication** | Each worker authenticates with a pre‑shared secret (environment variable) during WebSocket handshake. |
| **S3 access** | Pre‑signed URLs are short‑lived (e.g., 10 minutes) and restricted to the specific bucket/path. |
| **Strategy execution** | The C++ engine runs inside a **sandbox** (seccomp, Docker, or custom jail) to prevent malicious code from affecting the host. |
| **API security** | All user‑facing endpoints require GitHub OAuth; role‑based checks for strategy ownership. |

## Scalability & Evolution

The architecture is designed to evolve without major rewrites:

| Component | Current | Future |
|-----------|---------|--------|
| Database | SQLite (embedded) | PostgreSQL (cluster) – Prisma makes the switch seamless |
| Task queue | In‑memory + database polling | Dedicated queue (BullMQ + Redis) for better reliability |
| Worker scaling | Manual addition | Autoscaling based on queue depth (Kubernetes, cloud functions) |
| Storage | S3‑compatible (any provider) | Same abstraction, just change credentials |

The Next.js control plane remains a single node until traffic requires multiple instances (then a shared Redis session store and database connection pool become necessary).

## Deployment Overview (Simplified)

- **Control plane** – run on a small cloud VM (2C2G) behind Nginx with Let’s Encrypt.
- **Database** – SQLite file stored on the same VM (backed up regularly).
- **Object storage** – use Cloudflare R2, Wasabi, or a self‑hosted MinIO instance.
- **Workers** – run as systemd services on available machines (NAS, laptop). Each connects to the control plane via WebSocket.

## Technology Stack Summary

| Layer | Technology |
|-------|------------|
| Frontend / API | Next.js (TypeScript) |
| Authentication | NextAuth.js (GitHub) |
| ORM | Prisma |
| Database | SQLite / PostgreSQL |
| Object Storage | S3‑compatible (R2, MinIO, etc.) |
| Compression | zstd |
| Engine | C++ CLI (sandboxed) |
| Worker Runtime | Node.js (TypeScript) |
| Communication | WebSocket (workers), REST (users) |

This architecture prioritises **simplicity for early development** while retaining the ability to scale up each component independently when the user base grows.
