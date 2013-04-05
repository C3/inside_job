#include <ruby.h>

static void
process_event_hook(rb_event_flag_t event, VALUE data, VALUE self, ID mid, VALUE klass)
{
}


static VALUE
ruby_inside_job_start(VALUE self)
{
  rb_add_event_hook(process_event_hook, RUBY_EVENT_CALL | RUBY_EVENT_RETURN |
                                        RUBY_EVENT_C_CALL | RUBY_EVENT_C_RETURN |
                                        RUBY_EVENT_LINE, Qnil);

  return Qnil;
}

static VALUE
ruby_inside_job_stop(VALUE self)
{
  rb_remove_event_hook(process_event_hook);

  return Qnil;
}

void Init_inside_job_ext(void)
{
  VALUE klass = rb_define_module("InsideJob");
  rb_define_singleton_method(klass, "start", ruby_inside_job_start, 0);
  rb_define_singleton_method(klass, "stop", ruby_inside_job_stop, 0);
}

