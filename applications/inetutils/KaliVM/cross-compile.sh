#!/usr/bin/env bash
# Run from top-level of inetutils-1.9.4

export CROSS_COMPILE=/opt/gnu/gcc-linaro-7.4.1-2019.02-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu-
export ARCH=aarch64
autoreconf -f -i
./configure
make -j8
