//
//  utility.h
//  loxi - a Lox interpreter
//
//  Created by Marco Caldarelli on 23/10/2017.
//

#ifndef utility_h
#define utility_h

#include "common.h"
#include <stdbool.h>

/*  File I/O  */

char * readFile(const char *filename);


/* Char utils */

inline bool is_alpha(char c)
{
    return ((c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            c == '_');
}

inline bool is_digit(char c)
{
    return (c >= '0' && c <= '9');
}

inline bool is_alphanumeric(char c)
{
    return (is_alpha(c) || is_digit(c));
}


/* Time utils */

#ifdef USE_MACH_TIME
#include <mach/mach_time.h>
#define NANOSEC_PER_SEC 1000000000
#else
#include <time.h>
#endif


typedef struct Timer
{
#ifdef USE_MACH_TIME
    mach_timebase_info_data_t timebaseInfo;
    uint64_t absoluteStartTime;
#else
    clock_t start;
#endif
} Timer;

Timer timer_init(void);
void timer_reset(Timer *timer);

inline double timer_elapsedSec(Timer *timer)
{
    double elapsedSec;
    
#ifdef USE_MACH_TIME
    uint64_t absoluteTime = mach_absolute_time();
    uint64_t elapsedNanoSec = (absoluteTime - timer->absoluteStartTime) * timer->timebaseInfo.numer / timer->timebaseInfo.denom;
    elapsedSec = (double)elapsedNanoSec / NANOSEC_PER_SEC;
#else
    clock_t time_now = clock();
    elapsedSec = difftime(time_now, timer->start) / CLOCKS_PER_SEC;
#endif
    
    return elapsedSec;
}

#endif /* utility_h */
