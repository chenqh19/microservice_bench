# General

Set up containers (containers will use core 0-31):
```
./up_n_pin.sh
```

Run workload on another set of cores (change RPS to what you want):
```
taskset -c 32-63 ../wrk2/wrk -D fixed -t 100 -c 100 -d 30 -L -s ../wrk_scripts/scripts/hotel-reservation/mixed-workload_type_1.lua http://localhost:50050 -R {RPS}
```

# E2E throughput

1. For each of protobuf and ser1de:

  a. Change the line in `experiments/collect_latency.py` to `file_name = "tail_{protobuf/ser1de}_0B.txt"` correspondingly.

  b. Setup container and run `experiments/collect_latency.py`. The result will be in `experiments/tail_{protobuf/ser1de}_0B.txt`.

2. Use `experiments/get_avg_latency.py` to generate `experiments/latency_p99_all.pdf`.

# Breakdown of latency

1. Set `#define ENABLE_TIMING 1` in `serialization_utils.h` to collect timing.

2. For each of protobuf and ser1de:

  a. Set up container and run workload generation (at a small workload); The workload can run multiple times for a warmup. The logs will be inside docker containers, but `docker_compose.yml` has binded it to `logs/` so they are already there.

  b. Use `experiments/breakdown.py` to generate a summary, put it into `experiments/breakdown_ser1de.txt` or `experiments/breakdown_protobuf.txt` respectively.

3. Use `experiments/make_breakdown_stacked_plot.py` to generate `experiments/performance_breakdown_stacked.pdf`.

# With Compression

1. Go to branch `compression_server`.

2. In `config.h`, change `#define USE_HARDWARE_COMPRESSION` and `#define USE_SER1DE` across `{0/1}` accordingly.

3. For each config, setup container and run `experiments/collect_latency.py` to collect the result in separate files.

4. Use `experiments/plot_4.py` to make the plot into `experiments/latency_p99_four.pdf`.
