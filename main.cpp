#include "TimerThread.h"
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
uint64_t tot_exp = 0;
static void print_elapsed_time(void)
{
    static struct timespec start;
    struct timespec curr;
    static int first_call = 1;
    int secs, nsecs;
    
    if (first_call) {
        first_call = 0;
        if (clock_gettime(CLOCK_MONOTONIC, &start) == -1) 
            printf("clock_gettime error\n");
    }   
    
    if (clock_gettime(CLOCK_MONOTONIC, &curr) == -1) 
        printf("clock_gettime error\n");
    
    secs = curr.tv_sec - start.tv_sec;
    nsecs = curr.tv_nsec - start.tv_nsec;
    if (nsecs < 0) {
        secs--;
        nsecs += 1000000000;
    }   
    printf("%d.%03d: ", secs, (nsecs + 500000) / 1000000);
}

void timer_task1(void * data)
{
	uint64_t exp = 0;
    int *fd = static_cast<int*>(data);
    read(*fd, &exp, sizeof(uint64_t)); 
    tot_exp += exp;
    print_elapsed_time();
    printf("read: %llu, total: %llu\n", (unsigned long long)exp, (unsigned long long)tot_exp);
 
    return;
}
int main(void)
{
	TimerThread tt_obj;
	tt_obj.TimerThreadInit(10);
	int timer = tt_obj.TimerThreadAdd(2, -1, timer_task1, NULL, NULL);
    sleep(1);
    tt_obj.TimerThreadDel(timer);
    tt_obj.TimerThreadClear();
	return 0;
}
