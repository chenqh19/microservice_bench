FROM hotel-reservation-base:latest

COPY social_graph_service .

RUN mkdir build && cd build && \
    /app/ser1de/protobuf/protoc -I ../protos --cpp_out=. ../protos/social_network.proto && \
    cmake .. && \
    make -j 8

CMD ["./build/social_graph_service"]
