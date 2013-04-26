#include <ruby.h>
#include <zmq.h>
#include <msgpack.h>
#include <signal.h>

#ifdef __linux__
#include <sys/types.h>
#include <sys/wait.h>
#endif

#include "timing.h"
#include "inside_job.h"

static int already_hooked = 0;
static VALUE thread_id;
static FILE *output_file;

static pid_t subscriber_pid;
static void *publisher_context;
static void *publisher;

static void
ruby_inside_job_deinit_publisher(void)
{
  kill(subscriber_pid, SIGTERM);
  wait(&subscriber_pid);
  zmq_close(publisher);
  zmq_ctx_destroy(publisher_context);
}

static VALUE
ruby_inside_job_init_publisher(VALUE self, VALUE child_pid)
{
  subscriber_pid = NUM2PIDT(child_pid);
  atexit(ruby_inside_job_deinit_publisher);

  publisher_context = zmq_ctx_new();
  publisher = zmq_socket(publisher_context, ZMQ_PUB);
  zmq_setsockopt(publisher, ZMQ_IDENTITY, "pub", 3);
  int rc = zmq_bind(publisher, endpoint);
  if (rc != 0) {
    printf("Unable to bind: %s\n", zmq_strerror(errno));
  }

  return Qnil;
}

static VALUE
ruby_inside_job_wait_for_subscriber(void)
{
  void *syncservice = zmq_socket(publisher_context, ZMQ_REP);
  zmq_bind (syncservice, sync_endpoint);
  int subscribers = 0;
  char buf[0];

  // drop sync messages down the pub/sub socket, this lets us know
  // that the the pub/sub connection is up. notification
  // will come through the rep/req socket from the subscriber
  while (subscribers < 1) {
    int status = EAGAIN;
    int keep_trying = 1;

    while (keep_trying) {
      msgpack_sbuffer *buffer = msgpack_sbuffer_new();
      msgpack_packer *pk = msgpack_packer_new(buffer, msgpack_sbuffer_write);
      msgpack_pack_array(pk, 1);
      msgpack_pack_uint16(pk, INSIDE_JOB_EVENT_SYNC);

      zmq_msg_t message;
      zmq_msg_init_data(&message, buffer->data, buffer->size, NULL, NULL);
      zmq_msg_send(&message, publisher, 0);
      zmq_msg_close(&message);

      msgpack_sbuffer_free(buffer);
      msgpack_packer_free(pk);

      status = zmq_recv(syncservice, buf, 0, ZMQ_DONTWAIT);
      if (status == 0)
        keep_trying = 0;
    }
    zmq_send(syncservice, "", 0, 0);
    subscribers++;
  }
  zmq_close(syncservice);

  return Qtrue;
}

static VALUE
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

    return rb_class_name(klass);
  }
  else {
    return rb_str_new2("Unknown");
  }
}

static void
inside_job_send_call_message(VALUE class_name, VALUE method_name)
{
  msgpack_sbuffer *buffer = msgpack_sbuffer_new();
  msgpack_packer *pk = msgpack_packer_new(buffer, msgpack_sbuffer_write);

  msgpack_pack_array(pk, 5);
  msgpack_pack_uint16(pk, INSIDE_JOB_EVENT_CALL);
  msgpack_pack_raw(pk, RSTRING_LEN(class_name));
  msgpack_pack_raw_body(pk, RSTRING_PTR(class_name), RSTRING_LEN(class_name));
  msgpack_pack_raw(pk, RSTRING_LEN(method_name));
  msgpack_pack_raw_body(pk, RSTRING_PTR(method_name), RSTRING_LEN(method_name));
  msgpack_pack_double(pk, inside_job_wall_clock_value());
  msgpack_pack_double(pk, inside_job_cpu_clock_value());

  zmq_msg_t message;
  zmq_msg_init_size(&message, buffer->size);
  memcpy(zmq_msg_data(&message), buffer->data, buffer->size);
  zmq_msg_send(&message, publisher, 0);
  zmq_msg_close(&message);

  msgpack_sbuffer_free(buffer);
  msgpack_packer_free(pk);
}

