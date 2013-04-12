// os identification: http://sourceforge.net/p/predef/wiki/OperatingSystems/

#include <time.h>
#include <sys/time.h>
#include <ruby.h>

#ifdef __MACH__
#include <mach/clock.h>
#include <mach/mach.h>
#include <sys/resource.h>
#endif

static int already_hooked = 0;
static unsigned int line_number = 0;
static const char *file_name;
static VALUE thread_id;
static FILE *output_file;

// cpu clock value in nanoseconds
static double
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
static double
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

const char *
inside_job_class_name(VALUE klass)
{
  if (klass) {
    VALUE target_klass;

    // if klass is a module proxy class, fetch the actual klass that provided the method
    if (TYPE(klass) == T_ICLASS)
      target_klass = RBASIC(klass)->klass;

    // if klass is a singleton (i.e. the eigenclass) then self will be the actual class
    if (FL_TEST(klass, FL_SINGLETON)) {
      target_klass = rb_iv_get(klass, "__attached__");
    }
    else {
      target_klass = klass;
    }

    return rb_class2name(klass);
  }
  else {
    return "unknown";
  }
}

static void
inside_job_process_event_hook(rb_event_flag_t event, VALUE data, VALUE self, ID mid, VALUE klass)
{
  // escape if we're already hooked
  if (already_hooked) return;

  #ifdef ID_ALLOCATOR
  // don't respond to the object allocation method
  if (mid == ID_ALLOCATOR) return;
  #endif

  already_hooked++;

  thread_id = rb_obj_id(rb_thread_current());

  // we need to lookup this information for things other than c call/return?
  if (event != RUBY_EVENT_C_CALL && event != RUBY_EVENT_C_RETURN) {
    rb_frame_method_id_and_class(&mid, &klass);
  }

  switch(event) {
    case RUBY_EVENT_LINE:
      // grab the line/file we're currently up to
      // when the call event comes in the line/file will be the definition of the method
      // not the line that actually called the method
      line_number = rb_sourceline();
      file_name = rb_sourcefile();

      break;
    case RUBY_EVENT_CALL:
    case RUBY_EVENT_C_CALL:
      fprintf(output_file, "call: %s:%d %s %s %E %E\n",
                           file_name,
                           line_number,
                           inside_job_class_name(klass),
                           rb_id2name(mid),
                           inside_job_wall_clock_value(),
                           inside_job_cpu_clock_value());

      break;
    case RUBY_EVENT_RETURN:
    case RUBY_EVENT_C_RETURN:
      fprintf(output_file, "return: %s:%d %s %s %E %E\n",
                           file_name,
                           line_number,
                           inside_job_class_name(klass),
                           rb_id2name(mid),
                           inside_job_wall_clock_value(),
                           inside_job_cpu_clock_value());

      break;
  }

  already_hooked--;
}


static VALUE
ruby_inside_job_start(VALUE self, VALUE output_file_name)
{
  output_file = fopen(StringValueCStr(output_file_name), "w");

  rb_add_event_hook(inside_job_process_event_hook, RUBY_EVENT_CALL | RUBY_EVENT_RETURN |
                                                   RUBY_EVENT_C_CALL | RUBY_EVENT_C_RETURN |
                                                   RUBY_EVENT_LINE, Qnil);

  return Qnil;
}

static VALUE
ruby_inside_job_stop(VALUE self)
{
  fclose(output_file);

  rb_remove_event_hook(inside_job_process_event_hook);

  return Qnil;
}

void Init_inside_job_ext(void)
{
  VALUE klass = rb_define_module("InsideJob");
  rb_define_singleton_method(klass, "start", ruby_inside_job_start, 1);
  rb_define_singleton_method(klass, "stop", ruby_inside_job_stop, 0);
}

