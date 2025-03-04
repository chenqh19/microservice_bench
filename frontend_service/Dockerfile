FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    libprotobuf-dev \
    protobuf-compiler \
    libjsoncpp-dev \
    git \
    libssl-dev

WORKDIR /app
COPY frontend_service .
COPY protos/*.proto /app/protos/
COPY *.h /app/

RUN mkdir build && cd build && \
    protoc -I ../protos --cpp_out=. ../protos/hotel_reservation.proto && \
    cmake .. && \
    make

CMD ["./build/frontend_service"] 