static void
inside_job_send_return_message(void)
{
  msgpack_sbuffer *buffer = msgpack_sbuffer_new();
  msgpack_packer *pk = msgpack_packer_new(buffer, msgpack_sbuffer_write);

  msgpack_pack_array(pk, 3);
  msgpack_pack_uint16(pk, INSIDE_JOB_EVENT_RETURN);
  msgpack_pack_double(pk, inside_job_wall_clock_value());
  msgpack_pack_double(pk, inside_job_cpu_clock_value());

  zmq_msg_t message;
  zmq_msg_init_size(&message, buffer->size);
  memcpy(zmq_msg_data(&message), buffer->data, buffer->size);
  zmq_msg_send(&message, publisher, 0);
  zmq_msg_close(&message);

  msgpack_sbuffer_free(buffer);
  msgpack_packer_free(pk);
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
      inside_job_send_call_message(inside_job_class_name(klass),
                                   rb_id2str(mid));

      break;
    case RUBY_EVENT_RETURN:
    case RUBY_EVENT_C_RETURN:
      inside_job_send_return_message();

      break;
  }

  already_hooked--;
}

VALUE
ruby_inside_job_publisher_init(VALUE inside_job_module)
{
  VALUE publisher_module = rb_define_module_under(inside_job_module, "Publisher");
  rb_define_singleton_method(publisher_module, "init", ruby_inside_job_init_publisher, 1);
  rb_define_singleton_method(publisher_module, "wait_for_subscriber", ruby_inside_job_wait_for_subscriber, 0);
}

VALUE
ruby_inside_job_publisher_start(VALUE self, VALUE output_file_name)
{
  char *output = RSTRING_PTR(output_file_name);

  msgpack_sbuffer *buffer = msgpack_sbuffer_new();
  msgpack_packer *pk = msgpack_packer_new(buffer, msgpack_sbuffer_write);
  msgpack_pack_array(pk, 2);
  msgpack_pack_uint16(pk, INSIDE_JOB_EVENT_START);
  msgpack_pack_raw(pk, RSTRING_LEN(output_file_name));
  msgpack_pack_raw_body(pk, output, RSTRING_LEN(output_file_name));

  zmq_msg_t message;
  zmq_msg_init_size(&message, buffer->size);
  memcpy(zmq_msg_data(&message), buffer->data, buffer->size);
  zmq_msg_send(&message, publisher, 0);
  zmq_msg_close(&message);

  msgpack_sbuffer_free(buffer);
  msgpack_packer_free(pk);

  rb_add_event_hook(inside_job_process_event_hook, RUBY_EVENT_CALL | RUBY_EVENT_RETURN |
                                                   RUBY_EVENT_C_CALL | RUBY_EVENT_C_RETURN,
                                                   Qnil);

  return Qnil;
}

VALUE
ruby_inside_job_publisher_stop(VALUE self)
{
  msgpack_sbuffer *buffer = msgpack_sbuffer_new();
  msgpack_packer *pk = msgpack_packer_new(buffer, msgpack_sbuffer_write);
  msgpack_pack_array(pk, 1);
  msgpack_pack_uint16(pk, INSIDE_JOB_EVENT_STOP);

  zmq_msg_t message;
  zmq_msg_init_data(&message, buffer->data, buffer->size, NULL, NULL);
  zmq_msg_send(&message, publisher, 0);
  zmq_msg_close(&message);

  msgpack_sbuffer_free(buffer);
  msgpack_packer_free(pk);

  rb_remove_event_hook(inside_job_process_event_hook);

  return Qnil;
}

VALUE
ruby_inside_job_publisher_trace(VALUE self, VALUE output_file_name)
{
  if (!rb_block_given_p())
    rb_raise(rb_eArgError, "Expected block");

  ruby_inside_job_publisher_start(self, output_file_name);
  rb_yield(Qnil);
  ruby_inside_job_publisher_stop(self);

  return Qnil;
}

