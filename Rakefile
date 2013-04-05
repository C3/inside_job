require "bundler/gem_tasks"
require 'rake/testtask'
require 'rake/clean'
require 'rake/extensiontask'
require 'rspec/core/rake_task'

RSpec::Core::RakeTask.new(:spec)
Rake::ExtensionTask.new('inside_job')

task :spec => :compile

desc "Run tests"
task :default => :spec
