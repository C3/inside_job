require 'spec_helper'

describe InsideJob do

  describe "wall timing" do

    it "produces wall times that are always increasing" do
      trace_for {
        " thing ".strip
      }.should have_always_increasing_wall_clock_values
    end

  end

  describe "cpu timing" do

    it "produces cpu times that are always increasing" do
      trace_for {
        " thing ".strip
      }.should have_always_increasing_cpu_clock_values
    end

  end

  it "traces simple things" do
    trace_for {
      " thing ".strip
    }.should produce(
      call_tree do
        method_call "String", "strip", "inside_job_spec.rb", 27
      end
    )
  end

  describe "class names" do

    it "fetches the class name ignoring inspect overrides" do
      class Special
        def omg
          "omg"
        end

        def self.inspect
          " thing ".strip
        end
      end

      trace_for {
        Special.new.omg
      }.should produce(
        call_tree do
          method_call "Class", "new", "inside_job_spec.rb", 49 do
            method_call "BasicObject", "initialize", "inside_job_spec.rb", 49 do
              method_call "Special", "omg", "inside_job_spec.rb", 49
            end
          end
        end
      )
    end

  end

end