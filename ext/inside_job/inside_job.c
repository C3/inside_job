#include <ruby.h>
#include "timing.h"

static int already_hooked = 0;
static VALUE thread_id;
static FILE *output_file;

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
    case RUBY_EVENT_CALL:
    case RUBY_EVENT_C_CALL:
      fprintf(output_file, "call: %s %s %E %E\n",
                           inside_job_class_name(klass),
                           rb_id2name(mid),
                           inside_job_wall_clock_value(),
                           inside_job_cpu_clock_value());

      break;
    case RUBY_EVENT_RETURN:
    case RUBY_EVENT_C_RETURN:
      fprintf(output_file, "return: %E %E\n",
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
                                                   RUBY_EVENT_C_CALL | RUBY_EVENT_C_RETURN,
                                                   Qnil);

  return Qnil;
}

static VALUE
ruby_inside_job_stop(VALUE self)
{
  fclose(output_file);

  rb_remove_event_hook(inside_job_process_event_hook);

  return Qnil;
}

static VALUE
ruby_inside_job_trace(VALUE self, VALUE output_file_name)
{
  if (!rb_block_given_p())
    rb_raise(rb_eArgError, "Expected block");

  ruby_inside_job_start(self, output_file_name);
  rb_yield(Qnil);
  ruby_inside_job_stop(self);

  return Qnil;
}

void Init_inside_job_ext(void)
{
  VALUE klass = rb_define_module("InsideJob");
  rb_define_singleton_method(klass, "start", ruby_inside_job_start, 1);
  rb_define_singleton_method(klass, "stop", ruby_inside_job_stop, 0);
  rb_define_singleton_method(klass, "trace", ruby_inside_job_trace, 1);
}

