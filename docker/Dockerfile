# Use Renode's official Docker image as base
FROM antmicro/renode:latest

# Switch to root to install additional packages
USER root

# Install ARM toolchain and Python dependencies
RUN apt-get update && apt-get install -y \
    # Build tools for bootloader
    build-essential \
    cmake \
    gcc-arm-none-eabi \
    libnewlib-arm-none-eabi \
    libstdc++-arm-none-eabi-newlib \
    # Python for tests
    python3 \
    python3-pip \
    git \
    # Clean up
    && rm -rf /var/lib/apt/lists/*

# Install Python packages for tests
RUN pip3 install robotframework

# Set PICO_SDK_PATH to use the mounted workspace
ENV PICO_SDK_PATH=/workspace/pico-sdk

# Switch back to the user
USER developer
WORKDIR /home/developer

# Set the entrypoint
ENTRYPOINT ["/bin/bash"] 