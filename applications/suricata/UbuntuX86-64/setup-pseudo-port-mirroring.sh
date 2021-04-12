#!/usr/bin/bash
ip route del 192.168.25.0/24
ip route add 192.168.25.0/24 via 192.168.25.130
iptables -t mangle -A PREROUTING -d 192.168.25.130 -j TEE --gateway 192.168.25.132
iptables -t mangle -A POSTROUTING -s 192.168.25.130 -j TEE --gateway 192.168.25.132
iptables -A FORWARD -o ens39 -j DROP
iptables -A OUTPUT -o ens39 -j DROP

