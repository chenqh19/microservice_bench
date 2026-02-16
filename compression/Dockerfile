FROM hotel-reservation-base:latest

# Build context is the repo root (..). Copy compression, scheduler, utils, config.
COPY compression .
COPY scheduler ./scheduler
COPY utils ./utils
COPY config.h ./

RUN rm -rf build
RUN mkdir build && cd build && \
    cmake .. && \
    make -j 8

CMD ["./build/compress_frontend_service"]


