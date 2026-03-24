** In-progress updates for Ubuntu 24.04 **

# Application overview
## Experiment
This experiment uses our email server that is controlled by a Thunderbird client via either POP3 or IMAP.   Using the Thunderbird client, we interface with our server and send test messages to the external server, https://temp-mail.org, while we are intercepting packets locally (IMAP, POP3, SMTP).
<img width="606" height="221" alt="email" src="https://github.com/user-attachments/assets/5ff4de0c-33fe-4e86-bd68-3fda70707445" />

## Individual server architecture
<img width="783" height="512" alt="email-detailed" src="https://github.com/user-attachments/assets/89d91cc4-746e-404c-98d5-f3456a6d8348" />

Postfix (SMTP) and Dovecot (IMAP/POP3) servers setup to run in a single Docker container.  Postfixadmin suite is also installed to facilitate easy configuration of the servers.

Postfix is Wietse Venema's mail server that started life at IBM research as an alternative to the widely-used Sendmail program. Postfix attempts to be fast, easy to administer, and secure. The outside has a definite Sendmail-ish flavor, but the inside is completely different.

Dovecot is an open source IMAP and POP3 email server for Linux/UNIX-like systems, written with security primarily in mind. Dovecot is an excellent choice for both small and large installations. It’s fast, simple to set up, requires no special administration and it uses very little memory.

Postfixadmin is an open source, web based interface for managing domains/mailboxes/aliases etc on a Postfix based mail server.  It integrates with Postfix, Dovecot IMAP/POP3 server, MariaDB database back end.

