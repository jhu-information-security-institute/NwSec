# Build instructions
1. Build the container by running (from this directory): `$ sudo docker build --build-arg CONTMNTSCRIPTS=/mnt/hgfs/scripts --build-arg CONTMNTSHARED=/mnt/hgfs/shared --build-arg CONTMNTSANDBOX=/mnt/hgfs/sandbox -t t_openwrtapu4buildenv .`
1. To identify $DISPLAY and its xauth token, run the following from the host: `$ echo $DISPLAY && xauth list $DISPLAY`
1. Start the container by running: `$ docker run -d --name openwrtapu4buildenv -v /mnt/hgfs/scripts:/mnt/hgfs/scripts -v /mnt/hgfs/shared:/mnt/hgfs/shared -v /mnt/hgfs/sandbox:/mnt/hgfs/sandbox -t t_openwrtapu4buildenv:latest`
1. Attach to the container by running: `$ sudo docker exec -it openwrtapu4buildenv bash`
1. Create a user in the container (e.g., for git commits) via: `/mnt/hgfs/scripts/create-user.sh -u user -p password`
1. Log in as user via: `su user`
1. From the running container, use these display and xauthtoken values and call: `$ export DISPLAY=display && ./mnt/hgfs/scripts/
display-runtime.sh -x xauthtoken`
1. git clone `$ https://github.com/openwrt/openwrt.git`
1. cd into openwrt
1. `$ make menuconfig`
    * Target system->x86, Subtarget->x86_64 (x86-64, x4 AMD GX-412TC SOC)
    * Base system->
        * deselect dnsmasq
        * deselect logd
    * Network->
        * SSH->openssh-server
        * VPN->openvpn-openssl
        * Web Servers/proxies->nginx
        * Routing and Redirection->ip-full
        * Ip addresses and Names->isc-dhcp-client-ipv6
        * SSH-openssh-client, openssh-keygen, openssh-server
        * WirelessAPD-wpa-cli, wpa-supplicant
        * iw-full, iputils-ping, iputils-ping6
    	* deselect ppp
    * Administration->
        * monit, syslog-ng
    * Kernel Modules->
        * Wireless Drivers->kmod-ath, kmod-ath10 (Qualcomm Atheros QCA9882 802.11ac Wireless Network Adapter)
        * Network Devices->
    	    * kmod-igb, kmod-igbvf?? (x4 Intel Corporation I211AT Gigabit Network Connection), kmod-macvlan, kmod-mii
    		* deselect kmod-e1000, kmod-e1000e
        * Filesystems-kmod-fs-exfat, kmod-fs-ext4, kmod-fuse, kmod-fs-msdos
        * Input modules->kmod-hid, kmod-hid-generic
        * LED modules->kmod-leds-gpio
        * Network support->kmod-vxlan, kmod-tun
        * Sound support->kmod-sound-core, kmod-pcspkr
        * USB support->kmod-usb-core, kmod-usb-ohci, kmod-usb2, kmod-usb3, kmod-usb-storage, kmod-usb-storage-extras
        * Other modules->kmod-sp5100_tco, kmod-gpio-nct5104d,  kmod-gpio-button-hotplug, kmod-pcengines-apuv2
        * Cryptographic API modules->kmod-crypto-hw-ccp
    * Firmware->
	    * ath10k-firmware-qca988x
        * amd64-microcode
    	* deselect bnx2-firmware, r8169-firmware
    * Utilities->
        * flashrom,irqbalance
        * Filesystem->fstrim
    * Luci->
        * Collections->luci-nginx
        * Applications->luci-app-openvpn
1. `$ make -j4 V=s`
    * Note: to clean up after a failed build, run `$ make dirclean && make clean`

# Installation instructions
1. docker copy openwrtapu4buildenv:/home/dummy/openwrt/bin/targets/x86/64/openwrt-x86-64-combined-ext4.img.gz onto a USB drive
1. insert USB drive containing bootable, live debian image in bottom USB port on apu4
    * Note: many usb devices do not work with apu4; Kingston DataTraveler 100G3 devices are confirmed to work with apu4
