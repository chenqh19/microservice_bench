#!/bin/bash

sudo docker compose down
sudo docker compose build
sudo docker compose up -d

# sleep 1

sudo docker update --cpuset-cpus="0-15" socialnet-frontend-1
sudo docker update --cpuset-cpus="0-15" socialnet-compose_post-1
sudo docker update --cpuset-cpus="0-15" socialnet-social_graph-1
sudo docker update --cpuset-cpus="0-15" socialnet-user-1
sudo docker update --cpuset-cpus="0-15" socialnet-post_storage-1
sudo docker update --cpuset-cpus="0-15" socialnet-user_timeline-1
sudo docker update --cpuset-cpus="0-15" socialnet-home_timeline-1
sudo docker update --cpuset-cpus="0-15" socialnet-unique_id-1

echo "All containers have been pinned to CPU cores" 

# For the main dockerd process
sudo taskset -pc 0-15 $(pgrep -f "dockerd --containerd=/run/containerd/containerd.sock")

# For all dockerd threads
for pid in $(ps -eLf | grep dockerd | awk '{print $4}'); do
    # do not show output
    sudo taskset -pc 0-15 $pid
done

# sudo docker update --cpuset-cpus="0-11" hotel-frontend-1
# sudo docker update --cpuset-cpus="12-19" hotel-user-1
# sudo docker update --cpuset-cpus="12-19" hotel-geo-1
# sudo docker update --cpuset-cpus="12-19" hotel-rate-1
# sudo docker update --cpuset-cpus="12-19" hotel-search-1
# sudo docker update --cpuset-cpus="12-19" hotel-recommendation-1
# sudo docker update --cpuset-cpus="12-19" hotel-profile-1
# sudo docker update --cpuset-cpus="12-19" hotel-reservation-1

# echo "All containers have been pinned to CPU cores" 

# # For the main dockerd process
# sudo taskset -pc 20-152 $(pgrep -f "dockerd --containerd=/run/containerd/containerd.sock")

# # For all dockerd threads
# for pid in $(ps -eLf | grep dockerd | awk '{print $4}'); do
#     sudo taskset -pc 20-152 $pid
# done