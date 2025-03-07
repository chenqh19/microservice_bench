#!/bin/bash

# Define the CPU range we want to use (cores 96-99)
CPU_RANGE="96-99"

# Get all running container IDs
containers=$(docker ps -q)

# Counter to distribute containers across cores
current_core=96

# Loop through each container
for container_id in $containers; do
    # Get the main process ID (PID) of the container
    pid=$(docker inspect -f '{{.State.Pid}}' "$container_id")
    
    # Pin the container's process to the current core
    taskset -pc $current_core $pid
    
    # Get container name for logging
    container_name=$(docker inspect -f '{{.Name}}' "$container_id" | sed 's/\///')
    
    echo "Container $container_name (ID: $container_id) pinned to CPU core $current_core"
    
    # Move to next core, loop back to 96 if we exceed 99
    current_core=$((current_core + 1))
    if [ $current_core -gt 99 ]; then
        current_core=96
    fi
done

echo "All containers have been pinned to CPU cores $CPU_RANGE" 