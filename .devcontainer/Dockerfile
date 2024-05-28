FROM ubuntu:22.04

# Install dependencies
RUN apt-get update && apt-get install -y \
    build-essential libncurses5-dev fakeroot lzop bc git bison rsync flex libssl-dev gcc-arm-linux-gnueabihf clang-format

RUN cd /root && \
    git clone https://github.com/wirenboard/linux.git --single-branch --depth=1 && \
    cd linux && \
    git submodule update --init --recursive 

ENV KDIR /root/linux
ENV ARCH arm
ENV CROSS_COMPILE arm-linux-gnueabihf-
ENV KBUILD_OUTPUT .build-wb6
