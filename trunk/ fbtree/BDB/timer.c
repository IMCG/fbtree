#include "timer.h"
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#ifndef WIN32

static struct timeval tstart;
/* record current time */
void start_timer(){
    if(gettimeofday(&tstart,NULL)==-1){
        fprintf(stderr,"gettimeofday error!");
        exit(-1);
    }
}

/* get number of seconds since last call to start timer */
double get_timer(){
    struct timeval tfinish;
    long sec,usec;

    if(gettimeofday(&tfinish,NULL)==-1){
        fprintf(stderr,"gettimeofday error!");
        exit(-1);
    }
    sec = tfinish.tv_sec - tstart.tv_sec;
    usec = tfinish.tv_usec - tstart.tv_usec;
    return sec + 1e-6 * usec;

}

#else							/* WIN32 */
#include <Windows.h>
static __inline__ double
GetTimerFrequency(void)
{
	LARGE_INTEGER f;

	QueryPerformanceFrequency(&f);
	return (double) f.QuadPart;
}

static LARGE_INTEGER tstart;

/* record current time */
void start_timer(){
    QueryPerformanceCounter(&tstart);
}

/* get number of seconds since last call to start timer */
double get_timer(){
    LARGE_INTEGER tfinish;

    QueryPerformanceCounter(&tfinish);
    return ((double)(tfinish.QuadPart-tstart.QuadPart) / GetTimerFrequency());
}
#endif   /* WIN32 */

#include <stdio.h>
void pr_times(const char * zFilename ,double time){

    static int no=1;
    FILE *fp = fopen(zFilename,"a");

    fprintf(fp,"%d \t%.6f\n",no++,time);
    fclose(fp);

}

