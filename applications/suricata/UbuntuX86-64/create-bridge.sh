#!/usr/bin/bash
brctl addbr br0
brctl addif br0 ens38
ip addr add 192.168.25.1/24 dev br0
ip link set dev br0 up
