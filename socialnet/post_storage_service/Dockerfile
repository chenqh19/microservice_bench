FROM hotel-reservation-base:latest

COPY post_storage_service .

RUN mkdir build && cd build && \
    /app/ser1de/protobuf/protoc -I ../protos --cpp_out=. ../protos/social_network.proto && \
    cmake .. && \
    make -j 8

CMD ["./build/post_storage_service"]
