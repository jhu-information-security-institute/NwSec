#!/usr/bin/env bash
cd /sandbox

git clone https://github.com/openwrt/openwrt.git

cd openwrt

git checkout tags/v19.07.2

#https://openwrt.org/docs/guide-developer/feeds
./scripts/feeds update -a
./scripts/feeds install -a

make menuconfig

#https://www.pcengines.ch/wle600vx.htm
#Qualcomm Atheros QCA9882

#https://openwrt.org/docs/techref/buildroot
#    V=99 and V=1 are now deprecated in favor of a new verbosity class system,
#    though the old flags are still supported.
#    You can set the V variable on the command line (or OPENWRT_VERBOSE in the
#    environment) to one or more of the following characters:
    
#    - s: stdout+stderr (equal to the old V=99)
#    - c: commands (for build systems that suppress commands by default, e.g. kbuild, cmake)
#    - w: warnings/errors only (equal to the old V=1)
#make -j8 V=sc
