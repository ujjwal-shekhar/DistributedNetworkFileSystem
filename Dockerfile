FROM ubuntu:24.04 AS builder

RUN apt-get update && apt-get install -y \
    build-essential \
    ninja-build \
    curl \
    git \
    software-properties-common \
    gpg \
    lsb-release

# Install latest CMake from Kitware for C++26 support
RUN curl -sSL https://apt.kitware.com/keys/kitware-archive-latest.asc | gpg --dearmor -o /usr/share/keyrings/kitware-archive-keyring.gpg && \
    echo "deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] https://apt.kitware.com/ubuntu/ $(lsb_release -cs) main" | tee /etc/apt/sources.list.d/kitware.list > /dev/null && \
    apt-get update && \
    apt-get install -y cmake

# Install g++-16
RUN add-apt-repository ppa:ubuntu-toolchain-r/test && \
    apt-get update && \
    apt-get install -y g++-16

WORKDIR /app
COPY . .

RUN /usr/bin/cmake -G Ninja -B build -DCMAKE_CXX_COMPILER=g++-16 -DCMAKE_CXX_SCAN_FOR_MODULES=ON .
RUN /usr/bin/cmake --build build --target nm ss clt

FROM ubuntu:24.04
RUN apt-get update && apt-get install -y software-properties-common && \
    add-apt-repository ppa:ubuntu-toolchain-r/test && \
    apt-get update && \
    apt-get install -y libstdc++6
WORKDIR /app
COPY --from=builder /app/build/nm .
COPY --from=builder /app/build/ss .
COPY --from=builder /app/build/clt .

CMD ["./nm", "3", "3"]
