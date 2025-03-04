../wrk2/wrk -D exp -t 10 -c 10 -d 10 -L -s ./wrk2/scripts/hotel-reservation/mixed-workload_type_1.lua http://localhost:12345 -R 100
