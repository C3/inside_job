// os identification: http://sourceforge.net/p/predef/wiki/OperatingSystems/

#include <time.h>
#include <sys/time.h>

#ifdef __MACH__
#include <mach/clock.h>
#include <mach/mach.h>
#include <sys/resource.h>
#endif

// cpu clock value in nanoseconds
double
inside_job_cpu_clock_value()
{
#ifdef __MACH__
  // ru_utime gives user mode time
  // ru_stime gives system mode time
  // can get potentially helpful stuff like io block counts
  // supported by all unix-y oses
  struct rusage rusage;
  if (getrusage(RUSAGE_SELF, &rusage) != -1)
    return ((double)rusage.ru_utime.tv_sec * 1000000000.0) + ((double)rusage.ru_utime.tv_usec * 1000.0);
#else
  timespec cpu_clock;
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
  clock_serv_t cclock;
  mach_timespec_t mts;
  if (host_get_clock_service(mach_host_self(), SYSTEM_CLOCK, &cclock) != -1)
    if (clock_get_time(cclock, &mts) != -1)
      if (mach_port_deallocate(mach_task_self(), cclock) != -1)
        return ((double)mts.tv_sec * 1000000000.0) + (double)mts.tv_nsec;
#else
  timespec wall_clock;
  if (clock_gettime(CLOCK_MONOTONIC, &wall_clock) != -1)
    return ((double)wall_clcok.tv_sec * 1000000000.0) + (double)wall_clock.tv_nsec;
#endif

  return -1.0;
}
