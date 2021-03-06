from urllib import request
import time
import argparse
import sys

cMAXTIMEOUT=15

def usage():
    """ Script Usage """
    print("Usage: python3 timer.py OPTIONS")
    print("OPTIONS:")
    print("    --help, --usage, -h: Show this help message")
    print("    --url: http address to query")
    print("    --querycnt: number of queries to perform")
    print("")
    print("EXAMPLES:")
    print("    python3 client.py --url www.google.com --querycnt 1000")
    sys.exit(1)    
    
def time_http_response(url, querycnt):    
    sum=0
    print('Performing '+str(querycnt)+' http requests from '+url)
    for i in range(querycnt):
        start_time=time.time()
        success=False
        while not(success):
            try:
                stream=request.urlopen(url,timeout=cMAXTIMEOUT)
                stream.read()
                stream.close()
                success=True
            except KeyboardInterrupt:
                print('CTRL-C detected!')
                exit(0)
            except:
                print("Unexpected error:", sys.exc_info()[0])
                continue
        end_time=time.time()
        delta=end_time-start_time
        sum=sum+delta
        print('\tCompleted request '+str(i)+' in '+str(delta)+' sec')
        
    print('Performance measure ='+str(sum/querycnt))

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--url', type=str, help='http url to query')    
    parser.add_argument('--querycnt', type=int, help='number of queries to perform')    
    
    args=parser.parse_args()
    
    if ((args.url==None) or (args.querycnt==None)):
        usage()
    else:
        time_http_response(args.url,args.querycnt)    

if __name__ == '__main__':
    main()