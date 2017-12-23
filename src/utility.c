//
//  utility.c
//  loxi - a Lox interpreter
//
//  Created by Marco Caldarelli on 23/10/2017.
//

#include "utility.h"
#include "common.h"
#include "string.h"

#include <errno.h>
#include <strings.h>

/* File I/O */

char * readFile(const char *filename)
{
    char *buffer = NULL;
    
    FILE *fp = fopen(filename, "r");
    if(fp != NULL)
    {
        fseek(fp, 0L, SEEK_END);
        size_t size = ftell(fp);
        if(size > UINT32_MAX)
        {
            fprintf(stderr, "File %s too large (2GB max).\n", filename);
        }
        else if (size == 0)
        {
            buffer = str_alloc((uint32_t)size);
        }
        else
        {
            buffer = str_alloc((uint32_t)size);
            rewind(fp);
            
            size_t result = fread(buffer, size, 1, fp);
            buffer[size] = '\0';
            str_setLength(buffer);
            
            if(result != 1)
            {
                fprintf(stderr, "Read file %s failed.\n", filename);
                str_free(buffer);
                buffer = NULL;
            }
        }
        fclose(fp);
    }
    else
    {
        fprintf(stderr, "Could not open file %s: %s\n", filename, strerror(errno));
    }
    
    return buffer;
}


/* Char utilities */

extern inline bool is_alpha(char c);
extern inline bool is_digit(char c);
extern inline bool is_alphanumeric(char c);


/* Time utils */

#include <unistd.h>
#include <mach/mach.h>

extern inline double timer_elapsedSec(Timer *timer);

Timer timer_init()
{
    Timer timer;

#ifdef USE_MACH_TIME
    // NOTE: Retrieve conversion factors for performance timers
    kern_return_t timebaseError = mach_timebase_info(&timer.timebaseInfo);
    if (timebaseError != KERN_SUCCESS)
    {
        fprintf(stderr, "Could not retrieve mach timebase from the system.");
        assert(false);
    }
    timer.absoluteStartTime = mach_absolute_time();
#else
    timer.start = clock();
#endif

    return timer;
}

void timer_reset(Timer *timer)
{
#ifdef USE_MACH_TIME
    timer->absoluteStartTime = mach_absolute_time();
#else
    timer->start = clock();
#endif
}
