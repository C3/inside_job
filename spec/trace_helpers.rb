module TraceHelpers

  def spying_on(&block)
    output_file = Tempfile.new('inside_job')

    InsideJob.start(output_file.path)
    yield
    InsideJob.stop

    result = output_file.read
    output_file.close
    result
  end

end

# TODO: fix the output on assertion failure, it's currently useless
RSpec::Matchers.define :produce_trace do |expected|
  match do |actual|
    line_format = /(return|call): (.*):(\d+) (\w+) (\w+) (.*) (.*)/

    trace_lines = actual.each_line.collect do |line|
      line =~ line_format

      {$1.to_sym => {:file => File.basename($2),
                     :line => $3.to_i,
                     :class => $4,
                     :method => $5}}
    end

    wall_clock_values = actual.each_line.collect { |line| line =~ line_format; $6.to_f }
    cpu_clock_values = actual.each_line.collect { |line| line =~ line_format; $7.to_f }

    # assert the captured call/returns match
    # add lines from the spec helper so the spec doesn't have to assert them itself
    trace_lines.should =~ [{:return => {:file => '(null)',
                                        :line => 0,
                                        :class => 'InsideJob',
                                        :method => 'start'}},
                             {:call => {:file => 'trace_helpers.rb',
                                        :line => 8,
                                        :class => 'InsideJob',
                                        :method => 'stop'}}] + expected

    # assert that the call/return timings are always increasing
    wall_clock_values.sort.should eql wall_clock_values
    cpu_clock_values.sort.should eql cpu_clock_values
  end
end