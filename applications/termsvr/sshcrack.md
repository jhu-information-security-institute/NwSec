
Launch a new devel container on Kali and setup to capture the ssh session keys
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

