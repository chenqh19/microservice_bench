FROM hotel-reservation-base:latest

# Copy service-specific source code
COPY services/search_service .
# Copy shared directories needed for build
COPY protos ./protos
COPY utils ./utils
COPY config.h ./

# Build the service
RUN mkdir build && cd build && \
    /app/ser1de/protobuf/protoc -I ../protos --cpp_out=. ../protos/hotel_reservation.proto && \
    cmake .. && \
    make -j 8

CMD ["./build/search_service"] 