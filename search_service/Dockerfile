FROM hotel-reservation-base:latest

# Copy service-specific source code
COPY search_service .

# Build the service
RUN mkdir build && cd build && \
    protoc -I ../protos --cpp_out=. ../protos/hotel_reservation.proto && \
    cmake .. && \
    make

CMD ["./build/search_service"] 