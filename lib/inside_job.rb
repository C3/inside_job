# -*- encoding : utf-8 -*-
require 'inside_job_ext'
require "inside_job/version"
require "inside_job/callback"

module InsideJob

  def self.init
    subscriber_pid = Process.fork do
      InsideJob::Subscriber.init
      InsideJob::Subscriber.wait_for_publisher
      InsideJob::Subscriber.handle_events(Callback.new)
    end

    if subscriber_pid
      InsideJob::Publisher.init(subscriber_pid)
      InsideJob::Publisher.wait_for_subscriber
    end
  end

end
