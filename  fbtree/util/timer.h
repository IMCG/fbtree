#ifndef MX_TIMER_H
#define MX_TIMER_H
void start_timer(); /* record current time */
double get_timer(); /* get number of seconds since last call to start timer */
void pr_times(const char * zFilename ,double time); /* print the time into file 'zFileName' */
#endif
