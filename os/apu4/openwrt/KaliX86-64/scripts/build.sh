#!/usr/bin/env bash
cd /mnt/hgfs/sandbox

git clone https://github.com/openwrt/openwrt.git

cd openwrt

git checkout tags/v21.02.2

#https://openwrt.org/docs/guide-developer/feeds
./scripts/feeds update -a
./scripts/feeds install -a

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
echo 'Create a default based on diffconfig by running $ cp /mnt/hgfs/scripts/diffconfig .config'
echo 'Then expand to a full config by running $ make defconfig'
echo 'Customize config by running $ make menuconfig'
echo 'Customize kernel config by running $ make kernel_menuconfig'
echo 'Build by running $ make -j $nproc) V=sc download clean world'