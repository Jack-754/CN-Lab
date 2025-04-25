#include <stdio.h>
#include <stdlib.h>
#include <sys/utsname.h>
#include <sys/time.h>
#include <sys/sysinfo.h>
#include <unistd.h>

// Function to get and print hostname
void print_hostname() {
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        printf("Hostname: %s\n", hostname);
    } else {
        perror("gethostname");
    }
}

// Function to get and print system uptime and load averages
void print_sysinfo() {
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        printf("System Uptime: %ld seconds\n", info.uptime);
        printf("Load Average: %.2f, %.2f, %.2f\n", 
               info.loads[0] / 65536.0, 
               info.loads[1] / 65536.0, 
               info.loads[2] / 65536.0);
    } else {
        perror("sysinfo");
    }
}

// Function to get and print current time
void print_time() {
    struct timeval tv;
    if (gettimeofday(&tv, NULL) == 0) {
        printf("Current Time: %ld.%06ld seconds since Epoch\n", tv.tv_sec, tv.tv_usec);
    } else {
        perror("gettimeofday");
    }
}

int main() {
    print_hostname();
    print_sysinfo();
    print_time();
    return 0;
}
