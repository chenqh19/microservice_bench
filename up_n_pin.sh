#!/bin/bash

sudo docker compose down
sudo docker compose build
sudo docker compose up -d

# sleep 1

sudo docker update --cpuset-cpus="0-31" microservice_bench-frontend-1
sudo docker update --cpuset-cpus="0-31" microservice_bench-user-1
sudo docker update --cpuset-cpus="0-31" microservice_bench-geo-1
sudo docker update --cpuset-cpus="0-31" microservice_bench-rate-1
sudo docker update --cpuset-cpus="0-31" microservice_bench-search-1
sudo docker update --cpuset-cpus="0-31" microservice_bench-recommendation-1
sudo docker update --cpuset-cpus="0-31" microservice_bench-profile-1
sudo docker update --cpuset-cpus="0-31" microservice_bench-reservation-1

echo "All containers have been pinned to CPU cores" 

# For the main dockerd process
sudo taskset -pc 0-31 $(pgrep -f "dockerd --containerd=/run/containerd/containerd.sock")

# For all dockerd threads
for pid in $(ps -eLf | grep dockerd | awk '{print $4}'); do
    # do not show output
    sudo taskset -pc 0-31 $pid
done

# sudo docker update --cpuset-cpus="0-11" microservice_bench-frontend-1
# sudo docker update --cpuset-cpus="12-19" microservice_bench-user-1
# sudo docker update --cpuset-cpus="12-19" microservice_bench-geo-1
# sudo docker update --cpuset-cpus="12-19" microservice_bench-rate-1
# sudo docker update --cpuset-cpus="12-19" microservice_bench-search-1
# sudo docker update --cpuset-cpus="12-19" microservice_bench-recommendation-1
# sudo docker update --cpuset-cpus="12-19" microservice_bench-profile-1
# sudo docker update --cpuset-cpus="12-19" microservice_bench-reservation-1

# echo "All containers have been pinned to CPU cores" 

# # For the main dockerd process
# sudo taskset -pc 20-312 $(pgrep -f "dockerd --containerd=/run/containerd/containerd.sock")

# # For all dockerd threads
# for pid in $(ps -eLf | grep dockerd | awk '{print $4}'); do
#     sudo taskset -pc 20-312 $pid
# done