//Build using: $ gcc goroot.c -o goroot
//Set capability on the executable using: $ setcap CAP_SETUID+ep goroot
//CAP_SETUID is the capability for arbitrary manipulations of process UIDs
//See https://man7.org/linux/man-pages/man7/capabilities.7.html
//+e makes the capability effected (i.e., activated)
//+p makes the capability permitted (i.e., can be used/is allowed)
//See https://www.insecure.ws/linux/getcap_setcap.html

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

int main() {
	printf("uid=%d, euid=%d\n",getuid(),geteuid());
	printf("Go go gadget root!\n");
	setreuid(0,geteuid());
	printf("uid=%d, euid=%d\n",getuid(),geteuid());
	system("/bin/bash");//This line is dangerous: It allows an attacker to execute arbitrary code on your machine (even by accident).
	return 0;
}
