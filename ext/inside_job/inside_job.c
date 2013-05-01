#include <ruby.h>
#include <stdlib.h>
#include <unistd.h>

#include "publisher.h"
#include "subscriber.h"

char *endpoint;
char *sync_endpoint;

const int INSIDE_JOB_EVENT_SYNC = 88;
const int INSIDE_JOB_EVENT_START = 1;
const int INSIDE_JOB_EVENT_STOP = 2;
const int INSIDE_JOB_EVENT_CALL = 3;
const int INSIDE_JOB_EVENT_RETURN = 4;

static void
DeInit_inside_job_ext(void)
{
  free(endpoint);
  free(sync_endpoint);
}

void Init_inside_job_ext(void)
{
  VALUE klass = rb_define_module("InsideJob");

  rb_define_singleton_method(klass, "start", ruby_inside_job_publisher_start, 1);
  rb_define_singleton_method(klass, "stop", ruby_inside_job_publisher_stop, 0);
  rb_define_singleton_method(klass, "trace", ruby_inside_job_publisher_trace, 1);

  if (asprintf(&endpoint, "ipc:///tmp/inside_job-%i", getpid()) == -1)
    rb_raise(rb_eRuntimeError, "allocation of endpoint failed, out of memory?");
  if (asprintf(&sync_endpoint, "ipc:///tmp/inside_job-sync-%i", getpid()) == -1)
    rb_raise(rb_eRuntimeError, "allocation of sync_endpoint failed, out of memory?");

  ruby_inside_job_publisher_init(klass);
  ruby_inside_job_subscriber_init(klass);

  atexit(DeInit_inside_job_ext);
}
