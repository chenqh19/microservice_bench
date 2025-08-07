FROM hotel-reservation-base:latest

# Copy service-specific source code
COPY services/frontend_service .
# Copy shared directories needed for build
COPY protos ./protos
COPY utils ./utils
COPY config.h ./

# Build the service
RUN rm -rf build
RUN mkdir build && cd build && \
    /app/ser1de/protobuf/protoc -I ../protos --cpp_out=. ../protos/hotel_reservation.proto && \
    cmake .. && \
    make -j 8

CMD ["./build/frontend_service"] 