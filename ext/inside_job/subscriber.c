#include <ruby.h>
#include <zmq.h>
#include <msgpack.h>
#include <signal.h>

#ifdef __linux__
#include <sys/types.h>
#include <sys/wait.h>
#endif

#include "inside_job.h"

static void *subscriber_context;
static void *subscriber;

static int subscriber_interrupted = 0;

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

      if (method == EVENT_SYNC) {
        // discard, these are popped onto the pub/sub socket
        // during synchronisation
      }
      else if (method == EVENT_START) {
        void *output_file_name = malloc(message[1].via.raw.size);
        memcpy(output_file_name, message[1].via.raw.ptr, message[1].via.raw.size);

        VALUE _output_file_name = rb_str_new(output_file_name, message[1].via.raw.size);

        rb_funcall(callback, rb_intern("start_event"), 1, _output_file_name);
      }
      else if (method == EVENT_STOP) {
        rb_funcall(callback, rb_intern("stop_event"), 0);
      }
      else if (method == EVENT_CALL) {
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
      else if (method == EVENT_RETURN) {
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
ruby_inside_job_wait_for_publisher(void)
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

    if (method == EVENT_SYNC) {
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

VALUE
ruby_inside_job_subscriber_init(VALUE inside_job_module)
{
  VALUE subscriber_module = rb_define_module_under(inside_job_module, "Subscriber");
  rb_define_singleton_method(subscriber_module, "init", ruby_inside_job_init_subscriber, 0);
  rb_define_singleton_method(subscriber_module, "wait_for_publisher", ruby_inside_job_wait_for_publisher, 0);
  rb_define_singleton_method(subscriber_module, "handle_events", ruby_inside_job_handle_events, 1);
}

