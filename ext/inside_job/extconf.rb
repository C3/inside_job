require 'mkmf'

header_locations = %w(/opt/local/include /usr/local/include /usr/include)
library_locations = %w(/opt/local/lib /usr/local/lib /usr/lib)

puts "Checking for zmq, msgpack headers in: #{header_locations.join(', ')}"
has_zmq = have_header('zmq.h') || find_header('zmq.h', *header_locations)
has_msgpack = have_header('msgpack.h') || find_header('msgpack.h', *header_locations)

puts "Checking for zmq, msgpack libs in: #{library_locations.join(', ')}"
has_zmq_libs = have_library('zmq', 'zmq_init') || find_library('zmq', *library_locations)
has_msgpack_libs = have_library('msgpack') || find_library('msgpack', *library_locations)

CONFIG['cflags'] = '-std=c99'

unless has_zmq && has_zmq_libs
  raise "zmq wasn't found, please specify using --with-zmq-dir=<path>"
end

unless has_msgpack && has_msgpack_libs
  raise "msgpack wasn't found, please specify using --with-msgpack-dir=<path>"
end

create_makefile('inside_job_ext')
