# -*- encoding : utf-8 -*-
class Callback

  def start_event(output_file_name)
    @file = File.open(output_file_name, 'w')
  end

  def stop_event
    @file.close
  end

  def call_event(klass, method, wall_clock, cpu_clock)
    @file.puts("call: #{klass} #{method} #{wall_clock} #{cpu_clock}")
  end

  def return_event(wall_clock, cpu_clock)
    @file.puts("return: #{wall_clock} #{cpu_clock}")
  end

end
