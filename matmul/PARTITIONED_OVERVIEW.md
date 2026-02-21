# Partitioned Matmul App — Whole Picture

## Core layout (single process)

```
Cores 0–3   : AMX (hardware) — blackbox, 4 threads, no control logic, no stats
Cores 4–31  : Service — 28 threads, request handling + all stats
Core 32     : Generator (workload) or Acceptor (server) — 1 thread
```

- **AMX_CORE_START** = 0, **NUM_AMX_CORES** = 4  
- **SERVICE_CORE_START** = 4, **NUM_SERVICE_CORES** = 28  
- **GENERATOR_CORE_START** = 32  

---

## Architecture (both workload and server)

```
                    ┌─────────────────────────────────────────────────────────┐
                    │                    Single process                        │
                    │                                                          │
  Workload:         │   Generator (core 32)                                   │
  ─────────        │        │                                                  │
  [time, rps]      │        ▼                                                  │
                   │   JobQueue ──► Service workers (cores 4–31)                │
  Server:          │        │            │                                      │
  ───────          │   Acceptor (core 32)                                      │
  [TCP clients]   │        │            │                                      │
                   │        ▼            │                                      │
                   │   ClientQueue ──────┘                                     │
                   │                │                                           │
                   │                ▼                                           │
                   │         service_matmul_one(M,K,N,A,B, use_amx, amx_queue)  │
                   │                │                                           │
                   │     ┌─────────┴─────────┐                                  │
                   │     │                   │                                  │
                   │  use_amx?            use_amx?                             │
                   │  Hardware             Software                             │
                   │     │                   │                                  │
                   │     │                   └──► sw_matmul_impl (on service)   │
                   │     │                                                      │
                   │     │  BEFORE SUBMISSION (on service core 4–31):            │
                   │     │    record_hw_submission()                             │
                   │     │    record_hw_start_get_inflight_after()  → key        │
                   │     │                                                      │
                   │     ▼                                                      │
                   │   AmxJobQueue ──────────────────────────────────────────► │
                   │     (M,K,N,A,B,promise)                    Cores 0–3      │
                   │                                                             │
                   │                                         ┌──────────────────┴──┐
                   │                                         │  AMX workers        │
                   │                                         │  (blackbox)         │
                   │                                         │  pop → amx_matmul   │
                   │                                         │  → set_value(lat)   │
                   │                                         └──────────────────┬──┘
                   │                                                             │
                   │     AFTER RETURN (on service core 4–31):                    │
                   │     fut.get() ◄────────────────────────────────────────────┘
                   │     record_hw_finish(key, latency_us)  [or nosample]         │
                   │                                                             │
                   └─────────────────────────────────────────────────────────┘
```

- **Cores 0–3**: Only pop from `AmxJobQueue`, run `amx_matmul_impl`, set promise. No metrics, no decision logic.  
- **Cores 4–31**: Do all stats **before** pushing to the blackbox and **after** getting the result; they never observe the inside of 0–3.

---

## Data flow

| Stage | Where | What |
|-------|--------|------|
| Input | Core 32 | Workload: generator pushes (M,K,N) to JobQueue. Server: acceptor pushes client fd to ClientQueue. |
| Dequeue | Cores 4–31 | Service worker pops job/client, allocates A/B, decides use_amx. |
| HW path | Cores 4–31 | **Before submit**: record_hw_submission(), record_hw_start_get_inflight_after(). Push AmxJob to AmxJobQueue. Block on future. |
| Blackbox | Cores 0–3 | Pop AmxJob, run amx_matmul_impl, set_value(latency_us). No other logic. |
| HW path | Cores 4–31 | **After return**: fut.get(); record_hw_finish(key, latency_us) (or nosample). |
| SW path | Cores 4–31 | sw_matmul_impl on same thread; no HW metrics. |
| Output | Cores 4–31 | Workload: completed++, total_latency_us. Server: write "OK \<latency_us>\n", close fd. |

---

## Stats (all on service side, cores 4–31)

- **Before submission** (when use_amx and path is Hardware, inside `execute_with_path`):
  - `record_hw_submission()` — total HW submissions
  - `record_hw_start_get_inflight_after()` — inflight++, capture key (inflight_at_submit for ring)
- **After return** (when `op()` returns, i.e. after `fut.get()`):
  - `record_hw_finish(key, latency_us)` — inflight--, optionally write (latency_us, inflight_at_submit) to ring
  - or `record_hw_finish_nosample()` — inflight-- only

**Inflight** = number of jobs “submitted to the blackbox (cores 0–3) and not yet returned” (queue + running inside 0–3). Measured only from submission and return on cores 4–31; cores 0–3 are not observed.

---

## Files

| File | Role |
|------|------|
| **matmul_partitioned_common.h** | Core constants, AmxJob, AmxJobQueue, `amx_worker`, `service_matmul_one` |
| **matmul_workload_partitioned.cpp** | Standalone workload: generator, JobQueue, 4 AMX threads, 28 service threads; init_hw_counters_master, start_global_hw_logger_to_file |
| **matmul_server_partitioned.cpp** | TCP server: acceptor, ClientQueue, 4 AMX threads, 28 service threads; same scheduler init |
| **scheduler/decision.h** | `execute_with_path` — before/after op() does record_hw_submission, record_hw_start_get_inflight_after, record_hw_finish |
| **scheduler/metrics.h** | Shared-memory counters (total, inflight, ring), record_* and logger |

---

## Run

- **Workload**: `./matmul_workload_partitioned [rps] [duration_sec] [use_amx] [M K N]`  
  Example: `./matmul_workload_partitioned 20 60 1 2048 2048 1024`
- **Server**: `./matmul_server_partitioned [port] [use_amx]`  
  Example: `echo "768,768,768" | nc localhost 50061`
