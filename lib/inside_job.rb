require 'inside_job_ext'
require "inside_job/version"
require "inside_job/callback"

module InsideJob

  def self.init
    child_pid = Process.fork do
      InsideJob.init_subscriber
      InsideJob.wait_for_producer
      InsideJob.handle_events(Callback.new)
    end

    if child_pid
      InsideJob.init_producer(child_pid)
      InsideJob.wait_for_subscriber
    end
  end

end
