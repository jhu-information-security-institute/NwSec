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

## MacOS guidance (Solution provided by Spring 2025 student, Da Li)
On MacOS, it is likely you will encounter issues such as:
*  Difficulties installing the required Python2 dependencies (gevent, cryptography) for OpenSSH-Network-Parser on an ARM (aarch64) system. The installation fails due to compatibility issues, specifically:
* Compilation errors related to OpenSSL (functions like ERR_GET_FUNC and FIPS_mode have been deprecated or removed in OpenSSL 3.x).
* Inability to build wheel files for gevent and cryptography using Python 2, as required by the provided instructions.

Here are some specific problems with solutions:
```
“error: command 'aarch64-linux-gnu-gcc' failed with exit status 1” when run python setup.py build
```
Cause: libnids.a can’t be auto generated when compile due to the original version of config.guess and config.sub doesn’t recognize aarch64
Solution: 
* copy /usr/share/misc/config.guess and /usr/share/misc/config.sub to replace the file under the libnids-1.25 directory
* Run ./configure CFLAGS="-fPIC" --disable-libglib --disable-libnet under libnids-1.25 directory
* Run make
Now we can see libnids.a under libnids-1.25/src
```
pip2 install . failed
```
Cause: Under Big Sur, the wheel can’t be autodetected
Solution:
* First we need to manually construct the wheel
```
curl -O https://files.pythonhosted.org/packages/source/c/cryptography/cryptography-3.3.2.tar.gz
tar -xzvf cryptography-3.3.2.tar.gz
cd cryptography-3.3.2
python setup.py bdist_wheel --plat-name=manylinux2014_aarch64
```
* Then we can see cryptography-3.3.2-cp27-cp27mu-manylinux2014_aarch64.whl under dist/
* Now, we still face incompatibility due to the cryptography compiled with a new version of OpenSSL, so we also need to install old version OpenSSL
```
curl -O https://www.openssl.org/source/openssl-1.1.1l.tar.gz
tar -xzvf openssl-1.1.1l.tar.gz
cd openssl-1.1.1l ./config --prefix=/usr/local/ssl1.1 --openssldir=/usr/local/ssl1.1 
make 
make install
export CFLAGS="-I/usr/local/ssl1.1/include -fPIC" 
export LDFLAGS="-L/usr/local/ssl1.1/lib"
```
* Under /cryptography 3.3.2
```
python setup.py clean --all
python setup.py bdist_wheel --plat-name=manylinux2014_aarch64
pip install dist/cryptography-3.3.2-cp27-cp27mu-manylinux2014_aarch64.whl
```
* Now we can run pip2 install . with no error

## Useful links
* https://research.nccgroup.com/2020/11/11/decrypting-openssh-sessions-for-fun-and-profit
* https://github.com/jhu-information-security-institute/netsec-OpenSSH-Session-Key-Recovery
* https://github.com/jhu-information-security-institute/netsec-OpenSSH-Network-Parser
* https://github.com/jhu-information-security-institute/netsec-pynids
