FROM hotel-reservation-base:latest

# Copy service-specific source code
COPY frontend_service .

# Build the service
RUN rm -rf build
RUN mkdir build && cd build && \
    protoc -I ../protos --cpp_out=. ../protos/hotel_reservation.proto && \
    cmake .. && \
    make

CMD ["./build/frontend_service"] 