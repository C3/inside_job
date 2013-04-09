module TraceHelpers

  def trace_for(&block)
    output_file = Tempfile.new('inside_job')

    InsideJob.start(output_file.path)
    yield
    InsideJob.stop

    trace = Trace.new(output_file.read)
    output_file.close
    trace
  end

end

RSpec::Matchers.define :have_produced_lines do |expected|
  match do |actual|
    # the trace will contain the return from InsideJob.start and the call
    # from InsideJob.stop, the line/file of InsideJob.start isn't consistent
    # between runs
    # seems to be (null):0 for the first run, trace_helpers.rb:8 after?!!?
    # delete them for comparing the desired parts
    actual.lines[1..-2].should == expected
  end

  failure_message_for_should do |actual|
    <<-EOF
  expected:
    #{PP.pp actual, ""}
  to match:
    #{PP.pp expected, ""}
    EOF
  end

  failure_message_for_should_not do |acutal|
  end
end

RSpec::Matchers.define :be_always_increasing do
  match do |actual|
    actual.sort.should eql actual
  end

  failure_message_for_should do |actual|
    "expected #{actual} to be always increasing"
  end

  failure_message_for_should_not do |acutal|
    "expected #{actual} to not be always increasing"
  end
end

class Trace

  attr :lines, :wall_times, :cpu_times

  def initialize(trace)
    line_format = /(return|call): (.+):(\d+) (.+) (\w+) (.*) (.*)/

    @lines = trace.each_line.collect do |line|
      line =~ line_format

      {$1.to_sym => {:file => File.basename($2),
                     :line => $3.to_i,
                     :class => $4,
                     :method => $5}}
     end

     @wall_times = trace.each_line.collect { |line| line =~ line_format; $6.to_f }
     @cpu_times = trace.each_line.collect { |line| line =~ line_format; $7.to_f }
   end

end