# Reuben Johnston, reub@jhu.edu
# This utility program performs network activity on a timed interval.  This program runs on one host and it communicates with corresponding servers running on another system.
#
# Python deprecated telnetlib (cool, now we have more work for teaching demonstrations).  Install python 3.12 and python venv.
# For Ubuntu, see instructions below where we use the deadsnakes/ppa apt repository with older python builds.  For others, you likely have to build from source as the deadsnakes/ppa is only for Ubuntu.
# Install software-properties-common to get the add-apt-repository utility.
# # apt-get update
# # apt-get install software-properties-common
# # add-apt-repository ppa:deadsnakes/ppa
# # apt-get install python3.12 python3.12-venv
# # python3.12 -m venv python3p12
# # source python3p12/bin/activate
# Run the script using the venv that is activated.

import ftplib
import telnetlib
import time
import argparse
import getpass
import sys

ftpconn=None
telnetconn=None
detectctrlc=None

cDEFAULT_FTP_PORT=21
cDEFAULT_TELNET_PORT=23

#https://pythonspot.com/ftp-client-in-python/
#https://docs.python.org/3/library/ftplib.html
#https://docs.python.org/3/library/telnetlib.html

def ftp_connect(server,port,username,password):
    global ftpconn,detectctrlc

    try:    
        print("connect to "+server+":"+str(port))
        ftpconn = ftplib.FTP()
        ftpconn.set_pasv(False)
        ftpconn.connect(server, port)
        print("ftp login in to "+server+" as "+username)
        ftpconn.login(username, password)   
    except KeyboardInterrupt:
        print('CTRL-C detected!')
        detectctrlc=True
        raise
    except:
        raise       

def telnet_connect(server,port,username,password):
    global telnetconn,detectctrlc

    try:    
        print("connect to "+server+":"+str(port))
        telnetconn = telnetlib.Telnet(server,port)
        print("telnet login in to "+server+" as "+username)
        print(telnetconn.read_until(b"login: ").decode('utf-8'))
        telnetconn.write(username.encode('utf-8') + b"\n")
        if password:
            print(telnetconn.read_until(b"Password: ").decode('utf-8'))
            telnetconn.write(password.encode('utf-8') + b"\n")
        time.sleep(0.1)  
        print(telnetconn.read_lazy().decode('utf-8'))
    except KeyboardInterrupt:
        print('CTRL-C detected!')
        detectctrlc=True
        raise     
    except:
        raise
    
def ftp_disconnect():
    global ftpconn,detectctrlc

    try:
        if not(ftpconn==None):
            if not(ftpconn.sock==None):
                print("logout from ftp")
                ftpconn.quit()
    except KeyboardInterrupt:
        print('CTRL-C detected!')
        detectctrlc=True
        raise       
    except:
        raise
      
def telnet_disconnect():
    global telnetconn,detectctrlc

    try:
        if not(telnetconn==None):
            print("logout from telnet")
            telnetconn.write(b"exit\n")
            print(telnetconn.read_all().decode('utf-8'))
    except KeyboardInterrupt:
        print('CTRL-C detected!')
        detectctrlc=True
        raise         
    except:
        raise

def ftp_listdir():
    global ftpconn,detectctrlc

    try:    
        data = []
    
        ftpconn.dir(data.append)
    
        for line in data:
            print("-"+line)
    except KeyboardInterrupt:
        print('CTRL-C detected!')
        detectctrlc=True
        raise            
    except:
        raise

def telnet_listdir():
    global telnetconn,detectctrlc

    try:    
        telnetconn.write(b"ls\n")
    
        time.sleep(0.1)
        print(telnetconn.read_lazy().decode('utf-8'))
    except KeyboardInterrupt:
        print('CTRL-C detected!')
        detectctrlc=True
        raise       
    except:
        raise

def telnet_runscript(scriptabsname):
    global telnetconn,detectctrlc

    try:    
        telnetconn.write(scriptabsname.encode('utf-8')+b"\n")
    
        time.sleep(0.1)
        print(telnetconn.read_lazy().decode('utf-8'))
    except KeyboardInterrupt:
        print('CTRL-C detected!')
        detectctrlc=True
        raise       
    except:
        raise

def getPasswordFromFile(passwordFile):
        #read contents of passwordFile into buffer
        with open(passwordFile, "r") as f:
            buffer=f.read()
        return buffer.rstrip('\n')#remove any trailing newlines

def usage():
    """ Script Usage """
    print("Usage: python3 client.py OPTIONS")
    print("OPTIONS:")
    print("    --help, --usage, -h: Show this help message")
    print("    --server: IP address of server")
    print("    --user: Username on server")
    print("    --passwordFile: Password file")
    print("    --noftp: Do not perform ftp")
    print("    --notelnet: Do not perform telnet")    
    print("    --ftpport: Port for ftp")
    print("    --telnetport: Port for telnet")
    print("")
    print("EXAMPLES:")
    print("    python3 client.py --server 192.168.70.11 --user anonymous --ftpport 21 --telnetport 23")
    sys.exit(1)    
        
def main():
    global detectctrlc

    parser = argparse.ArgumentParser()
    parser.add_argument('--server', help='server ip')    
    parser.add_argument('--user', help='username')
    parser.add_argument('--passwordFile', type=str, help='password file')
    parser.add_argument('--interval', type=int, default=45, help='interval in seconds to repeat on')
    parser.add_argument('--noftp', action="store_true", help='if present, do not perform ftp')
    parser.add_argument('--notelnet', action="store_true", help='if present, do not perform telnet')
    parser.add_argument('--ftpport', type=int, help='ftp port')
    parser.add_argument('--telnetport', type=int, help='telnet port')
    
    args=parser.parse_args()
    
    if (args.server==None) or (args.user==None):
        usage()

    if args.passwordFile==None:    
        password=getpass.getpass(prompt='Password for '+args.user+':', stream=None)
    else: #read password from passwordFile
        password=getPasswordFromFile(args.passwordFile)
    
    if (args.ftpport==None):
        ftpport=cDEFAULT_FTP_PORT
    else:
        ftpport=args.ftpport

    if (args.telnetport==None):
        telnetport=cDEFAULT_TELNET_PORT
    else:
        telnetport=args.telnetport    

    interval=args.interval
    
    print('Running. Press CTRL-C to exit.')
    detectctrlc=False
    while detectctrlc==False:
        try:
            # Do nothing and hog CPU forever until SIGINT received.
            if not args.noftp:
                time.sleep(interval)
                ftp_connect(args.server,ftpport,args.user,password)
                time.sleep(1)
                ftp_listdir()
                ftp_disconnect()
            if not args.notelnet:
                time.sleep(interval)
                telnet_connect(args.server,telnetport,args.user,password)
                time.sleep(1)
                telnet_listdir()
                telnet_disconnect()            
            pass
        except KeyboardInterrupt:
            print('CTRL-C detected!')
            detectctrlc=True
            pass
        except:
            print("Unexpected error:", sys.exc_info()[0])
            time.sleep(5)
            pass        
    
    print('Quitting!')    
    ftp_disconnect()

if __name__ == '__main__':
    main()
    
