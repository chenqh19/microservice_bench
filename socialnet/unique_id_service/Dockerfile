FROM hotel-reservation-base:latest

COPY unique_id_service .

RUN mkdir build && cd build && \
    /app/ser1de/protobuf/protoc -I ../protos --cpp_out=. ../protos/social_network.proto && \
    cmake .. && \
    make -j 8

CMD ["./build/unique_id_service"]
