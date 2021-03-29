import ftplib
import telnetlib
import time
import argparse
import getpass
import sys

ftpconn=None
telnetconn=None
detectctrlc=None

#https://pythonspot.com/ftp-client-in-python/

def ftp_connect(server,username,password):
    global ftpconn,detectctrlc

    try:    
        print("connect to "+server)
        ftpconn = ftplib.FTP(server)
        print("ftp login in to "+server+" as "+username)
        ftpconn.login(username, password)   
    except KeyboardInterrupt:
        print('CTRL-C detected!')
        detectctrlc=True
        pass          

def telnet_connect(server,username,password):
    global telnetconn,detectctrlc

    try:    
        print("connect to "+server)
        telnetconn = telnetlib.Telnet(server)
        print("telnet login in to "+server+" as "+username)
        print(telnetconn.read_until(b"login: ").decode('ascii'))
        telnetconn.write(username.encode('ascii') + b"\n")
        if password:
            print(telnetconn.read_until(b"Password: ").decode('ascii'))
            telnetconn.write(password.encode('ascii') + b"\n")
        time.sleep(0.1)  
        print(telnetconn.read_lazy().decode('ascii'))
    except KeyboardInterrupt:
        print('CTRL-C detected!')
        detectctrlc=True
        pass     

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
        pass       
      
def telnet_disconnect():
    global telnetconn,detectctrlc

    try:
        if not(telnetconn==None):
            print("logout from telnet")
            telnetconn.write(b"exit\n")
            print(telnetconn.read_all().decode('ascii'))
    except KeyboardInterrupt:
        print('CTRL-C detected!')
        detectctrlc=True
        pass         

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
        pass            

def telnet_listdir():
    global telnetconn,detectctrlc

    try:    
        telnetconn.write(b"ls\n")
    
        time.sleep(0.1)
        print(telnetconn.read_lazy().decode('ascii'))
    except KeyboardInterrupt:
        print('CTRL-C detected!')
        detectctrlc=True
        pass       
        
def usage():
    """ Script Usage """
    print("Usage: python3 client.py OPTIONS")
    print("OPTIONS:")
    print("    --help, --usage, -h: Show this help message")
    print("    --server: IP address of server")
    print("    --user: Username on server")
    print("")
    print("EXAMPLES:")
    print("    python3 client.py --server 192.168.70.11 --user anonymous")
    sys.exit(1)    
        
def main():
    global ftpconn,detectctrlc

    parser = argparse.ArgumentParser()
    parser.add_argument('--server', help='server ip')    
    parser.add_argument('--user', help='username')    
    
    args=parser.parse_args()
    
    if (args.server==None) or (args.user==None):
        usage()
    
    password=getpass.getpass(prompt='Password for '+args.user+':', stream=None)

    print('Running. Press CTRL-C to exit.')
    detectctrlc=False
    while detectctrlc==False:
        try:
            # Do nothing and hog CPU forever until SIGINT received.
            time.sleep(5)
            ftp_connect(args.server,"anonymous","")
            ftp_listdir()
            ftp_disconnect()
            time.sleep(5)
            telnet_connect(args.server,args.user,password)
            telnet_listdir()
            telnet_disconnect()            
            pass
        except KeyboardInterrupt:
            print('CTRL-C detected!')
            detectctrlc=True
            pass         
    
    print('Quitting!')    
    ftp_disconnect()

if __name__ == '__main__':
    main()
    
