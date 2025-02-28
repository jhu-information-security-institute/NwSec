
1. Launch a new devel container on Kali and setup to capture the ssh session keys
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
1. Launch another connection into the devel container: `$ docker exec -it devel /bin/bash`
1. Startup wireshark in attack VM to capture the following ssh session (use filter tcp.port eq 2222).
1. SSH into the termsvr (dns isn't setup in the devel): `# ssh -p 2222 -c aes256-gcm@openssh.com student@192.168.25.120`
    * Note that we specify aes256-gcm, which is supported by the network-parser tool
1. Once you see the keys appear in the first console running keys.py, you can exit out of the keys.py command and also the ssh session.  For ther former, you'll want the results that were output to the screen and save the pcap for the latter.
1. Install and build OpenSSH-Network-Parser, that will allow decrypting the ssh session using the keys, and its pynids dependency.
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
1. Copy the previously saved *.pcap into your devel docker container.  Then, use the network-parser utility to decrypt the ssh session that was previously saved in the *.pcap mentioned above by providing the pcap file and captured keys to the following command: `(pynids) # network-parser -p <PCAPFILE> -o my/output/dir --proto=[protocol] [--popt key=value] [-s] [-vvvv]`
