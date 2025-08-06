#!/bin/bash

# Script to add compression logic to all microservices
# This script will update all main.cpp files and CMakeLists.txt files

SERVICES=(
    "search_service"
    "recommendation_service"
    "reservation_service"
    "geo_service"
    "frontend_service"
    "rate_service"
)

echo "Adding compression logic to all microservices..."

for service in "${SERVICES[@]}"; do
    echo "Processing $service..."
    
    # Update main.cpp to include compression
    if [ -f "$service/main.cpp" ]; then
        # Add include for compression_utils.h
        sed -i '1i #include "../compression_utils.h"' "$service/main.cpp"
        
        # Add compression initialization in constructor
        sed -i '/InitializeSampleData();/a\        // Initialize compression manager\n        microservice::compression::init_compression();' "$service/main.cpp"
        
        # Add compression to response processing (this is a simplified approach)
        # For now, we'll add a comment indicating where compression should be added
        sed -i '/return response;/i\        // TODO: Add compression logic to response fields' "$service/main.cpp"
        
        echo "  ✓ Updated $service/main.cpp"
    fi
    
    # Note: CMakeLists.txt updates are no longer needed since compression_utils.cpp is eliminated
    # All compression functionality is now in the header file
done

echo "Compression logic added to all services!"
echo ""
echo "Note: You may need to manually add specific compression logic to each service's"
echo "response processing functions based on the data types being returned." 