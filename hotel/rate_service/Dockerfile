FROM hotel-reservation-base:latest

# Copy service-specific source code
COPY rate_service .

# Build the service
RUN mkdir build && cd build && \
    /app/ser1de/protobuf/protoc -I ../protos --cpp_out=. ../protos/hotel_reservation.proto && \
    cmake .. && \
    make -j 8

CMD ["./build/rate_service"] 