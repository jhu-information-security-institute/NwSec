# Application overview
* Denial of service attacks on Nginx using hping3
* Username guessing attacks (via RCPT TO) on Postfix using ismtp
* Email spoofing attacks on Postfix using SEToolkit
* Brute-force dictionary attacks on Dovecot using hydra
* Firefox web client
* Thunderbird email client

# Runtime environment setup
1. Download files to build container
    ```
    $ wget https://raw.githubusercontent.com/jhu-information-security-institute/NwSec/master/applications/attack/attack-KaliX86-64.sh
    $ chmod +x attack-KaliX86-64.sh
    $ ./attack-KaliX86-64.sh
    ```
1. Build, run, attach to container
    ```
    $ docker build -t tattack .
    $ docker run -d --name attack --hostname attack.netsec-docker.isi.jhu.edu --add-host attack:127.0.1.1 --dns 192.168.25.10 --dns-search netsec-docker.isi.jhu.edu --privileged -e DISPLAY=$DISPLAY --security-opt seccomp=unconfined --cgroup-parent=docker.slice --cgroupns private --tmpfs /tmp --tmpfs /run --tmpfs /run/lock -v /tmp/.X11-unix:/tmp/.X11-unix:rw -v /etc/group:/etc/group:ro -v /etc/passwd:/etc/passwd:ro -v /etc/shadow:/etc/shadow:ro -v /home/$USER/.Xauthority:/home/$USER/.Xauthority:rw --network host tattack:latest
    $ docker exec -it attack bash 
    ```
1. Setup xauth with client container
    1. Generate a MIT_MAGIC_COOKIE-1 by running on VM: `$ mcookie`
    1. Get <COOKIEHASH> hash by running on VM: `$ xauth list`
    1. Share cookie with container’s X11 server by running (on container): `$ xauth add attack/unix$DISPLAY . <COOKIEHASH>`
1. Create a user folder in the container by running: `# /root/setup_user.sh -u <USER> -H <HOME> `
# Notes

# Useful websites
