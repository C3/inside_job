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

  def parse_profile(profile)
    profile.each_line.collect do |line|
      line =~ /(return|call): (.*):(\d+) (\w+) (\w+)/

      {$1.to_sym => {:file => File.basename($2),
                     :line => $3.to_i,
                     :class => $4,
                     :method => $5}}
    end
  end

end

RSpec::Matchers.define :produce_trace do |expected|
  match do |actual|
    parse_profile(actual).should eql [{:return => {:file => '(null)',
                                                   :line => 0,
                                                   :class => 'InsideJob',
                                                   :method => 'start'}},
                                       {:call => {:file => 'trace_helpers.rb',
                                                  :line => 8,
                                                  :class => 'InsideJob',
                                                  :method => 'stop'}}] + expected
  end
end