```bash
./up_n_pin.sh
```

run two times to warm up:
```bash
taskset -c 32-63 ./wrk2/wrk -D fixed -t 16 -c 16 -d 30 -L -s ./wrk_scripts/scripts/hotel-reservation/mixed-workload_type_1.lua http://localhost:50050 -R 1000
```

```bash
sudo rm logs/*
```

really collect logs:
```bash
taskset -c 32-63 ./wrk2/wrk -D fixed -t 16 -c 16 -d 30 -L -s ./wrk_scripts/scripts/hotel-reservation/mixed-workload_type_1.lua http://localhost:50050 -R 1000
```


```bash
cd experiments
```

collect timestamps:
```bash
python3 collect_timestamps.py
```
then rename `latency_summary.py` to `latency_summary_ser1de.py` or `latency_summary_protobuf.py` according to what is it from

after collecting both, compare them:
```bash
python3 compare_timestamps.py
```