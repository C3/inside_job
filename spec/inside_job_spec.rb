require 'spec_helper'

describe InsideJob do

  it "doesn't segfault" do
    spying_on {
      # noop
    }.should produce_trace []
  end

end