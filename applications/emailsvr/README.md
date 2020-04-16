# Application overview
postfix (SMTP) and dovecot (IMAP/POP3) servers

## Server (on RPI4B)
The email servers on the RPI4B runs in a docker container.   The RPI4B is using Ubuntu Server OS, version 18.04.

## Client (on VM)
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
1. Set your hostname: udo hostnamectl set-hostname your-fqdn`
1. Disable the firewall on the appropriate ports:
`$ sudo ufw allow 25/tcp`, `$ sudo ufw allow 80,443,587,465,143,993/tcp`, and `$ sudo ufw allow 110,995/tcp`
1. Build the Docker container using: `$ sudo docker build -t temailsvr .`
1. Start the Docker container using: `$ sudo docker run -d --name emailsvr --privileged -v /sys/fs/cgroup:/sys/fs/cgroup:ro --network host temailsvr:latest`
1. Log in to the running container using: `$ sudo docker exec -it emailsvr bash`
1. From inside the docker session, enable servers using: `# systemctl enable postfix && systemctl enable dovecot`

# Useful websites
* http://www.postfix.org
* https://www.dovecot.org
