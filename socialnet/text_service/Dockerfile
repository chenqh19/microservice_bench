FROM hotel-reservation-base:latest

COPY text_service .

RUN mkdir build && cd build && \
    /app/ser1de/protobuf/protoc -I ../protos --cpp_out=. ../protos/social_network.proto && \
    cmake .. && \
    make -j 8

CMD ["./build/text_service"]


