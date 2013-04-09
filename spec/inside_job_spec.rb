require 'spec_helper'

describe InsideJob do

  it "doesn't segfault" do
    trace_for {
      # noop
    }.should have_produced_lines []
  end

  describe "wall timing" do

    it "produces wall times that are always increasing" do
      trace_for {
        " thing ".strip
      }.wall_times.should be_always_increasing
    end

  end

  describe "cpu timing" do

    it "produces cpu times that are always increasing" do
      trace_for {
        " thing ".strip
      }.cpu_times.should be_always_increasing
    end

  end

  it "traces simple things" do
    trace_for {
      " thing ".strip
    }.should have_produced_lines [
      {:call=>
        {:file => "inside_job_spec.rb",
         :line => 33,
         :class => "String",
         :method => "strip"}},
      {:return=>
        {:file => "inside_job_spec.rb",
         :line => 33,
         :class => "String",
         :method => "strip"}}
    ]
  end

end