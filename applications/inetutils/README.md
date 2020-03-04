# initial research
The steps here setup the target (Ubuntu Server 18.04 running on a RPI4B), identified the telnetd package to build and installed its binaries using the package manager, and explored available documentation.   
1. install clean image of ubuntu server 18.04 on sd-card
1. boot ubuntu server and login over console (login: ubuntu, ubuntu)
1. `$ sudo apt-get update`
1. setup network, ssh access (`$ sudo apt-get install openssh-server && sudo systemctl start sshd`), and login via ssh
1. search for telnetd with: `$ apt-cache search telnetd`
1. install telnetd with: `$ sudo apt-get install inetutils-inetd inetutils-telnetd` (installs it into /usr/sbin/telnetd)
1. documentation available [here](https://www.gnu.org/software/inetutils/manual/html_node/) and with: `$ man telnetd`

# overview of steps
The steps here outline the sequence needed for obtaining, building, and running telnetd on the target within Docker.  Also included are the additional steps that setup and use a telnet client to interact with it for capturing related network transactions between them.
1. install clean image of ubuntu server 18.04 on sd-card
1. boot ubuntu server and login over console (login: ubuntu, ubuntu)
1. `$ sudo apt-get update`
1. setup network, ssh access (`$ sudo apt-get install openssh-server && sudo systemctl start sshd`), and login via ssh
1. install docker (using install-docker.sh)
1. create a docker container (using Dockerfile) for the build environment and source gathering (native)
1. run the container and execute sequence to build telnetd
1. create a docker container for the runtime environment and telnetd installation/setup
1. run the container and execute sequence to start the server
1. interact with the running server from an external telnet client and capture the network transaction using wireshark that is running elsewhere (e.g., another VM, another RPI, etc.)

# setup native build environment (RPI4B running Ubuntu OS)
## obtain source
1. `$ sudo apt-get install dpkg-dev`
1. edit `/etc/apt/sources.list` and uncomment all the deb-src package instances (note: there is one that needs to remain commented as it causes an error)
1. `$ sudo apt-get update`
1. `$ apt-get source inetutils`

## install dependencies
1. `$ sudo apt-get install build-essential vim autotools-dev autoconf help2man`

## build inetutils
1. `$ ./configure`
1. `$ make`

# setup runtime environment
1. `$ sudo apt-get install inetutils-inetd`
1. `$ sudo make install` (inetutils installs telnetd into /usr/local/libexec)
1. add the following line to /etc/inetd.conf (contains inetd configuration details that include the daemons)
<pre><code>
#:BSD: Shell, login, exec and talk are BSD protocols.
telnet                  stream  tcp     nowait  root    /usr/local/libexec/telnetd      telnetd
</code></pre>
1. edit /etc/pam.d/login and comment out (prefix with #) the following line to allow root user login
<pre><code>
auth [success=ok new_authtok_reqd=ok ignore=ignore user_unknown=bad default=die] pam_securetty.so
</code></pre>
1. `$ sudo systemctl restart inetutils-inetd`

## inetd controls and queries telnetd status
* sudo systemctl start inetutils-inetd
* sudo systemctl status inetutils-inetd
* sudo systemctl stop inetutils-inetd
* sudo systemctl restart inetutils-inetd



