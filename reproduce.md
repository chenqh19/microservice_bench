# General

Set up containers:

```

```

# E2E throughput

1. For each of protobuf and ser1de:

  a. Change the line in `experiments/collect_latency.py` to `file_name = "tail_{protobuf/ser1de}_0B.txt"` correspondingly.

  b. Setup container and run `experiments/collect_latency.py`. The result will be in `experiments/tail_{protobuf/ser1de}_0B.txt`.

2. Use `experiments/get_avg_latency.py` to generate `experiments/latency_p99_all.pdf`.

# Breakdown of latency

1. Set `#define ENABLE_TIMING 1` in `serialization_utils.h` to collect timing.

2. For each of protobuf and ser1de:

  a. Set up container and run workload generation; The logs will be inside docker containers, but `docker_compose.yml` has binded it to `logs/` so they are already there.

  b. Use `experiments/breakdown.py` to generate a summary, put it into `experiments/breakdown_ser1de.txt` or `experiments/breakdown_protobuf.txt` respectively.

3. Use `experiments/make_breakdown_stacked_plot.py` to generate `experiments/performance_breakdown_stacked.pdf`.

# With Compression

