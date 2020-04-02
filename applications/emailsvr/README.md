# Application overview
Citadel-Suite is easy, versatile, and powerful, thanks to its exclusive "rooms" based architecture and is a plaform packed with various operations that includes the feature in focus, _email_. Our application sets up the citadel email server on the RPI4B in a docker container and allows local interfacing with the server using a browser or ssh from the same LAN.  Remote access to the server over SMTP is possible, but port 25 must be enabled through any firewalls.

## Server (on RPI4B)
The server on the RPI4B in a docker container. The server is an email server, which we have chose to be Citadel server, and it handles in-bound and out-bound mails.  There is an http server for configuration.

## Client (on VM)
The client that communicates with the email server is any remote SMTP server.  One can also directly interface by manually applying direct SMTP commands using telnet over port 25.  E.g., see the command/response sequence below (note: commands have no indentation and  responses are indented).
<pre><code>
$ # telnet 192.168.50.46 25
Trying 192.168.50.46...
Connected to 192.168.50.46.
Escape character is '^]'.
    220 ubuntu.MHI ESMTP Citadel server ready.
HELO reubenjohnston.com
    250 Hello reubenjohnston.com (kali.MHI [192.168.50.31])
MAIL FROM:<reuben@reubenjohnston.com>
    250 Sender ok
RCPT TO:<admin@ubuntu.mhi>
    250 RCPT ok <admin@ubuntu.mhi>
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
1. Build the Docker container using: `$ sudo docker build -t temailsvr .`
1. Start the Docker container using: `$ sudo docker run -d --name emailsvr --privileged -v /sys/fs/cgroup:/sys/fs/cgroup:ro --network host temailsvr:latest`
1. Log in to the running container using: `$ sudo docker exec -it emailsvr bash`
1. From inside the docker session, setup citadel-server using: `# /usr/lib/citadel-server/setup`
    * example sequence is below
    <pre><code>
    # /usr/lib/citadel-server/setup
    Connecting to Citadel server
   	       *** Citadel setup program ***
    <Citadel administrator username:>

    Please enter the name of the Citadel user account that should 
    be granted administrative privileges once created. If using 
    internal authentication this user account will be created if 
    it does not exist.  For external authentication this user 
    account has to exist.
    This is currently set to:

    Enter new value or press return to leave unchanged:
    admin

    <Administrator password:>

    Enter a password for the system administrator. When setup
    completes it will attempt to create the administrator user
    and set the password specified here.

    This is currently set to:

    Enter new value or press return to leave unchanged:
    citadel

    <Citadel User ID:>

    Citadel needs to run under its own user ID.  This would
    typically be called "citadel", but if you are running Citadel
    as a public site, you might also call it "bbs" or "guest".
    The server will run under this user ID.  Please specify that
    user ID here.  You may specify either a user name or a numeric
    UID.

    This is currently set to:
    root
    Enter new value or press return to leave unchanged:
    citadel

    <Listening address for the Citadel server:>

    Please specify the IP address which the server should be 
    listening to. You can name a specific IPv4 or IPv6 address, or 
    you can specify
    "*" for "any address", "::" for "any IPv6 address", or "0.0.0.0"
    for "any IPv4 address". If you leave this blank, Citadel will
    listen on all addresses. This can usually be left to the default
    unless multiple instances of Citadel are running on the same 
    computer.
    This is currently set to:
    *
    Enter new value or press return to leave unchanged:

    <Server port number:>

    Specify the TCP port number on which your server will run.
    Normally, this will be port 504, which is the official port
    assigned by the IANA for Citadel servers.  You will only need
    to specify a different port number if you run multiple instances
    of Citadel on the same computer and there is something else
    already using port 504.

    This is currently set to:
    504
    Enter new value or press return to leave unchanged:

    <Authentication method to use:>

    Please choose the user authentication mode. By default Citadel
    will use its own internal user accounts database. If you choose
    Host, Citadel users will have accounts on the host system, 
    authenticated via /etc/passwd or a PAM source. LDAP chooses an 
    RFC 2307 compliant directory server, the last option chooses the 
    nonstandard MS Active Directory LDAP scheme.
    Do not change this option unless you are sure it is required, 
    since changing back requires a full reinstall of Citadel.
     0. Self contained authentication
     1. Host system integrated authentication
     2. External LDAP - RFC 2307 POSIX schema
     3. External LDAP - MS Active Directory schema

    For help: http://www.citadel.org/doku.php/faq:installation:authmodes

    ANSWER "0" UNLESS YOU COMPLETELY UNDERSTAND THIS OPTION.

    This is currently set to:
    0
    Enter new value or press return to leave unchanged:

    Reconfiguring Citadel server

    /etc/nsswitch.conf is configured to use the 'db' module for
    one or more services.  This is not necessary on most systems,
    and it is known to crash the Citadel server when delivering
    mail to the Internet.

    Do you want this module to be automatically disabled?

    Yes/No [Yes] --> 
    Restarting Citadel server to apply changes
    </code></pre>
1. Set the time zone using: `# /usr/lib/citadel-server/sendcommand "CONF PUTVAL|c_default_cal_zone|America/New York"`
1. Restart the server using: `# systemctl restart citadel`
1. Check the server status (there should be no errors) using: `# systemctl status citadel`
