# Application overview
Postfix (SMTP) and Dovecot (IMAP/POP3) servers setup to run in a single Docker container.  Postfixadmin suite is also installed to facilitate easy configuration of the servers.

What is Postfix? It is Wietse Venema's mail server that started life at IBM research as an alternative to the widely-used Sendmail program. Now at Google, Wietse continues to support Postfix.

Postfix attempts to be fast, easy to administer, and secure. The outside has a definite Sendmail-ish flavor, but the inside is completely different.

Dovecot is an open source IMAP and POP3 email server for Linux/UNIX-like systems, written with security primarily in mind. Dovecot is an excellent choice for both small and large installations. It’s fast, simple to set up, requires no special administration and it uses very little memory.

## Server (on RPI4B)
The RPI4B is using Ubuntu Server OS, version 18.04.

## Server (on UbuntuServerx86-64-target VM)
The VM is using Ubuntu Server OS, version 20.04.  It is designed to run within a Docker container in our [UbuntuServerx86-64-target VM](https://github.com/jhu-information-security-institute/NwSec/blob/main/config/UbuntuServerX86-64/targetVm-README.md).

## Client
The client that communicates with the email server is any remote SMTP server or IMAP/POP3 client.  One can also directly interface by manually applying direct SMTP commands using telnet over port 25.  E.g., see the command/response sequence below (note: commands have no indentation and  responses are indented).
<pre><code>
$ # telnet 192.168.50.46 25
Trying 192.168.50.46...
Connected to 192.168.50.46.
Escape character is '^]'.
    220 ubuntu.MHI ESMTP Postfix (Ubuntu)
HELO reubenjohnston.com
    250 Hello reubenjohnston.com (kali.MHI [192.168.50.31])
MAIL FROM:&ltreuben@reubenjohnston.com&gt
    250 Sender ok
RCPT TO:&ltadmin@ubuntu.mhi&gt
    250 RCPT ok &ltadmin@ubuntu.mhi&gt
DATA
    354 Transmit message now - terminate with '.' by itself
Subject: sample message
From: reuben@reubenjohnston.com
To: admin@ubuntu.mhi

Hey,
Can you send me your password?
Thanks!
.
    250 Message accepted.
QUIT
    221 Goodbye...
Connection closed by foreign host.
</code></pre>

# Runtime environment setup
## Server (on RPI4B)
1. Set your hostname: `$ sudo hostnamectl set-hostname your-fqdn`
1. Disable the firewall on the appropriate ports:
`$ sudo ufw allow 25/tcp`, `$ sudo ufw allow 80,443,587,465,143,993/tcp`, and `$ sudo ufw allow 110,995/tcp`
1. Build the Docker container using: `$ sudo docker build -t temailsvr .`
1. Start the Docker container using: `$ sudo docker run -d --name emailsvr --privileged -v /sys/fs/cgroup:/sys/fs/cgroup:ro --network host temailsvr:latest`
1. Log in to the running container using: `$ sudo docker exec -it emailsvr bash`
1. From inside the docker session, enable servers using: `# systemctl enable postfix && systemctl enable dovecot`

## Server (on VM)
1. Download files to build container
    ```
    $ wget https://raw.githubusercontent.com/jhu-information-security-institute/NwSec/main/applications/emailsvr/emailsvr-UbuntuServerX86-64.sh
    $ chmod +x emailsvr-UbuntuServerX86-64.sh
    $ ./emailsvr-UbuntuServerX86-64.sh
    ```
1. Build, run, attach to container
    ```
    $ docker build -t temailsvr .
    $ docker run -d --name emailsvr --hostname mail --add-host mail.netsec-docker.isi.jhu.edu:127.0.1.1 --dns 192.168.25.10 --dns-search netsec-docker.isi.jhu.edu --privileged -v /sys/fs/cgroup:/sys/fs/cgroup:ro --network host --cpus=1 temailsvr:latest
    $ docker exec -it emailsvr bash 
    ```
1. Configure using postfixadmin by running the following command and using the guidance below `$ dpkg-reconfigure postfixadmin`
    ```
    Reinstall database for postfixadmin? [yes/no] yes
    ...
      1. Unix socket  2. TCP/IP
    Connection method for MySQL database of postfixadmin: 1
    ...
      1. default  2. mysql_native_password  3. sha256_password  4. caching_sha2_password
    Authentication plugin for MySQL database: 1
    ...
    MySQL database name for postfixadmin: postfixadmin
    ...
    MySQL username for postfixadmin: postfixadmin@localhost
    ...
    MySQL application password for postfixadmin: nwsec123
    Password confirmation: nwsec123
    ...
    Name of the database's administrative user: root
    ...
    ```
1. Edit `/etc/postfixadmin/dbconfig.inc.php` and change value of line with `$dbtype` from `mysql` to `mysqli`
1. Edit `/etc/dbconfig-common/postfixadmin.conf` and change value of line with `dbc_dbtype` from `mysql` to `mysqli`
1. Log in to `http://mail.netsec-docker.isi.jhu.edu/postfixadmin/setup.php`
    * Specify a setup password and copy the generated hash into the value for the setup_password variable in `/etc/postfixadmin/config.inc.php`
    * Then, create a superadmin account `postfixadmin@netsec-docker.isi.jhu.edu` and specify its password
1. Use that superadmin account to log in to `http://mail.netsec-docker.isi.jhu.edu/postfixadmin/login.php`
    * Create a new `netsec-docker.isi.jhu.edu` domain 
    * Create new email accounts in the `netsec-docker.isi.jhu.edu` domain
    * Note, you will need to create an email account for superadmin user
1. Use Mozilla Thunderbird for a mail client from the VM or another container to access email

# Notes
* Restart the server using: `$ sudo systemctl restart postfix`
* Restart the server using: `$ sudo systemctl restart dovecot`
* Check the server status (there should be no errors) using: `$ sudo systemctl status postfix`
* Check the server status (there should be no errors) using: `$ sudo systemctl status dovecot`
* View the server log: `$ sudo journalctl -u postfix`
* View the server log: `$ sudo journalctl -u dovecot` and `$ sudo tail -f /var/log/dovecot.log`

# Useful websites
* http://www.postfix.org
* https://www.dovecot.org
* http://postfixadmin.sourceforge.net
* https://www.linuxbabe.com/mail-server/postfixadmin-ubuntu
