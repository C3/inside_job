#ifndef PUBLISHER_H_INCLUDED
#define PUBLISHER_H_INCLUDED
VALUE ruby_inside_job_publisher_init(VALUE inside_job_module);
VALUE ruby_inside_job_publisher_start(VALUE self, VALUE output_file_name);
VALUE ruby_inside_job_publisher_stop(VALUE self);
VALUE ruby_inside_job_publisher_trace(VALUE self, VALUE output_file_name);
#endif