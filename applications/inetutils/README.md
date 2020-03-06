# initial research
The steps here setup the target (Ubuntu Server 18.04 running on a RPI4B), identified the telnetd package to build and installed its binaries using the package manager, and explored available documentation.   
1. install clean image of ubuntu server 18.04 on sd-card
1. boot ubuntu server and login over console (login: ubuntu, ubuntu)
1. `$ sudo apt-get update`
1. identify the kernel and gcc versions
    <pre><code>
    root@ubuntu:/# uname -a
    Linux ubuntu 5.3.0-1017-raspi2 #19~18.04.1-Ubuntu SMP Fri Jan 17 11:14:07 UTC 2020 aarch64 aarch64 aarch64 GNU/Linux
    root@ubuntu:/# gcc -v
    Using built-in specs.
    COLLECT_GCC=gcc
    COLLECT_LTO_WRAPPER=/usr/lib/gcc/aarch64-linux-gnu/7/lto-wrapper
    Target: aarch64-linux-gnu
    Configured with: ../src/configure -v --with-pkgversion='Ubuntu/Linaro 7.4.0-1ubuntu1~18.04.1' --with-bugurl=file:///usr/share/doc/gcc-7/README.Bugs --enable-languages=c,ada,c++,go,d,fortran,objc,obj-c++ --prefix=/usr --with-gcc-major-version-only --program-suffix=-7 --program-prefix=aarch64-linux-gnu- --enable-shared --enable-linker-build-id --libexecdir=/usr/lib --without-included-gettext --enable-threads=posix --libdir=/usr/lib --enable-nls --with-sysroot=/ --enable-clocale=gnu --enable-libstdcxx-debug --enable-libstdcxx-time=yes --with-default-libstdcxx-abi=new --enable-gnu-unique-object --disable-libquadmath --disable-libquadmath-support --enable-plugin --enable-default-pie --with-system-zlib --enable-multiarch --enable-fix-cortex-a53-843419 --disable-werror --enable-checking=release --build=aarch64-linux-gnu --host=aarch64-linux-gnu --target=aarch64-linux-gnu
    Thread model: posix
    gcc version 7.4.0 (Ubuntu/Linaro 7.4.0-1ubuntu1~18.04.1)
</code></pre>
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
1. `$ tar -cvf inetutils-1.9.4.tar inetutils-1.9.4`

## install dependencies
1. `$ sudo apt-get install build-essential vim autotools-dev automake autoconf help2man`

## build inetutils
1. `$ ./configure`
1. `$ make`

# setup cross-build environment (VM running Kali OS)
1. Download [gcc-linaro-7.4.1-2019.02-x86_64_aarch64-linux-gnu](https://releases.linaro.org/components/toolchain/binaries/7.4-2019.02/aarch64-linux-gnu) and install in `/opt/gnu/gcc-linaro-7.4.1-2019.02-x86_64_aarch64-linux-gnu` (note: this version is closest to the gcc binary installed on the RPI4B, Ubuntu/Linaro 7.4.0-1ubuntu1~18.04.1)
1. from top-level git folder, `$ git apply applications/inetutils/inetutils-1.9.4-PATH_PROCNET_DEV.patch`
1. then, from same folder, `$ git ls-files -z applications/inetutils/inetutils-1.9.4 | xargs -0 tar -cvf inetutils-1.9.4.tar`
1. cd into applications/inetutils/KaliVM
1. build the container `$ sudo docker build -t tkalibuildenv .`
1. run the container `$ sudo docker run --name kalibuildenv -v /opt/gnu:/opt/gnu -it tkalibuildenv:latest`
1. from the host, copy inetutils-1.9.4.tar into it using `$ sudo docker cp inetutils-1.9.4.tar kalibuildenv:/home/dummy/.`
1. then, within the running container, switch to dummy user `$ su dummy`
1. next, extract the tar file (from `/home/dummy`) and cd into the `applications/inetutils/inetutils-1.9.4` directory
1. then, build it by running `$ /home/dummy/cross-compile.sh`

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
* start the daemon: `$ sudo systemctl start inetutils-inetd`
* stop the daemon: `$ sudo systemctl stop inetutils-inetd`
* restart the daemon: `$ sudo systemctl restart inetutils-inetd`
* query daemon status: `$ sudo systemctl status inetutils-inetd`
* to confirm that telnetd is listening on port 23, use `$ netcat -a`