1. insert USB drive with openwrt-x86-64-combined-ext4.img image on top USB port on apu4
1. boot apu4 with USB drive containing bootable, live debian image
    * press F10 after start to select appropriate boot device
    * press tab and add console=ttyS0,115200n8 to the end of the arguments
    * login is: user, live
1. install the openwrt-x86-64-combined-ext4.img onto the msata disk and the USB disk with the openwrt-x86-64-combined-ext4.img image
	* run `$ lsblk -p` to identify the msata disk (e.g., it might be /dev/sda)
	* mount the USB disk with the openwrt-x86-64-combined-ext4.img image
	    * e.g., `$ sudo mount /dev/sdc1 /mnt`
    * copy the image file into /tmp and cd into /tmp
	    * e.g., `$ cp /mnt/openwrt-x86-64-combined-ext4.img /tmp/. && cd /tmp`
	* image the msata disk device with openwrt-x86-64-combined-ext4.img
        * e.g., `$ sudo dd if=/tmp/openwrt-x86-64-combined-ext4.img of=/dev/sda status=progress; sync;`
    * list the size of the partitions created by the imaging step above
    <pre><code>
    user@debian:~$ sudo parted /dev/sda print
    Model: ATA KINGSTON SUV500M (scsi)
    Disk /dev/sda: 120GB
    Sector size (logical/physical): 512B/4096B
    Partition Table: msdos
    Disk Flags: 

    Number  Start   End     Size    Type     File system  Flags
     1      262kB   17.0MB  16.8MB  primary  ext2         boot
     2      17.3MB  286MB   268MB   primary  ext2
    </code></pre>
	* resize the second partition (system)
	<pre><code>
    user@debian:~$ sudo parted /dev/sda resizepart 2 110GB
    Information: You may need to update /etc/fstab.
    </code></pre>
	* check the ext4 filesystem on the second partition
	<pre><code>
	user@debian:~$ sudo e2fsck -f /dev/sda2                                   
	e2fsck 1.44.5 (15-Dec-2018)
	Pass 1: Checking inodes, blocks, and sizes
	Pass 2: Checking directory structure
	Pass 3: Checking directory connectivity
	Pass 4: Checking reference counts
	Pass 5: Checking group summary information
	/dev/sda2: 1407/16384 files (0.0% non-contiguous), 6089/65536 blocks
	</code></pre>
	* resize the ext4 filesystem on the second partition to match the size of the partition
	<pre><code>
	user@debian:~$ sudo resize2fs /dev/sda2
	resize2fs 1.44.5 (15-Dec-2018)
	Resizing the filesystem on /dev/sda2 to 26851244 (4k) blocks.
	The filesystem on /dev/sda2 is now 26851244 (4k) blocks long.
	</code></pre>
	* check the ext4 filesystem on the second partition
	<pre><code>
	user@debian:~$ sudo parted /dev/sda print
	Model: ATA KINGSTON SUV500M (scsi)
	Disk /dev/sda: 120GB
	Sector size (logical/physical): 512B/4096B
	Partition Table: msdos
	Disk Flags: 

    Number  Start   End     Size    Type     File system  Flags
     1      262kB   17.0MB  16.8MB  primary  ext2         boot
     2      17.3MB  110GB   110GB   primary  ext2
	</code></pre>
1. optional: mount the first logical partition on msata to edit cmdline (/boot/grub/grub.cfg)
    e.g., $ sudo mount /dev/sda1 /mnt
1. powerdown and remove the USB drives `$ sudo poweroff`

