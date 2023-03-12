import time
import argparse
import sys

def usage():
    """ Script Usage """
    print("Usage: python3 makeemaildict OPTIONS")
    print("OPTIONS:")
    print("    --help, --usage, -h: Show this help message")
    print("    --userdb: database (text) list of users")
    print("    --domain: email domain to append with users")
    print("    --emaildb:  output email database (text) list of emails")
    print("")
    print("EXAMPLES:")
    print("    python3 makeemaildict.py --userdb common_roots.txt --domain nwsecdocker.jhu.edu --emaildb email_common_roots.txt")
    sys.exit(1)    

def create_emaildb(userdb, domain, emaildb): 
    destfile=open(emaildb, 'w')
    
    with open(userdb, 'r') as reader:
        user=reader.readline()
        while user !='': #EOF is empty string
            useremail=user.replace('\n','')+'@'+domain
            destfile.write(useremail+'\n')
            user=reader.readline()
    
def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--userdb', type=str, help='database (text) list of users')    
    parser.add_argument('--domain', type=str, help='email domain to append with users')    
    parser.add_argument('--emaildb', type=str, help='output email database (text) list of emails')
    
    args=parser.parse_args()
    
    if ((args.userdb==None) or (args.domain==None) or (args.emaildb==None)):
        usage()
    else:
        create_emaildb(args.userdb, args.domain, args.emaildb)    

if __name__ == '__main__':
    main()
