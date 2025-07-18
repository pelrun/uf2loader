FROM ubuntu:22.04

# Set frontend to non-interactive
ENV DEBIAN_FRONTEND=noninteractive

# Install base dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    gcc-arm-none-eabi \
    libnewlib-arm-none-eabi \
    libstdc++-arm-none-eabi-newlib \
    clang-tidy \
    python3 \
    python3-pip \
    git \
    wget \
    tar \
    --no-install-recommends && \
    rm -rf /var/lib/apt/lists/*

# Install Python packages
RUN pip3 install robotframework

# Install Renode
RUN wget https://github.com/renode/renode/releases/download/v1.14.0/renode-1.14.0.linux-portable.tar.gz -O /tmp/renode.tar.gz && \
    tar -xzf /tmp/renode.tar.gz -C /opt && \
    rm /tmp/renode.tar.gz

# Set up environment variables
ENV RENODE_DIR=/opt/renode_1.14.0
ENV PATH="/opt/renode_1.14.0:${PATH}"

# Create a non-root user
RUN useradd -ms /bin/bash user
USER user
WORKDIR /home/user

# Copy pico-sdk
COPY --chown=user:user pico-sdk /home/user/pico-sdk
ENV PICO_SDK_PATH=/home/user/pico-sdk

# Set the entrypoint
ENTRYPOINT ["/bin/bash"] 