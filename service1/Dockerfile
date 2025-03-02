FROM ubuntu:24.04

RUN apt-get update && apt-get install -y \
    build-essential \
    manpages-dev \
    cmake \
    pkg-config \
    libgrpc++-dev 

#RUN wget http://ftp.gnu.org/gnu/libc/glibc-2.38.tar.gz && \
#    tar -xvzf glibc-2.38.tar.gz && cd glibc-2.38 && \
#    mkdir build && cd build && \
#    ../configure --prefix=/usr && make -j$(nproc) && make install

# Copy custom Protobuf installation
COPY protobuf /usr/local/protobuf

# Set environment variables for Protobuf
ENV Protobuf_ROOT=/usr/local/protobuf
ENV PATH="/usr/local/protobuf/bin:${PATH}"
ENV LD_LIBRARY_PATH="/usr/local/protobuf/src:${LD_LIBRARY_PATH}"


WORKDIR /app
COPY service1 .
COPY protos /app/protos/

RUN mkdir build && cd build && \
    /usr/local/protobuf/protoc -I ../protos --cpp_out=. --grpc_out=. \
    --plugin=protoc-gen-grpc=`which grpc_cpp_plugin` ../protos/service.proto && \
    cmake .. && \
    make

CMD ["./build/service1"] 