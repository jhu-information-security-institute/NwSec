import ftplib
import time
from dns.rdataclass import NONE

ftpconn=None
detectctrlc=None

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
        
def main():
    global ftpconn,detectctrlc

    server="192.168.70.11"
    username="anonymous"
    password=""

    print('Running. Press CTRL-C to exit.')
    detectctrlc=False
    while detectctrlc==False:
        try:
            # Do nothing and hog CPU forever until SIGINT received.
            time.sleep(10)
            ftp_connect(server,username,password)
            ftp_listdir()
            ftp_disconnect()
            pass
        except KeyboardInterrupt:
            print('CTRL-C detected!')
            detectctrlc=True
            pass         
    
    print('Quitting!')    
    ftp_disconnect()

if __name__ == '__main__':
    main()
    
