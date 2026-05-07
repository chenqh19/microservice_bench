import threading
import time
import random
from collections import deque
from typing import Callable, Optional, Dict


class BoundedExecutor:
    def __init__(self, num_workers: int, maxsize: int):
        self.num_workers = num_workers
        self.maxsize = maxsize

        # task queue
        self._queue = deque()

        # synchronization
        self._lock = threading.Lock()
        self._not_empty = threading.Condition(self._lock)
        self._not_full = threading.Condition(self._lock)

        # lifecycle
        self._shutdown = False
        self._workers = []

        # metrics (all protected by self._lock)
        self._submitted = 0
        self._dropped = 0
        self._completed = 0
        self._active_workers = 0

    # ----------------------------
    # Public APIs
    # ----------------------------
    def start(self) -> None:
        """Start worker threads."""
        # TODO: create and start num_workers daemon threads
        # target should be self._worker_loop
        raise NotImplementedError

    def submit(self, task: Callable[[], None], timeout: Optional[float] = None) -> bool:
        """
        Submit a task into the bounded queue.
        Return True on success, False if timeout or shutdown.

        Backpressure behavior:
        - if queue full, wait until space is available or timeout expires
        """
        # TODO:
        # 1) lock with self._not_full
        # 2) if shutdown => dropped++, return False
        # 3) while queue is full:
        #      wait(timeout remaining)
        #      handle timeout -> dropped++, return False
        #      if shutdown during wait -> dropped++, return False
        # 4) append task
        # 5) submitted++
        # 6) notify one waiting worker via self._not_empty
        # 7) return True
        raise NotImplementedError

    def shutdown(self, wait: bool = True) -> None:
        """
        Stop accepting new tasks and ask workers to exit.
        If wait=True, block until all workers exit.
        """
        # TODO:
        # 1) acquire lock, set _shutdown=True
        # 2) notify_all on both conditions (wake blocked submitters/workers)
        # 3) if wait: join all worker threads
        raise NotImplementedError

    def metrics(self) -> Dict[str, int]:
        """Return a snapshot of current metrics."""
        # TODO: lock and return dict:
        # submitted, dropped, completed, queue_size, active_workers
        raise NotImplementedError

    # ----------------------------
    # Internal worker loop
    # ----------------------------
    def _worker_loop(self) -> None:
        while True:
            task = None

            # Phase 1: dequeue
            # TODO:
            # with self._not_empty:
            #   while queue empty and not shutdown: wait()
            #   if queue empty and shutdown: return
            #   pop left task
            #   notify one submitter via self._not_full
            #   active_workers += 1
            raise NotImplementedError

            try:
                task()
            except Exception as e:
                # In real infra systems we'd log this
                print(f"[worker] task error: {e}")
            finally:
                # TODO:
                # with self._lock:
                #   active_workers -= 1
                #   completed += 1
                raise NotImplementedError


# ----------------------------
# Demo / simple stress test
# ----------------------------
def make_task(i: int, min_ms: int = 50, max_ms: int = 200) -> Callable[[], None]:
    def _task():
        # simulate work
        t = random.randint(min_ms, max_ms) / 1000.0
        time.sleep(t)
        # Uncomment to see detailed execution:
        # print(f"task {i} done in {t:.3f}s")
    return _task


def producer(executor: BoundedExecutor, producer_id: int, n: int):
    ok = 0
    fail = 0
    for i in range(n):
        task_id = producer_id * 10000 + i
        # try submit with timeout (backpressure)
        accepted = executor.submit(make_task(task_id), timeout=0.3)
        if accepted:
            ok += 1
        else:
            fail += 1
        # produce a bit faster than workers to trigger pressure
        time.sleep(random.uniform(0.005, 0.03))
    print(f"[producer {producer_id}] accepted={ok}, failed={fail}")


def main():
    ex = BoundedExecutor(num_workers=4, maxsize=20)
    ex.start()

    producers = []
    for pid in range(3):
        t = threading.Thread(target=producer, args=(ex, pid, 60), daemon=True)
        producers.append(t)
        t.start()

    # metrics reporter
    for _ in range(15):
        time.sleep(0.5)
        print("[metrics]", ex.metrics())

    for t in producers:
        t.join()

    # wait a bit for workers to drain queue
    time.sleep(2)
    ex.shutdown(wait=True)

    print("[final]", ex.metrics())


if __name__ == "__main__":
    main()