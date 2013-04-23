#include <ruby.h>
#include <unistd.h>
#include <zmq.h>
#include <msgpack.h>
#include <signal.h>
#include <stdlib.h>

#ifdef __linux__
#include <sys/types.h>
#include <sys/wait.h>
#endif

#include "timing.h"

static int already_hooked = 0;
static int subscriber_interrupted = 0;
static VALUE thread_id;
static FILE *output_file;

static char *endpoint;
static char *sync_endpoint;

static pid_t subscriber_pid;

static void *publisher_context;
static void *publisher;
static void *subscriber_context;
static void *subscriber;

const int event_sync = 88;
const int event_start = 1;
const int event_stop = 2;
const int event_call = 3;
const int event_return = 4;

VALUE
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
  msgpack_pack_uint16(pk, event_call);
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
inside_job_send_return_message()
{
  msgpack_sbuffer *buffer = msgpack_sbuffer_new();
  msgpack_packer *pk = msgpack_packer_new(buffer, msgpack_sbuffer_write);

  msgpack_pack_array(pk, 3);
  msgpack_pack_uint16(pk, event_return);
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

static VALUE
ruby_inside_job_start(VALUE self, VALUE output_file_name)
{
  char *output = RSTRING_PTR(output_file_name);

  msgpack_sbuffer *buffer = msgpack_sbuffer_new();
  msgpack_packer *pk = msgpack_packer_new(buffer, msgpack_sbuffer_write);
  msgpack_pack_array(pk, 2);
  msgpack_pack_uint16(pk, event_start);
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

static VALUE
ruby_inside_job_stop(VALUE self)
{
  msgpack_sbuffer *buffer = msgpack_sbuffer_new();
  msgpack_packer *pk = msgpack_packer_new(buffer, msgpack_sbuffer_write);
  msgpack_pack_array(pk, 1);
  msgpack_pack_uint16(pk, event_stop);

  zmq_msg_t message;
  zmq_msg_init_data(&message, buffer->data, buffer->size, NULL, NULL);
  zmq_msg_send(&message, publisher, 0);
  zmq_msg_close(&message);

  msgpack_sbuffer_free(buffer);
  msgpack_packer_free(pk);

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

static void
publisher_exit(void)
{
  free(endpoint);
  free(sync_endpoint);
  kill(subscriber_pid, SIGTERM);
  wait(&subscriber_pid);
  zmq_close(publisher);
  zmq_ctx_destroy(publisher_context);
}

static void
subscriber_handle_signal(int signal_value)
{
  subscriber_interrupted = 1;
}

static VALUE
ruby_inside_job_init_subscriber(void)
{
  // register signal handler
  struct sigaction action;
  action.sa_handler = subscriber_handle_signal;
  action.sa_flags = 0;
  sigemptyset(&action.sa_mask);
  sigaction(SIGINT, &action, NULL);
  sigaction(SIGTERM, &action, NULL);

  subscriber_context = zmq_ctx_new();
  subscriber = zmq_socket(subscriber_context, ZMQ_SUB);
  int rc = zmq_connect(subscriber, endpoint);
  if (rc != 0) {
    printf("Unable to connect: %s\n", zmq_strerror(errno));
  }
  else {
    zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, "", 0);
    zmq_setsockopt(subscriber, ZMQ_IDENTITY, "sub", 3);
  }

  return Qnil;
}

static VALUE
ruby_inside_job_handle_events(VALUE self, VALUE callback)
{
  while (1) {
    if (subscriber_interrupted)
      break;

    zmq_msg_t zmq_message;
    zmq_msg_init(&zmq_message);
    zmq_msg_recv(&zmq_message, subscriber, 0);

    // size_t size = zmq_msg_size(&zmq_message);
    // VALUE buffer = rb_str_buf_new(size);

    // void *message = malloc(zmq_msg_size(&zmq_message));
    // memcpy(message, zmq_msg_data(&zmq_message), zmq_msg_size(&zmq_message));
    // rb_funcall(callback, rb_intern("handle"), 1, message);
    // free(message);

    msgpack_unpacked msgpack_message;
    msgpack_unpacked_init(&msgpack_message);
    msgpack_unpack_next(&msgpack_message, zmq_msg_data(&zmq_message), zmq_msg_size(&zmq_message), NULL);
    msgpack_object obj = msgpack_message.data;

    if (obj.type == MSGPACK_OBJECT_ARRAY) {
      msgpack_object_array array = obj.via.array;
      msgpack_object* message = obj.via.array.ptr;

      int method = (int)message->via.u64;

      if (method == event_sync) {
        // discard, these are popped onto the pub/sub socket
        // during synchronisation
      }
      else if (method == event_start) {
        void *output_file_name = malloc(message[1].via.raw.size);
        memcpy(output_file_name, message[1].via.raw.ptr, message[1].via.raw.size);

        VALUE _output_file_name = rb_str_new(output_file_name, message[1].via.raw.size);

        rb_funcall(callback, rb_intern("start_event"), 1, _output_file_name);
      }
      else if (method == event_stop) {
        rb_funcall(callback, rb_intern("stop_event"), 0);
      }
      else if (method == event_call) {
        void *klass = malloc(message[1].via.raw.size);
        memcpy(klass, message[1].via.raw.ptr, message[1].via.raw.size);
        VALUE _klass = rb_str_new(klass, message[1].via.raw.size);

        void *method = malloc(message[2].via.raw.size);
        memcpy(method, message[2].via.raw.ptr, message[2].via.raw.size);
        VALUE _method = rb_str_new(method, message[2].via.raw.size);

        VALUE _wall_clock = DBL2NUM(message[3].via.dec);
        VALUE _cpu_clock = DBL2NUM(message[4].via.dec);

        rb_funcall(callback, rb_intern("call_event"), 4, _klass, _method, _wall_clock, _cpu_clock);
      }
      else if (method == event_return) {
        VALUE _wall_clock = DBL2NUM(message[1].via.dec);
        VALUE _cpu_clock = DBL2NUM(message[2].via.dec);

        rb_funcall(callback, rb_intern("return_event"), 2, _wall_clock, _cpu_clock);
      }
    }

    msgpack_unpacked_destroy(&msgpack_message);
    zmq_msg_close(&zmq_message);
  }

  zmq_close(subscriber);
  zmq_ctx_destroy(subscriber_context);

  return Qnil;
}

static VALUE
ruby_inside_job_wait_for_producer(void)
{
  // connect res/rsp pair
  void *syncclient = zmq_socket(subscriber_context, ZMQ_REQ);
  zmq_connect(syncclient, sync_endpoint);

  zmq_msg_t message;
  zmq_msg_init(&message);
  // wait for a message on pub/sub
  zmq_msg_recv(&message, subscriber, 0);

  msgpack_unpacked msgpack_message;
  msgpack_unpacked_init(&msgpack_message);

  size_t size = zmq_msg_size(&message);
  msgpack_unpack_next(&msgpack_message, zmq_msg_data(&message), size, NULL);

  msgpack_object obj = msgpack_message.data;

  if (obj.type == MSGPACK_OBJECT_ARRAY) {
    msgpack_object_array array = obj.via.array;
    int method = (int)array.ptr[0].via.u64;

    if (method == event_sync) {
      printf("received sync event\n");
    }
  }
  else {
    rb_raise(rb_eArgError, "Received non-sync message during synchronisation");
  }

  // send a message indicating we're awake and receiving on pub/sub
  zmq_send(syncclient, "", 0, 0);

  char buf[0];
  zmq_recv(syncclient, buf, 0, 0);

  zmq_close(syncclient);

  return Qtrue;
}

static VALUE
ruby_inside_job_init_producer(VALUE self, VALUE child_pid)
{
  subscriber_pid = NUM2PIDT(child_pid);
  atexit(publisher_exit);

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
ruby_inside_job_wait_for_subscriber()
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
      msgpack_pack_uint16(pk, event_sync);

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

void Init_inside_job_ext(void)
{
  VALUE klass = rb_define_module("InsideJob");
  // user actions
  rb_define_singleton_method(klass, "start", ruby_inside_job_start, 1);
  rb_define_singleton_method(klass, "stop", ruby_inside_job_stop, 0);
  rb_define_singleton_method(klass, "trace", ruby_inside_job_trace, 1);

  // subscriber setup
  rb_define_singleton_method(klass, "init_subscriber", ruby_inside_job_init_subscriber, 0);
  rb_define_singleton_method(klass, "wait_for_producer", ruby_inside_job_wait_for_producer, 0);
  rb_define_singleton_method(klass, "handle_events", ruby_inside_job_handle_events, 1);

  // producer setup
  rb_define_singleton_method(klass, "init_producer", ruby_inside_job_init_producer, 1);
  rb_define_singleton_method(klass, "wait_for_subscriber", ruby_inside_job_wait_for_subscriber, 0);

  asprintf(&endpoint, "ipc:///tmp/inside_job-%i", getpid());
  asprintf(&sync_endpoint, "ipc:///tmp/inside_job-sync-%i", getpid());
}
