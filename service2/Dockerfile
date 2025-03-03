FROM ubuntu:latest

# Install build essentials and dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    autoconf \
    libtool \
    curl \
    unzip \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy the entire project including the protobuf submodule
COPY . .

# Build and install Abseil first
RUN git clone https://github.com/abseil/abseil-cpp.git && \
    cd abseil-cpp && \
    git checkout lts_2023_08_02 && \
    mkdir build && cd build && \
    cmake -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
          -DCMAKE_BUILD_TYPE=Release .. && \
    make -j4 && \
    make install && \
    cd ../.. && \
    rm -rf abseil-cpp

# Install gRPC
RUN git clone --recurse-submodules -b v1.60.0 --depth 1 --shallow-submodules https://github.com/grpc/grpc && \
    cd grpc && \
    mkdir -p cmake/build && \
    cd cmake/build && \
    cmake -DgRPC_INSTALL=ON \
          -DgRPC_BUILD_TESTS=OFF \
          -DgRPC_ABSL_PROVIDER=package \
          -DCMAKE_INSTALL_PREFIX=/usr/local \
          ../.. && \
    make -j4 && \
    make install && \
    cd ../../.. && \
    rm -rf grpc

# Build and install local protobuf
RUN cd protobuf && \
    git submodule update --init --recursive && \
    cmake -B build -DCMAKE_BUILD_TYPE=Release \
                   -Dprotobuf_BUILD_TESTS=OFF \
                   -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
                   -Dprotobuf_ABSL_PROVIDER=package && \
    cmake --build build --parallel 4 && \
    cmake --install build && \
    cd .. && \
    rm -rf protobuf/build

# Build the service
RUN cd service2 && \
    mkdir build && \
    cd build && \
    cmake .. && \
    make

# Run the service
CMD ["./service2/build/service2"] 

