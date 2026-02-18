# Matmul (AMX / SW)

## Threading model

- **run_matmul**: Single thread. One matmul, then exit.
- **AMX kernel**: Uses `thread_local` state; safe to call from multiple threads.
- **matmul_workload**: Thread pool (N workers) + one generator thread. Generator enqueues jobs at **fixed RPS** with **exponential inter-arrival**; workers pop and run matmul.

## Build

```bash
cd matmul/build && cmake .. && make
```

Produces `run_matmul` and `matmul_workload`.

## One-shot

```bash
./run_matmul [M K N]   # default 768 768 768
```

## Fixed RPS workload (no TCP)

```bash
./matmul_workload [rps] [duration_sec] [num_workers] [M K N]
```

- **rps**: Target requests per second. Same as **wrk2 -D exp**: inter-arrival times are exponential with **rate = rps** (i.e. λ = rps, mean inter-arrival = 1/rps seconds). So the distribution matches wrk2’s exponential arrival model.
- **duration_sec**: How long to submit new jobs.
- **num_workers**: Thread-pool size (concurrent matmuls).
- **M K N**: Fixed matmul dimensions (default 768 768 768).

Example: 2 req/s for 60 s, 4 workers, 768³ matmul:

```bash
./matmul_workload 2 60 4 768 768 768
```

Output: `submitted=... completed=... avg_latency_us=...`
