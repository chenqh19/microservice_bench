#!/usr/bin/env python3
"""
scheduler_demo.py
A tiny async "inference gateway" style demo:
- scheduler: EDF (earliest deadline first) + priority
- backpressure: bounded queue with block or reject
- metrics: throughput, queue depth, inflight, drops, latency p50/p95/p99 (sliding window)

Pure stdlib. Python 3.10+ recommended.
"""

import asyncio
import dataclasses
import heapq
import random
import signal
import time
from collections import deque
from typing import Deque, Dict, Optional, Tuple


# ----------------------------
# Data model
# ----------------------------
@dataclasses.dataclass(frozen=True)
class Request:
    req_id: int
    created_at: float
    deadline_at: float
    priority: int          # higher = more important
    payload_cost_ms: int   # simulated work cost


# ----------------------------
# Metrics
# ----------------------------
class Metrics:
    def __init__(self, latency_window: int = 5000):
        self.start = time.monotonic()

        # counters
        self.accepted = 0
        self.rejected = 0
        self.completed = 0
        self.timed_out = 0

        # gauges
        self.inflight = 0
        self.queue_depth = 0

        # latency samples (seconds)
        self.latencies: Deque[float] = deque(maxlen=latency_window)

        # for QPS
        self._last_report_t = self.start
        self._last_completed = 0

        # async lock for safe updates
        self._lock = asyncio.Lock()

    async def on_accept(self):
        async with self._lock:
            self.accepted += 1

    async def on_reject(self):
        async with self._lock:
            self.rejected += 1

    async def on_timeout(self):
        async with self._lock:
            self.timed_out += 1

    async def on_start(self):
        async with self._lock:
            self.inflight += 1

    async def on_finish(self, latency_s: float):
        async with self._lock:
            self.inflight -= 1
            self.completed += 1
            self.latencies.append(latency_s)

    async def set_queue_depth(self, depth: int):
        async with self._lock:
            self.queue_depth = depth

    @staticmethod
    def _percentile(sorted_vals, p: float) -> Optional[float]:
        if not sorted_vals:
            return None
        # nearest-rank
        k = int((p / 100.0) * (len(sorted_vals) - 1))
        k = max(0, min(k, len(sorted_vals) - 1))
        return sorted_vals[k]

    async def snapshot(self) -> Dict[str, object]:
        async with self._lock:
            now = time.monotonic()
            elapsed = now - self.start

            # QPS over last reporting interval
            interval = max(1e-6, now - self._last_report_t)
            done_delta = self.completed - self._last_completed
            qps = done_delta / interval
            self._last_report_t = now
            self._last_completed = self.completed

            lat_sorted = sorted(self.latencies)
            p50 = self._percentile(lat_sorted, 50)
            p95 = self._percentile(lat_sorted, 95)
            p99 = self._percentile(lat_sorted, 99)

            return {
                "uptime_s": round(elapsed, 2),
                "accepted": self.accepted,
                "rejected": self.rejected,
                "completed": self.completed,
                "timed_out": self.timed_out,
                "inflight": self.inflight,
                "queue_depth": self.queue_depth,
                "qps_recent": round(qps, 2),
                "lat_p50_ms": None if p50 is None else round(p50 * 1000, 2),
                "lat_p95_ms": None if p95 is None else round(p95 * 1000, 2),
                "lat_p99_ms": None if p99 is None else round(p99 * 1000, 2),
                "lat_samples": len(self.latencies),
            }


# ----------------------------
# Scheduler with backpressure
# ----------------------------
class Scheduler:
    """
    Accepts incoming Requests via submit().
    Internally:
      - bounded ingress queue (for backpressure)
      - heap for scheduling (EDF + priority)
      - worker pool for processing
    """

    def __init__(
        self,
        *,
        max_queue: int = 200,
        backpressure: str = "block",   # "block" or "reject"
        workers: int = 4,
        max_heap: int = 10000,         # protect against unbounded heap
        metrics: Metrics,
    ):
        if backpressure not in ("block", "reject"):
            raise ValueError("backpressure must be 'block' or 'reject'")
        self.backpressure = backpressure
        self.metrics = metrics

        self.ingress = asyncio.Queue(maxsize=max_queue)
        self._heap: list[Tuple[float, int, int, Request]] = []
        self._seq = 0
        self._max_heap = max_heap

        self._stop = asyncio.Event()

        self._dispatcher_task: Optional[asyncio.Task] = None
        self._worker_tasks: list[asyncio.Task] = []
        self._work_q = asyncio.Queue()  # unbounded internal work queue for workers

        self._workers = workers

    async def start(self):
        self._dispatcher_task = asyncio.create_task(self._dispatcher_loop(), name="dispatcher")
        for i in range(self._workers):
            self._worker_tasks.append(asyncio.create_task(self._worker_loop(i), name=f"worker-{i}"))

    async def stop(self):
        self._stop.set()
        # let loops exit
        if self._dispatcher_task:
            await self._dispatcher_task
        # drain worker queue by sending sentinels
        for _ in self._worker_tasks:
            await self._work_q.put(None)
        await asyncio.gather(*self._worker_tasks, return_exceptions=True)

    async def submit(self, req: Request) -> bool:
        """
        Returns True if accepted, False if rejected (only possible when backpressure='reject').
        backpressure='block' will await until space is available.
        """
        if self.backpressure == "reject":
            if self.ingress.full():
                await self.metrics.on_reject()
                return False

        # block until there's space (or immediate if not full)
        await self.ingress.put(req)
        await self.metrics.on_accept()
        await self.metrics.set_queue_depth(self.ingress.qsize())
        return True

    async def _dispatcher_loop(self):
        """
        Pull from ingress -> push into heap -> pop scheduled into worker queue.
        EDF + priority:
          heap key = (deadline_at, -priority, seq)
        """
        while not self._stop.is_set():
            try:
                req = await asyncio.wait_for(self.ingress.get(), timeout=0.05)
            except asyncio.TimeoutError:
                req = None

            # move ingress item into heap
            if req is not None:
                if len(self._heap) < self._max_heap:
                    self._seq += 1
                    heapq.heappush(self._heap, (req.deadline_at, -req.priority, self._seq, req))
                else:
                    # heap overflow => reject/drop
                    await self.metrics.on_reject()

                self.ingress.task_done()
                await self.metrics.set_queue_depth(self.ingress.qsize())

            # schedule as many as we can to workers
            # Here: keep internal work queue from growing too fast by limiting burst.
            burst = 0
            while self._heap and burst < 50:
                # If internal work queue is too large, pause scheduling => backpressure inside system
                if self._work_q.qsize() > 4 * self._workers:
                    break

                _, _, _, next_req = heapq.heappop(self._heap)

                # deadline check: if already missed, count timeout and drop
                now = time.monotonic()
                if now > next_req.deadline_at:
                    await self.metrics.on_timeout()
                    continue

                await self._work_q.put(next_req)
                burst += 1

    async def _worker_loop(self, worker_id: int):
        while True:
            item = await self._work_q.get()
            if item is None:
                self._work_q.task_done()
                return

            req: Request = item
            await self.metrics.on_start()

            # Simulate work (e.g., model inference)
            # payload_cost_ms controls per-request cost
            await asyncio.sleep(req.payload_cost_ms / 1000.0)

            done_at = time.monotonic()
            latency_s = done_at - req.created_at
            await self.metrics.on_finish(latency_s)

            self._work_q.task_done()


