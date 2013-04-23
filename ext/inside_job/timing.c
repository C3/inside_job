// os identification: http://sourceforge.net/p/predef/wiki/OperatingSystems/

#include <time.h>

#ifdef __linux__
#include <linux/time.h>
#endif

#ifdef __MACH__
#include <sys/time.h>
#endif

// cpu clock value in nanoseconds
double
inside_job_cpu_clock_value()
{
#ifdef __MACH__
  return (((double)clock()) / CLOCKS_PER_SEC) * 1000000000.0;
#else
  struct timespec cpu_clock;
  if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &cpu_clock) != -1)
    return ((double)cpu_clock.tv_sec * 1000000000.0) + (double)cpu_clock.tv_nsec;
#endif

  return -1.0;
}

// system clock value in nanoseconds
double
inside_job_wall_clock_value()
{
#ifdef __MACH__
  struct timeval wall_clock;
  gettimeofday(&wall_clock, NULL);
  return ((double)wall_clock.tv_sec * 1000000000.0) + ((double)wall_clock.tv_usec * 1000.0);
#else
  struct timespec wall_clock;
  if (clock_gettime(CLOCK_MONOTONIC, &wall_clock) != -1)
    return ((double)wall_clock.tv_sec * 1000000000.0) + (double)wall_clock.tv_nsec;
#endif

  return -1.0;
}
