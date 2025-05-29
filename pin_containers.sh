#!/bin/bash

sudo docker update --cpuset-cpus="20-30" microservice_bench-frontend-1
sudo docker update --cpuset-cpus="31-31" microservice_bench-user-1
sudo docker update --cpuset-cpus="31-31" microservice_bench-geo-1
sudo docker update --cpuset-cpus="31-31" microservice_bench-rate-1
sudo docker update --cpuset-cpus="31-31" microservice_bench-search-1
sudo docker update --cpuset-cpus="31-31" microservice_bench-recommendation-1
sudo docker update --cpuset-cpus="31-31" microservice_bench-profile-1
sudo docker update --cpuset-cpus="31-31" microservice_bench-reservation-1

echo "All containers have been pinned to CPU cores" 

# For the main dockerd process
sudo taskset -pc 40-50 $(pgrep -f "dockerd --containerd=/run/containerd/containerd.sock")

# For all dockerd threads
for pid in $(ps -eLf | grep dockerd | awk '{print $4}'); do
    sudo taskset -pc 40-49 $pid
done