## Server (on UbuntuServerx86-64-target VM)
The VM is using Ubuntu Server OS, version 24.04.  It is designed to run within a Docker container in our [UbuntuServerx86-64-target VM](https://github.com/jhu-information-security-institute/NwSec/blob/main/config/UbuntuServerX86-64/targetVm-README.md).

## Client
The client that communicates with the email server is any remote SMTP server or IMAP/POP3 client.  One can also directly interface by manually applying direct SMTP commands using telnet over the port for SMTP (usually 25, or 5025 for our container below).  E.g., see the command/response sequence below for port 25 (note: commands have no indentation and  responses are indented).
<pre><code>
└─$ telnet email.netsec-docker.isi.jhu.edu 25
Trying 192.168.25.122...
Connected to email.netsec-docker.isi.jhu.edu.
Escape character is '^]'.
220 localhost ESMTP Postfix (Ubuntu)
ehlo fakeemailserver.com
250-localhost
250-PIPELINING
250-SIZE 10240000
250-VRFY
250-ETRN
250-ENHANCEDSTATUSCODES
250-8BITMIME
250-DSN
250 CHUNKING
mail from: &lt;fakeenvelopesender@fakedomain.com&gt;
250 2.1.0 Ok
rcpt to: &lt;student@netsec-docker.isi.jhu.edu&gt;
250 2.1.5 Ok
data
354 End data with <CR><LF>.<CR><LF>
From: "Fake sender" &lt;fakeheadersender@fakedomain.com&gt;
To: "Student" &lt;student@netsec-docker.isi.jhu.edu&gt;
Subject: Free bitcoins, click here!

Hello, this is my malicious message!  I see you like bitcoins.

.
250 2.0.0 Ok: queued as 7FF76A1AB3
quit
221 2.0.0 Bye
Connection closed by foreign host.
</code></pre>
<img width="1475" height="619" alt="image" src="https://github.com/user-attachments/assets/1f7bc4ee-1515-4dbc-af78-e924e22f469a" />

# Runtime environment setup
Port summary: http (80), imap/s (143/993), pop3/s (110/995), smtp/s (25/587)
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
    $ docker run -d --name emailsvr --hostname email.netsec-docker.isi.jhu.edu --add-host email.netsec-docker.isi.jhu.edu:127.0.1.1 --dns 192.168.25.10 --dns 8.8.8.8 --dns-search netsec-docker.isi.jhu.edu --privileged --security-opt seccomp=unconfined --cgroup-parent=docker.slice --cgroupns private --tmpfs /tmp --tmpfs /run --tmpfs /run/lock --network bridge -p 5080:80 -p 5025:25 -p 5587:587 -p 5143:143 -p 5993:993 -p 5110:110 -p 5995:995 --cpus=1 temailsvr:latest
    ```
## Container setup
1. Log in to the container with a shell: `$ docker exec -it <NAME> bash`
1. Update /etc/nginx/conf.d/postfixadmin.conf and update the hostname appropriately   
1. Configure using postfixadmin by running the following command and using the guidance below `# dpkg-reconfigure postfixadmin`
    ```
        ...
        Configuring postfixadmin
        ------------------------
        ...
        Reinstall database for postfixadmin? [yes/no] yes
        
        By default, postfixadmin will be configured to use a MySQL server through a local Unix socket (this provides the best performance). To connect with a different
        method, or to a different server entirely, select the appropriate option from the choices here.
        
          1. Unix socket  2. TCP/IP
        Connection method for MySQL database of postfixadmin: 1
        
        Database user accounts can be configured to use a variety of plugins for authentication with MySQL. If the server default won't work with this application, it is
        necessary to specify one that will. Please select one from the list of available plugins. Leaving the selection set to its original value should work unless a
        remote server is using unpredictable defaults, but other options may not be supported by postfixadmin. If problems arise, the package's documentation should give
        hints; see /usr/share/doc/postfixadmin/.
        
        Your options are:
         * default - use the default determined by the server.
         * mysql_native_password - no MySQL authentication plugin is used.
         * sha256_password - a more secure password encryption algorithm.
         * caching_sha2_password - SHA2 plus an in-memory authentication cache.
        
          1. default  2. mysql_native_password  3. sha256_password  4. caching_sha2_password
        Authentication plugin for MySQL database: 1
        
        Please provide a name for the MySQL database to be used by postfixadmin.
        
        MySQL database name for postfixadmin: postfixadmin
        
        Please provide a MySQL username for postfixadmin to register with the database server. A MySQL user is not necessarily the same as a system login, especially if
        the database is on a remote server.
        
        This is the user which will own the database, tables, and other objects to be created by this installation. This user will have complete freedom to insert,
        change, or delete data in the database.
        
        If your username contains an @, you need to specify the domain as well (see below).
        
        Advanced usage: if you need to define the domain that the user will log in from, you can write "username@domain".
        
        MySQL username for postfixadmin: postfixadmin@localhost
        
        Please provide a password for postfixadmin to register with the database server. If left blank, a random password will be generated.
        
        MySQL application password for postfixadmin: nwsec123
        
        Password confirmation: nwsec123
        
        Determining localhost credentials from /etc/mysql/debian.cnf: succeeded.
        Please provide the name of the account with which this package should perform administrative actions. This user is the one with the power to create new database
        users.
        
        For MySQL, this is almost always "root". Note that this is not the same as the Unix login "root".
        
        Name of the database's administrative user: root
        ...   
    ```
1. Edit `/etc/postfixadmin/dbconfig.inc.php` and change value of line with `$dbtype` from `mysql` to `mysqli`
1. Edit `/etc/dbconfig-common/postfixadmin.conf` and change value of line with `dbc_dbtype` from `mysql` to `mysqli`
1. Log in to `http://email.netsec-docker.isi.jhu.edu:5080/postfixadmin/setup.php`
    * Specify a setup password and copy the generated hash into the value for the setup_password variable in `/etc/postfixadmin/config.inc.php`
    * Then, login with the setup password to create a superadmin account `postfixadmin@netsec-docker.isi.jhu.edu` and specify its password
1. Use that superadmin account to log in to `http://email.netsec-docker.isi.jhu.edu:5080/postfixadmin/login.php`
    * Create a new `netsec-docker.isi.jhu.edu` domain
        * Domain List -> New Domain 
    * Create new email accounts in the `netsec-docker.isi.jhu.edu` domain
        * Virtual List -> Add Mailbox
        * Note, you will need to create an email account for superadmin user
## Client setup
1. Use Mozilla Thunderbird for a mail client from the VM or another container (recommended to use attack container in the Kali VM) to access email
    * Start Thunderbird, create a new account and enter the following, then click configure manually
   ```
   Your full name: postfixadmin (imap)
   Email address: postfixadmin@email.netsec-docker.isi.jhu.edu
   Password: nwsec123
   ```
   * Enter the following for hostnames and then click Re-test (everything should be autopopulated afterwards)
   ```
   Incoming Server
   Protocol: IMAP
   Hostname: email.netsec-docker.isi.jhu.edu
   Port: 5143
   Outgoing Server
   Hostname: email.netsec-docker.isi.jhu.edu
   Port: 5025
   ```
   * Create another new account and enter the following, then click configure manually
   ```
   Your full name: postfixadmin (pop3)
   Email address: postfixadmin@email.netsec-docker.isi.jhu.edu
   Password: nwsec123
   ```
   * Enter the following for hostnames and then click Re-test (everything should be autopopulated afterwards)
   ```
   Incoming Server
   Protocol: POP3
   Hostname: email.netsec-docker.isi.jhu.edu
   Port: 5110
   Outgoing Server
   Hostname: email.netsec-docker.isi.jhu.edu
   Port: 5025
   ```   
   * Ensure the outgoing server settings are setup for port 5025 (Thunderbird Settings->Outgoing mail server->Edit)
<img width="1122" height="1388" alt="insecure emailsvr imap" src="https://github.com/user-attachments/assets/dfeea2f5-2e2e-48e9-b948-484907990b5c" />
<img width="1171" height="1442" alt="insecure emailsvr pop3" src="https://github.com/user-attachments/assets/7e8bfe0b-0a8c-4b34-a9a4-8e1e7cccdd6a" />
<img width="1208" height="771" alt="insecure emailsvr smtp" src="https://github.com/user-attachments/assets/04991a0d-ebda-486e-b0f2-2d7046aa1de2" />

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
* https://github.com/postfixadmin/postfixadmin
* https://www.linuxbabe.com/mail-server/setup-basic-postfix-mail-sever-ubuntu
* https://www.linuxbabe.com/mail-server/secure-email-server-ubuntu-postfix-dovecot
* https://www.linuxbabe.com/mail-server/postfixadmin-ubuntu


