# ----------------------------
# Load generator
# ----------------------------
async def load_generator(
    sched: Scheduler,
    *,
    rate_rps: float = 80.0,
    duration_s: float = 30.0,
    p_high_prio: float = 0.2,
    base_deadline_ms: int = 400,
    jitter_deadline_ms: int = 400,
):
    """
    Generates requests at ~rate_rps for duration_s.
    Some requests have higher priority and tighter deadlines.
    """
    start = time.monotonic()
    req_id = 0
    interval = 1.0 / max(1e-6, rate_rps)

    while time.monotonic() - start < duration_s:
        now = time.monotonic()
        req_id += 1

        # priority & deadline
        high = random.random() < p_high_prio
        priority = 10 if high else 1

        deadline_ms = base_deadline_ms + random.randint(0, jitter_deadline_ms)
        if high:
            deadline_ms = max(80, int(deadline_ms * 0.6))  # tighter deadline for high-prio

        # simulated compute cost (ms): mix of short and long
        if high:
            cost_ms = random.randint(20, 60)
        else:
            cost_ms = random.randint(30, 120)

        req = Request(
            req_id=req_id,
            created_at=now,
            deadline_at=now + (deadline_ms / 1000.0),
            priority=priority,
            payload_cost_ms=cost_ms,
        )

        await sched.submit(req)

        # keep approximate rate
        await asyncio.sleep(interval)


async def metrics_reporter(metrics: Metrics, every_s: float = 1.0):
    while True:
        await asyncio.sleep(every_s)
        snap = await metrics.snapshot()
        print(
            f"[uptime={snap['uptime_s']}s] "
            f"qps={snap['qps_recent']} done={snap['completed']} "
            f"acc={snap['accepted']} rej={snap['rejected']} to={snap['timed_out']} "
            f"inflight={snap['inflight']} q={snap['queue_depth']} "
            f"p50={snap['lat_p50_ms']}ms p95={snap['lat_p95_ms']}ms p99={snap['lat_p99_ms']}ms "
            f"samples={snap['lat_samples']}"
        )


# ----------------------------
# Main
# ----------------------------
async def main():
    metrics = Metrics(latency_window=5000)

    # Try toggling backpressure:
    #   backpressure="block"  -> producers slow down when queue is full
    #   backpressure="reject" -> immediate rejection when queue is full
    sched = Scheduler(
        max_queue=200,
        backpressure="reject",
        workers=4,
        metrics=metrics,
    )

    await sched.start()

    reporter = asyncio.create_task(metrics_reporter(metrics, every_s=1.0))

    # Stop on Ctrl+C gracefully
    stop_event = asyncio.Event()

    def _handle_sigint(*_):
        stop_event.set()

    loop = asyncio.get_running_loop()
    try:
        loop.add_signal_handler(signal.SIGINT, _handle_sigint)
        loop.add_signal_handler(signal.SIGTERM, _handle_sigint)
    except NotImplementedError:
        # Windows / limited environments
        pass

    # Run load for 30s or until Ctrl+C
    load_task = asyncio.create_task(
        load_generator(
            sched,
            rate_rps=120.0,       # increase to stress
            duration_s=30.0,
            p_high_prio=0.2,
            base_deadline_ms=300,
            jitter_deadline_ms=500,
        )
    )

    done, _ = await asyncio.wait(
        {load_task, stop_event.wait()},
        return_when=asyncio.FIRST_COMPLETED,
    )

    # If stopped early, cancel load
    if not load_task.done():
        load_task.cancel()
        with contextlib.suppress(asyncio.CancelledError):
            await load_task

    reporter.cancel()
    with contextlib.suppress(asyncio.CancelledError):
        await reporter

    await sched.stop()

    final = await metrics.snapshot()
    print("\nFinal metrics:", final)


if __name__ == "__main__":
    import contextlib
    asyncio.run(main())