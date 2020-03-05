#!/usr/bin/env bash
#Usage: Run from top-level of inetutils-1.9.4 and do not run as root

#This is probably unnecessary here (as we are using automake), but ...
export CROSS_COMPILE=/opt/gnu/gcc-linaro-7.4.1-2019.02-x86_64_aarch64-linux-gnu/bin/aarch64-linux-gnu- && export ARCH=aarch64

#Because inetutils uses automake, we have to configure with --host=aarch64-linux-gnu --build=x86_64-pc-linux-gnu
#So, add the cross-compiler bin folder to PATH
export PATH=$PATH:/opt/gnu/gcc-linaro-7.4.1-2019.02-x86_64_aarch64-linux-gnu/bin

./configure --host=aarch64-linux-gnu --build=x86_64-pc-linux-gnu
autoreconf -f -i
make -j8
