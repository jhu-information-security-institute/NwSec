
Get xauth and display info in VM:
$ echo $DISPLAY && xauth list

Setup x11 in container:
$ export DISPLAY=:0.0 && /mnt/docker/sandbox/NwSec/os/scripts/display-runtime.sh -x 4930106e8a8a8dc34cc6d6f74b93a105

#https://openwrt.org/docs/guide-developer/toolchain/use-buildsystem

git clone https://github.com/openwrt/openwrt.git
git checkout v24.10.0

#update the feeds
./scripts/feeds update -a
./scripts/feeds install -a

#configure the firmware image
make menuconfig

Target System (MediaTek ARM)
Subtarget (MT7622)
Target Profile (Bananapi BPi-R64)
Enable WPA_ENABLE_WEP: Network->WirelessAPD->Enable support for unsecure and obsolete WEP

#build the firmware image
make -j $(nproc) V=1 download clean world
  

bin/targets/mediatek/mt7622/openwrt-mediatek-mt7622-bananapi_bpi-r64-sdcard.img.gz


docker cp openwrtbpir64buildenv:/home/rjohn177/openwrt/bin/targets/mediatek/mt7622/openwrt-mediatek-mt7622-bananapi_bpi-r64-sdcard.img.gz wep-enabled-openwrt-mediatek-mt7622-bananapi_bpi-r64-sdcard.img.gz

gunzip wep-enabled-openwrt-mediatek-mt7622-bananapi_bpi-r64-sdcard.img.gz

sudo dd if=wep-enabled-openwrt-mediatek-mt7622-bananapi_bpi-r64-sdcard.img of=/dev/sdd status=progress bs=1M && sync

parted /dev/sdd
(parted) print
Warning: Not all of the space available to /dev/sdd appears to be used, you can fix the GPT to use all of the space (an extra 123725791 blocks) or continue with the current
setting? 
Fix/Ignore? Fix                                                           
Model: Generic STORAGE DEVICE (scsi)
Disk /dev/sdd: 63.9GB
Sector size (logical/physical): 512B/512B
Partition Table: gpt
Disk Flags: 

Number  Start   End     Size    File system  Name        Flags
128     17.4kB  524kB   507kB                            bios_grub
 1      524kB   1049kB  524kB                bl2         hidden, legacy_boot
 2      2097kB  4194kB  2097kB               fip         boot, hidden, esp
 3      4194kB  5243kB  1049kB               ubootenv    hidden
 4      6291kB  39.8MB  33.6MB               recovery    hidden
 5      39.8MB  47.2MB  7340kB               install     hidden
 6      47.2MB  517MB   470MB                production

(parted) resizepart 6 100%