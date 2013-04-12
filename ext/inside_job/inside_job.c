#include <ruby.h>

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
  if (event != RUBY_EVENT_C_CALL && event != RUBY_EVENT_C_RETURN)
  {
    rb_frame_method_id_and_class(&mid, &klass);
  }

  // as per thread.c#4484
  if (klass)
  {
    // if klass is a module proxy class, fetch the actual klass that provided the method
    if (TYPE(klass) == T_ICLASS)
    {
      klass = RBASIC(klass)->klass;
    }
    // if klass is a singleton (i.e. the eigenclass) then self will be the actual class
    else if (FL_TEST(klass, FL_SINGLETON))
    {
      klass = rb_iv_get(klass, "__attached__");
    }
  }

  switch(event)
  {
    case RUBY_EVENT_LINE:
    {
      // grab the line/file we're currently up to
      // when the call event comes in the line/file will be the definition of the method
      // not the line that actually called the method
      line_number = rb_sourceline();
      file_name = rb_sourcefile();

      break;
    }
    case RUBY_EVENT_CALL:
    case RUBY_EVENT_C_CALL:
    {
      fprintf(output_file, "call: %s:%d %s %s %E %E\n",
                           file_name,
                           line_number,
                           rb_class2name(klass),
                           rb_id2name(mid));

      break;
    }
    case RUBY_EVENT_RETURN:
    case RUBY_EVENT_C_RETURN:
    {
      fprintf(output_file, "return: %s:%d %s %s %E %E\n",
                           file_name,
                           line_number,
                           rb_class2name(klass),
                           rb_id2name(mid));

      break;
    }
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

