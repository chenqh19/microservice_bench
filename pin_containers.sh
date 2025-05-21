#!/bin/bash

sudo docker update --cpuset-cpus="20-30" microservice_bench-frontend-1
sudo docker update --cpuset-cpus="31-31" microservice_bench-user-1
sudo docker update --cpuset-cpus="32-32" microservice_bench-geo-1
sudo docker update --cpuset-cpus="33-33" microservice_bench-rate-1
sudo docker update --cpuset-cpus="34-34" microservice_bench-search-1
sudo docker update --cpuset-cpus="35-35" microservice_bench-recommendation-1
sudo docker update --cpuset-cpus="36-36" microservice_bench-profile-1
sudo docker update --cpuset-cpus="37-37" microservice_bench-reservation-1

echo "All containers have been pinned to CPU cores" 