# Configuration
1. boot into openwrt from msata
    * Note, the ath10k_pci driver only outputs failure details (not success details); thus, even with the following errors the radio is still functioning fine (e.g., see [here](https://forum.openwrt.org/t/showing-ath10k-firmware-load-successes/35550))
	<pre><code>
	[    8.499662] ath10k_pci 0000:05:00.0: pci irq msi oper_irq_mode 2 irq_mode 0 reset_mode 0
	[    8.736076] ath10k_pci 0000:05:00.0: Direct firmware load for ath10k/pre-cal-pci-0000:05:00.0.bin failed with error -2
	[    8.746832] ath10k_pci 0000:05:00.0: Falling back to user helper
	[    8.761221] firmware ath10k!pre-cal-pci-0000:05:00.0.bin: firmware_loading_store: map pages failed
	[    8.770475] ath10k_pci 0000:05:00.0: Direct firmware load for ath10k/cal-pci-0000:05:00.0.bin failed with error -2
	[    8.780851] ath10k_pci 0000:05:00.0: Falling back to user helper
	[    8.793023] firmware ath10k!cal-pci-0000:05:00.0.bin: firmware_loading_store: map pages failed
	[    8.802374] ath10k_pci 0000:05:00.0: Direct firmware load for ath10k/QCA988X/hw2.0/firmware-6.bin failed with error -2
	[    8.813132] ath10k_pci 0000:05:00.0: Falling back to user helper
	[    8.825597] firmware ath10k!QCA988X!hw2.0!firmware-6.bin: firmware_loading_store: map pages failed
	[    8.836192] ath10k_pci 0000:05:00.0: qca988x hw2.0 target 0x4100016c chip_id 0x043222ff sub 0000:0000
	[    8.845507] ath10k_pci 0000:05:00.0: kconfig debug 0 debugfs 1 tracing 0 dfs 1 testmode 1
	[    8.856664] ath10k_pci 0000:05:00.0: firmware ver 10.2.4-1.0-00047 api 5 features no-p2p,raw-mode,mfp,allows-mesh-bcast crc32 35bd9258
	[    8.901660] ath10k_pci 0000:05:00.0: Direct firmware load for ath10k/QCA988X/hw2.0/board-2.bin failed with error -2
	[    8.912124] ath10k_pci 0000:05:00.0: Falling back to user helper
	[    8.926593] firmware ath10k!QCA988X!hw2.0!board-2.bin: firmware_loading_store: map pages failed
	[    8.936041] ath10k_pci 0000:05:00.0: board_file api 1 bmi_id N/A crc32 bebc7c08
	[   10.079169] ath10k_pci 0000:05:00.0: htt-ver 2.1 wmi-op 5 htt-op 2 cal otp max-sta 128 raw 0 hwcrypto 1
	[   10.201092] ath: EEPROM regdomain: 0x0
	[   10.201099] ath: EEPROM indicates default country code should be used
	[   10.201101] ath: doing EEPROM country->regdmn map search
	[   10.201105] ath: country maps to regdmn code: 0x3a
	[   10.201108] ath: Country alpha2 being used: US
	[   10.201110] ath: Regpair used: 0x3a
	</code></pre>
1. `# opkg update`
1. `# opkg install openssh-server openvpn-openssl hostapd`
1. `# opkg install luci luci-app-openvpn`
1. `# opkg install iwinfo`
1. `# opkg install pciutils usbutils lsblk lrzsz unzip`
1. set the administrator password`
1. reboot the router
1. setup the router manually or load a previous configuration

# Networking setup
1. ssh root@192.168.1.1



# Useful websites
* Embedded router board [PC Engines apu4](https://pcengines.ch/apu4d4.htm)
* 802.11ac miniPCI express radio [COMPEX wle600vx](https://www.pcengines.ch/wle600vx.htm)
* https://openwrt.org/toh/pcengines/apu2
* https://openwrt.org/docs/guide-developer/toolchain/use-buildsystem

# Notes
* Create a diffconfig file based on the current .config and its differences from defconfig
    ```
    ./scripts/diffconfig.sh > diffconfig
    ```
* Use the diffconfig in a built
    ```
    cp diffconfig .config
    make defconfig
    ```
