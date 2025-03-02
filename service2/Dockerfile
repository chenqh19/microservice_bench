FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    libgrpc++-dev \
    libprotobuf-dev \
    protobuf-compiler-grpc

WORKDIR /app
COPY service2 .
COPY protos /app/protos/

RUN mkdir build && cd build && \
    protoc -I ../protos --cpp_out=. --grpc_out=. \
    --plugin=protoc-gen-grpc=`which grpc_cpp_plugin` ../protos/service.proto && \
    cmake .. && \
    make

CMD ["./build/service2"] 